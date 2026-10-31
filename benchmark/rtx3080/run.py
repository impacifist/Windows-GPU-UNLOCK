# SPDX-License-Identifier: GPL-2.0-only
"""Ordinary CUDA runner: default is offline; only --execute may query/run GPUs."""
from __future__ import annotations
import argparse
import csv
import hashlib
import io
import json
import math
import os
from pathlib import Path, PurePosixPath
import re
import secrets
import shutil
import stat
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from datetime import datetime, timezone
import result_protocol

PROTOCOL = 3
EXPECTED_NAME = "NVIDIA GeForce RTX 3080"
EXPECTED_PCI = "00000000:07:00.0"
ARTIFACTS = frozenset({"bin/rtx3080-baseline.exe", "bin/cublas64_12.dll", "bin/cublasLt64_12.dll"})
SOURCES = frozenset("benchmark/rtx3080/" + n for n in
    ("baseline.cu", "numerics.hpp", "result_protocol.py", "run.py", "build.ps1", "setup.ps1", "packages-lock.json", "package-tools.ps1"))
FIELDS = ("name","pci.bus_id","pci.device_id","pci.sub_device_id","memory.total",
    "driver_version","utilization.gpu","memory.used","temperature.gpu","power.limit",
    "clocks.max.sm","clocks.max.memory","clocks.current.sm","clocks.current.memory")
STDOUT_LIMIT = 2 * 1024 * 1024
STDERR_LIMIT = 64 * 1024
MIN_HOST_MEMORY = 1024**3
ENTRY_PROFILES = {
    "quiet":{"max_utilization_percent":5,"max_memory_used_mib":2048,"max_temperature_celsius":65},
    "desktop":{"max_utilization_percent":5,"max_memory_used_mib":3072,"max_temperature_celsius":65},
}
REPARSE = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)

class RunnerError(ValueError):
    """Short, path-free error code."""
    def __init__(self, code):
        self.code = code
        super().__init__(code)

@dataclass(frozen=True)
class ProcessResult:
    returncode: int
    stdout: str
    stderr: str
    elapsed_seconds: float

def utc_now():
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")

def _unique(pairs):
    result = {}
    for k,v in pairs:
        if k in result:
            raise RunnerError("duplicate_json_key")
        result[k] = v
    return result

def _bad(_):
    raise RunnerError("nonfinite_json_number")

def _finite_number(value):
    if type(value) not in (int,float):
        return False
    try:
        return math.isfinite(value)
    except OverflowError:
        return False

def _finite_tree(x):
    if isinstance(x,float) and not math.isfinite(x):
        raise RunnerError("nonfinite_json_number")
    for v in x.values() if isinstance(x,dict) else x if isinstance(x,list) else ():
        _finite_tree(v)

def load_json(text):
    try:
        x = json.loads(text,object_pairs_hook=_unique,parse_constant=_bad)
        _finite_tree(x)
    except RunnerError:
        raise
    except (ValueError, UnicodeError, RecursionError) as e:
        raise RunnerError("invalid_json") from e
    if not isinstance(x,dict):
        raise RunnerError("json_object_required")
    return x

def _parts(s):
    if not isinstance(s,str) or not s or len(s)>512:
        raise RunnerError("invalid_relative_path")
    p=s.split("/")
    reserved={"CON","PRN","AUX","NUL"}|{f"{a}{n}" for a in ("COM","LPT") for n in range(1,10)}
    if ("\\" in s or ":" in s or PurePosixPath(s).is_absolute()
        or any(ord(c)<32 for c in s) or any(c in s for c in '*?"<>|')
        or any(v in ("",".","..") or v.endswith((" ",".")) or v.split(".")[0].upper() in reserved for v in p)):
        raise RunnerError("invalid_relative_path")
    return p

def _stat(p):
    try:
        x=p.lstat()
    except OSError as e:
        raise RunnerError("required_file_unavailable") from e
    if stat.S_ISLNK(x.st_mode) or getattr(x,"st_file_attributes",0)&REPARSE:
        raise RunnerError("symlink_or_reparse_path")
    return x

def checked_file(base,rel,limit=1024**3):
    base=Path(base).absolute()
    current=Path(base.anchor)
    for part in base.parts[1:]:
        current/=part
        if not stat.S_ISDIR(_stat(current).st_mode):
            raise RunnerError("invalid_root_directory")
    parts=_parts(rel)
    for i,part in enumerate(parts):
        current/=part
        info=_stat(current)
        if i<len(parts)-1 and not stat.S_ISDIR(info.st_mode):
            raise RunnerError("invalid_parent_directory")
    if not stat.S_ISREG(info.st_mode) or not 0<=info.st_size<=limit:
        raise RunnerError("invalid_file_type_or_size")
    try:
        current.resolve(strict=True).relative_to(base.resolve(strict=True))
    except (OSError,ValueError) as e:
        raise RunnerError("path_escapes_root") from e
    return current

