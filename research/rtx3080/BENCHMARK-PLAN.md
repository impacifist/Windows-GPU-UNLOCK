# Proposed ordinary RTX 3080 baseline

**Historical design proposal:** the separate implementation and completed GPU
measurement are now documented in
[PR #45](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/45) and its
[native validation report](https://github.com/danhol86/Windows-GPU-UNLOCK/blob/a04471a8c2cec047c25383315d122f1d6513462c/benchmark/rtx3080/NATIVE-VALIDATION.md).
The text below records the design stage before that implementation and execution.
This plan concerns normal CUDA computation on the configuration in
[inventory.json](inventory.json). Its execution is a separate scope from this
research contribution. Inspected upstream revision:
`5c98f11eaafdcb7a2a06fb14fcdcde7297eb409e`.

Call any eventual result an **ordinary CUDA baseline on current settings** until
factory settings are independently confirmed. A version string or settings
snapshot cannot establish firmware history, stock configuration or an unlock
state.

## Gaps found in the existing benchmark

| Source at the inspected revision | Finding | Proposed acceptance criterion |
|---|---|---|
| [build.ps1](../../benchmark/build.ps1), [run-benchmark.ps1](../../benchmark/run-benchmark.ps1), [compare_results.py](../../benchmark/compare_results.py) | Named targets are 3090, 3090 Ti and 3070 Ti. | Add an explicit ordinary-compute 3080 target and matching result identity; retain existing targets and refusal tests. |
| Runner and [3090 CUDA source](../../benchmark/compute-benchmark-3090.cu) | Device admission expects PCI `01:00.0`; this card was observed at `07:00.0`. | Match the inventoried device and current location explicitly; a different card's name or location is not an acceptable substitute. |
| CUDA source | Inputs are all ones; only first and last output values are checked. | Record precise correctness coverage and add nonuniform, signed and interior-output checks. |
| CUDA source | Thirty timings are collected internally, but only min/median/max and count are emitted. | Retain each raw timing so summaries can be independently reproduced. |
| Runner | Child execution has no explicit wall-time deadline. | Bound the owned benchmark processes and reject partial or timed-out results. |
| Build script / local environment | Default compiler path is absent; installed compiler is outside the pinned toolkit's published compiler family. | Establish a documented compiler/toolkit combination and record its identity before compiling. See [FOLLOW-UP.md](FOLLOW-UP.md#ordinary-benchmark-build-readiness). |

These observations concern the ordinary benchmark. This plan introduces no
firmware profile, firmware diagnostic or change to experimental boot code.

## Precision contract

The following is the proposed contract for a new benchmark protocol, not a
claim that the current program has been changed:

| Mode | A/B storage | Compute selection | Output |
|---|---|---|---|
| FP32 control | FP32 | `CUBLAS_COMPUTE_32F_PEDANTIC` | FP32 |
| FP16 | FP16 | `CUBLAS_COMPUTE_32F` | FP32 |
| BF16 | BF16 | `CUBLAS_COMPUTE_32F` | FP32 |
| TF32 | FP32 | `CUBLAS_COMPUTE_32F_FAST_TF32` | FP32 |

Record the actual API, math mode, scalar type, transpose flags and leading
dimensions alongside these types. Require FP32 `alpha=1`, `beta=0` for
independent timed operations. Compute type alone does not specify output type;
the output declaration must also be checked. TF32 does not mean full FP32
arithmetic. Requested modes do not prove which hardware instructions executed.

The current source uses `cublasSgemm` with pedantic math for its FP32 control.
Changing that to an explicit compute-type API would be a protocol change.

[NVIDIA's cuBLAS 12.6 reference](https://docs.nvidia.com/cuda/archive/12.6.0/cublas/index.html)
defines these combinations and the effect of `NVIDIA_TF32_OVERRIDE=0`.
Record relevant environment variables and reject a conflicting override.
Keep environment handling local to the benchmark process; do not change global
settings to make a comparison pass.

## Numerical validation proposal

Use three complementary checks, outside the timed regions:

1. **Small matrices, every output checked.** Include a square case and
   `M=31, N=47, K=65`, signed nonuniform values, zeros, identity, transpose and
   leading-dimension cases. Use an independent FP64 CPU reference. Include
   bounded exact binary fractions as well as seeded random inputs.
2. **Benchmark-sized known-answer case, every output checked.** Multiply a
   signed diagonal scaling matrix by a nonuniform matrix, using representable
   values and bounded products. An independent reference then costs quadratic
   work instead of a full cubic CPU matrix multiply.
3. **Timed random workload, explicitly sampled arithmetic checks.** Scan all
   returned outputs for NaN/Inf. Check 1024 deterministic, stratified positions
   against independent CPU dot products, including interior tiles and edges.
   At `K=4096`, this is approximately 4.2 million reference terms. Record the
   selected positions/seed and identify this as sampled coverage.

For FP16/BF16, construct the reference from the actual converted input values.
For TF32, use a declared tolerance accounting for its computation precision.
Record maximum absolute error, relative/norm error, reference precision and
a mode-specific tolerance rule fixed before execution. Treat signed zeros as
numerically equal. A failed check must not be made to pass by widening its
tolerance after seeing the result.

[NVIDIA's verification guidance](https://docs.nvidia.com/cuda/archive/12.6.3/cuda-c-best-practices-guide/index.html#verification)
supports reference comparisons with an appropriate error criterion; its
[floating-point guide](https://docs.nvidia.com/cuda/floating-point/index.html)
explains why evaluation order and precision affect results. The specific cases
and sample count above are a research proposal, not NVIDIA-prescribed tests.

Useful CPU-only checks for the future implementation include interior
corruption with unchanged endpoints, non-finite outputs, incorrect precision
declarations, wrong sample counts, invalid repetition counts and mismatched
identity/provenance. A boolean `reference_checked` alone is insufficient to
describe validation coverage.

## Timing and provenance proposal

Retain three fresh process runs, two seconds of warmup per mode and exactly
30 timed samples per mode per process. For every sample, preserve acquisition
index, total CUDA-event milliseconds and a positive integer GEMM repetition
count. Keep allocation, input conversion, transfers and validation outside
the timed operation. Use one explicitly selected CUDA stream for measured work,
bind the cuBLAS handle to it, and record both timing events in that same stream.
Record this stream policy in the results. Synchronize the stop event before
reading its duration.
This follows the timing distinction in
[NVIDIA's CUDA event guidance](https://docs.nvidia.com/cuda/archive/12.6.3/cuda-c-best-practices-guide/index.html#using-cuda-gpu-timers).

For each sample:

```text
TFLOPS = 2 * M * N * K * repetitions / (elapsed_ms * 1e9)
```

Reject zero, negative or non-finite times. Normalize every sample to milliseconds
per GEMM. Define each process's median normalized time as the mean of the middle
two sorted times, then derive its TFLOPS from that median time. Preserve all
three process results and report the median of their TFLOPS values. This avoids
ambiguity between averaging times and averaging their reciprocals. Preserve
repetitions and input generation consistently across comparisons.

Record the source commit, relevant source/executable/library SHA-256 hashes,
compiler and build flags, CUDA/driver/cuBLAS versions, GPU identity/location,
dimensions, layout, seed, input-generation version, precision fields and all
validation outcomes. Keep the original samples in acquisition order even if
a copy is sorted to compute statistics.

A changed input distribution, precision selection or validation/timing contract
needs an explicit new protocol version. Preserve old result parsing where
appropriate, but refuse cross-protocol performance ratios.
[NVIDIA documents](https://docs.nvidia.com/cuda/archive/12.6.0/cublas/index.html)
that bitwise reproducibility has conditions and is not guaranteed across
toolkit versions.

## Resource and interruption proposal

The following are proposed software budgets for a later reviewed execution,
not measured operating limits or a recovery guarantee:

- Fixed `4096 x 4096` matrices; three processes maximum.
- 120 seconds per process and six minutes overall; no unattended retries.
- At most 512 MiB of application-controlled GPU allocations, with separate
  headroom for the CUDA context and library workspace.
- Check available RAM/VRAM immediately before execution. The A/B/C buffers
  alone require 128 MiB for two 16-bit inputs plus FP32 output, or 192 MiB
  for three FP32 buffers; neither number is total process VRAM consumption.
- Require a quiet entry window and record temperature/utilization. The existing
  runner uses 5% utilization, 2048 MiB used VRAM and 65 C entry limits.
  Reassess these as test-policy thresholds, not hardware safety ratings.
- If other activity prevents a clean baseline, report the interference.
  Do not terminate unrelated applications or alter clocks/power/security settings.
- Cancellation stops scheduling work and closes the owned benchmark process.
  Timeouts or numerical failures produce incomplete results that cannot enter
  comparisons. A host deadline does not guarantee recovery from a driver hang.

Use selective, read-only telemetry with timestamps and explicit units.
Preserve unavailable fields as missing, never zero. Record actual operating
clocks separately from maximum-clock metadata. The
[NVIDIA SMI documentation](https://docs.nvidia.com/deploy/nvidia-smi/index.html)
distinguishes those fields and warns that CLI output can change between driver
versions. Tests should cover unsupported fields and parser changes.

## What a baseline could establish

A validated result could characterize this particular matrix workload, software
stack and settings. [NVIDIA's matrix-multiplication guide](https://docs.nvidia.com/deeplearning/performance/dl-performance-matrix-multiplication/index.html)
explains how dimensions and arithmetic intensity change the balance between
computation and memory traffic.

Application benefit would require a separate matched application benchmark.
For local language models, record the model, quantization, context length,
batch size, backend and memory use; measure prompt processing and generation
separately. [NVIDIA's inference discussion](https://developer.nvidia.com/blog/mastering-llm-techniques-inference-optimization/)
distinguishes the prefill and decode phases and their different bottlenecks.
A square GEMM rate alone establishes neither tokens/second nor gaming FPS.

## Readiness and remaining work

No setup/build script or native CUDA benchmark was run during this research.
The offline Python suite passed; the mocked PowerShell runner suite remains
incomplete because of the local access-denied failure recorded in
[FOLLOW-UP.md](FOLLOW-UP.md#offline-validation-actually-performed).

A later ordinary-compute contribution would need to resolve the compiler
configuration, implement and host-test the explicit 3080 target and new result
contract, review its exact artifact/dependencies and obtain authorization for
the bounded GPU workload. This document is not authorization to execute it.
