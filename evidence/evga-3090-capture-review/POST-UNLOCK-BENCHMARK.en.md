# EVGA post-attempt benchmark

[English](POST-UNLOCK-BENCHMARK.en.md) | [한국어](POST-UNLOCK-BENCHMARK.ko.md)

After the [successful EVGA19 transaction](EVGA19-RESULT.en.md) and reported automatic Windows boot, Movian ran the same three-trial GEMM benchmark. Median TFLOPS:

| Workload | Before | After | Change |
| --- | ---: | ---: | ---: |
| FP32 pedantic | 26.379272 | 26.493910 | +0.43% |
| FP16 tensor | 64.527754 | 122.869566 | +90.41% |
| BF16 tensor | 64.558792 | 122.809845 | +90.23% |
| TF32 tensor | 32.982266 | 64.157611 | +94.52% |

The offline comparison accepted both completed result sets and their recorded artifacts, runtime, settings and trial checks. The supplied post-run build manifest and runtime verification exactly match the corresponding recorded contexts for both runs, including executable and library hashes. Declan relayed Movian's confirmation that settings were unchanged; the confirmation itself was not visible in the inspected thread messages. The runtime verification's `gpu_workload_run=false` describes the preflight, not the three benchmark trials. These results, alongside the firmware log, are strong evidence of increased tensor GEMM throughput on this machine; they do not establish causality in isolation, general workload gains, full numerical correctness or long-term stability.

Raw uploads remain private. Post-run ZIP SHA-256: `adac78190fc98d5918420e926b6a129dfc60c607b2dab91a226c03436652baa2`.
The supplied manifest SHA-256 is `a4d0c6b8bf761c2ba3dbad3e1e9e32c3d0f8ea55ebb8f2ab165680d04a0cdcde`; runtime verification SHA-256 is `b2a683aa89facc5f1ace5ab449150b964a564099d6b24b1d183f4a1f6bbcde69`.
