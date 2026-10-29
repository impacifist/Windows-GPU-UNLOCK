"""Recalculate exact-binary evidence without loading CUDA or tools."""
import hashlib
import json
from pathlib import Path
import statistics
import struct

from check_gpu_results import check_sources, require

HERE = Path(__file__).resolve().parent


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    manifest_path = HERE / 'gpu-binary-manifest.json'
    manifest = json.loads(manifest_path.read_text())
    require(manifest['source_sha256'] == sha(HERE / 'gpu_binary_probe.py'), 'Compiler wrapper changed')
    for mode, item in manifest['kernels'].items():
        sass = HERE / item['sass_file']
        require(sha(sass) == item['sass_sha256'], 'Disassembly changed')
        lines = [line.strip() for line in sass.read_text().splitlines() if 'HMMA.' in line]
        require(lines == item['static_hmma_instructions'] and len(lines) == 32, 'Static HMMA mismatch')
        require(all('HMMA.16816.' + mode.upper() in line for line in lines), 'Wrong accumulator opcode')
        require('0 bytes spill stores, 0 bytes spill loads' in item['compiler_log'], 'Unexpected spills')
    ratios = []
    for index in (1, 2, 3):
        report = json.loads((HERE / f'gpu-binary-result-{index}.json').read_text())
        check_sources(report)
        require(report['binary_manifest_sha256'] == sha(manifest_path), 'Manifest mismatch')
        require(report['cleanup_completed'] and report['grid'] == 120 and report['iterations_per_chain'] == 768,
                'Unexpected launch configuration')
        for mode, result in zip(('f16', 'f32'), report['results']):
            item = manifest['kernels'][mode]
            require(report['executed_cubin_sha256'][mode] == item['cubin_sha256'], 'Binary mismatch')
            metadata = report['loaded_function_metadata'][mode]
            require(metadata['registers'] == item['register_count'] and metadata['binary_version'] == 89
                    and metadata['local_bytes_per_thread'] == metadata['static_shared_bytes'] == 0
                    and metadata['predicted_max_active_blocks_per_sm_at_256_threads'] == 6, 'Resource mismatch')
            values = [16*768+112+16*i for i in range(4)]
            pattern = b''.join(struct.pack('<e' if mode == 'f16' else '<f', x)*4 for x in values)
            require(hashlib.sha256(pattern*(120*256)).hexdigest() == result['last_output_sha256'],
                    'Final CPU/GPU output hash differs')
            require(result['full_output_verified_every_launch'] and result['output_values_per_launch'] == 491520,
                    'Incomplete output verification')
            require(len(result['milliseconds']) == 7 and all(0 < x < 100 for x in result['milliseconds']),
                    'Invalid sample timing')
            median = statistics.median(result['milliseconds'])
            require(abs(median-result['median_ms']) < 1e-12 and abs(
                result['nominal_tflops']-report['nominal_flops_per_launch']/(median*1e9)) < 1e-9,
                'Timing arithmetic mismatch')
        ratios.append(report['results'][0]['nominal_tflops']/report['results'][1]['nominal_tflops'])
    print(json.dumps({'timed_launches': 42, 'output_values_checked': 42*491520,
                      'rate_ratios': ratios, 'equal_predicted_occupancy_ceiling': True}, indent=2))


if __name__ == '__main__':
    main()
