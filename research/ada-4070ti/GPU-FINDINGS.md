# Measurements on the actual RTX 4070 Ti

Date: 2026-10-25. The owner explicitly authorized ordinary GPU execution and
freeing competing GPU applications. This phase uses the stock CUDA driver for
bounded user-space compute, without raw PCI/MMIO, firmware, driver installation,
reset or clock/power configuration changes. Nothing is pushed remotely.

**Representative library workload:** [CUBLAS-BASELINE.md](CUBLAS-BASELINE.md)
records 4096×4096 GEMM on this GPU with FP32, FP16 and BF16 input modes and
full-output validation. The register probes below are separate experiments.

## Hardware-derived observations

| Observed field | Value |
|---|---|
| GPU / subsystem | RTX 4070 Ti, `10de:2782` / `1462:5132` |
| NVIDIA Windows driver / VBIOS | `610.88` / `95.04.31.00.f0` |
| CUDA compute capability | 8.9 |
| CUDA driver API version | 13030 (driver API version, not an installed toolkit/cuBLAS version) |
| Windows CUDA PCI address | **`0000:0B:00.0`** |

The last row is new measured portability evidence: this Windows address differs
from the external reference sampler's hardcoded `0000:01:00.0`. Do not change
the guard just to match it. Firmware-time enumeration/resource layout still
requires separate evidence; Windows BDF alone does not prove a preboot rejection.

Trackmania was closed normally using its window-close request, under the owner's
authorization. No force termination or system/display-process termination was
used. GPU memory dropped from approximately 6.8 GiB to 5.5 GiB in subsequent
telemetry, but other activity remained. No raw process inventory is published.

## Stock CUDA correctness smoke: passed on hardware

[gpu_smoke.py](gpu_smoke.py) uses [cuda_driver.py](cuda_driver.py) to load PTX
through the installed `System32/nvcuda.dll`. No Python GPU package or toolkit was
installed. It creates its own logical CUDA context and 256 KiB device buffer.
Ten launches each fill 65,536 uint32 values with `3*index + bias`; bias changes
from 7 through 16 so later successful checks cannot rely on a prior launch's
identical output. Every element is copied back and checked after each launch:
**655,360 exact values verified**. All own resources were released successfully.

[gpu-smoke-result.json](gpu-smoke-result.json) contains the measured device/API
identity, telemetry, output hash and source hashes. Host elapsed time, including
JIT/context/validation/cleanup, was about 0.35 s; this is not a compute-throughput
measurement. The allocation figure excludes normal driver/context overhead.

Before/after telemetry both reported 34 C, 20.06 W and a configured 285 W power
limit. These are coarse point samples, not peak measurements. Utilization was
49% and memory use about 5.5 GiB, so the system was not an idle benchmark host.
`cuMemGetInfo` values are retained as CUDA/WDDM allocation information; do not
subtract them from NVML memory-use values as if the APIs measured identical
physical residency.

Reproduce the same bounded test:

```powershell
python research/ada-4070ti/gpu_smoke.py
```

This proves successful ordinary CUDA execution and exact output for this test
on this GPU/driver. It does not prove any selector state, Tensor Core throughput,
unlockability, firmware compatibility or performance uplift. The per-launch
pattern varies to distinguish fresh results from stale output.

## Stock FP16/BF16 Tensor Core probe: first run

[gpu_tensor_probe.py](gpu_tensor_probe.py) then executed native `sm_89` PTX
`mma.sync.aligned.m16n8k16` with FP16 or BF16 inputs and FP32 accumulation.
It first checks one warp/eight iterations, then a 512-iteration warmup. Timed
launches use 120 blocks of 256 threads, four PTX accumulator chains and 4096
iterations per chain. Inputs are runtime-supplied packed ones. Chain-specific
seeds make the recurrences distinct, and seeds change for each timed launch.
All 491,520 FP32 output values are copied back and checked after every timed
launch. Expected values remain exactly representable in FP32.

| First process, seven samples/mode | Median time | Nominal TFLOP/s |
|---|---:|---:|
| FP16 inputs / FP32 accumulation | 0.923264 ms | 69.7791 |
| BF16 inputs / FP32 accumulation | 0.897152 ms | 71.8100 |

[gpu-tensor-result-1.json](gpu-tensor-result-1.json) records raw event timings,
telemetry, output/PTX/source hashes and successful cleanup. The output allocation
is 1,966,080 bytes. Timed nominal work is `960 warps * 4 chains * 4096 iterations
* (2*16*8*16)` = 64,424,509,440 FLOPs per launch. Event timing excludes CPU output
validation and JIT. Nominal TFLOP/s is that operation count divided by event time.

Point telemetry after each mode showed 34 C, 2745 MHz SM clock and P2. Power
samples were 28.14 W and 41.79 W, not peak power. The configured power limit
remained 285 W. Background activity and memory usage changed during the session;
samples contain visible timing outliers. No processes beyond the normally closed
game were stopped to obtain these results.

Limits: this is short register-resident work, not a matrix multiplication of
application-sized arrays, cuBLAS, sustained peak throughput or a stability soak.
No disassembly of the driver's generated machine code was obtained, so executed
instruction count is not independently certified. The 100 ms timing rejection
is checked after execution, not an execution watchdog. Neither the measured
rates nor FP16/BF16 similarity identify speed-selector state or show an unlock
percentage. There is no unlocked run of this machine for comparison.

