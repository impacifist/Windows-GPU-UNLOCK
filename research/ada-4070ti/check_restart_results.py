"""Verify saved restart follow-up evidence without loading a GPU library."""
import hashlib
import json
import struct
from pathlib import Path

from check_gpu_results import check_sources, require

HERE = Path(__file__).resolve().parent


def main():
    read = lambda name: json.loads((HERE / name).read_text(encoding='utf-8-sig'))
    restart = read('gpu-driver-restart-result.json')
    require(restart['restart_command_completed'] and restart['boot_session_unchanged'],
            'Incomplete request or changed boot session')
    require(restart['restart_exit_code'] is None and
            restart['original_controller_health_recovered'] is False,
            'Original reporting limitation must remain visible')
    require('Driver restarted successfully' in restart['vendor_stdout'], 'Missing vendor report')
    require(len(restart['followup_health']) == 2 and all(
        row['healthy'] and row['nvml_query_exit_code'] == 0 and row['problem_code'] == 0
        and row['pnp_status'] == 'OK' and row['kernel_driver_state'] == 'Running'
        and row['nvml_identity_ok'] for row in restart['followup_health']), 'Unhealthy follow-up')
    smoke, nvapi, blas = [read(name) for name in restart['fresh_compute_reports']]
    for report in (smoke, nvapi, blas):
        check_sources(report)
    require(smoke['full_output_verified_every_launch'] and smoke['verified_elements_total'] == 655360
            and smoke['cleanup_completed'], 'CUDA smoke mismatch')
    require(nvapi['cuda_multiprocessor_count'] == 60 and
            nvapi['shader_subpipe_count']['value'] == 30 and nvapi['client_unloaded'], 'NVAPI mismatch')
    expected = [hashlib.sha256(struct.pack('<f', 4096*(1+(i+1)/8))*(4096**2)).hexdigest()
                for i in range(7)]
    require(blas['n'] == 4096 and blas['samples_per_mode'] == 7 and blas['cleanup_completed'],
            'Unexpected GEMM configuration')
    require([r['mode'] for r in blas['results']] == ['fp32_pedantic', 'fp16_fp32', 'bf16_fp32'],
            'Missing GEMM modes')
    for mode in blas['results']:
        require(mode['full_output_verified_every_sample'] and
                mode['output_sha256_per_sample'] == expected, 'GEMM output hashes differ')
    print('Restart follow-up verified: two healthy snapshots, 655360 smoke values, '
          '21 complete GEMM outputs (352321536 FP32 values), source hashes current.')


if __name__ == '__main__':
    main()
