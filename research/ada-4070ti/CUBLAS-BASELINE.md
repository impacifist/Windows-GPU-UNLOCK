# cuBLAS baseline on this RTX 4070 Ti

Measured 2026-10-25 with the installed NVIDIA Windows driver 610.88 and VBIOS
95.04.31.00.f0. CUDA reports compute capability 8.9 and PCI `0000:0B:00.0`.
This is current-state performance with an unmodified driver, not a demonstrated
locked/unlocked comparison. No firmware, selector, clock, power-limit, driver
installation or boot configuration change was made.

## First fresh-process result

| 4096×4096 GEMM, seven event-timed samples | Median ms | TFLOP/s |
|---|---:|---:|
| FP32 input/output, pedantic FP32 compute | 4.501632 | 30.5309 |
| FP16 input, FP32 accumulation/output | 1.773920 | 77.4775 |
| BF16 input, FP32 accumulation/output | 1.824448 | 75.3318 |

Raw evidence: [gpu-cublas-result-1.json](gpu-cublas-result-1.json).
The CUDA API, library/source hashes, mode settings and point telemetry are
recorded. The cuBLAS version getter returned 120600. Point temperatures after
the modes were 35 C; this is not a peak-temperature measurement.

## Three-process baseline

Two additional fresh processes used the same source, DLLs and numerical policy.
Raw evidence: [process 2](gpu-cublas-result-2.json),
[process 3](gpu-cublas-result-3.json).

| TFLOP/s, process medians | Process 1 | Process 2 | Process 3 | Median of medians |
|---|---:|---:|---:|---:|
| FP32 pedantic | 30.531 | 29.374 | 29.750 | **29.750** |
| FP16 / FP32 accumulation | 77.478 | 75.525 | 76.729 | **76.729** |
| BF16 / FP32 accumulation | 75.332 | 75.802 | 73.806 | **75.332** |

All **63 timed output matrices** passed the complete CPU-pattern hash check,
covering **1,056,964,608 FP32 output values** across the processes. Pilot matrices
also passed. All recorded cleanup calls succeeded. Temperature point samples
across the complete runs were **31–47 C**, including after warmup; samples after
each mode alone do not describe the highest observed point, much less a peak.
The configured power limit remained 285 W in every recorded point sample.

[gpu-cublas-summary.json](gpu-cublas-summary.json) records the summary.
[check_cublas_results.py](check_cublas_results.py) independently recalculates
medians/TFLOP/s, validates configurations, source/library hashes and identity
point samples, and generates all seven complete expected CPU matrix hashes.
It reads files only, with no GPU or library loading:

```powershell
python research/ada-4070ti/check_cublas_results.py
```

## Method and numerical checks

[gpu_cublas_baseline.py](gpu_cublas_baseline.py) uses the CUDA Driver API plus
cuBLAS `GemmEx` through explicitly typed ctypes bindings. It selects the single
RTX 4070 Ti, creates a private logical CUDA context and uses a shared default
stream for library work and CUDA timing events. All API return codes are checked.
Each mode first runs a 256×256 pilot, then at least 0.25 s of synchronized warmup.
Seven 4096×4096 launches are timed individually, excluding host output copying
and checking. TFLOP/s is `2*n^3 / (median_ms*1e9)`.

Input matrices contain ones. The per-sample alpha changes from 1.125 through
1.875 and beta is zero; every expected FP32 output is therefore exactly
`4096*alpha` (4608 through 7680). Every timed output matrix is copied to the host
and its full 64 MiB hash is compared with the independently generated CPU byte
pattern. Changing alpha prevents a previous sample's correct output from passing
the next sample's check. This validates this uniform fixture, not arbitrary
matrix layout, precision or numerical accuracy. No output matrices are published.

FP32 uses pedantic compute (69) and math mode 2. FP16/BF16 use FP32 compute (68),
FP32 output and math mode 17, combining tensor math with disallowed reduced-
precision reduction. Enum values and ABI were checked against the downloaded
NVIDIA 12.6 headers. The accepted accumulator control in
[GPU-FINDINGS.md](GPU-FINDINGS.md) separately demonstrates why changing accumulator
precision is not an interchangeable benchmark setting.

At most 192 MiB of explicit matrix buffers are allocated concurrently, plus a
64 MiB host output buffer. CUDA/cuBLAS can use additional context/workspace memory.
Buffers are released between modes; library handle, timing events and the
private context are cleaned up after each process. No physical GPU reset occurs.

## Isolated NVIDIA libraries and reproduction

Only CUDA runtime 12.6.37 and cuBLAS package 12.6.0.22 were downloaded from
NVIDIA's CUDA 12.6 redistribution archive. Archives were checked against their
published sizes/SHA-256, and the three selected DLLs were checked against bytes
inside those verified archives. No installer, toolkit compiler or driver update
was run. Libraries and original notices stay under ignored `build/cuda-12.6/`.
No NVIDIA binary/header is added to Git.

[cublas-package-provenance.json](cublas-package-provenance.json) records URLs,
versions, archive hashes and selected DLL hashes. [prepare_cublas.py](prepare_cublas.py)
reproduces the isolated download/check/extraction. Existing extracted DLLs must
match; the measurement script verifies their hashes again before loading.

```powershell
python research/ada-4070ti/prepare_cublas.py
python research/ada-4070ti/gpu_cublas_baseline.py
```

This uses the installed CUDA-capable NVIDIA driver. It does not require a global
Python GPU package, CUDA toolkit installation, driver replacement or elevation.

## Interpretation limits

Background desktop/session activity remains, and clock/thermal data are point
samples. Warmup, sample count, varying alpha, cuBLAS version, driver, platform
and numerical checking differ from the external reference's published protocol.
Do not compare the two machines to calculate an unlock percentage. There is no
unlocked run on this machine, speed-selector readout, game/inference benchmark
or long-duration stability qualification. The useful contribution is a measured,
reproducible local library baseline with full-output fixture validation.
