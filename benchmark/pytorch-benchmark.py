# ASRock port changes: 2026-10-27; publication notice: 2026-10-28. GPL-2.0-only; see CHANGES.md.
"""Repeatable RTX 3060 GEMM benchmark; labels do not detect unlock state."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import statistics
import subprocess
import os

import torch


def gpu_state():
    result = subprocess.run([
        'nvidia-smi', '--query-gpu=name,pci.bus_id,driver_version,power.limit,'
        'temperature.gpu,clocks.sm,clocks.mem,power.draw,utilization.gpu',
        '--format=csv,noheader,nounits'], capture_output=True, text=True, check=True)
    rows = result.stdout.strip().splitlines()
    if len(rows) != 1:
        raise RuntimeError('Expected exactly one NVIDIA GPU')
    names = ['name', 'bus', 'driver', 'power_limit_w', 'temperature_c',
             'sm_mhz', 'memory_mhz', 'power_w', 'utilization_percent']
    return dict(zip(names, (part.strip() for part in rows[0].split(','))))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--label', choices=['unlocked', 'locked', 'unknown'], default='unknown')
    parser.add_argument('--size', type=int, choices=[4096, 8192], default=8192)
    args = parser.parse_args()
    if os.environ.get('NVIDIA_TF32_OVERRIDE'):
        raise RuntimeError('Unset NVIDIA_TF32_OVERRIDE for a comparable measurement')
    if not torch.cuda.is_available() or torch.cuda.device_count() != 1:
        raise RuntimeError('A CUDA-enabled PyTorch and exactly one GPU are required')
    initial = gpu_state()
    if initial['name'] != 'NVIDIA GeForce RTX 3060':
        raise RuntimeError('This benchmark is intended for the RTX 3060')
    if float(initial['utilization_percent']) > 10:
        raise RuntimeError('GPU is busy. Close GPU workloads and try again.')
    torch.manual_seed(42)
    torch.backends.fp32_precision = 'ieee'
    torch.backends.cuda.matmul.allow_fp16_reduced_precision_reduction = False
    torch.backends.cuda.matmul.allow_fp16_accumulation = False
    torch.backends.cuda.matmul.allow_bf16_reduced_precision_reduction = False
    n, iterations, samples = args.size, 10, 5
    report = {
        'protocol': 1, 'label': args.label, 'label_is_user_supplied': True,
        'utc': datetime.now(timezone.utc).isoformat(),
        'script_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'torch': torch.__version__, 'cuda': torch.version.cuda,
        'size': n, 'iterations': iterations, 'samples': samples,
        'fp32_accumulation_reduction': True, 'gpu_before': initial, 'benchmarks': [],
    }
    print(f"{initial['name']} | PyTorch {torch.__version__} | {args.label}", flush=True)
    with torch.inference_mode():
        for name, dtype, precision in [
            ('FP32', torch.float32, 'ieee'), ('FP16', torch.float16, 'ieee'),
            ('BF16', torch.bfloat16, 'ieee'), ('TF32', torch.float32, 'tf32')]:
            torch.backends.cuda.matmul.fp32_precision = precision
            a = (torch.randn(n, n, device='cuda') / math.sqrt(n)).to(dtype)
            b = torch.randn(n, n, device='cuda').to(dtype)
            c = torch.empty((n, n), device='cuda', dtype=dtype)
            for _ in range(20):
                torch.mm(a, b, out=c)
            torch.cuda.synchronize()
            reference = a[:16].double() @ b[:, :16].double()
            tolerance = {'FP32': 0.0001, 'FP16': 0.005, 'BF16': 0.025, 'TF32': 0.005}[name]
            torch.testing.assert_close(c[:16, :16].double(), reference,
                                       rtol=tolerance, atol=tolerance)
            timings = []
            for _ in range(samples):
                start, end = torch.cuda.Event(enable_timing=True), torch.cuda.Event(enable_timing=True)
                start.record()
                for _ in range(iterations):
                    torch.mm(a, b, out=c)
                end.record()
                end.synchronize()
                timings.append(start.elapsed_time(end) / iterations)
            milliseconds = statistics.median(timings)
            tflops = 2 * n ** 3 / (milliseconds * 1e9)
            report['benchmarks'].append({
                'mode': name, 'median_ms': milliseconds, 'tflops': tflops,
                'samples_ms': timings, 'correctness_checked': True, 'gpu_after': gpu_state(),
            })
            print(f'{name:5} {tflops:7.2f} TFLOPS | {milliseconds:7.2f} ms', flush=True)
            del a, b, c, reference
    output = Path(__file__).parent / 'results' / ('pytorch-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f') + '-' + args.label + '.json')
    output.parent.mkdir(exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'Results: {output}', flush=True)


if __name__ == '__main__':
    main()
