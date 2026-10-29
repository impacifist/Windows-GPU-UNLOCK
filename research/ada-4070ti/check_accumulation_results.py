"""Offline arithmetic and CPU-pattern checks for accumulator comparison evidence."""

import hashlib
import json
import math
from pathlib import Path
import statistics
import struct

from check_gpu_results import check_sources, require

HERE = Path(__file__).resolve().parent


def main():
    precision = json.loads((HERE / 'gpu-precision-result.json').read_text())
    check_sources(precision)
    require(precision['cleanup_completed'] is True, 'Precision probe cleanup absent')
    for row in precision['observations']:
        ideal = [16*row['iterations']+row['initial_bias']+16*i for i in range(4)]
        require(row['ideal_chain_values'] == ideal, 'Incorrect ideal recurrence')
        actual = row['observed_chain_values']
        require(row['matches_ideal_all_512_values'] == (actual == ideal), 'Wrong numerical status')
        dtype = 'e' if row['accumulation'] == 'f16' else 'f'
        pattern = struct.pack('<16'+dtype, *(actual[i//4] for i in range(16))) * 32
        require(hashlib.sha256(pattern).hexdigest() == row['output_sha256'], 'Precision CPU hash mismatch')
    reports = [json.loads((HERE / ('gpu-accumulation-result-'+str(i)+'.json')).read_text()) for i in (1,2,3)]
    rates, ratios = {'f16': [], 'f32': []}, []
    for report in reports:
        check_sources(report)
        require(report['cleanup_completed'] is True, 'Control cleanup absent')
        require((report['grid'], report['threads_per_block'], report['iterations_per_chain'],
                 report['ptx_chains'], report['sample_pairs']) == (120,256,768,4,7), 'Changed control configuration')
        work = (120*256//32)*(2*16*8*16)*4*768
        require(report['nominal_flops_per_launch'] == work, 'Nominal work mismatch')
        require([r['accumulation'] for r in report['results']] == ['f16','f32'], 'Mode mismatch')
        for mode in report['results']:
            dtype = mode['accumulation']
            times = mode['milliseconds']
            require(len(times)==7 and all(math.isfinite(t) and 0<t<100 for t in times), 'Invalid times')
            require(math.isclose(mode['median_ms'],statistics.median(times),rel_tol=1e-12), 'Median mismatch')
            require(math.isclose(mode['nominal_tflops'],work/(mode['median_ms']*1e9),rel_tol=1e-12), 'Rate mismatch')
            pattern = struct.pack('<16'+('e' if dtype=='f16' else 'f'),
                                  *(16*768+112+16*(i//4) for i in range(16))) * (120*256)
            require(hashlib.sha256(pattern).hexdigest()==mode['last_output_sha256'], 'Control CPU hash mismatch')
            require(mode['full_output_verified_every_launch'] is True, 'Missing full-output check')
            rates[dtype].append(mode['nominal_tflops'])
        for i,pair in enumerate(report['pairs']):
            require(pair['order'] == (['f16','f32'] if i%2==0 else ['f32','f16']), 'Order mismatch')
            require(pair['initial_bias']==16*(i+1), 'Seed mismatch')
        ratio=report['results'][0]['nominal_tflops']/report['results'][1]['nominal_tflops']
        require(math.isclose(ratio,report['fp16_accum_over_fp32_accum_nominal_rate_ratio'],rel_tol=1e-12), 'Ratio mismatch')
        ratios.append(ratio)
    print(json.dumps({'scope':'offline check of current-GPU numerical/control evidence',
        'precision_cases':len(precision['observations']), 'control_processes':3,
        'nominal_tflops_by_accumulator':rates,
        'fp16_over_fp32_rate_ratios':ratios, 'median_ratio':statistics.median(ratios),
        'cpu_output_hashes_match':True,
        'limit':'Different register/output traffic and short contended timings; not selector state or an unlock percentage.'},indent=2))


if __name__ == '__main__':
    main()