def sha256(path,limit=1024**3):
    h=hashlib.sha256()
    try:
        before=path.stat()
        if before.st_size>limit:
            raise RunnerError("file_size_limit")
        n=0
        with path.open("rb") as f:
            for block in iter(lambda:f.read(1024*1024),b""):
                n+=len(block)
                if n>limit:
                    raise RunnerError("file_size_limit")
                h.update(block)
        after=path.stat()
    except OSError as e:
        raise RunnerError("file_read_failed") from e
    if (before.st_size,before.st_mtime_ns,before.st_ino)!=(after.st_size,after.st_mtime_ns,after.st_ino) or n!=before.st_size:
        raise RunnerError("file_changed_during_hash")
    return h.hexdigest()

def _entries(entries,expected,base,limit):
    if not isinstance(entries,list) or len(entries)!=len(expected):
        raise RunnerError("manifest_entry_set_mismatch")
    names=set(); inodes=set(); result=[]
    for e in entries:
        if not isinstance(e,dict) or set(e)!={"path","sha256"}:
            raise RunnerError("invalid_manifest_entry")
        key="/".join(_parts(e["path"])).casefold()
        if key in names:
            raise RunnerError("duplicate_manifest_path")
        names.add(key)
        if not isinstance(e["sha256"],str) or not re.fullmatch(r"[a-fA-F0-9]{64}",e["sha256"]):
            raise RunnerError("invalid_sha256")
        path=checked_file(base,e["path"],limit)
        info=path.stat(); inode=(info.st_dev,info.st_ino)
        if info.st_ino and inode in inodes:
            raise RunnerError("aliased_manifest_files")
        inodes.add(inode)
        actual=sha256(path,limit)
        if actual!=e["sha256"].lower():
            raise RunnerError("manifest_hash_mismatch")
        result.append({"path":e["path"],"sha256":actual})
    if {e["path"] for e in result}!=expected:
        raise RunnerError("manifest_entry_set_mismatch")
    return sorted(result,key=lambda e:e["path"])

def _toolchain(t):
    fields={"cuda_toolkit","nvcc","cublas","msvc_toolset","target_arch","compiler_options",
        "msvc_compiler_version","msvc_compiler_sha256"}
    if not isinstance(t,dict) or set(t)!=fields:
        raise RunnerError("invalid_toolchain_metadata")
    for key,value in {"cuda_toolkit":"12.6.0","nvcc":"12.6.20","cublas":"12.6.0.22","target_arch":"sm_86"}.items():
        if t[key]!=value:
            raise RunnerError("unexpected_toolchain")
    if not isinstance(t["msvc_toolset"],str) or not re.fullmatch(r"14\.[23]\d\.\d+(?:\.\d+)?",t["msvc_toolset"]):
        raise RunnerError("unsupported_host_compiler_family")
    version=t["msvc_compiler_version"]
    if not isinstance(version,str) or not re.fullmatch(r"19\.[23]\d\.\d+\.\d+",version):
        raise RunnerError("invalid_host_compiler_provenance")
    if version.split(".")[1]!=t["msvc_toolset"].split(".")[1]:
        raise RunnerError("host_compiler_toolset_mismatch")
    if not isinstance(t["msvc_compiler_sha256"],str) or not re.fullmatch(r"[0-9a-f]{64}",t["msvc_compiler_sha256"]):
        raise RunnerError("invalid_host_compiler_provenance")
    opts=t["compiler_options"]
    if not isinstance(opts,list) or not opts or len(opts)>32:
        raise RunnerError("invalid_compiler_options")
    for v in opts:
        if (not isinstance(v,str) or not re.fullmatch(r"[-A-Za-z0-9_+.,=/ ]{1,128}",v)
            or ".." in v or v.count("/")>1 or v.startswith(("-I","-L","/I","/LIBPATH"))):
            raise RunnerError("unsafe_compiler_option_metadata")
    return dict(t)

def verify_manifest(root):
    root=Path(root).absolute()
    if root.name!="rtx3080" or root.parent.name!="benchmark":
        raise RunnerError("unexpected_project_layout")
    path=checked_file(root,"build-manifest.json",256*1024)
    try:
        raw=path.read_bytes()
        if len(raw)>256*1024:
            raise RunnerError("manifest_size_limit")
        d=load_json(raw.decode("utf-8-sig"))
    except (OSError,UnicodeError) as e:
        raise RunnerError("manifest_read_failed") from e
    req={"schema_version","benchmark_protocol","source_commit","toolchain","artifacts","sources"}
    if not req.issubset(d) or set(d)-req-{"source_dirty"}:
        raise RunnerError("invalid_manifest_schema")
    if type(d["schema_version"]) is not int or d["schema_version"]!=1:
        raise RunnerError("invalid_manifest_schema")
    if type(d["benchmark_protocol"]) is not int or d["benchmark_protocol"]!=PROTOCOL:
        raise RunnerError("invalid_manifest_protocol")
    if not isinstance(d["source_commit"],str) or not re.fullmatch(r"[a-fA-F0-9]{40}",d["source_commit"]):
        raise RunnerError("invalid_source_commit")
    if "source_dirty" in d and type(d["source_dirty"]) is not bool:
        raise RunnerError("invalid_source_dirty_flag")
    result={"schema_version":1,"benchmark_protocol":PROTOCOL,
        "source_commit":d["source_commit"].lower(),"source_dirty":d.get("source_dirty"),
        "toolchain":_toolchain(d["toolchain"]),
        "artifacts":_entries(d["artifacts"],ARTIFACTS,root,1024**3),
        "sources":_entries(d["sources"],SOURCES,root.parents[1],4*1024**2)}
    return {"root":root,"manifest_sha256":hashlib.sha256(raw).hexdigest(),"data":result}

