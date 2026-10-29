"""Bounded 4096-square cuBLAS GEMM baseline on the current RTX 4070 Ti.

Private CUDA buffers, full-output CPU-pattern hashes, no hardware controls.
Uses isolated hash-verified NVIDIA user libraries; installs no driver/toolkit.
"""

import ctypes as c
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import statistics
import struct
import time

from cuda_driver import Cuda
from gpu_tensor_probe import telemetry

HERE = Path(__file__).resolve().parent


def file_hash(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    provenance = json.loads((HERE / 'cublas-package-provenance.json').read_text())
    base = (HERE / 'build/cuda-12.6').resolve()
    paths = {}
    for relative, expected in provenance['dll_sha256'].items():
        path = (base / relative).resolve()
        if not path.is_relative_to(base) or file_hash(path) != expected:
            raise ValueError('Verified NVIDIA DLL mismatch')
        paths[path.name] = path
    directories = [os.add_dll_directory(str(folder)) for folder in sorted({p.parent for p in paths.values()})]
    runtime = c.WinDLL(str(paths['cudart64_12.dll']))
    library_lt = c.WinDLL(str(paths['cublasLt64_12.dll']))
    library = c.WinDLL(str(paths['cublas64_12.dll']))
    p, i = c.c_void_p, c.c_int
    signatures = {'cublasCreate_v2': [c.POINTER(p)], 'cublasDestroy_v2': [p],
        'cublasSetStream_v2': [p,p], 'cublasSetMathMode': [p,i],
        'cublasSetPointerMode_v2': [p,i], 'cublasGetVersion_v2': [p,c.POINTER(i)],
        'cublasGemmEx': [p,i,i,i,i,i,p,p,i,i,p,i,i,p,p,i,i,i,i]}
    for name, args in signatures.items():
        function = getattr(library, name);function.argtypes=args;function.restype=i

    def blas(name, *args):
        status = getattr(library, name)(*args)
        if status:
            raise RuntimeError(name+': cuBLAS status '+str(status))

    cuda = Cuda();handle = p()
    report = {'scope': 'current-settings cuBLAS GEMM; no unlock transaction or selector state query',
        'utc': datetime.now(timezone.utc).isoformat(), 'before': telemetry(),
        'n': 4096, 'samples_per_mode': 7, 'minimum_warmup_seconds': 0.25,
        'libraries_sha256': provenance['dll_sha256'], 'results': []}
    try:
        report['cuda'] = cuda.open()
        cuda.lib.cuMemsetD16_v2.argtypes=[c.c_uint64,c.c_ushort,c.c_size_t]
        cuda.lib.cuMemsetD16_v2.restype=i
        cuda.lib.cuMemsetD32_v2.argtypes=[c.c_uint64,c.c_uint,c.c_size_t]
        cuda.lib.cuMemsetD32_v2.restype=i
        blas('cublasCreate_v2', c.byref(handle))
        blas('cublasSetStream_v2', handle, None)
        blas('cublasSetPointerMode_v2', handle, 0)  # host alpha/beta
        version=i();blas('cublasGetVersion_v2',handle,c.byref(version));report['cublas_version']=version.value
        start, stop = p(), p()
        for event in (start,stop):
            cuda.call('cuEventCreate',c.byref(event),0);cuda.events.append(event)
        n=report['n'];count=n*n;output_bytes=count*4
        host=(c.c_ubyte*output_bytes)()

        def allocate(size):
            if not 0 < size <= 64*1024*1024:
                raise ValueError('Matrix allocation exceeds 64 MiB')
            pointer=c.c_uint64();cuda.call('cuMemAlloc_v2',c.byref(pointer),size)
            cuda.allocations.append(pointer);return pointer

        for label,datatype,bits,compute,algorithm,math_mode in (
            ('fp32_pedantic',0,0x3f800000,69,-1,2),
            ('fp16_fp32',2,0x3c00,68,99,17),
            ('bf16_fp32',14,0x3f80,68,99,17)):
            input_bytes=count*(4 if datatype==0 else 2)
            a,b,out=allocate(input_bytes),allocate(input_bytes),allocate(output_bytes)
            for pointer in (a,b):
                cuda.call('cuMemsetD32_v2' if datatype==0 else 'cuMemsetD16_v2',pointer,bits,count)
            cuda.call('cuCtxSynchronize');blas('cublasSetMathMode',handle,math_mode)

            def gemm(size,alpha_value):
                alpha,beta=c.c_float(alpha_value),c.c_float(0)
                blas('cublasGemmEx',handle,0,0,size,size,size,c.byref(alpha),p(a.value),datatype,size,
                     p(b.value),datatype,size,c.byref(beta),p(out.value),0,size,compute,algorithm)

            def verify(size,alpha_value):
                length=size*size*4
                cuda.call('cuMemcpyDtoH_v2',host,out,length)
                actual=hashlib.sha256(memoryview(host).cast('B')[:length]).hexdigest()
                expected=hashlib.sha256(struct.pack('<f',size*alpha_value)*(size*size)).hexdigest()
                if actual!=expected:
                    raise RuntimeError('Full-output GEMM hash mismatch: '+label)
                return actual

            gemm(256,1);cuda.call('cuCtxSynchronize');verify(256,1)
            warm_start=time.perf_counter();warmups=0
            while time.perf_counter()-warm_start < report['minimum_warmup_seconds']:
                gemm(n,1);cuda.call('cuCtxSynchronize');warmups+=1
            before=telemetry();times=[];hashes=[];alphas=[]
            for sample in range(report['samples_per_mode']):
                alpha=1+(sample+1)/8
                cuda.call('cuEventRecord',start,None);gemm(n,alpha)
                cuda.call('cuEventRecord',stop,None);cuda.call('cuEventSynchronize',stop)
                elapsed=c.c_float();cuda.call('cuEventElapsedTime',c.byref(elapsed),start,stop)
                if not 0<elapsed.value<100:
                    raise RuntimeError('Unexpected measured GEMM duration')
                times.append(elapsed.value);hashes.append(verify(n,alpha));alphas.append(alpha)
            median=statistics.median(times)
            report['results'].append({'mode':label,'input_type':datatype,'compute_type':compute,
                'output_type':'fp32','math_mode':math_mode,'algorithm':algorithm,
                'matrix_allocation_bytes':input_bytes*2+output_bytes,
                'warmup_launches':warmups,'milliseconds':times,'median_ms':median,
                'tflops':2*n**3/(median*1e9),'alpha_per_sample':alphas,
                'output_sha256_per_sample':hashes,'full_output_verified_every_sample':True,
                'telemetry_before_samples':before,'telemetry_after_samples':telemetry()})
            for pointer in (a,b,out):
                cuda.call('cuMemFree_v2',pointer);cuda.allocations.remove(pointer)
    finally:
        try:
            if handle.value:blas('cublasDestroy_v2',handle)
        finally:
            cuda.close()
            for directory in directories:directory.close()
    report['cleanup_completed']=True;report['after']=telemetry()
    report['research_source_sha256']={p.name:file_hash(p) for p in (
        Path(__file__),HERE/'cuda_driver.py',HERE/'gpu_tensor_probe.py',HERE/'gpu_smoke.py',
        HERE/'cublas-package-provenance.json')}
    print(json.dumps(report,indent=2))


if __name__=='__main__':
    main()
