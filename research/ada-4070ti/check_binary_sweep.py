"""Independently check saved fixed-binary grid sweep hashes and arithmetic."""
import hashlib
import json
import math
from pathlib import Path
import statistics
import struct

from check_gpu_results import check_sources, require

HERE = Path(__file__).resolve().parent


def main():
    manifest = HERE / 'gpu-binary-manifest.json'
    runs = [json.loads((HERE / f'gpu-binary-sweep-{i}.json').read_text()) for i in (1, 2, 3)]
    values_checked = 0
    for run in runs:
        check_sources(run)
        require(run['binary_manifest_sha256'] == hashlib.sha256(manifest.read_bytes()).hexdigest(),
                'Binary manifest mismatch')
        require(run['cleanup_completed'] and run['block'] == 256 and run['iterations'] == 768
                and run['batch'] == 16 and run['pairs_per_grid'] == 8, 'Unexpected run configuration')
        require([row['grid'] for row in run['results']] == [120, 360, 720], 'Grid mismatch')
        for row in run['results']:
            grid = row['grid']
            flops = grid*256//32 * (2*16*8*16) * 4 * 768
            require(row['nominal_flops_per_launch'] == flops, 'Work arithmetic mismatch')
            require(len(row['pairs']) == 8, 'Pair count mismatch')
            for index, pair in enumerate(row['pairs']):
                require(pair['bias'] == 16*(index+1) and pair['order'] ==
                        (['f16', 'f32'] if index % 2 == 0 else ['f32', 'f16']), 'Pair policy mismatch')
                for mode in ('f16', 'f32'):
                    pattern = b''.join(struct.pack('<e' if mode == 'f16' else '<f',
                        16*768+pair['bias']+16*i)*4 for i in range(4))
                    expected = hashlib.sha256(pattern*(grid*256)).hexdigest()
                    require(pair[mode]['final_output_sha256'] == expected, 'Final output differs')
                    values_checked += grid*256*16
            for mode in ('f16', 'f32'):
                samples = [pair[mode]['milliseconds_per_launch'] for pair in row['pairs']]
                require(all(math.isfinite(x) and 0 < x < 500/16 for x in samples), 'Invalid event time')
                median = statistics.median(samples)
                require(math.isclose(median, row['median_ms'][mode], rel_tol=1e-12) and math.isclose(
                    flops/(median*1e9), row['nominal_tflops'][mode], rel_tol=1e-12), 'Rate mismatch')
            require(math.isclose(row['f16_over_f32_rate_ratio'],
                    row['nominal_tflops']['f16']/row['nominal_tflops']['f32'], rel_tol=1e-12),
                    'Ratio mismatch')
    summary = {'scope': 'three-process stock-driver exact-binary sweep; no unlock inference',
               'timed_kernel_launches': 3*3*8*2*16, 'final_outputs_hash_checked': 3*3*8*2,
               'final_output_values_checked': values_checked, 'results': []}
    for index, grid in enumerate((120, 360, 720)):
        rows = [run['results'][index] for run in runs]
        summary['results'].append({'grid': grid,
            'f16_tflops': [r['nominal_tflops']['f16'] for r in rows],
            'f32_tflops': [r['nominal_tflops']['f32'] for r in rows],
            'ratios': [r['f16_over_f32_rate_ratio'] for r in rows]})
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