def _stop_owned_child(p):
    if p.poll() is None:
        p.terminate()
        try:
            p.wait(timeout=0.5)
        except subprocess.TimeoutExpired:
            p.kill()
            p.wait(timeout=1)

def run_process(argv,*,timeout_seconds,stdout_limit,stderr_limit,cwd):
    """Bound pipes and lifetime; terminate only this directly owned child."""
    if not _finite_number(timeout_seconds) or timeout_seconds<=0:
        raise RunnerError("process_deadline_exhausted")
    started=time.monotonic()
    try:
        p=subprocess.Popen(list(argv),cwd=str(cwd),stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,stderr=subprocess.PIPE,
            creationflags=getattr(subprocess,"CREATE_NO_WINDOW",0) if os.name=="nt" else 0)
    except OSError as e:
        raise RunnerError("process_start_failed") from e
    overflow=threading.Event(); read_error=threading.Event()
    buffers=[bytearray(),bytearray()]
    def reader(stream,buffer,limit):
        try:
            while True:
                block=stream.read1(8192)
                if not block:
                    break
                remaining=max(0,limit-len(buffer))
                buffer.extend(block[:remaining])
                if len(block)>remaining:
                    overflow.set()
        except (OSError,ValueError):
            read_error.set()
    threads=[threading.Thread(target=reader,args=(p.stdout,buffers[0],stdout_limit),daemon=True),
        threading.Thread(target=reader,args=(p.stderr,buffers[1],stderr_limit),daemon=True)]
    for t in threads:
        t.start()
    try:
        while p.poll() is None:
            if overflow.is_set():
                raise RunnerError("process_output_limit")
            remaining=timeout_seconds-(time.monotonic()-started)
            if remaining<=0:
                raise RunnerError("process_timeout")
            try:
                p.wait(timeout=min(0.025,remaining))
            except subprocess.TimeoutExpired:
                pass
        for t in threads:
            t.join(timeout=0.5)
        if any(t.is_alive() for t in threads):
            raise RunnerError("process_pipe_did_not_close")
        if overflow.is_set():
            raise RunnerError("process_output_limit")
        if read_error.is_set():
            raise RunnerError("process_pipe_read_failed")
        if time.monotonic()-started>timeout_seconds:
            raise RunnerError("process_timeout")
        try:
            out,err=[bytes(b).decode("utf-8-sig") for b in buffers]
        except UnicodeError as e:
            raise RunnerError("process_output_not_utf8") from e
        return ProcessResult(p.returncode,out,err,time.monotonic()-started)
    except BaseException:
        _stop_owned_child(p)
        raise
    finally:
        for t in threads:
            t.join(timeout=0.5)
        if not any(t.is_alive() for t in threads):
            p.stdout.close(); p.stderr.close()

def _number(s,optional=False):
    s=s.strip()
    if optional and s in ("","N/A","[N/A]","Not Supported","[Not Supported]"):
        return None
    try:
        v=float(s)
    except (ValueError,TypeError) as e:
        raise RunnerError("missing_or_invalid_telemetry") from e
    if not math.isfinite(v):
        raise RunnerError("nonfinite_telemetry")
    return int(v) if v.is_integer() else v

def canonical_pci(s):
    m=re.fullmatch(r"([a-fA-F0-9]{4}|[a-fA-F0-9]{8}):([a-fA-F0-9]{2}):([a-fA-F0-9]{2})\.([0-7])",s.strip()) if isinstance(s,str) else None
    if not m:
        raise RunnerError("invalid_pci_location")
    return f"{int(m[1],16):08x}:{int(m[2],16):02x}:{int(m[3],16):02x}.{m[4]}"

