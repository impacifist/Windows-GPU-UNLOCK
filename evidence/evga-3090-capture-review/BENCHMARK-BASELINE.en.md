# EVGA pre-attempt benchmark baseline

[English](BENCHMARK-BASELINE.en.md) | [한국어](BENCHMARK-BASELINE.ko.md)

Movian built successfully with VS2019 Community, passed metadata preflight and uploaded the three-trial results. Offline load_run validation passes the completion hashes, exact GPU/library/protocol metadata, four distinct modes, correctness flags, sample parameters and timing/throughput consistency. The uploaded context contains artifact hashes and runtime metadata; the separate build manifest/runtime files were not included. These hashes are recorded provenance, not independent inspection of the contributor executable.

| Mode | Median TFLOPS |
|---|---:|
| Pedantic FP32 | 26.379272 |
| FP16 tensor, FP32 accumulate | 64.527754 |
| BF16 tensor, FP32 accumulate | 64.558792 |
| TF32 tensor, FP32 accumulate | 32.982266 |

RTX3090, BDF0000:01:00.0, compute8.6, CUDA12.6/cuBLAS12.6, driver621.01. Recorded power limit420W, maximum SM2100MHz and memory9751MHz; these are recorded settings, not a claim of stock clocks or measured sustained clocks. Preserve the existing binary and settings, and run the same runner after an actual attempt without rebuilding. Supply the new complete result directory and report whether Windows arrived directly; we compare the saved runs offline. Do not infer a speedup until an after-run exists.

[Sanitized data and artifact hashes](benchmark-baseline.json). Full uploaded results remain ignored. This is a pre-attempt baseline based on the tested sequence containing no unlock transaction; it does not attest the hardware's locked state independently.
