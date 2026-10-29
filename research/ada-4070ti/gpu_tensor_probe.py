"""Short stock FP16/BF16 MMA probes via the installed CUDA driver, without cuBLAS.

Register-resident synthetic work, not application GEMM or a selector-state query.
No clock/power/firmware/driver settings, resets or raw device interfaces are used.
"""

import ctypes as c
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import statistics
import struct
import subprocess

from cuda_driver import Cuda
from gpu_smoke import telemetry as identity_telemetry


def telemetry():
    row = identity_telemetry()
    fields = 'clocks.sm,clocks.mem,pstate'
    result = subprocess.run(['nvidia-smi', '--query-gpu=' + fields,
                             '--format=csv,noheader,nounits'], capture_output=True,
                            text=True, timeout=15, check=True)
    values = [v.strip() for v in result.stdout.strip().split(',')]
    if len(values) != 3:
        raise RuntimeError('Unexpected clock telemetry')
    row.update(zip(fields.split(','), values))
    return row


def kernel_ptx(kind):
    if kind not in ('f16', 'bf16'):
        raise ValueError('Unexpected input type')
    # Different chain seeds prevent the four PTX recurrences being identical.
    init = '\n'.join(' add.f32 d' + str(i) + ', seed, 0f' +
                     struct.pack('>f', float(i // 4)).hex().upper() + ';' for i in range(16))
    mma = '\n'.join(
        ' mma.sync.aligned.m16n8k16.row.col.f32.' + kind + '.' + kind + '.f32 '
        '{' + ','.join('d' + str(j) for j in range(i, i+4)) + '}, '
        '{a0,a1,a2,a3}, {b0,b1}, '
        '{' + ','.join('d' + str(j) for j in range(i, i+4)) + '};'
        for i in range(0, 16, 4))
    stores = '\n'.join(
        ' st.global.v4.f32 [address+' + str(i*4) + '], {' +
        ','.join('d' + str(j) for j in range(i, i+4)) + '};'
        for i in range(0, 16, 4))
    return r'''.version 7.8
.target sm_89
.address_size 64
.visible .entry tensor(.param .u64 output, .param .u32 iterations,
                      .param .u32 operand_bits, .param .f32 initial) {
 .reg .pred p;
 .reg .b32 a<4>, b<2>, r<5>;
 .reg .f32 d<16>, seed;
 .reg .b64 pointer, offset, address;
 ld.param.u64 pointer, [output];
 ld.param.u32 r0, [iterations];
 ld.param.u32 a0, [operand_bits];
 ld.param.f32 seed, [initial];
 mov.b32 a1, a0; mov.b32 a2, a0; mov.b32 a3, a0;
 mov.b32 b0, a0; mov.b32 b1, a0;
''' + init + r'''
 mov.u32 r1, 0;
LOOP:
''' + mma + r'''
 add.u32 r1, r1, 1;
 setp.lt.u32 p, r1, r0;
 @p bra LOOP;
 mov.u32 r2, %ctaid.x;
 mov.u32 r3, %ntid.x;
 mov.u32 r4, %tid.x;
 mad.lo.u32 r2, r2, r3, r4;
 mul.wide.u32 offset, r2, 64;
 add.u64 address, pointer, offset;
''' + stores + r'''
 ret;
}
'''


def main():
    cuda = Cuda()
    report = {'scope': 'short stock register-resident MMA probe; not cuBLAS, application GEMM, or unlock evidence',
              'utc': datetime.now(timezone.utc).isoformat(), 'before': telemetry(),
              'grid': 120, 'threads_per_block': 256, 'ptx_accumulator_chains': 4,
              'iterations_per_chain': 4096, 'samples_per_mode': 7, 'results': []}
    values = report['grid'] * report['threads_per_block'] * 16
    size = values * 4
    report['device_buffer_bytes'] = size
    try:
        report['cuda'] = cuda.open()
        output = cuda.allocate(size)
        host = (c.c_float * values)()
        start, stop = c.c_void_p(), c.c_void_p()
        for event in (start, stop):
            cuda.call('cuEventCreate', c.byref(event), 0)
            cuda.events.append(event)
        for kind, bits in (('f16', 0x3c003c00), ('bf16', 0x3f803f80)):
            ptx = kernel_ptx(kind)
            kernel = cuda.load(ptx, 'tensor')
            operand = c.c_uint32(bits)

            def launch_and_verify(grid, block, iterations, bias, timed=False):
                if not 1 <= iterations <= 4096:
                    raise ValueError('Unexpected iteration count')
                count, seed = c.c_uint32(iterations), c.c_float(bias)
                if timed: cuda.call('cuEventRecord', start, None)
                cuda.launch(kernel, grid, block, [output, count, operand, seed])
                if timed:
                    cuda.call('cuEventRecord', stop, None)
                    cuda.call('cuEventSynchronize', stop)
                else:
                    cuda.call('cuCtxSynchronize')
                length = grid * block * 16
                cuda.call('cuMemcpyDtoH_v2', host, output, length * 4)
                expected = 16 * iterations + bias
                if any(host[i] != expected + (i % 16) // 4 for i in range(length)):
                    raise RuntimeError('Tensor output mismatch: ' + kind)
                if timed:
                    elapsed = c.c_float()
                    cuda.call('cuEventElapsedTime', c.byref(elapsed), start, stop)
                    if not 0 < elapsed.value < 100:
                        raise RuntimeError('Unexpected kernel duration; stop this run')
                    return elapsed.value

            # One warp / eight iterations verifies instruction semantics first.
            launch_and_verify(1, 32, 8, 1)
            launch_and_verify(report['grid'], report['threads_per_block'], 512, 2)
            before = telemetry()
            samples = [launch_and_verify(report['grid'], report['threads_per_block'], 4096, 3 + i, True)
                       for i in range(report['samples_per_mode'])]
            # 4096 FLOPs per warp MMA (2*m*n*k), four chains per loop.
            operations = (report['grid'] * report['threads_per_block'] // 32) * 4096 * 4 * 4096
            median = statistics.median(samples)
            report['results'].append({'input': kind, 'accumulation': 'f32',
                'milliseconds': samples, 'median_ms': median,
                'nominal_flops_per_launch': operations,
                'nominal_tflops': operations / (median * 1e9),
                'full_output_verified_every_launch': True, 'output_values_per_timed_launch': values,
                'last_output_sha256': hashlib.sha256(bytes(host)).hexdigest(),
                'ptx_sha256': hashlib.sha256(ptx.encode()).hexdigest(),
                'telemetry_before_samples': before, 'telemetry_after_samples': telemetry()})
    finally:
        cuda.close()
    report['cleanup_completed'] = True
    report['after'] = telemetry()
    report['research_source_sha256'] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
        for p in (Path(__file__), Path(__file__).with_name('cuda_driver.py'), Path(__file__).with_name('gpu_smoke.py'))}
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