def parse_telemetry(text):
    try:
        rows=[row for row in csv.reader(io.StringIO(text),strict=True) if row]
    except csv.Error as e:
        raise RunnerError("invalid_telemetry_csv") from e
    if len(rows)!=1 or len(rows[0])!=len(FIELDS):
        raise RunnerError("exactly_one_gpu_required")
    d=dict(zip(FIELDS,(v.strip() for v in rows[0])))
    if d["name"]!=EXPECTED_NAME or canonical_pci(d["pci.bus_id"])!=EXPECTED_PCI:
        raise RunnerError("unexpected_gpu_identity")
    for key,value in (("pci.device_id",0x221610DE),("pci.sub_device_id",0x403F1458)):
        if not re.fullmatch(r"0x[a-fA-F0-9]{8}",d[key]) or int(d[key],16)!=value:
            raise RunnerError("unexpected_gpu_identity")
    if not re.fullmatch(r"\d{3,4}\.\d{2}",d["driver_version"]):
        raise RunnerError("invalid_driver_version")
    n={k:_number(d[k]) for k in FIELDS[6:12]}
    memory=_number(d["memory.total"])
    actual_sm=_number(d["clocks.current.sm"],True)
    actual_mem=_number(d["clocks.current.memory"],True)
    if (memory!=10240 or not 0<=n["utilization.gpu"]<=100 or not 0<=n["memory.used"]<=memory
        or not 0<=n["temperature.gpu"]<=150 or any(n[k]<=0 for k in ("power.limit","clocks.max.sm","clocks.max.memory"))
        or (actual_sm is not None and actual_sm<0) or (actual_mem is not None and actual_mem<0)):
        raise RunnerError("telemetry_range_mismatch")
    return {"gpu_name":EXPECTED_NAME,"pci_bus_id":EXPECTED_PCI,"pci_device_id":"10de:2216",
        "pci_subsystem_id":"1458:403f","memory_total_mib":memory,"driver_version":d["driver_version"],
        "utilization_percent":n["utilization.gpu"],"memory_used_mib":n["memory.used"],
        "temperature_celsius":n["temperature.gpu"],"power_limit_watts":n["power.limit"],
        "maximum_sm_clock_mhz":n["clocks.max.sm"],"maximum_memory_clock_mhz":n["clocks.max.memory"],
        "operating_sm_clock_mhz":actual_sm,"operating_memory_clock_mhz":actual_mem}

def settings_identity(s):
    return {k:s[k] for k in ("gpu_name","pci_bus_id","pci_device_id","pci_subsystem_id",
        "memory_total_mib","driver_version","power_limit_watts","maximum_sm_clock_mhz","maximum_memory_clock_mhz")}

def entry_limits(profile):
    if type(profile) is not str or profile not in ENTRY_PROFILES:
        raise RunnerError("invalid_entry_profile")
    return dict(ENTRY_PROFILES[profile])

def require_entry(s,profile="quiet"):
    limits=entry_limits(profile)
    if (s["utilization_percent"]>limits["max_utilization_percent"]
        or s["memory_used_mib"]>limits["max_memory_used_mib"]
        or s["temperature_celsius"]>limits["max_temperature_celsius"]):
        raise RunnerError("entry_not_quiet" if profile=="quiet" else "entry_profile_limits_exceeded")

def require_quiet(s):
    require_entry(s,"quiet")

def _ok(p):
    if p.returncode!=0:
        raise RunnerError("child_nonzero_exit")
    if p.stderr.strip():
        raise RunnerError("child_reported_stderr")

def _remaining(deadline,clock,cap):
    value=deadline-clock()
    if value<=0:
        raise RunnerError("total_deadline_exhausted")
    return min(value,cap)

def _query(nvidia,root,launcher,deadline,clock,utc):
    stamp=utc()
    p=launcher([str(nvidia),"--query-gpu="+",".join(FIELDS),"--format=csv,noheader,nounits"],
        timeout_seconds=_remaining(deadline,clock,10),stdout_limit=16*1024,
        stderr_limit=STDERR_LIMIT,cwd=root)
    _ok(p)
    s=parse_telemetry(p.stdout)
    s["observed_at_utc"]=stamp
    return s

def atomic_json(path,value):
    try:
        raw=(json.dumps(value,indent=2,sort_keys=True,allow_nan=False)+"\n").encode("utf-8")
    except (ValueError,TypeError,RecursionError) as e:
        raise RunnerError("result_not_serializable") from e
    if len(raw)>4*1024**2:
        raise RunnerError("result_size_limit")
    temporary=path.with_name(path.name+".pending")
    try:
        with temporary.open("xb") as f:
            f.write(raw); f.flush(); os.fsync(f.fileno())
        os.replace(temporary,path)
    except OSError as e:
        raise RunnerError("result_write_failed") from e

def _results_directory(root):
    parent=root/"results"
    if parent.exists():
        if not stat.S_ISDIR(_stat(parent).st_mode):
            raise RunnerError("invalid_results_directory")
    else:
        parent.mkdir()
    for _ in range(4):
        label=datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")+"-"+secrets.token_hex(4)
        p=parent/label
        try:
            p.mkdir()
            return p
        except FileExistsError:
            pass
    raise RunnerError("result_directory_collision")

def _trial_identity(t):
    try:
        gpu=t["gpu"]
        if gpu["name"]!=EXPECTED_NAME or canonical_pci(gpu["pci_bus_id"])!=EXPECTED_PCI:
            raise RunnerError("native_gpu_identity_mismatch")
        runtime=t["runtime"]
        if any(type(runtime[key]) is not int or runtime[key]!=expected for key,expected in
               (("cuda_runtime",12060),("cublas_version",120600))):
            raise RunnerError("native_runtime_mismatch")
        if type(runtime["cuda_driver"]) is not int or runtime["cuda_driver"]<12060:
            raise RunnerError("native_driver_api_too_old")
    except (KeyError,TypeError) as e:
        raise RunnerError("native_identity_or_runtime_missing") from e

