# Exact-binary tensor characterization on RTX 4070 Ti

Date: 2026-10-25. Stock driver 610.88, AD104 RTX 4070 Ti, compute capability
8.9, VBIOS `95.04.31.00.f0`. No firmware, selector, power-limit or driver change
was made during these tests.

## Reportable result

**The same fixed FP16-input tensor binaries produce a repeatable 1.957–1.964x
FP16- versus FP32-accumulation nominal rate ratio in the 720-block batched
control, across three fresh processes.** Every final batch output matched its
CPU-generated expected hash. Disassembly and loaded-function attributes are
included, so the test no longer relies solely on PTX source to identify work.

This is a reproducible stock-driver characterization, not an unlock result or
a readout of an issue-rate selector. It provides a stronger control for future
before/after experiments and shows why the earlier short-launch ratio should
not be treated as a precise hardware limit.

| 720 blocks, 16 launches per batch | Process 1 | Process 2 | Process 3 |
|---|---:|---:|---:|
| FP16 accumulation, nominal TF/s | 163.234 | 171.453 | 162.722 |
| FP32 accumulation, nominal TF/s | 83.130 | 87.421 | 83.136 |
| FP16 / FP32 rate ratio | 1.9636 | 1.9612 | 1.9573 |

Raw records: [process 1](gpu-binary-sweep-1.json),
[process 2](gpu-binary-sweep-2.json), [process 3](gpu-binary-sweep-3.json).
Absolute rates vary more than this particular ratio. These are short local
measurements, not confidence intervals or a peak specification.

## What actually executes

Portable NVIDIA `ptxas` and `nvdisasm` 12.6.20 were downloaded from the official
redistribution manifest and hash-verified without installing a toolkit.
[binary-tools-provenance.json](binary-tools-provenance.json) records archive and
executable hashes. The existing probe's PTX was compiled with `-arch=sm_89 -O3`.
The resulting cubins are hashed immediately before documented CUDA module loading.

| Property | FP16 accumulation | FP32 accumulation |
|---|---:|---:|
| Disassembled tensor opcode | `HMMA.16816.F16` | `HMMA.16816.F32` |
| Static HMMA instructions | 32 | 32 |
| Static instruction lines | 189 | 197 |
| Registers per thread, compiler and loaded function | 20 | 28 |
| Compiler spill stores / loads | 0 / 0 bytes | 0 / 0 bytes |
| Loaded-function local memory per thread | 0 bytes | 0 bytes |
| Static shared memory | 0 bytes | 0 bytes |
| Predicted maximum active 256-thread blocks per SM | 6 | 6 |

The compiler unrolls/duplicates loop paths: **32 is a static code count, not a
dynamic instruction count per launch**. Resource-based occupancy prediction is
not measured achieved occupancy. Equal ceilings rule out a reduced predicted
block ceiling from the register-count difference in this configuration; they
do not rule out scheduling, dependency, clock or other throughput effects.
The two binaries also differ in accumulator representation and output bytes.

Full disassembly: [FP16](gpu-binary-f16.sass), [FP32](gpu-binary-f32.sass).
[gpu-binary-manifest.json](gpu-binary-manifest.json) binds PTX, cubin,
disassembly, compiler output and preparation source hashes. These offline
binaries are not claimed equivalent to earlier driver-JIT machine code.

## Why this improves the earlier control

The exact binaries were first tested with the earlier single-launch method in
three processes. All 20,643,840 timed output values passed, but ratios remained
1.243–1.436. Raw records:
[1](gpu-binary-result-1.json), [2](gpu-binary-result-2.json),
[3](gpu-binary-result-3.json).

The next experiment used the same binaries, 768 recurrence iterations, four
chains, 256 threads per block and exact-output fixtures. It swept grids 120,
360 and 720 with 16 launches per timed batch and eight pairs per grid, balancing
which mode ran first four times each. The old 120-block grid averages two blocks
per SM on this 60-SM GPU, below the six-block resource ceiling; a grid is not a
measurement of actual scheduling or occupancy.

| Grid | Process 1 ratio | Process 2 ratio | Process 3 ratio |
|---|---:|---:|---:|
| 120 | 1.9024 | 1.9085 | 1.9280 |
| 360 | 1.8635 | 1.9582 | 1.8942 |
| 720 | 1.9636 | 1.9612 | 1.9573 |

Warmup lasts at least 0.25 seconds of **host wall time**, including copying and
hashing, once per mode at grid 360. It is not 0.25 seconds of continuous GPU
work and is not repeated before each grid. CUDA event intervals encompass a
batch and may contain host submission gaps; times are divided by 16 once to
form per-launch averages. The experiment changes batching, warmup and grid,
so it does not isolate the causal contribution of any one change.

Across three processes the sweep timed **2,304 kernel launches**, checked
**144 final outputs** covering **235,929,600 values**, and completed cleanup.
Intermediate outputs overwritten within each batch were not independently
observed. Uniform fixtures do not establish general numerical accuracy. No
hardware selector was read, and no before/after unlock comparison occurred.

## Reproduce and verify

```powershell
python research/ada-4070ti/prepare_binary_tools.py
python research/ada-4070ti/gpu_binary_probe.py prepare
python research/ada-4070ti/gpu_binary_probe.py run
python research/ada-4070ti/gpu_binary_sweep.py
python research/ada-4070ti/check_binary_results.py
python research/ada-4070ti/check_binary_sweep.py
```

The preparation step writes generated cubins to ignored `build/` and updates
the manifest/disassembly. Execution is ordinary user-space compute; the last
two commands only check saved evidence. They recalculate timing arithmetic and
CPU output hashes and verify source/manifest agreement. Preserve new run output
separately when repeating; historical records describe the committed sources.

Primary references: [NVIDIA binary utilities](https://docs.nvidia.com/cuda/cuda-binary-utilities/),
[CUDA function attributes](https://docs.nvidia.com/cuda/cuda-driver-api/group__CUDA__EXEC.html),
and [CUDA occupancy API](https://docs.nvidia.com/cuda/cuda-driver-api/group__CUDA__OCCUPANCY.html).
