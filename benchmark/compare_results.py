"""Compare saved three-trial GEMM results offline; never access GPU or firmware."""

import argparse
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import re
import statistics
import sys


LEGACY_NAMES = ('fp32_pedantic', 'fp16_tensor_fp32_accumulate',
                'bf16_tensor_fp32_accumulate')
NAMES = LEGACY_NAMES + ('tf32_tensor_fp32_accumulate',)
IDENTITY = ('gpu', 'pci', 'compute_capability', 'cuda_runtime',
            'cuda_driver_api', 'cublas')
CARDS = {'NVIDIA GeForce RTX 3090': '3090',
         'NVIDIA GeForce RTX 3090 Ti': '3090ti',
         'NVIDIA GeForce RTX 3070 Ti': '3070ti',
         'NVIDIA GeForce RTX 3060': '3060'}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_json(path):
    def unique_pairs(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, f'{path.name}: duplicate key {key}')
            result[key] = value
        return result

    return json.loads(path.read_text(encoding='utf-8-sig'),
                      object_pairs_hook=unique_pairs)


def positive(value):
    return (type(value) in (int, float) and math.isfinite(value) and value > 0)


def pci_key(value):
    match = re.fullmatch(r'([0-9a-fA-F]{1,8}):([0-9a-fA-F]{2}):([0-9a-fA-F]{2})\.([0-7])', value)
    require(match is not None, 'Invalid PCI location')
    return tuple(int(part, 16) for part in match.groups())


def context_key(context):
    require(isinstance(context, dict) and type(context.get('schema_version')) is int
            and context['schema_version'] in (1, 2),
            'Missing or unsupported comparison context')
    artifacts = context.get('artifacts')
    require(isinstance(artifacts, list) and artifacts, 'Missing build artifacts')
    normalized = {}
    for artifact in artifacts:
        require(isinstance(artifact, dict), 'Invalid artifact record')
        path = artifact.get('Path')
        digest = artifact.get('SHA256')
        require(isinstance(path, str) and path, 'Missing artifact path')
        path = path.replace('\\', '/').lower()
        require(not PurePosixPath(path).is_absolute() and ':' not in path
                and all(part not in ('', '.', '..') for part in path.split('/')),
                'Artifact paths must be relative and unambiguous')
        require(path not in normalized, 'Duplicate artifact path')
        require(isinstance(digest, str) and re.fullmatch('[0-9a-fA-F]{64}', digest),
                'Invalid artifact SHA256')
        normalized[path] = digest.lower()
    require(any(path.startswith('bin/') and path.endswith('.dll')
                and 'cublas' in path for path in normalized),
            'Missing cuBLAS DLL hash')
    settings = context.get('gpu_settings')
    require(isinstance(settings, list) and len(settings) == 6
            and all(isinstance(value, str) and value.strip() for value in settings),
            'Incomplete GPU settings')
    require(settings[0] in CARDS, 'Unsupported benchmark GPU model')
    executable = f'bin/compute-benchmark-{CARDS[settings[0]]}.exe'
    require(executable in normalized, 'Missing selected benchmark executable hash')
    # Match recorded power/clock limits, but never infer actual clocks from them.
    require(all(re.fullmatch(r'[0-9]+(?:\.[0-9]+)?', value)
                and float(value) > 0 for value in settings[3:]),
            'Power and maximum clock settings must be available')
    runtime = context.get('runtime')
    require(isinstance(runtime, dict) and runtime.get('gpu_workload_run') is False,
            'Missing runtime inspection')
    for key in ('cuda_runtime', 'cublas_major', 'cublas_minor', 'cublas_patch'):
        require(type(runtime.get(key)) is int and runtime[key] >= 0,
                f'Invalid runtime field: {key}')
    names = LEGACY_NAMES
    if context['schema_version'] == 2:
        require(context.get('benchmark_names') == list(NAMES),
                'Incomplete four-mode protocol')
        require(type(runtime.get('benchmark_protocol')) is int
                and runtime['benchmark_protocol'] == 2
                and runtime.get('tf32_allowed') is True,
                'Missing TF32 runtime protocol/environment confirmation')
        names = NAMES
    else:
        require('benchmark_protocol' not in runtime and 'tf32_allowed' not in runtime,
                'New runtime cannot use legacy context')
    return {'artifacts': normalized, 'gpu_settings': settings, 'runtime': runtime,
            'benchmark_names': names}


