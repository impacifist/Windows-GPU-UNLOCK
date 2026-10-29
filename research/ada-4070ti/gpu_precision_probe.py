"""Observe FP16 versus FP32 accumulator recurrence on one CUDA warp.

This records numerical differences, not a general accuracy/throughput verdict.
Only private CUDA memory is written; no hardware controls are used.
"""

import ctypes as c
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import struct

from cuda_driver import Cuda
from gpu_accumulation_probe import ptx_f16, ptx_f32
from gpu_tensor_probe import telemetry


def main():
    cuda = Cuda()
    report = {'scope': 'one-warp numerical recurrence on current GPU; no selector-state inference',
              'utc': datetime.now(timezone.utc).isoformat(), 'before': telemetry(),
              'grid': 1, 'threads': 32, 'device_buffer_bytes': 2048, 'observations': []}
    try:
        report['cuda'] = cuda.open()
        output = cuda.allocate(2048)
        for mode, ptx in (('f16', ptx_f16()), ('f32', ptx_f32())):
            kernel = cuda.load(ptx, 'tensor')
            for iterations in (8, 256, 512, 768, 1000, 1024, 1536):
                count, bits, seed = c.c_uint32(iterations), c.c_uint32(0x3c003c00), c.c_float(16)
                host = (c.c_uint16*512)() if mode == 'f16' else (c.c_float*512)()
                cuda.launch(kernel, 1, 32, [output, count, bits, seed])
                cuda.call('cuCtxSynchronize')
                cuda.call('cuMemcpyDtoH_v2', host, output, c.sizeof(host))
                values = ([struct.unpack('<e', struct.pack('<H', x))[0] for x in host]
                          if mode == 'f16' else list(host))
                first = [values[i*4] for i in range(4)]
                if any(not math.isfinite(v) or v != first[(i%16)//4] for i,v in enumerate(values)):
                    raise RuntimeError('Unexpected nonuniform/nonfinite recurrence result')
                ideal = [16*iterations+16+16*i for i in range(4)]
                exact = first == ideal
                if (mode == 'f32' or iterations <= 768) and not exact:
                    raise RuntimeError('Required exact recurrence failed')
                report['observations'].append({'accumulation': mode, 'iterations': iterations,
                    'initial_bias': 16, 'observed_chain_values': first,
                    'ideal_chain_values': ideal, 'matches_ideal_all_512_values': exact,
                    'all_outputs_finite_and_chain_uniform': True,
                    'output_sha256': hashlib.sha256(bytes(host)).hexdigest()})
    finally:
        cuda.close()
    report['cleanup_completed'] = True
    report['after'] = telemetry()
    report['research_source_sha256'] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
        for p in (Path(__file__), Path(__file__).with_name('gpu_accumulation_probe.py'),
                  Path(__file__).with_name('gpu_tensor_probe.py'), Path(__file__).with_name('cuda_driver.py'),
                  Path(__file__).with_name('gpu_smoke.py'))}
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