def capture_environment():
    values={key:os.environ.get(key) for key in
        ("NVIDIA_TF32_OVERRIDE","CUBLAS_WORKSPACE_CONFIG","CUDA_VISIBLE_DEVICES","CUDA_DEVICE_ORDER")}
    if values["NVIDIA_TF32_OVERRIDE"]:
        raise RunnerError("tf32_override_present")
    try:
        return result_protocol.validate_environment(values)
    except ValueError as e:
        raise RunnerError("unsupported_environment") from e

def available_host_memory_bytes():
    """Read available physical RAM without starting another process."""
    if os.name!="nt":
        raise RunnerError("host_memory_query_unsupported")
    import ctypes
    class MemoryStatus(ctypes.Structure):
        _fields_=[("length",ctypes.c_uint32),("load",ctypes.c_uint32)]+[
            (name,ctypes.c_uint64) for name in
            ("total_physical","available_physical","total_pagefile","available_pagefile",
                "total_virtual","available_virtual","available_extended_virtual")]
    try:
        status=MemoryStatus()
        status.length=ctypes.sizeof(status)
        query=ctypes.WinDLL("kernel32",use_last_error=True).GlobalMemoryStatusEx
        query.argtypes=[ctypes.POINTER(MemoryStatus)]
        query.restype=ctypes.c_int
        if not query(ctypes.byref(status)) or not 0<status.available_physical<=status.total_physical:
            raise RunnerError("host_memory_query_failed")
        return int(status.available_physical)
    except (OSError,AttributeError) as e:
        raise RunnerError("host_memory_query_failed") from e

def require_host_memory(value):
    if type(value) is not int or not MIN_HOST_MEMORY<=value<2**64:
        raise RunnerError("insufficient_or_unknown_host_memory")

def execute_baseline(verified,*,profile="quiet",launcher=run_process,nvidia=None,clock=time.monotonic,utc=utc_now):
    limits=entry_limits(profile)
    environment=capture_environment()
    nvidia=nvidia or shutil.which("nvidia-smi.exe")
    if not nvidia:
        raise RunnerError("nvidia_smi_not_found")
    root=verified["root"]; deadline=clock()+360
    directory=_results_directory(root)
    trials=[]; observations=[]; expected=None; phase="manifest"
    try:
        for index in range(1,4):
            phase="manifest"
            if not _same_json(capture_environment(),environment):
                raise RunnerError("environment_changed_during_run")
            current=verify_manifest(root)
            if current!=verified:
                raise RunnerError("manifest_changed_during_run")
            _remaining(deadline,clock,120)
            phase="pre_query"
            before=_query(nvidia,root,launcher,deadline,clock,utc)
            require_entry(before,profile)
            if expected is None:
                expected=settings_identity(before)
            elif settings_identity(before)!=expected:
                raise RunnerError("settings_changed_between_trials")
            if verify_manifest(root)!=verified:
                raise RunnerError("manifest_changed_during_run")
            if not _same_json(capture_environment(),environment):
                raise RunnerError("environment_changed_during_run")
            phase="host_memory_preflight"
            memory_observed=utc()
            available_memory=available_host_memory_bytes()
            require_host_memory(available_memory)
            phase="native_trial"; started=utc()
            p=launcher([str(root/"bin"/"rtx3080-baseline.exe"),"--run","--expected-pci",EXPECTED_PCI],
                timeout_seconds=_remaining(deadline,clock,120),stdout_limit=STDOUT_LIMIT,
                stderr_limit=STDERR_LIMIT,cwd=root)
            ended=utc(); _ok(p)
            phase="trial_validation"
            try:
                trial=result_protocol.load_text(p.stdout)
                result_protocol.validate_trial(trial)
            except (ValueError,TypeError,KeyError,RecursionError,OverflowError) as e:
                raise RunnerError("invalid_native_trial") from e
            _trial_identity(trial)
            if not _same_json(trial["environment"],environment):
                raise RunnerError("native_environment_mismatch")
            phase="post_query"
            after=_query(nvidia,root,launcher,deadline,clock,utc)
            if settings_identity(after)!=expected:
                raise RunnerError("settings_changed_during_trial")
            observations.append({"trial_index":index,"fresh_process":True,
                "started_at_utc":started,"ended_at_utc":ended,"elapsed_seconds":p.elapsed_seconds,
                "available_host_memory_bytes":available_memory,"host_memory_observed_at_utc":memory_observed,
                "before":before,"after":after})
            atomic_json(directory/f"trial-{index}.json",trial)
            trials.append(trial)
        phase="final_validation"
        if verify_manifest(root)!=verified:
            raise RunnerError("manifest_changed_during_run")
        if not _same_json(capture_environment(),environment):
            raise RunnerError("environment_changed_during_run")
        summary=result_protocol.summarize_trials(trials)
        data=verified["data"]
        context={"schema_version":2,"benchmark_protocol":PROTOCOL,
            "entry_profile":profile,"entry_limits":limits,
            "build_manifest_sha256":verified["manifest_sha256"],
            "source_commit":data["source_commit"],"source_dirty":data["source_dirty"],
            "toolchain":data["toolchain"],"artifacts":data["artifacts"],"sources":data["sources"],
            "environment":environment,
            "execution":{"fresh_process_count":3,"sequential":True,
                "native_arguments":["--run","--expected-pci",EXPECTED_PCI],
                "per_process_deadline_seconds":120,"overall_deadline_seconds":360,
                "query_deadline_seconds":10,"stdout_limit_bytes":STDOUT_LIMIT,
                "minimum_available_host_memory_bytes":MIN_HOST_MEMORY,
                "stderr_limit_bytes":STDERR_LIMIT,"tf32_override_present":False},
            "trials":observations,
            "limitations":[
                "Ordinary computation on current settings; factory stock state is not established.",
                "Matrix results do not establish firmware compatibility, unlock state, gaming or application gains.",
                "SHA-256 records detect changes; they are not signed hardware attestation.",
                "This runner launches separate native processes; JSON alone cannot prove freshness.",
                "A process deadline cannot guarantee recovery from a driver or hardware hang."]}
        _validate_context(context,trials,verified["manifest_sha256"])
        _remaining(deadline,clock,120)
        atomic_json(directory/"context.json",context)
        atomic_json(directory/"summary.json",summary)
        completion={"schema_version":1,"benchmark_protocol":PROTOCOL,"status":"complete",
            "trial_count":3,"completed_at_utc":utc(),"build_manifest_sha256":verified["manifest_sha256"],
            "files":[{"path":n,"sha256":sha256(directory/n,4*1024**2)}
                for n in ("context.json","summary.json","trial-1.json","trial-2.json","trial-3.json")]}
        _remaining(deadline,clock,120)
        # Completion is the final file mutation, only after all three trials pass.
        atomic_json(directory/"complete.json",completion)
        return {"status":"complete","benchmark_protocol":PROTOCOL,"gpu_workload_run":True,
            "entry_profile":profile,"entry_limits":limits,
            "trial_count":3,"result_directory":directory.relative_to(root).as_posix()}
    except BaseException as e:
        code=e.code if isinstance(e,RunnerError) else "cancelled" if isinstance(e,KeyboardInterrupt) else "validation_or_io_failure"
        try:
            atomic_json(directory/"failure.json",{"schema_version":1,"benchmark_protocol":PROTOCOL,
                "status":"incomplete","phase":phase,"reason":code,"validated_trials":len(trials),
                "observed_at_utc":utc(),"completion_record_written":False})
        except (OSError,RunnerError):
            pass
        if isinstance(e,(KeyboardInterrupt,SystemExit,RunnerError)):
            raise
        raise RunnerError("validation_or_io_failure") from e