def load_run(directory):
    completion = read_json(directory / 'comparison-complete.json')
    require(isinstance(completion, dict) and type(completion.get('schema_version')) is int
            and completion['schema_version'] == 1, 'Missing completion record')
    files = completion.get('files')
    require(isinstance(files, list) and len(files) == 4, 'Incomplete result hash list')
    expected = {'comparison-context.json', 'trial-1.json', 'trial-2.json', 'trial-3.json'}
    seen_files = set()
    for record in files:
        require(isinstance(record, dict), 'Invalid completion hash record')
        name = record.get('Path')
        require(isinstance(name, str) and name in expected and name not in seen_files,
                'Unexpected or duplicate completed file')
        seen_files.add(name)
        digest = record.get('SHA256')
        require(isinstance(digest, str) and re.fullmatch('[0-9a-fA-F]{64}', digest),
                'Invalid result SHA256')
        require(hashlib.sha256((directory / name).read_bytes()).hexdigest() == digest.lower(),
                f'{name}: changed after run completion')
    context = context_key(read_json(directory / 'comparison-context.json'))
    paths = sorted(directory.glob('trial-*.json'))
    require({p.name for p in paths} == {'trial-1.json', 'trial-2.json', 'trial-3.json'},
            'Expected exactly trial-1.json, trial-2.json and trial-3.json')
    identity = None
    names = context['benchmark_names']
    values = {name: [] for name in names}
    for path in paths:
        trial = read_json(path)
        require(isinstance(trial, dict), f'{path.name}: invalid trial')
        require(all(key in trial for key in IDENTITY), 'Incomplete trial metadata')
        current = {key: trial[key] for key in IDENTITY}
        if names == NAMES:
            require(type(trial.get('benchmark_protocol')) is int
                    and trial['benchmark_protocol'] == 2
                    and trial.get('tf32_allowed') is True,
                    'Missing TF32 trial protocol/environment confirmation')
        else:
            require('benchmark_protocol' not in trial and 'tf32_allowed' not in trial,
                    'New trial cannot use legacy context')
        require(all(isinstance(current[key], str) and current[key]
                    for key in IDENTITY[:3]), 'Invalid GPU identity')
        require(all(type(current[key]) is int and current[key] > 0
                    for key in IDENTITY[3:]), 'Invalid library metadata')
        require(identity is None or current == identity, 'Metadata changed between trials')
        identity = current
        settings = context['gpu_settings']
        require(current['gpu'] == settings[0] and pci_key(current['pci']) == pci_key(settings[1]),
                'Trial GPU differs from recorded settings')
        runtime = context['runtime']
        require(current['cuda_runtime'] == runtime['cuda_runtime']
                and current['cublas'] == runtime['cublas_major'] * 10000
                + runtime['cublas_minor'] * 100 + runtime['cublas_patch'],
                'Trial libraries differ from runtime inspection')
        benchmarks = trial.get('benchmarks')
        require(isinstance(benchmarks, list) and len(benchmarks) == len(names),
                'Incomplete benchmark set')
        seen = set()
        for benchmark in benchmarks:
            require(isinstance(benchmark, dict), 'Invalid benchmark record')
            name = benchmark.get('name')
            require(isinstance(name, str) and name in names and name not in seen,
                    'Unknown or duplicate benchmark')
            seen.add(name)
            for key, expected in (('n', 4096), ('samples', 30), ('warmup_seconds', 2)):
                require(type(benchmark.get(key)) is int and benchmark[key] == expected,
                        f'{name}: unsupported protocol field {key}')
            require(benchmark.get('reference_checked') is True,
                    f'{name}: missing result check')
            require(all(positive(benchmark.get(key)) for key in
                        ('median_ms', 'min_ms', 'max_ms', 'gflops')),
                    f'{name}: non-finite or non-positive measurement')
            require(benchmark['min_ms'] <= benchmark['median_ms'] <= benchmark['max_ms'],
                    f'{name}: inconsistent timing range')
            expected_gflops = 2 * benchmark['n'] ** 3 / (benchmark['median_ms'] * 1e6)
            require(math.isclose(benchmark['gflops'], expected_gflops, rel_tol=0.0001),
                    f'{name}: throughput does not match timing')
            values[name].append(benchmark['gflops'] / 1000)
    return context, identity, values


def compare(before, after, *, same_machine_confirmed=False):
    require(same_machine_confirmed,
            'Confirm the same physical machine, GPU, OS and unchanged manual tuning')
    require(before.resolve() != after.resolve(), 'Choose two distinct result directories')
    left, left_id, left_values = load_run(before)
    right, right_id, right_values = load_run(after)
    require(left == right, 'Build, benchmark protocol, runtime, driver or GPU settings differ')
    require(left_id == right_id, 'GPU or library metadata differ')
    rows = []
    for name in left['benchmark_names']:
        baseline = statistics.median(left_values[name])
        candidate = statistics.median(right_values[name])
        rows.append({'name': name, 'before_median_tflops': baseline,
                     'after_median_tflops': candidate, 'ratio': candidate / baseline,
                     'change_percent': (candidate / baseline - 1) * 100,
                     'before_trials_tflops': left_values[name],
                     'after_trials_tflops': right_values[name]})
    return {'schema_version': 1, 'same_machine_operator_confirmed': True,
            'benchmarks': rows,
            'fp32_control_change_percent': rows[0]['change_percent'],
            'limitations': 'Observed GEMM ratios only. No automatic proof of unlock, '
            'causality, hardware support, complete numerical correctness or stability. '
            'Inspect temperature, clocks, power and background workloads separately.'}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('before', type=Path)
    parser.add_argument('after', type=Path)
    parser.add_argument('--same-machine-confirmed', action='store_true',
                        help='Confirm same physical machine/GPU/OS and unchanged manual tuning')
    args = parser.parse_args(argv)
    try:
        result = compare(args.before, args.after,
                         same_machine_confirmed=args.same_machine_confirmed)
    except (OSError, ValueError, TypeError, KeyError, OverflowError) as exc:
        print(f'Comparison refused: {exc}', file=sys.stderr)
        return 2
    print(json.dumps(result, indent=2, allow_nan=False))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
