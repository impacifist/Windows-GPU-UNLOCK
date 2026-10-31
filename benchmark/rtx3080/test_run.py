# SPDX-License-Identifier: GPL-2.0-only
"""Offline runner tests: mocked NVIDIA/CUDA; only small CPU Python children run."""
from copy import deepcopy
import json
import io
import os
from pathlib import Path
import sys
import tempfile
import time
import unittest
from unittest import mock
import run as runner
from test_result_protocol import make_trial

def telemetry(**changes):
    d=dict(zip(runner.FIELDS,(runner.EXPECTED_NAME,runner.EXPECTED_PCI,"0x221610DE",
        "0x403F1458","10240","610.88","0","100","32","370","2115","9501","210","405")))
    d.update(changes)
    return ", ".join(str(d[k]) for k in runner.FIELDS)+"\n"

class Fixture(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.repo=Path(self.temp.name)/"repo"
        self.root=self.repo/"benchmark"/"rtx3080"
        self.root.mkdir(parents=True)
        for rel in runner.SOURCES:
            p=self.repo/rel; p.parent.mkdir(parents=True,exist_ok=True)
            p.write_text("synthetic source "+rel,encoding="utf-8")
        for rel in runner.ARTIFACTS:
            p=self.root/rel; p.parent.mkdir(parents=True,exist_ok=True)
            p.write_bytes(("non-executable fixture "+rel).encode())
        self.manifest={"schema_version":1,"benchmark_protocol":3,"source_commit":"5c98f11eaafdcb7a2a06fb14fcdcde7297eb409e","source_dirty":True,
            "toolchain":{"cuda_toolkit":"12.6.0","nvcc":"12.6.20","cublas":"12.6.0.22",
                "msvc_toolset":"14.39.33519","target_arch":"sm_86",
                "msvc_compiler_version":"19.39.33523.0","msvc_compiler_sha256":"a"*64,
                "compiler_options":["-std=c++17","-O3","-arch=sm_86","--cudart=static"]},
            "artifacts":[{"path":p,"sha256":runner.sha256(self.root/p)} for p in sorted(runner.ARTIFACTS)],
            "sources":[{"path":p,"sha256":runner.sha256(self.repo/p)} for p in sorted(runner.SOURCES)]}
        self.save_manifest()
        self.env=mock.patch.dict(os.environ,{k:"" for k in
            ("NVIDIA_TF32_OVERRIDE","CUBLAS_WORKSPACE_CONFIG","CUDA_VISIBLE_DEVICES","CUDA_DEVICE_ORDER")})
        self.env.start()
        self.memory=mock.patch.object(runner,"available_host_memory_bytes",return_value=2*runner.MIN_HOST_MEMORY)
        self.memory.start()
    def tearDown(self):
        self.memory.stop(); self.env.stop(); self.temp.cleanup()
    def save_manifest(self):
        (self.root/"build-manifest.json").write_text(json.dumps(self.manifest),encoding="utf-8")
    def refusal(self,code=None):
        with self.assertRaises(runner.RunnerError) as caught:
            runner.run(self.root,execute=True,launcher=mock.Mock(side_effect=AssertionError("process started")),nvidia="mock-nvidia")
        if code:
            self.assertEqual(caught.exception.code,code)
    def successful_launcher(self,**telemetry_changes):
        self.launches=[]; self.native_count=0; self.query_count=0
        def launch(argv,**kw):
            self.launches.append((argv,kw))
            if "--run" in argv:
                self.native_count+=1
                self.assertEqual(argv[1:],["--run","--expected-pci",runner.EXPECTED_PCI])
                self.assertLessEqual(kw["timeout_seconds"],120)
                trial=make_trial(timing_scale=1+self.native_count/100)
                trial["environment"]=runner.capture_environment()
                return runner.ProcessResult(0,json.dumps(trial),"",0.01)
            self.query_count+=1
            self.assertLessEqual(kw["timeout_seconds"],10)
            self.assertIn("--format=csv,noheader,nounits",argv)
            return runner.ProcessResult(0,telemetry(**telemetry_changes),"",0.001)
        return launch
    def only_result(self):
        values=list((self.root/"results").iterdir())
        self.assertEqual(len(values),1)
        return values[0]

class ManifestTests(Fixture):
    def test_default_offline_without_queries_or_results(self):
        launch=mock.Mock(side_effect=AssertionError("must not launch"))
        with mock.patch.object(runner.subprocess,"Popen",side_effect=AssertionError("forbidden")):
            result=runner.run(self.root,launcher=launch)
        self.assertEqual(result["status"],"offline-artifacts-verified")
        self.assertEqual(result["entry_profile"],"quiet")
        self.assertEqual(result["entry_limits"]["max_memory_used_mib"],2048)
        self.assertFalse(result["gpu_workload_run"])
        self.assertFalse((self.root/"results").exists())
        launch.assert_not_called()
    def test_tampered_native_dll_and_source_refused_before_queries(self):
        for base,path in ((self.root,"bin/rtx3080-baseline.exe"),(self.root,"bin/cublasLt64_12.dll"),
                (self.repo,"benchmark/rtx3080/result_protocol.py")):
            with self.subTest(path=path):
                original=(base/path).read_bytes(); (base/path).write_bytes(b"modified")
                self.refusal("manifest_hash_mismatch"); (base/path).write_bytes(original)
    def test_missing_required_files(self):
        original=deepcopy(self.manifest)
        for section in ("artifacts","sources"):
            self.manifest=deepcopy(original); self.manifest[section].pop(); self.save_manifest()
            self.refusal("manifest_entry_set_mismatch")

    def test_duplicate_paths_case_insensitive(self):
        self.manifest["artifacts"][1]=dict(self.manifest["artifacts"][0])
        self.manifest["artifacts"][1]["path"]=self.manifest["artifacts"][0]["path"].upper()
        self.save_manifest(); self.refusal("duplicate_manifest_path")
    def test_ambiguous_and_absolute_paths(self):
        for p in ("../outside.exe","bin/../outside.exe","/absolute.exe",r"C:\private\outside.exe",
                r"bin\file.exe","bin//file.exe","bin/NUL","bin/file."):
            with self.subTest(path=p):
                self.manifest["artifacts"][0]["path"]=p; self.save_manifest()
                self.refusal("invalid_relative_path")
    def test_duplicate_json_and_nonfinite_values(self):
        p=self.root/"build-manifest.json"
        p.write_text('{"schema_version":1,"schema_version":1}',encoding="utf-8")
        self.refusal("duplicate_json_key")
        for token in ("NaN","Infinity","1e9999"):
            p.write_text('{"value":'+token+'}',encoding="utf-8")
            self.refusal("nonfinite_json_number")
    def test_oversized_integer_parser_error_is_sanitized(self):
        with self.assertRaisesRegex(runner.RunnerError,"invalid_json"):
            runner.load_json('{"value":'+"9"*5000+'}')
    def test_wrong_protocol_compiler_and_private_paths(self):
        original=deepcopy(self.manifest)
        changes=(lambda d:d.update(benchmark_protocol=True),
            lambda d:d["toolchain"].update(cuda_toolkit="13.0"),
            lambda d:d["toolchain"].update(msvc_toolset="14.44.35207"),
            lambda d:d["toolchain"].update(compiler_options=[r"C:\Users\private\compiler.exe"]))
        for change in changes:
            self.manifest=deepcopy(original); change(self.manifest); self.save_manifest(); self.refusal()
    def test_symlink_refused(self):
        p=self.root/"bin"/"cublas64_12.dll"; real=self.root/"actual-fixture.bin"; p.rename(real)
        try:
            p.symlink_to(real)
        except OSError:
            real.rename(p); self.skipTest("Test symlink privilege unavailable")
        self.refusal("symlink_or_reparse_path")
    def test_aliased_artifacts_refused(self):
        a=self.root/"bin"/"cublas64_12.dll"; b=self.root/"bin"/"cublasLt64_12.dll"; b.unlink()
        try:
            os.link(a,b)
        except OSError:
            self.skipTest("Test hard link unavailable")
        for e in self.manifest["artifacts"]:
            e["sha256"]=runner.sha256(self.root/e["path"])
        self.save_manifest(); self.refusal("aliased_manifest_files")
    def test_override_refused_without_process(self):
        with mock.patch.dict(os.environ,{"NVIDIA_TF32_OVERRIDE":"0"}):
            self.refusal("tf32_override_present")
    def test_both_documented_msvc_families_and_build_flags(self):
        for version in ("14.29.30133","14.39.33519"):
            with self.subTest(version=version):
                self.manifest["toolchain"]["msvc_toolset"]=version
                self.manifest["toolchain"]["msvc_compiler_version"]="19."+version.split(".")[1]+".33523.0"
                self.manifest["toolchain"]["compiler_options"]=[
                    "-std=c++17","-O3","-arch=sm_86","--cudart=static","-Xcompiler","/EHsc"]
                self.save_manifest()
                self.assertEqual(runner.run(self.root)["status"],"offline-artifacts-verified")
    def test_private_or_unsupported_environment_refused_before_queries(self):
        for key,value in (("CUDA_VISIBLE_DEVICES","GPU-private-uuid"),
                ("CUDA_VISIBLE_DEVICES","0,0"),("CUDA_VISIBLE_DEVICES","01"),
                ("CUDA_VISIBLE_DEVICES","64"),("CUDA_VISIBLE_DEVICES","0, 1"),
                ("CUBLAS_WORKSPACE_CONFIG",r"C:\Users\private"),
                ("CUDA_DEVICE_ORDER","other")):
            with self.subTest(key=key,value=value), mock.patch.dict(os.environ,{key:value}):
                self.refusal("unsupported_environment")
        self.assertFalse((self.root/"results").exists())
    def test_allowed_environment_is_recorded_without_transformation(self):
        values={"CUDA_VISIBLE_DEVICES":"0,63","CUDA_DEVICE_ORDER":"PCI_BUS_ID",
            "CUBLAS_WORKSPACE_CONFIG":":4096:8","NVIDIA_TF32_OVERRIDE":""}
        with mock.patch.dict(os.environ,values):
            self.assertEqual(runner.capture_environment(),values)
    def test_compiler_provenance_requires_matching_family_and_safe_hash(self):
        original=deepcopy(self.manifest)
        for change in ({"msvc_compiler_version":"19.44.35207.1"},
                {"msvc_compiler_version":"19.29.30133.0"},
                {"msvc_compiler_version":r"C:\Users\private\cl.exe"},
                {"msvc_compiler_sha256":"not-a-hash"}):
            self.manifest=deepcopy(original)
            self.manifest["toolchain"].update(change)
            self.save_manifest(); self.refusal()

class TelemetryTests(unittest.TestCase):
    def test_limits_and_operating_clocks_are_separate(self):
        s=runner.parse_telemetry(telemetry())
        self.assertEqual(s["maximum_sm_clock_mhz"],2115)
        self.assertEqual(s["operating_sm_clock_mhz"],210)
    def test_optional_unsupported_values_are_null(self):
        s=runner.parse_telemetry(telemetry(**{"clocks.current.sm":"[N/A]","clocks.current.memory":"Not Supported"}))
        self.assertIsNone(s["operating_sm_clock_mhz"]); self.assertIsNone(s["operating_memory_clock_mhz"])
    def test_identity_and_required_values_fail_closed(self):
        changes=({"name":"NVIDIA GeForce RTX 3090"},{"pci.device_id":"0x220610DE"},
            {"pci.sub_device_id":"0x20007377"},{"pci.bus_id":"00000000:01:00.0"},
            {"memory.total":"12288"},{"power.limit":"N/A"},{"temperature.gpu":"NaN"},
            {"clocks.max.sm":"1e9999"},{"utilization.gpu":"-1"},{"memory.used":"20000"})
        for change in changes:
            with self.subTest(change=change), self.assertRaises(runner.RunnerError):
                runner.parse_telemetry(telemetry(**change))
    def test_multiple_gpus_and_missing_columns_rejected(self):
        for text in (telemetry()+telemetry(),"RTX 3080, 07:00.0",""):
            with self.assertRaises(runner.RunnerError):
                runner.parse_telemetry(text)
    def test_quiet_thresholds_exactly_inclusive(self):
        runner.require_quiet(runner.parse_telemetry(telemetry(**{"utilization.gpu":5,"memory.used":2048,"temperature.gpu":65})))
        for key,val in (("utilization.gpu",6),("memory.used",2049),("temperature.gpu",66)):
            with self.assertRaisesRegex(runner.RunnerError,"entry_not_quiet"):
                runner.require_quiet(runner.parse_telemetry(telemetry(**{key:val})))

class ExecutionTests(Fixture):
    def test_exact_three_fresh_trials_and_completion_last(self):
        launch=self.successful_launcher(); writes=[]
        real_write=runner.atomic_json
        def write(path,value):
            writes.append(path.name); real_write(path,value)
        with mock.patch.object(runner,"atomic_json",side_effect=write):
            result=runner.run(self.root,execute=True,launcher=launch,nvidia="mock-nvidia")
        self.assertEqual((self.native_count,self.query_count),(3,6))
        self.assertEqual(result["status"],"complete")
        self.assertEqual(writes[-1],"complete.json")
        folder=self.only_result()
        complete=json.loads((folder/"complete.json").read_text())
        self.assertEqual(complete["trial_count"],3)
        self.assertEqual(complete["schema_version"],1)
        for f in complete["files"]:
            self.assertEqual(runner.sha256(folder/f["path"]),f["sha256"])
        context=(folder/"context.json").read_text()
        self.assertNotIn(str(self.repo),context)
        self.assertNotIn("mock-nvidia",context)
        self.assertEqual(len(json.loads(context)["trials"]),3)
        self.assertEqual(json.loads(context)["schema_version"],2)
        self.assertEqual(json.loads(context)["entry_profile"],"quiet")
    def test_settings_change_during_run_leaves_no_completion(self):
        launch=self.successful_launcher()
        def changed(argv,**kwargs):
            value=launch(argv,**kwargs)
            if self.query_count==2 and "--run" not in argv:
                return runner.ProcessResult(0,telemetry(**{"power.limit":350}),"",0.001)
            return value
        with self.assertRaisesRegex(runner.RunnerError,"settings_changed_during_trial"):
            runner.run(self.root,execute=True,launcher=changed,nvidia="mock-nvidia")
        self.assertEqual(self.native_count,1)
        folder=self.only_result()
        self.assertFalse((folder/"complete.json").exists())
        self.assertEqual(json.loads((folder/"failure.json").read_text())["validated_trials"],0)
    def test_corrupted_third_trial_preserves_only_completed_prefix(self):
        launch=self.successful_launcher()
        def invalid(argv,**kwargs):
            value=launch(argv,**kwargs)
            if "--run" in argv and self.native_count==3:
                return runner.ProcessResult(0,'{"invalid":true}',"",0.01)
            return value
        with self.assertRaisesRegex(runner.RunnerError,"invalid_native_trial"):
            runner.run(self.root,execute=True,launcher=invalid,nvidia="mock-nvidia")
        folder=self.only_result()
        self.assertEqual(len(list(folder.glob("trial-*.json"))),2)
        self.assertFalse((folder/"complete.json").exists())
    def test_artifact_modified_between_trials_detected(self):
        launch=self.successful_launcher()
        def modify(argv,**kwargs):
            value=launch(argv,**kwargs)
            if "--run" not in argv and self.query_count==2:
                (self.root/"bin"/"cublas64_12.dll").write_bytes(b"modified")
            return value
        with self.assertRaisesRegex(runner.RunnerError,"manifest_hash_mismatch"):
            runner.run(self.root,execute=True,launcher=modify,nvidia="mock-nvidia")
        self.assertEqual(self.native_count,1)
        self.assertFalse((self.only_result()/"complete.json").exists())
    def test_stderr_failure_does_not_publish_private_text(self):
        def fail(argv,**kw):
            return runner.ProcessResult(1,"",r"C:\Users\private\secret-path",0.01)
        with self.assertRaisesRegex(runner.RunnerError,"child_nonzero_exit"):
            runner.run(self.root,execute=True,launcher=fail,nvidia="mock-nvidia")
        text=(self.only_result()/"failure.json").read_text()
        self.assertNotIn("secret-path",text); self.assertNotIn("Users",text)
    def test_exhausted_total_deadline_prevents_queries(self):
        verified=runner.verify_manifest(self.root)
        launch=mock.Mock(side_effect=AssertionError("must not launch"))
        ticks=iter((0,361))
        with self.assertRaisesRegex(runner.RunnerError,"total_deadline_exhausted"):
            runner.execute_baseline(verified,launcher=launch,nvidia="mock-nvidia",clock=lambda:next(ticks))
        launch.assert_not_called()
        self.assertFalse((self.only_result()/"complete.json").exists())
    def test_busy_gpu_refused_before_native_work(self):
        def query(argv,**kw):
            self.assertNotIn("--run",argv)
            return runner.ProcessResult(0,telemetry(**{"utilization.gpu":50}),"",0.01)
        with self.assertRaisesRegex(runner.RunnerError,"entry_not_quiet"):
            runner.run(self.root,execute=True,launcher=query,nvidia="mock-nvidia")
        self.assertFalse((self.only_result()/"complete.json").exists())
    def test_artifact_modified_during_prequery_prevents_first_native(self):
        launch=self.successful_launcher()
        def modify(argv,**kwargs):
            value=launch(argv,**kwargs)
            if "--run" not in argv:
                (self.root/"bin"/"cublas64_12.dll").write_bytes(b"modified")
            return value
        with self.assertRaisesRegex(runner.RunnerError,"manifest_hash_mismatch"):
            runner.run(self.root,execute=True,launcher=modify,nvidia="mock-nvidia")
        self.assertEqual(self.native_count,0)
    def test_host_memory_checked_before_every_native_trial(self):
        order=[]; launch=self.successful_launcher()
        def memory():
            order.append("memory"); return runner.MIN_HOST_MEMORY
        def traced(argv,**kwargs):
            order.append("native" if "--run" in argv else "query")
            return launch(argv,**kwargs)
        with mock.patch.object(runner,"available_host_memory_bytes",side_effect=memory):
            runner.run(self.root,execute=True,launcher=traced,nvidia="mock-nvidia")
        self.assertEqual(order,["query","memory","native","query"]*3)
        runner.verify_completed_run(self.only_result())
    def test_low_host_memory_refused_without_native_work(self):
        launch=self.successful_launcher()
        with mock.patch.object(runner,"available_host_memory_bytes",return_value=runner.MIN_HOST_MEMORY-1), \
                self.assertRaisesRegex(runner.RunnerError,"insufficient_or_unknown_host_memory"):
            runner.run(self.root,execute=True,launcher=launch,nvidia="mock-nvidia")
        self.assertEqual((self.native_count,self.query_count),(0,1))
        self.assertFalse((self.only_result()/"complete.json").exists())
    def test_environment_mismatch_refused(self):
        launch=self.successful_launcher()
        def mismatch(argv,**kwargs):
            value=launch(argv,**kwargs)
            if "--run" in argv:
                trial=json.loads(value.stdout); trial["environment"]["CUDA_DEVICE_ORDER"]="PCI_BUS_ID"
                return runner.ProcessResult(0,json.dumps(trial),"",0.01)
            return value
        with self.assertRaisesRegex(runner.RunnerError,"native_environment_mismatch"):
            runner.run(self.root,execute=True,launcher=mismatch,nvidia="mock-nvidia")
        self.assertFalse((self.only_result()/"complete.json").exists())
    def test_pinned_runtime_and_driver_api_are_checked(self):
        trial=make_trial()
        for key,value in (("cuda_runtime",12050),("cublas_version",120601),
                ("cuda_driver",12050),("cuda_runtime",True)):
            changed=deepcopy(trial); changed["runtime"][key]=value
            with self.subTest(key=key,value=value), self.assertRaises(runner.RunnerError):
                runner._trial_identity(changed)
        trial["runtime"]["cuda_driver"]=13000
        runner._trial_identity(trial)

class EntryProfileTests(Fixture):
    def test_desktop_2136_mib_completes_and_verifies_with_explicit_profile(self):
        result=runner.run(self.root,execute=True,profile="desktop",
            launcher=self.successful_launcher(**{"memory.used":2136}),nvidia="mock-nvidia")
        self.assertEqual((self.native_count,self.query_count),(3,6))
        self.assertEqual(result["entry_profile"],"desktop")
        folder=self.only_result()
        context=json.loads((folder/"context.json").read_text())
        self.assertEqual(context["schema_version"],2)
        self.assertEqual(context["entry_profile"],"desktop")
        self.assertEqual(context["entry_limits"],runner.entry_limits("desktop"))
        checked=runner.verify_completed_run(folder)
        self.assertEqual(checked["entry_profile"],"desktop")
        self.assertEqual(checked["entry_limits"]["max_memory_used_mib"],3072)
    def test_default_quiet_still_rejects_2136_mib(self):
        with self.assertRaisesRegex(runner.RunnerError,"entry_not_quiet"):
            runner.run(self.root,execute=True,
                launcher=self.successful_launcher(**{"memory.used":2136}),nvidia="mock-nvidia")
        self.assertEqual((self.native_count,self.query_count),(0,1))
        self.assertFalse((self.only_result()/"complete.json").exists())
    def test_desktop_rejects_3073_mib_before_native_work(self):
        with self.assertRaisesRegex(runner.RunnerError,"entry_profile_limits_exceeded"):
            runner.run(self.root,execute=True,profile="desktop",
                launcher=self.successful_launcher(**{"memory.used":3073}),nvidia="mock-nvidia")
        self.assertEqual((self.native_count,self.query_count),(0,1))
        self.assertFalse((self.only_result()/"complete.json").exists())
    def test_profile_thresholds_include_exact_boundaries(self):
        for profile,memory in (("quiet",2048),("desktop",3072)):
            runner.require_entry(runner.parse_telemetry(telemetry(
                **{"utilization.gpu":5,"memory.used":memory,"temperature.gpu":65})),profile)
    def test_both_profiles_keep_utilization_and_temperature_gates(self):
        for profile in ("quiet","desktop"):
            for values in ({"utilization.gpu":6},{"temperature.gpu":66}):
                with self.subTest(profile=profile,values=values), self.assertRaises(runner.RunnerError):
                    runner.run(self.root,execute=True,profile=profile,
                        launcher=self.successful_launcher(**values),nvidia="mock-nvidia")
                self.assertEqual((self.native_count,self.query_count),(0,1))
    def test_unknown_and_malformed_profiles_refused_before_any_query(self):
        launch=mock.Mock(side_effect=AssertionError("query forbidden"))
        for profile in ("","Desktop","quiet ",None,True,{},["quiet"]):
            with self.subTest(profile=profile):
                with self.assertRaisesRegex(runner.RunnerError,"invalid_entry_profile"):
                    runner.run(self.root,execute=True,profile=profile,launcher=launch,nvidia="mock-nvidia")
                with self.assertRaisesRegex(runner.RunnerError,"invalid_entry_profile"):
                    runner.execute_baseline({},profile=profile,launcher=launch,nvidia="mock-nvidia")
        launch.assert_not_called()
        self.assertFalse((self.root/"results").exists())
    def test_desktop_cli_without_execute_is_offline(self):
        with mock.patch.object(runner,"__file__",str(self.root/"run.py")), \
                mock.patch.object(runner.subprocess,"Popen",side_effect=AssertionError("forbidden")), \
                mock.patch.object(runner.sys,"stdout",new_callable=io.StringIO) as stdout:
            self.assertEqual(runner.main(["--profile","desktop"]),0)
        result=json.loads(stdout.getvalue())
        self.assertEqual(result["entry_profile"],"desktop")
        self.assertFalse(result["gpu_workload_run"])
        self.assertFalse((self.root/"results").exists())
    def test_cli_unknown_profile_is_rejected(self):
        with mock.patch.object(runner,"run",side_effect=AssertionError("forbidden")), \
                mock.patch.object(runner.sys,"stderr",new_callable=io.StringIO), self.assertRaises(SystemExit) as caught:
            runner.main(["--execute","--profile","unknown"])
        self.assertEqual(caught.exception.code,2)

class SavedBundleTests(Fixture):
    def setUp(self):
        super().setUp()
        runner.run(self.root,execute=True,launcher=self.successful_launcher(),nvidia="mock-nvidia")
        self.folder=self.only_result()
    def rewrite(self,name,change):
        path=self.folder/name
        value=json.loads(path.read_text())
        change(value)
        runner.atomic_json(path,value)
        if name!="complete.json":
            marker=json.loads((self.folder/"complete.json").read_text())
            for entry in marker["files"]:
                if entry["path"]==name:
                    entry["sha256"]=runner.sha256(path)
            runner.atomic_json(self.folder/"complete.json",marker)
    def assert_invalid(self,code=None):
        with self.assertRaises(runner.RunnerError) as caught:
            runner.verify_completed_run(self.folder)
        if code:
            self.assertEqual(caught.exception.code,code)
    def test_offline_verification_needs_no_installed_artifacts_or_manifest(self):
        for path in runner.ARTIFACTS:
            (self.root/path).unlink()
        (self.root/"build-manifest.json").unlink()
        with mock.patch.object(runner.subprocess,"Popen",side_effect=AssertionError("forbidden")), \
                mock.patch.object(runner.shutil,"which",side_effect=AssertionError("forbidden")):
            result=runner.verify_completed_run(self.folder)
        self.assertEqual(result["status"],"offline-run-verified")
        self.assertEqual(result["saved_trial_count"],3)
        self.assertFalse(result["native_process_run"])
        self.assertFalse(result["gpu_workload_run"])
        self.assertIn("authenticity",result["limitation"])
    def test_missing_completion_refused(self):
        (self.folder/"complete.json").unlink()
        self.assert_invalid("required_file_unavailable")
    def test_changed_or_truncated_trial_context_and_summary_refused(self):
        for name in ("trial-1.json","trial-2.json","trial-3.json","context.json","summary.json"):
            with self.subTest(name=name):
                path=self.folder/name; original=path.read_bytes()
                path.write_bytes(original[:-10])
                self.assert_invalid("manifest_hash_mismatch")
                path.write_bytes(original)
    def test_extra_trial_and_incomplete_marker_refused(self):
        for name in ("trial-4.json","failure.json","complete.json.pending"):
            with self.subTest(name=name):
                path=self.folder/name; path.write_text("{}")
                self.assert_invalid("unexpected_bundle_entry")
                path.unlink()
    def test_duplicate_and_traversal_completion_paths_refused(self):
        marker=(self.folder/"complete.json").read_bytes()
        for change in (lambda d:d["files"].__setitem__(1,dict(d["files"][0])),
                lambda d:d["files"][0].update(path="../outside.json"),
                lambda d:d["files"].append(dict(d["files"][0]))):
            self.rewrite("complete.json",change)
            self.assert_invalid()
            (self.folder/"complete.json").write_bytes(marker)
    def test_rehashed_false_summary_refused(self):
        self.rewrite("summary.json",lambda d:d["modes"][0].update(median_tflops=12345))
        self.assert_invalid("saved_summary_mismatch")
    def test_rehashed_invalid_native_trial_refused(self):
        self.rewrite("trial-3.json",lambda d:d["modes"][0].update(median_ms=-1))
        self.assert_invalid("invalid_saved_trial")
    def test_context_freshness_identity_settings_and_time_are_checked(self):
        original=(self.folder/"context.json").read_bytes()
        marker=(self.folder/"complete.json").read_bytes()
        changes=(lambda d:d["execution"].update(fresh_process_count=2),
            lambda d:d["trials"][0].update(fresh_process=False),
            lambda d:d["trials"][1].update(trial_index=1),
            lambda d:d["trials"][0]["after"].update(power_limit_watts=300),
            lambda d:d["trials"][0]["before"].update(pci_subsystem_id="0000:0000"),
            lambda d:d["trials"][1].update(started_at_utc="2000-01-01T00:00:00Z"),
            lambda d:d["trials"][0].update(available_host_memory_bytes=0),
            lambda d:d.update(build_manifest_sha256="0"*64),
            lambda d:d["environment"].update(CUDA_VISIBLE_DEVICES="GPU-private-uuid"))
        for change in changes:
            self.rewrite("context.json",change)
            self.assert_invalid()
            (self.folder/"context.json").write_bytes(original)
            (self.folder/"complete.json").write_bytes(marker)
    def test_completion_status_protocol_count_and_time_are_checked(self):
        original=(self.folder/"complete.json").read_bytes()
        changes=(dict(status="incomplete"),dict(benchmark_protocol=2),dict(trial_count=True),
            dict(schema_version=True),dict(completed_at_utc="2000-01-01T00:00:00Z"),
            dict(build_manifest_sha256="private-path"))
        for change in changes:
            self.rewrite("complete.json",lambda d:d.update(change))
            self.assert_invalid()
            (self.folder/"complete.json").write_bytes(original)
    def test_duplicate_json_completion_refused(self):
        (self.folder/"complete.json").write_text('{"status":"complete","status":"complete"}')
        self.assert_invalid("duplicate_json_key")
    def test_cli_verify_bypasses_manifest_and_processes(self):
        with mock.patch.object(runner,"run",side_effect=AssertionError("execution forbidden")), \
                mock.patch.object(runner.subprocess,"Popen",side_effect=AssertionError("forbidden")), \
                mock.patch.object(runner.sys,"stdout",new_callable=io.StringIO) as stdout:
            self.assertEqual(runner.main(["--verify-run",str(self.folder)]),0)
        self.assertEqual(json.loads(stdout.getvalue())["status"],"offline-run-verified")
    def test_cli_execution_modes_are_mutually_exclusive(self):
        with mock.patch.object(runner.sys,"stderr",new_callable=io.StringIO), self.assertRaises(SystemExit) as caught:
            runner.main(["--execute","--verify-run",str(self.folder)])
        self.assertEqual(caught.exception.code,2)
    def test_cli_relative_bundle_labels(self):
        self.assertEqual(runner._bundle_path(self.root,self.folder.name),self.folder)
        self.assertEqual(runner._bundle_path(self.root,"results/"+self.folder.name),self.folder)
    def test_oversized_numeric_context_is_refused_without_overflow(self):
        original=(self.folder/"context.json").read_bytes()
        marker=(self.folder/"complete.json").read_bytes()
        for change in (lambda d:d["trials"][0].update(elapsed_seconds=10**1000),
                lambda d:d["trials"][0]["before"].update(temperature_celsius=10**1000)):
            self.rewrite("context.json",change)
            self.assert_invalid()
            (self.folder/"context.json").write_bytes(original)
            (self.folder/"complete.json").write_bytes(marker)
    def test_cli_parser_refusal_contains_no_traceback_or_private_path(self):
        (self.folder/"complete.json").write_text('{"value":'+"9"*5000+'}')
        with mock.patch.object(runner.sys,"stdout",new_callable=io.StringIO) as stdout:
            self.assertEqual(runner.main(["--verify-run",str(self.folder)]),2)
        self.assertEqual(json.loads(stdout.getvalue()),{"status":"refused","reason":"invalid_json"})
    def test_profile_tampering_is_refused_before_and_after_rehash(self):
        context=json.loads((self.folder/"context.json").read_text())
        context["entry_profile"]="desktop"
        runner.atomic_json(self.folder/"context.json",context)
        self.assert_invalid("manifest_hash_mismatch")
        self.rewrite("context.json",lambda d:None)
        self.assert_invalid("invalid_entry_limits")
    def test_rehashed_profiles_and_limits_require_exact_whitelist(self):
        original=(self.folder/"context.json").read_bytes()
        marker=(self.folder/"complete.json").read_bytes()
        changes=(lambda d:d.update(entry_profile="unknown"),
            lambda d:d.update(entry_profile={"name":"quiet"}),
            lambda d:d.update(entry_profile=True),
            lambda d:d["entry_limits"].update(max_memory_used_mib=3072),
            lambda d:d["entry_limits"].update(max_utilization_percent=100),
            lambda d:d["entry_limits"].update(max_temperature_celsius=66),
            lambda d:d["entry_limits"].update(max_memory_used_mib=2048.0),
            lambda d:d["entry_limits"].update(extra_limit=1),
            lambda d:d.pop("entry_profile"),
            lambda d:d.update(schema_version=1))
        for change in changes:
            self.rewrite("context.json",change)
            self.assert_invalid()
            (self.folder/"context.json").write_bytes(original)
            (self.folder/"complete.json").write_bytes(marker)
    def test_saved_admission_uses_recorded_profile_and_cannot_claim_quiet_at_2136(self):
        def desktop_context(d):
            d["entry_profile"]="desktop"
            d["entry_limits"]=runner.entry_limits("desktop")
            for observation in d["trials"]:
                observation["before"]["memory_used_mib"]=2136
                observation["after"]["memory_used_mib"]=2136
        self.rewrite("context.json",desktop_context)
        self.assertEqual(runner.verify_completed_run(self.folder)["entry_profile"],"desktop")
        self.rewrite("context.json",lambda d:d.update(
            entry_profile="quiet",entry_limits=runner.entry_limits("quiet")))
        self.assert_invalid("entry_not_quiet")

class OwnedPythonChildTests(unittest.TestCase):
    def call(self,code,seconds=5,stdout=4096,stderr=4096):
        return runner.run_process([sys.executable,"-c",code],timeout_seconds=seconds,
            stdout_limit=stdout,stderr_limit=stderr,cwd=Path(__file__).parent)
    def test_small_cpu_child_capture(self):
        r=self.call("print('cpu-only child')")
        self.assertEqual(r.returncode,0); self.assertIn("cpu-only child",r.stdout)
    def test_owned_sleeping_child_deadline(self):
        started=time.monotonic()
        with self.assertRaisesRegex(runner.RunnerError,"process_timeout"):
            self.call("import time; time.sleep(10)",seconds=0.2)
        self.assertLess(time.monotonic()-started,3)
    def test_stdout_and_stderr_are_bounded(self):
        for stream in ("stdout","stderr"):
            with self.subTest(stream=stream), self.assertRaisesRegex(runner.RunnerError,"process_output_limit"):
                self.call(f"import sys; sys.{stream}.write('x'*1000000)",stdout=1024,stderr=1024)
    def test_non_utf8_refused(self):
        with self.assertRaisesRegex(runner.RunnerError,"process_output_not_utf8"):
            self.call("import os; os.write(1,bytes([255,254,253]))")
    def test_no_start_after_expired_budget(self):
        with mock.patch.object(runner.subprocess,"Popen") as popen:
            with self.assertRaisesRegex(runner.RunnerError,"process_deadline_exhausted"):
                self.call("raise SystemExit(99)",seconds=0)
        popen.assert_not_called()

if __name__=="__main__":
    unittest.main()
