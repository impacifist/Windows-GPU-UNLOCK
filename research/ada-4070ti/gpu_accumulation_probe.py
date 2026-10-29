"""Compare short stock FP16-input MMA work with FP16 versus FP32 accumulation.

Identical shape/grid/iteration count, alternating order, full output checks.
No selector reads/writes, hardware controls, driver installation or GPU reset.
"""

import ctypes as c
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import statistics
import struct

from cuda_driver import Cuda
from gpu_tensor_probe import kernel_ptx, telemetry


def ptx_f32():
    text = kernel_ptx('f16')
    for old, new in ((1, 16), (2, 32), (3, 48)):
        source = '0f' + struct.pack('>f', float(old)).hex().upper()
        target = '0f' + struct.pack('>f', float(new)).hex().upper()
        if text.count(source) != 4:
            raise ValueError('Unexpected source chain initialization')
        text = text.replace(source, target)
    return text


def ptx_f16():
    constants = '\n'.join(' mov.b32 chain' + str(i) + ', 0x' +
                          (struct.pack('>e', float(16*i)).hex()*2) + ';' for i in range(4))
    init = constants + '\n' + '\n'.join(' add.rn.f16x2 d' + str(i) + ', packed, chain' +
                                        str(i//2) + ';' for i in range(8))
    mma = '\n'.join(
        ' mma.sync.aligned.m16n8k16.row.col.f16.f16.f16.f16 '
        '{d' + str(i) + ',d' + str(i+1) + '}, {a0,a1,a2,a3}, {b0,b1}, '
        '{d' + str(i) + ',d' + str(i+1) + '};' for i in range(0, 8, 2))
    return r'''.version 7.8
.target sm_89
.address_size 64
.visible .entry tensor(.param .u64 output, .param .u32 iterations,
                      .param .u32 operand_bits, .param .f32 initial) {
 .reg .pred p;
 .reg .b32 a<4>, b<2>, d<8>, r<5>, packed, chain<4>;
 .reg .b16 halfseed;
 .reg .f32 seed;
 .reg .b64 pointer, offset, address;
 ld.param.u64 pointer, [output];
 ld.param.u32 r0, [iterations];
 ld.param.u32 a0, [operand_bits];
 ld.param.f32 seed, [initial];
 cvt.rn.f16.f32 halfseed, seed;
 mov.b32 packed, {halfseed,halfseed};
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
 mul.wide.u32 offset, r2, 32;
 add.u64 address, pointer, offset;
 st.global.v4.b32 [address], {d0,d1,d2,d3};
 st.global.v4.b32 [address+16], {d4,d5,d6,d7};
 ret;
}
'''


def main():
    # Longer FP16 recurrences plateau on this GPU; retain exact-output checks.
    grid, block, iterations, pairs = 120, 256, 768, 7
    values = grid * block * 16
    cuda = Cuda()
    report = {'scope': 'stock short register-resident FP16-input accumulation comparison; no selector-state inference',
              'utc': datetime.now(timezone.utc).isoformat(), 'before': telemetry(),
              'grid': grid, 'threads_per_block': block, 'iterations_per_chain': iterations,
              'ptx_chains': 4, 'sample_pairs': pairs, 'device_buffer_bytes': values*4,
              'nominal_flops_per_launch': (grid*block//32)*(2*16*8*16)*4*iterations,
              'pairs': [], 'results': []}
    try:
        report['cuda'] = cuda.open()
        output = cuda.allocate(values*4)
        kernels, ptx = {}, {'f16': ptx_f16(), 'f32': ptx_f32()}
        for mode in ptx:
            kernels[mode] = cuda.load(ptx[mode], 'tensor')
        start, stop = c.c_void_p(), c.c_void_p()
        for event in (start, stop):
            cuda.call('cuEventCreate', c.byref(event), 0)
            cuda.events.append(event)
        host = {'f16': (c.c_uint16*values)(), 'f32': (c.c_float*values)()}
        hashes, timings = {}, {'f16': [], 'f32': []}

        def launch(mode, count, bias, small=False, timed=False):
            if count not in (8, 256, iterations) or bias % 16 or not 0 <= bias <= 112:
                raise ValueError('Unexpected bounded control parameters')
            launch_grid, launch_block = (1, 32) if small else (grid, block)
            count_arg, operand, seed = c.c_uint32(count), c.c_uint32(0x3c003c00), c.c_float(bias)
            if timed: cuda.call('cuEventRecord', start, None)
            cuda.launch(kernels[mode], launch_grid, launch_block, [output, count_arg, operand, seed])
            if timed:
                cuda.call('cuEventRecord', stop, None);cuda.call('cuEventSynchronize', stop)
            else:
                cuda.call('cuCtxSynchronize')
            length = launch_grid * launch_block * 16
            size = length * (2 if mode == 'f16' else 4)
            cuda.call('cuMemcpyDtoH_v2', host[mode], output, size)
            expected = [16*count+bias+16*i for i in range(4)]
            if mode == 'f16':
                expected = [struct.unpack('<H', struct.pack('<e', float(x)))[0] for x in expected]
            if any(host[mode][i] != expected[(i % 16)//4] for i in range(length)):
                actual = list(host[mode][:16])
                if mode == 'f16':
                    actual = [struct.unpack('<e', struct.pack('<H', x))[0] for x in actual]
                raise RuntimeError('Accumulation output mismatch: ' + str(
                    {'mode': mode, 'iterations': count, 'bias': bias, 'first16': actual,
                     'expected': [16*count+bias+16*i for i in range(4)]}))
            if timed:
                elapsed = c.c_float()
                cuda.call('cuEventElapsedTime', c.byref(elapsed), start, stop)
                if not 0 < elapsed.value < 100:
                    raise RuntimeError('Unexpected measured duration; stop run')
                hashes[mode] = hashlib.sha256(bytes(host[mode])).hexdigest()
                return elapsed.value

        for mode in ('f16', 'f32'):
            launch(mode, 8, 0, True)
            launch(mode, 256, 0)
        report['before_samples'] = telemetry()
        for pair in range(pairs):
            order = ('f16', 'f32') if pair % 2 == 0 else ('f32', 'f16')
            row = {'order': list(order), 'initial_bias': (pair+1)*16}
            for mode in order:
                value = launch(mode, iterations, (pair+1)*16, timed=True)
                timings[mode].append(value);row[mode+'_ms'] = value
            report['pairs'].append(row)
        for mode in ('f16', 'f32'):
            median = statistics.median(timings[mode])
            report['results'].append({'input': 'f16', 'accumulation': mode,
                'milliseconds': timings[mode], 'median_ms': median,
                'nominal_tflops': report['nominal_flops_per_launch']/(median*1e9),
                'output_values_per_launch': values, 'full_output_verified_every_launch': True,
                'last_output_sha256': hashes[mode], 'ptx_sha256': hashlib.sha256(ptx[mode].encode()).hexdigest()})
        report['fp16_accum_over_fp32_accum_nominal_rate_ratio'] = (
            report['results'][0]['nominal_tflops']/report['results'][1]['nominal_tflops'])
    finally:
        cuda.close()
    report['cleanup_completed'] = True
    report['after'] = telemetry()
    report['research_source_sha256'] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
        for p in (Path(__file__), Path(__file__).with_name('cuda_driver.py'),
                  Path(__file__).with_name('gpu_tensor_probe.py'), Path(__file__).with_name('gpu_smoke.py'))}
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
