"""Check local cuBLAS evidence offline; no GPU/library loading."""

import hashlib
import json
import math
from pathlib import Path
import statistics
import struct

from check_gpu_results import check_sources, require

HERE = Path(__file__).resolve().parent


def main():
    reports = [json.loads((HERE/('gpu-cublas-result-'+str(i)+'.json')).read_text()) for i in (1,2,3)]
    expected_hashes = [hashlib.sha256(struct.pack('<f',4096*(1+(i+1)/8))*(4096**2)).hexdigest()
                       for i in range(7)]
    provenance=json.loads((HERE/'cublas-package-provenance.json').read_text())
    configurations = [('fp32_pedantic',0,69,2,-1),('fp16_fp32',2,68,17,99),('bf16_fp32',14,68,17,99)]
    temperatures=[]
    for report in reports:
        check_sources(report)
        require(report['cleanup_completed'] is True and report['n']==4096 and
                report['samples_per_mode']==7 and report['cublas_version']==120600 and
                report['minimum_warmup_seconds']==0.25, 'Run configuration mismatch')
        require(report['libraries_sha256']==provenance['dll_sha256'], 'Library hashes differ from provenance')
        require(report['cuda']['pci_bus_id']==reports[0]['cuda']['pci_bus_id'] and
                report['cuda']['compute_capability']==[8,9], 'Device mismatch')
        require(len(report['results'])==3,'Missing modes')
        telemetry=[report['before'],report['after']]
        for mode,expected in zip(report['results'],configurations):
            require((mode['mode'],mode['input_type'],mode['compute_type'],mode['math_mode'],mode['algorithm'])==expected,
                    'Numerical policy mismatch')
            require(mode['full_output_verified_every_sample'] is True and mode['output_type']=='fp32',
                    'Output verification absent')
            require(mode['warmup_launches']>0 and mode['matrix_allocation_bytes']==
                    (192 if mode['input_type']==0 else 128)*1024*1024, 'Allocation/warmup mismatch')
            require(mode['output_sha256_per_sample']==expected_hashes, 'Full-matrix CPU hash mismatch')
            require(mode['alpha_per_sample']==[1+(i+1)/8 for i in range(7)], 'Changed sample outputs')
            times=mode['milliseconds']
            require(len(times)==7 and all(math.isfinite(t) and 0<t<100 for t in times),'Invalid times')
            median=statistics.median(times)
            require(math.isclose(mode['median_ms'],median,rel_tol=1e-12) and
                    math.isclose(mode['tflops'],2*4096**3/(median*1e9),rel_tol=1e-12),'Timing arithmetic mismatch')
            telemetry += [mode['telemetry_before_samples'],mode['telemetry_after_samples']]
        for row in telemetry:
            for key in ('name','pci.device_id','pci.sub_device_id','driver_version','vbios_version','power.limit'):
                require(row[key]==reports[0]['before'][key],'Identity/settings point sample changed')
            temperatures.append(float(row['temperature.gpu']))
    rows=[]
    for i,(label,*_) in enumerate(configurations):
        values=[r['results'][i]['tflops'] for r in reports]
        rows.append({'mode':label,'process_medians_tflops':values,
                     'median_of_process_medians_tflops':statistics.median(values)})
    print(json.dumps({'scope':'offline verification of three local current-GPU cuBLAS runs',
        'fresh_processes':3,'timed_matrices_checked':63,'elements_per_matrix':4096**2,
        'full_output_fp32_values_covered_by_hashes':63*4096**2,
        'cpu_expected_output_hashes_match':True,'sampled_temperature_range_c':[min(temperatures),max(temperatures)],
        'results':rows,'limit':'Uniform-fixture local baseline; not general numerical accuracy, selector state or unlock uplift.'},indent=2))


if __name__=='__main__':
    main()
