"""Read documented Windows NVAPI identity/architecture fields; no setters."""

import ctypes as c
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path

from gpu_smoke import telemetry
from cuda_driver import Cuda


class Arch(c.Structure):
    _fields_ = [(name,c.c_uint32) for name in ('version','architecture','implementation','revision')]


class Gsp(c.Structure):
    _fields_ = [('version',c.c_uint32),('firmware',c.c_char*64),('reserved',c.c_uint32)]


def main():
    before=telemetry()
    path=Path(os.environ.get('SystemRoot',r'C:\Windows'))/'System32/nvapi64.dll'
    library=c.WinDLL(str(path))
    query=library.nvapi_QueryInterface;query.argtypes=[c.c_uint32];query.restype=c.c_void_p
    def function(identifier,*args):
        pointer=query(identifier)
        if not pointer:raise RuntimeError('Documented NVAPI interface unavailable: '+hex(identifier))
        return c.CFUNCTYPE(c.c_int,*args)(pointer)
    initialize=function(0x0150e828);unload=function(0xd22bdd7e)
    status=initialize()
    if status:raise RuntimeError('NVAPI initialize failed: '+str(status))
    report={'scope':'documented read-only NVAPI queries; no SM rate/selector inference',
            'utc':datetime.now(timezone.utc).isoformat(),'before':before}
    try:
        handles=(c.c_void_p*64)();count=c.c_uint32()
        status=function(0xe5ac921f,c.POINTER(c.c_void_p),c.POINTER(c.c_uint32))(handles,c.byref(count))
        if status or count.value!=1:raise RuntimeError('Expected exactly one NVAPI physical GPU')
        gpu=handles[0];name=c.create_string_buffer(64)
        status=function(0xceee8e9f,c.c_void_p,c.c_void_p)(gpu,name)
        if status or not name.value.decode().endswith('RTX 4070 Ti'):raise RuntimeError('NVAPI identity mismatch')
        report['gpu']=name.value.decode()
        for field,identifier in (('bus_id',0x1be0b8e5),('slot_id_raw',0x2a0a350f),
                                 ('shader_subpipe_count',0x0be17923),('gpu_core_count',0xc7026a87)):
            value=c.c_uint32()
            status=function(identifier,c.c_void_p,c.POINTER(c.c_uint32))(gpu,c.byref(value))
            report[field]={'status':status,'value':value.value if status==0 else None}
        arch=Arch();arch.version=c.sizeof(Arch)|(2<<16)
        status=function(0xd8265d24,c.c_void_p,c.POINTER(Arch))(gpu,c.byref(arch))
        report['architecture']={'status':status}
        if status==0:
            report['architecture'].update({field:hex(getattr(arch,field)) for field in
                                           ('architecture','implementation','revision')})
        gsp=Gsp();gsp.version=c.sizeof(Gsp)|(1<<16)
        status=function(0x581c4391,c.c_void_p,c.POINTER(Gsp))(gpu,c.byref(gsp))
        report['gsp_query']={'status':status,'firmware_version':gsp.firmware.decode('ascii',errors='replace') if status==0 else None,
                             'limit':'Unsupported does not distinguish not-running from not-supported.'}
    finally:
        status=unload()
        if status:raise RuntimeError('NVAPI unload failed: '+str(status))
    report['client_unloaded']=True
    cuda=Cuda()
    try:
        cuda.call('cuInit',0)
        device=c.c_int();sm_count=c.c_int()
        cuda.call('cuDeviceGet',c.byref(device),0)
        cuda.call('cuDeviceGetAttribute',c.byref(sm_count),16,device)  # MULTIPROCESSOR_COUNT
        report['cuda_multiprocessor_count']=sm_count.value
        report['counter_limit']='NVAPI shader subpipes are retained raw, not equated to CUDA SM count.'
    finally:
        cuda.close()
    report['after']=telemetry()
    report['nvapi_dll_sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
    source=Path(__file__)
    report['research_source_sha256']={p.name:hashlib.sha256(p.read_bytes()).hexdigest()
                                    for p in (source,source.with_name('gpu_smoke.py'),source.with_name('cuda_driver.py'))}
    print(json.dumps(report,indent=2))


if __name__=='__main__':
    main()
