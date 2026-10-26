"""Reproduce the RTX3090Ti BF16 measurement with installed CUDA PyTorch.

Usage: python benchmark_bf16.py new-result.json
Coordinate exclusive GPU use first. Changes no driver, clocks, power limit,
model residency or firmware settings. Tested with PyTorch2.12.1+cu130.
"""
import hashlib
import json
from pathlib import Path
import statistics
import subprocess
import sys
import time

import torch


def smi():
    return subprocess.check_output([
        'nvidia-smi', '--query-gpu=name,driver_version,power.limit,memory.used,utilization.gpu',
        '--format=csv,noheader'], text=True).strip()


def measure(n):
    torch.manual_seed(42)
    a = torch.randint(-8, 9, (n, n), device='cuda', dtype=torch.int8).to(torch.bfloat16).div_(8)
    b = torch.randint(-8, 9, (n, n), device='cuda', dtype=torch.int8).to(torch.bfloat16).div_(8)
    c = torch.empty((n, n), device='cuda', dtype=torch.float32)
    for _ in range(200):
        torch.mm(a, b, out_dtype=torch.float32, out=c)
    torch.cuda.synchronize()
    samples = []
    for _ in range(5):
        start, stop = torch.cuda.Event(enable_timing=True), torch.cuda.Event(enable_timing=True)
        started = time.time()
        start.record()
        for _ in range(500):
            torch.mm(a, b, out_dtype=torch.float32, out=c)
        stop.record()
        stop.synchronize()
        ms = start.elapsed_time(stop) / 500
        sample = {'started_epoch': started, 'finished_epoch': time.time(),
                  'gemm_ms': ms, 'tflops': 2 * n**3 / (ms * 1e9)}
        samples.append(sample)
        print(json.dumps({'n': n, **sample}), flush=True)
    # Independent CPU FP64 dot products for 64 deterministic output elements.
    max_error = 0.0
    for i in range(64):
        row, col = (i * 127 + 13) % n, (i * 193 + 29) % n
        expected = torch.dot(a[row].cpu().double(), b[:, col].cpu().double()).item()
        actual = c[row, col].item()
        error = abs(actual - expected)
        if not (error <= 1e-3):
            raise RuntimeError(f'GEMM mismatch at {row},{col}: {actual} != {expected}')
        max_error = max(max_error, error)
    return {'n': n, 'warmup': 200, 'iterations_per_sample': 500, 'samples': samples,
            'median_tflops': statistics.median(s['tflops'] for s in samples),
            'checked_elements': 64, 'max_abs_error': max_error}


def main():
    output = Path(sys.argv[1])
    if output.exists():
        raise FileExistsError(output)
    if torch.cuda.device_count() != 1 or torch.cuda.get_device_name(0) != 'NVIDIA GeForce RTX 3090 Ti':
        raise RuntimeError('Expected one RTX 3090 Ti')
    before = smi()
    if float(before.split(',')[-1].split()[0]) > 5:
        raise RuntimeError('GPU busy; no workload was stopped')
    torch.backends.cuda.matmul.allow_bf16_reduced_precision_reduction = False
    torch.backends.cuda.matmul.allow_tf32 = False
    result = {'torch': torch.__version__, 'cuda': torch.version.cuda,
              'source_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'input_dtype': 'bfloat16', 'output_dtype': 'float32',
              'bf16_reduced_precision_reduction': False, 'gpu_before': before,
              'measurements': []}
    telemetry_path = output.with_suffix('.telemetry.csv')
    telemetry = subprocess.Popen(['nvidia-smi',
        '--query-gpu=timestamp,power.draw,power.limit,clocks.sm,temperature.gpu,utilization.gpu,memory.used',
        '--format=csv,noheader', '--loop-ms=200', '-f', str(telemetry_path)])
    try:
        result['measurements'] = [measure(8192), measure(4096)]
    finally:
        telemetry.terminate()
        telemetry.wait(timeout=10)
    result.update(gpu_after=smi(), complete=True)
    with output.open('x') as f:
        json.dump(result, f, indent=2)
    print(json.dumps({'complete': True, 'medians': {m['n']: m['median_tflops'] for m in result['measurements']}}))


if __name__ == '__main__':
    main()
