"""Recalculate published AD104 benchmark JSONs offline; never import GPU code.

Pass a local gpu-unlock-ada checkout. Reads files only and prints JSON to stdout.
Hashes establish consistency with the supplied manifest, not authenticity or
independent proof that measurements occurred. No third-party code is executed.
"""

import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics


def require(condition, message):
    if not condition:
        raise ValueError(message)


def load(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def validate_run(run, reference):
    for key in ('schema', 'python', 'torch', 'torch_cuda', 'gpu', 'capability',
                'benchmark_sha256', 'cublas_sha256', 'cublasLt_sha256',
                'cublas_version', 'math_mode', 'seed', 'warmup_seconds'):
        require(run[key] == reference[key], 'Unmatched run field: ' + key)
    require(run['initial_telemetry']['returncode'] == 0, 'Missing telemetry')
    require(run['initial_telemetry']['fields'] == reference['initial_telemetry']['fields'],
            'Telemetry column mismatch')
    identity = lambda r: [x.strip() for x in r['initial_telemetry']['csv'].split(',')[:6]]
    require(len(identity(run)) == 6 and identity(run) == identity(reference),
            'Identity/driver/VBIOS/power mismatch')
    require([r['mode'] for r in run['results']] ==
            [r['mode'] for r in reference['results']], 'Mode mismatch')
    for result, original in zip(run['results'], reference['results']):
        for key in ('n', 'samples', 'batch', 'compute_type', 'input_type',
                    'output_type', 'output_sha256'):
            require(result[key] == original[key], 'Unmatched mode field: ' + key)
        require(result['full_256_exact'] is True and result['sampled_large_exact'] is True,
                'Recorded numerical validation absent')
        digest = result['output_sha256']
        require(len(digest) == 64 and all(c in '0123456789abcdef' for c in digest),
                'Invalid output digest')
        times = result['milliseconds']
        require(len(times) == result['samples'] and len(times) >= 3,
                'Sample count mismatch')
        require(all(isinstance(t, (float, int)) and math.isfinite(t) and t > 0 for t in times),
                'Invalid timing sample')
        median = statistics.median(times)
        # Upstream stores per-product milliseconds, already divided by batch.
        expected = {'median_ms': median, 'min_ms': min(times), 'max_ms': max(times),
                    'median_tflops': 2 * result['n'] ** 3 / (median * 1e9)}
        for key, value in expected.items():
            require(math.isclose(result[key], value, rel_tol=1e-10, abs_tol=1e-10),
                    'Recalculation mismatch: ' + key)


def verify_sources(root, provenance):
    for name, expected in provenance['files'].items():
        path = (root / name).resolve()
        require(path.is_relative_to(root.resolve()), 'Source path escapes checkout')
        require(hashlib.sha256(path.read_bytes()).hexdigest() == expected,
                'Reviewed source hash mismatch: ' + name)


def audit(root):
    provenance = load(Path(__file__).with_name('external-provenance.json'))
    verify_sources(root, provenance)
    evidence = root / 'docs/evidence/ad104-windows'
    manifest = load(evidence / 'SHA256SUMS.json')
    for name, expected in manifest.items():
        path = (evidence / name).resolve()
        require(path.is_relative_to(evidence.resolve()), 'Manifest path escapes evidence')
        require(hashlib.sha256(path.read_bytes()).hexdigest() == expected,
                'Evidence hash mismatch: ' + name)
    paths = [sorted(evidence.glob(pattern)) for pattern in
             ('stock-*.json', 'unlocked-v054-*.json')]
    require(all(len(group) == 3 for group in paths), 'Expected three runs per phase')
    require(all(p.name in manifest for group in paths for p in group),
            'Run absent from hash manifest')
    groups = [[load(p) for p in group] for group in paths]
    reference = groups[0][0]
    require([r['mode'] for r in reference['results']] ==
            ['fp32_pedantic', 'fp16_fp32', 'bf16_fp32'], 'Expected three published modes')
    script_hash = hashlib.sha256((root / 'windows/ad104-msi/benchmark.py').read_bytes()).hexdigest()
    require(reference['benchmark_sha256'] == script_hash, 'Benchmark source hash mismatch')
    for group in groups:
        for run in group:
            validate_run(run, reference)
    claimed = load(evidence / 'validation.json')['rows']
    rows = []
    for i, result in enumerate(reference['results']):
        before, after = [statistics.median(r['results'][i]['median_tflops'] for r in g)
                         for g in groups]
        row = {'mode': result['mode'], 'stock_tflops': before,
               'after_tflops': after, 'change_percent': (after / before - 1) * 100}
        match = [c for c in claimed if c['mode'] == row['mode']]
        require(len(match) == 1, 'Missing/duplicate published summary mode')
        for key in ('stock_tflops', 'after_tflops', 'change_percent'):
            require(math.isclose(row[key], match[0][key], rel_tol=1e-10, abs_tol=1e-10),
                    'Published summary mismatch: ' + key)
        rows.append(row)
    return {'scope': 'offline consistency audit of third-party published evidence',
            'reviewed_source_commit': provenance['commit'],
            'source_inputs_checked': len(provenance['files']),
            'hardware_exercised': False, 'hashes_checked': len(manifest),
            'runs_checked': 6, 'results': rows,
            'limit': 'No original output matrices or local GPU execution; numerical flags and hashes are recorded claims.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('checkout', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(audit(args.checkout.resolve()), indent=2))
    except (OSError, KeyError, TypeError, ValueError) as error:
        parser.exit(1, str(error) + '\n')