def run(root,*,execute=False,profile="quiet",launcher=run_process,nvidia=None):
    limits=entry_limits(profile)
    verified=verify_manifest(root)
    if not execute:
        d=verified["data"]
        return {"schema_version":1,"benchmark_protocol":PROTOCOL,"status":"offline-artifacts-verified",
            "entry_profile":profile,"entry_limits":limits,
            "gpu_workload_run":False,"native_process_run":False,"execution_requested":False,
            "build_manifest_sha256":verified["manifest_sha256"],
            "source_commit":d["source_commit"],"source_dirty":d["source_dirty"],
            "toolchain":d["toolchain"],"artifacts":d["artifacts"],"sources":d["sources"],
            "limitation":"GPU identity, idle state and runtime operation were not queried; --execute is required for computation."}
    return execute_baseline(verified,profile=profile,launcher=launcher,nvidia=nvidia)

def _timestamp(value):
    if not isinstance(value,str) or not value.endswith("Z"):
        raise RunnerError("invalid_result_timestamp")
    try:
        stamp=datetime.fromisoformat(value[:-1]+"+00:00")
        if stamp.utcoffset().total_seconds()!=0:
            raise ValueError("not UTC")
        return stamp
    except (ValueError,AttributeError,OverflowError) as e:
        raise RunnerError("invalid_result_timestamp") from e

def _metadata_entries(entries,expected):
    if not isinstance(entries,list) or len(entries)!=len(expected):
        raise RunnerError("invalid_context_manifest")
    names=set()
    for entry in entries:
        if not isinstance(entry,dict) or set(entry)!={"path","sha256"}:
            raise RunnerError("invalid_context_manifest")
        name="/".join(_parts(entry["path"]))
        if name.casefold() in names:
            raise RunnerError("duplicate_manifest_path")
        names.add(name.casefold())
        if name not in expected or not isinstance(entry["sha256"],str) or not re.fullmatch(r"[0-9a-fA-F]{64}",entry["sha256"]):
            raise RunnerError("invalid_context_manifest")

