"""Bounded grid/batch sweep using exact cubins; no device configuration changes."""
import ctypes as c
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import statistics
import struct
import time

from gpu_binary_probe import BinaryCuda, MANIFEST, sha
from gpu_accumulation_probe import ptx_f16, ptx_f32
from gpu_smoke import telemetry

HERE = Path(__file__).resolve().parent


def main():
    grids, block, count, batch, pairs = (120, 360, 720), 256, 768, 16, 8
    cuda = BinaryCuda()
    BinaryCuda.loaded_metadata = {}
    report = {'scope': 'fixed-binary grid/batch characterization; no selector state or unlock inference',
        'utc': datetime.now(timezone.utc).isoformat(), 'before': telemetry(),
        'grids': list(grids), 'block': block, 'iterations': count, 'batch': batch,
        'pairs_per_grid': pairs, 'minimum_warmup_seconds_per_mode': 0.25,
        'verification_limit': 'Full final output after each batch; intermediate overwritten outputs are not observed.',
        'binary_manifest_sha256': sha(MANIFEST), 'results': []}
    try:
        report['cuda'] = cuda.open()
        output = cuda.allocate(max(grids)*block*16*4)
        kernels = {'f16': cuda.load(ptx_f16(), 'tensor'), 'f32': cuda.load(ptx_f32(), 'tensor')}
        start, stop = c.c_void_p(), c.c_void_p()
        for event in (start, stop):
            cuda.call('cuEventCreate', c.byref(event), 0)
            cuda.events.append(event)
        report['loaded_function_metadata'] = BinaryCuda.loaded_metadata

        def execute(mode, grid, bias, launches):
            size = grid*block*16*(2 if mode == 'f16' else 4)
            host = c.create_string_buffer(size)
            args = [output, c.c_uint32(count), c.c_uint32(0x3c003c00), c.c_float(bias)]
            cuda.call('cuEventRecord', start, None)
            for _ in range(launches):
                cuda.launch(kernels[mode], grid, block, args)
            cuda.call('cuEventRecord', stop, None)
            cuda.call('cuEventSynchronize', stop)
            elapsed = c.c_float()
            cuda.call('cuEventElapsedTime', c.byref(elapsed), start, stop)
            if not 0 < elapsed.value < 500:
                raise RuntimeError('Batch duration outside bounds')
            cuda.call('cuMemcpyDtoH_v2', host, output, size)
            actual = hashlib.sha256(host.raw).hexdigest()
            pattern = b''.join(struct.pack('<e' if mode == 'f16' else '<f', 16*count+bias+16*i)*4
                               for i in range(4))
            if actual != hashlib.sha256(pattern*(grid*block)).hexdigest():
                raise RuntimeError('Full output mismatch')
            return elapsed.value / launches, actual

        for mode in ('f16', 'f32'):
            begin = time.perf_counter()
            while time.perf_counter()-begin < 0.25:
                execute(mode, 360, 16, 1)
        for grid in grids:
            rows, timings = [], {'f16': [], 'f32': []}
            before = telemetry()
            for pair in range(pairs):
                order = ('f16', 'f32') if pair % 2 == 0 else ('f32', 'f16')
                row = {'bias': 16*(pair+1), 'order': list(order)}
                for mode in order:
                    milliseconds, digest = execute(mode, grid, row['bias'], batch)
                    timings[mode].append(milliseconds)
                    row[mode] = {'milliseconds_per_launch': milliseconds, 'final_output_sha256': digest}
                rows.append(row)
            flops = (grid*block//32)*(2*16*8*16)*4*count
            medians = {mode: statistics.median(samples) for mode, samples in timings.items()}
            rates = {mode: flops/(milliseconds*1e9) for mode, milliseconds in medians.items()}
            report['results'].append({'grid': grid, 'nominal_flops_per_launch': flops,
                'pairs': rows, 'median_ms': medians, 'nominal_tflops': rates,
                'f16_over_f32_rate_ratio': rates['f16']/rates['f32'],
                'telemetry_before': before, 'telemetry_after': telemetry()})
    finally:
        cuda.close()
    report['cleanup_completed'] = True
    report['after'] = telemetry()
    report['research_source_sha256'] = {name: sha(HERE/name) for name in
        ('gpu_binary_sweep.py', 'gpu_binary_probe.py', 'gpu_accumulation_probe.py',
         'gpu_tensor_probe.py', 'gpu_smoke.py', 'cuda_driver.py')}
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
