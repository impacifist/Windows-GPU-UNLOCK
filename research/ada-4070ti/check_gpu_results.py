"""Recalculate the local GPU-run evidence offline. No CUDA/device calls."""

import hashlib
import json
import math
from pathlib import Path
import statistics
import struct

HERE = Path(__file__).resolve().parent


def require(value, message):
    if not value:
        raise ValueError(message)


def check_sources(report):
    for name, digest in report['research_source_sha256'].items():
        path = (HERE / name).resolve()
        require(path.parent == HERE, 'Unexpected source path')
        require(hashlib.sha256(path.read_bytes()).hexdigest() == digest,
                'Recorded source hash mismatch: ' + name)


def main():
    smoke = json.loads((HERE / 'gpu-smoke-result.json').read_text())
    check_sources(smoke)
    expected = b''.join(struct.pack('<I', 3*i+16) for i in range(65536))
    require(smoke['output_sha256'] == hashlib.sha256(expected).hexdigest(), 'Smoke CPU hash mismatch')
    require(smoke['cleanup_completed'] is True and smoke['full_output_verified_every_launch'] is True,
            'Smoke execution/cleanup not recorded as successful')
    groups = [json.loads((HERE / ('gpu-tensor-result-' + str(n) + '.json')).read_text()) for n in (1, 2, 3)]
    reference = groups[0]
    nominal = (120 * 256 // 32) * (2*16*8*16) * 4 * 4096
    pattern = struct.pack('<16f', *(65536+9+i//4 for i in range(16))) * (120*256)
    expected_hash = hashlib.sha256(pattern).hexdigest()
    identity_keys = ('name', 'pci.device_id', 'pci.sub_device_id', 'driver_version', 'vbios_version', 'power.limit')
    temperatures = []
    for report in groups:
        check_sources(report)
        for key in ('grid', 'threads_per_block', 'ptx_accumulator_chains', 'iterations_per_chain',
                    'samples_per_mode', 'device_buffer_bytes', 'research_source_sha256'):
            require(report[key] == reference[key], 'Run configuration mismatch: ' + key)
        require((report['grid'], report['threads_per_block'], report['ptx_accumulator_chains'],
                 report['iterations_per_chain'], report['samples_per_mode'], report['device_buffer_bytes']) ==
                (120, 256, 4, 4096, 7, 1966080), 'Unexpected bounded run configuration')
        require(report['cuda']['pci_bus_id'] == reference['cuda']['pci_bus_id'] and
                report['cuda']['compute_capability'] == [8, 9], 'CUDA identity mismatch')
        require(report['cleanup_completed'] is True, 'Cleanup not complete')
        require([r['input'] for r in report['results']] == ['f16', 'bf16'], 'Mode mismatch')
        telemetry = [report['before'], report['after']]
        for mode in report['results']:
            times = mode['milliseconds']
            require(len(times) == 7 and all(math.isfinite(t) and 0 < t < 100 for t in times),
                    'Invalid event samples')
            require(mode['accumulation'] == 'f32' and mode['nominal_flops_per_launch'] == nominal,
                    'Wrong accumulation or nominal work')
            require(mode['full_output_verified_every_launch'] is True and
                    mode['output_values_per_timed_launch'] == 491520, 'Missing full output check')
            require(mode['last_output_sha256'] == expected_hash, 'Tensor CPU hash mismatch')
            require(math.isclose(mode['median_ms'], statistics.median(times), rel_tol=1e-12),
                    'Median mismatch')
            require(math.isclose(mode['nominal_tflops'], nominal/(statistics.median(times)*1e9), rel_tol=1e-12),
                    'Throughput arithmetic mismatch')
            telemetry += [mode['telemetry_before_samples'], mode['telemetry_after_samples']]
        for row in telemetry:
            require(all(row[k] == reference['before'][k] for k in identity_keys),
                    'Identity/driver/VBIOS/power-limit point sample changed')
            temperatures.append(float(row['temperature.gpu']))
    rows = []
    for index, kind in enumerate(('f16', 'bf16')):
        values = [g['results'][index]['nominal_tflops'] for g in groups]
        rows.append({'input': kind, 'accumulation': 'f32', 'process_medians_nominal_tflops': values,
                     'median_of_process_medians_nominal_tflops': statistics.median(values),
                     'minimum_process_median': min(values), 'maximum_process_median': max(values)})
    summary = {'scope': 'offline arithmetic/hash check of local stock-GPU evidence',
               'gpu_runs': 3, 'timed_launches': 42, 'timed_output_values_checked': 42*491520,
               'recorded_output_hashes_match_cpu_pattern': True,
               'sampled_temperature_range_c': [min(temperatures), max(temperatures)],
               'pci_bus_id': reference['cuda']['pci_bus_id'], 'results': rows,
               'limit': 'Nominal register-work rates under background activity; no unlock, peak, GEMM or application claim.'}
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