Reproduce:

```powershell
python research/ada-4070ti/gpu_tensor_probe.py
```

Instruction semantics: [NVIDIA PTX ISA, warp-level MMA](https://docs.nvidia.com/cuda/parallel-thread-execution/index.html#warp-level-matrix-instructions-mma).
CUDA uses an ordinary private logical context; destroying that context is not a
physical GPU reset.

## Three-process repeatability of the FP32-accumulation probe

Two additional fresh processes used the same source and launch configuration.
[gpu-tensor-result-2.json](gpu-tensor-result-2.json) and
[gpu-tensor-result-3.json](gpu-tensor-result-3.json) preserve their raw samples.

| Nominal TFLOP/s, process medians | Process 1 | Process 2 | Process 3 | Median of medians |
|---|---:|---:|---:|---:|
| FP16 input / FP32 accumulation | 69.779 | 72.305 | 66.504 | **69.779** |
| BF16 input / FP32 accumulation | 71.810 | 71.764 | 69.932 | **71.764** |

These 42 timed launches verified **20,643,840 FP32 output values**, in addition
to the pilot and warmup checks. Point temperature readings across the three
processes were 32–34 C. Process-to-process spread and per-launch outliers confirm
that these short runs are not a tightly controlled peak-throughput measurement.

[check_gpu_results.py](check_gpu_results.py) recalculates event medians, nominal
work/rates and source/configuration agreement without accessing the GPU. It also
independently constructs the expected final CPU byte patterns and matches every
recorded final output hash, including the smoke result. This is a check of the
recorded evidence, not a re-creation of the earlier GPU execution. It verifies
matching GPU/driver/VBIOS/power-limit point samples across these runs.
[gpu-results-summary.json](gpu-results-summary.json) contains the summary.

```powershell
python research/ada-4070ti/check_gpu_results.py
```

Numerical coverage here uses unit input operands and seeded accumulators. It is
not a general numerical-accuracy validation over arbitrary matrices or formats.

## FP16 accumulation: numerical boundary and matched-shape control

[gpu_precision_probe.py](gpu_precision_probe.py) ran a one-warp recurrence with
FP16 inputs and either FP16 or FP32 accumulator/output registers. Runtime seed
16 and four chain offsets spaced by 16 were used. All 512 output values per
case were checked for finiteness and consistency within their chain.
[gpu-precision-result.json](gpu-precision-result.json) records the observations.

| Iterations | Ideal chain values | FP16 accumulation observed | FP32 accumulation |
|---|---|---|---|
| 768 | 12304, 12320, 12336, 12352 | matches all | matches all |
| 1000 | 16016, 16032, 16048, 16064 | matches all | matches all |
| 1024 | 16400, 16416, 16432, 16448 | **16384 for every chain** | matches all |
| 1536 | 24592, 24608, 24624, 24640 | **16384 for every chain** | matches all |

The initial long FP16 timing attempt failed its exact-output check and produced
no accepted timing result. Even representability of the ideal final sum does
not establish exact intermediate MMA arithmetic. NVIDIA specifies minimum half
precision for FP16 accumulators and leaves internal accumulation order/rounding
unspecified ([PTX MMA precision rules](https://docs.nvidia.com/cuda/parallel-thread-execution/index.html#warp-level-matrix-instructions-mma)).
The observed plateau is a property of this recurrence on this card/driver, not
proof of a hardware defect, speed-selector state or a general FP16 error bound.
An initial PTX initialization form was also rejected by JIT compilation; the
accepted source uses register operands and explicit rounding for packed addition.

[gpu_accumulation_probe.py](gpu_accumulation_probe.py) therefore uses **768**
iterations for its accepted timing comparison, preserving exact-output checks.
Both modes have the same PTX matrix shape and nominal MMA count, with alternating
mode order across seven pairs and varied per-launch seeds. Nominal work is
12,079,595,520 FLOPs per launch. All 491,520 values are verified per timed launch;
the FP16 output occupies 983,040 bytes and FP32 occupies 1,966,080 bytes.

This is matched nominal work, not identical machine work: accumulator register
counts, output bytes and potentially compiler scheduling/occupancy differ. Short
kernel timing, background activity and imperfect ordering balance also affect
the ratio. No machine-code disassembly or selector readout was obtained.
Results from three fresh processes are retained in `gpu-accumulation-result-*.json`.
They must not be used to calculate an unlock percentage.

| Accepted short control, nominal TFLOP/s | Process 1 | Process 2 | Process 3 |
|---|---:|---:|---:|
| FP16 input / FP16 accumulation | 61.350 | 65.017 | 53.865 |
| FP16 input / FP32 accumulation | 40.730 | 40.871 | 45.552 |
| Rate ratio | 1.506 | 1.591 | 1.183 |

The ratio varies substantially; a precise hardware-rate limitation cannot be
deduced from this short synthetic control. Its useful findings are the observed
numerical recurrence boundary, a working exact-output comparison below that
boundary and the need for a more representative library workload.
[check_accumulation_results.py](check_accumulation_results.py) independently
recalculates the timing arithmetic and CPU byte-pattern hashes for both the
precision cases and all three control processes.
[gpu-accumulation-summary.json](gpu-accumulation-summary.json) records that check.