def _saved_snapshot(s):
    numeric=("memory_total_mib","utilization_percent","memory_used_mib","temperature_celsius",
        "power_limit_watts","maximum_sm_clock_mhz","maximum_memory_clock_mhz")
    optional=("operating_sm_clock_mhz","operating_memory_clock_mhz")
    required=set(numeric+optional+("gpu_name","pci_bus_id","pci_device_id","pci_subsystem_id",
        "driver_version","observed_at_utc"))
    if not isinstance(s,dict) or set(s)!=required:
        raise RunnerError("invalid_saved_telemetry")
    if s["pci_device_id"]!="10de:2216" or s["pci_subsystem_id"]!="1458:403f":
        raise RunnerError("unexpected_gpu_identity")
    for key in numeric+optional:
        if key in optional and s[key] is None:
            continue
        if not _finite_number(s[key]):
            raise RunnerError("invalid_saved_telemetry")
    _timestamp(s["observed_at_utc"])
    values=(s["gpu_name"],s["pci_bus_id"],"0x221610DE","0x403F1458",s["memory_total_mib"],
        s["driver_version"],s["utilization_percent"],s["memory_used_mib"],s["temperature_celsius"],
        s["power_limit_watts"],s["maximum_sm_clock_mhz"],s["maximum_memory_clock_mhz"],
        s["operating_sm_clock_mhz"],s["operating_memory_clock_mhz"])
    return parse_telemetry(", ".join("N/A" if x is None else str(x) for x in values))

def _same_json(a,b):
    return json.dumps(a,sort_keys=True,separators=(",",":"),allow_nan=False)==json.dumps(b,sort_keys=True,separators=(",",":"),allow_nan=False)

def _read_bundle_json(directory,name,expected_hash=None):
    path=checked_file(directory,name,4*1024**2)
    try:
        raw=path.read_bytes()
    except OSError as e:
        raise RunnerError("bundle_read_failed") from e
    if len(raw)>4*1024**2:
        raise RunnerError("bundle_size_limit")
    if expected_hash is not None and hashlib.sha256(raw).hexdigest()!=expected_hash.lower():
        raise RunnerError("bundle_hash_mismatch")
    try:
        return load_json(raw.decode("utf-8-sig"))
    except UnicodeError as e:
        raise RunnerError("invalid_bundle_encoding") from e


def _validate_context(context,trials,manifest_hash):
    fields={"schema_version","benchmark_protocol","build_manifest_sha256","source_commit",
        "source_dirty","toolchain","artifacts","sources","execution","trials","limitations","environment",
        "entry_profile","entry_limits"}
    if not isinstance(context,dict) or set(context)!=fields:
        raise RunnerError("invalid_context_schema")
    if type(context["schema_version"]) is not int or context["schema_version"]!=2 or type(context["benchmark_protocol"]) is not int or context["benchmark_protocol"]!=3:
        raise RunnerError("invalid_context_schema")
    limits=entry_limits(context["entry_profile"])
    if not _same_json(context["entry_limits"],limits):
        raise RunnerError("invalid_entry_limits")
    if context["build_manifest_sha256"]!=manifest_hash:
        raise RunnerError("context_manifest_hash_mismatch")
    if not isinstance(context["source_commit"],str) or not re.fullmatch(r"[0-9a-f]{40}",context["source_commit"]):
        raise RunnerError("invalid_source_commit")
    if context["source_dirty"] is not None and type(context["source_dirty"]) is not bool:
        raise RunnerError("invalid_source_dirty_flag")
    _toolchain(context["toolchain"])
    _metadata_entries(context["artifacts"],ARTIFACTS)
    _metadata_entries(context["sources"],SOURCES)
    try:
        environment=result_protocol.validate_environment(context["environment"])
    except ValueError as e:
        raise RunnerError("invalid_context_environment") from e
    policy={"fresh_process_count":3,"sequential":True,
        "native_arguments":["--run","--expected-pci",EXPECTED_PCI],
        "per_process_deadline_seconds":120,"overall_deadline_seconds":360,
        "query_deadline_seconds":10,"stdout_limit_bytes":STDOUT_LIMIT,
        "minimum_available_host_memory_bytes":MIN_HOST_MEMORY,
        "stderr_limit_bytes":STDERR_LIMIT,"tf32_override_present":False}
    if not _same_json(context["execution"],policy):
        raise RunnerError("invalid_execution_context")
    observations=context["trials"]
    if not isinstance(observations,list) or len(observations)!=3:
        raise RunnerError("invalid_context_trial_count")
    expected=None; previous=None
    for i,(observation,trial) in enumerate(zip(observations,trials),1):
        keys={"trial_index","fresh_process","started_at_utc","ended_at_utc","elapsed_seconds",
            "available_host_memory_bytes","host_memory_observed_at_utc","before","after"}
        if not isinstance(observation,dict) or set(observation)!=keys:
            raise RunnerError("invalid_trial_context")
        if type(observation["trial_index"]) is not int or observation["trial_index"]!=i or observation["fresh_process"] is not True:
            raise RunnerError("invalid_trial_context")
        elapsed=observation["elapsed_seconds"]
        if not _finite_number(elapsed) or not 0<elapsed<=120:
            raise RunnerError("invalid_trial_duration")
        start=_timestamp(observation["started_at_utc"]); end=_timestamp(observation["ended_at_utc"])
        memory_time=_timestamp(observation["host_memory_observed_at_utc"])
        require_host_memory(observation["available_host_memory_bytes"])
        pre_time=_timestamp(observation["before"].get("observed_at_utc")) if isinstance(observation["before"],dict) else None
        post_time=_timestamp(observation["after"].get("observed_at_utc")) if isinstance(observation["after"],dict) else None
        if pre_time is None or post_time is None or not pre_time<=memory_time<=start<=end<=post_time or (previous is not None and pre_time<previous):
            raise RunnerError("inconsistent_trial_timestamps")
        previous=post_time
        before=_saved_snapshot(observation["before"]); after=_saved_snapshot(observation["after"])
        require_entry(before,context["entry_profile"])
        if expected is None:
            expected=settings_identity(before)
        if settings_identity(before)!=expected or settings_identity(after)!=expected:
            raise RunnerError("saved_settings_changed")
        _trial_identity(trial)
        if not _same_json(trial["environment"],environment):
            raise RunnerError("native_environment_mismatch")
    if not isinstance(context["limitations"],list) or not all(isinstance(v,str) and len(v)<=500 for v in context["limitations"]):
        raise RunnerError("invalid_context_limitations")


def verify_completed_run(directory):
    """Verify saved JSON/hash consistency offline; this is not authenticity proof."""
    directory=Path(directory).absolute()
    marker=_read_bundle_json(directory,"complete.json")
    fields={"schema_version","benchmark_protocol","status","trial_count",
        "completed_at_utc","build_manifest_sha256","files"}
    if set(marker)!=fields or marker["status"]!="complete":
        raise RunnerError("invalid_completion_record")
    for key,value in (("schema_version",1),("benchmark_protocol",3),("trial_count",3)):
        if type(marker[key]) is not int or marker[key]!=value:
            raise RunnerError("invalid_completion_record")
    completed=_timestamp(marker["completed_at_utc"])
    manifest_hash=marker["build_manifest_sha256"]
    if not isinstance(manifest_hash,str) or not re.fullmatch(r"[0-9a-f]{64}",manifest_hash):
        raise RunnerError("invalid_completion_manifest_hash")
    expected={"context.json","summary.json","trial-1.json","trial-2.json","trial-3.json"}
    entries=_entries(marker["files"],expected,directory,4*1024**2)
    try:
        names={p.name for p in directory.iterdir()}
    except OSError as e:
        raise RunnerError("bundle_read_failed") from e
    if names!=expected|{"complete.json"}:
        raise RunnerError("unexpected_bundle_entry")
    records={e["path"]:_read_bundle_json(directory,e["path"],e["sha256"]) for e in entries}
    trials=[records[f"trial-{i}.json"] for i in range(1,4)]
    try:
        for trial in trials:
            result_protocol.validate_trial(trial)
        summary=result_protocol.summarize_trials(trials)
    except (ValueError,TypeError,KeyError,RecursionError,OverflowError) as e:
        raise RunnerError("invalid_saved_trial") from e
    context=records["context.json"]
    _validate_context(context,trials,manifest_hash)
    if not _same_json(records["summary.json"],summary):
        raise RunnerError("saved_summary_mismatch")
    first=_timestamp(context["trials"][0]["before"]["observed_at_utc"])
    last=_timestamp(context["trials"][-1]["after"]["observed_at_utc"])
    if not first<=last<=completed or (completed-first).total_seconds()>360:
        raise RunnerError("inconsistent_completion_timestamp")
    if not _same_json(marker,_read_bundle_json(directory,"complete.json")):
        raise RunnerError("completion_changed_during_verification")
    return {"schema_version":1,"benchmark_protocol":3,"status":"offline-run-verified",
        "entry_profile":context["entry_profile"],"entry_limits":dict(context["entry_limits"]),
        "native_process_run":False,"gpu_workload_run":False,"saved_trial_count":3,
        "build_manifest_sha256":manifest_hash,"source_commit":context["source_commit"],
        "summary":summary,
        "limitation":"JSON and file-hash consistency does not establish authenticity or physical execution."}

def _bundle_path(root,value):
    path=Path(value)
    if path.is_absolute():
        return path
    # A simple run label is under results; other paths are relative to this harness.
    return root/"results"/path if len(path.parts)==1 else root/path

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    modes=parser.add_mutually_exclusive_group()
    modes.add_argument("--execute",action="store_true",
        help="Explicitly request three bounded ordinary GPU runs after all checks.")
    modes.add_argument("--verify-run",metavar="RUN",
        help="Verify a completed saved bundle offline, without installed native artifacts.")
    parser.add_argument("--profile",choices=tuple(ENTRY_PROFILES),default="quiet",
        help="Entry profile for readiness or --execute (default: quiet). --verify-run uses the saved profile.")
    args=parser.parse_args(argv)
    try:
        root=Path(__file__).absolute().parent
        output=verify_completed_run(_bundle_path(root,args.verify_run)) if args.verify_run else run(root,execute=args.execute,profile=args.profile)
    except KeyboardInterrupt:
        print(json.dumps({"status":"incomplete","reason":"cancelled"}))
        return 130
    except (RunnerError,OSError) as e:
        print(json.dumps({"status":"refused","reason":e.code if isinstance(e,RunnerError) else "filesystem_error"}))
        return 2
    print(json.dumps(output,indent=2,sort_keys=True,allow_nan=False))
    return 0

if __name__=="__main__":
    raise SystemExit(main())
