# Compare performance under the same conditions

**English** | [한국어](BENCHMARK.ko.md)

## RTX 3080 development baseline

The separate [ordinary RTX 3080 baseline](../benchmark/rtx3080/README.md)
targets the inventoried 10 GiB card with a stricter numerical/result protocol.
Its native CUDA build, metadata inspection and three GPU trials passed using
the explicit desktop entry profile. Median throughput was 21.57 TFLOPS FP32,
60.44 FP16, 60.30 BF16 and 30.60 TF32. See the conditions, numerical coverage
and original evidence in its
[native validation record](../benchmark/rtx3080/NATIVE-VALIDATION.md).
It adds no firmware or unlock support. The commands and historical comparisons
below continue to apply to the existing 3090, 3090 Ti and 3070 Ti benchmarks.

## Existing benchmark protocol

The benchmark extends the upstream 4096×4096 cuBLAS GEMM with TF32. It measures
these four modes in order, with two seconds of warmup and 30 samples per mode:

| Result name | Arithmetic |
|---|---|
| `fp32_pedantic` | FP32 arithmetic control |
| `fp16_tensor_fp32_accumulate` | FP16 input, Tensor operations, FP32 accumulation/output |
| `bf16_tensor_fp32_accumulate` | BF16 input, Tensor operations, FP32 accumulation/output |
| `tf32_tensor_fp32_accumulate` | FP32 input/output buffers, TF32 Tensor operations, FP32 accumulation |

TF32 is appended after the existing three modes. It explicitly requests
`CUBLAS_COMPUTE_32F_FAST_TF32`; this is not a pure FP32 precision measurement or
instruction-level proof of a particular kernel. The benchmark and runner refuse
any nonempty `NVIDIA_TF32_OVERRIDE` rather than silently changing the environment
or reporting overridden FP32 work as TF32. `tf32_allowed` reports this environment
policy, not a measured hardware unlock state.

Select the RTX 3090 (default), RTX 3090 Ti or RTX 3070 Ti with
`-Card 3090|3090ti|3070ti` when building and running. Each binary requires its named
GPU at PCI `0000:01:00.0`; this is a measurement target, not an EFI compatibility
verdict. Result checking remains limited to the first and last output elements,
as in upstream.

The setup script downloads CUDA 12.6 redistributables from NVIDIA's official server, verifies sizes/hashes and prepares them within the project. It does not change the driver, ComfyUI or system PATH. Downloads and disk space are required.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\benchmark\setup.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\benchmark\build.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\benchmark\run-benchmark.ps1 -PreflightOnly
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\benchmark\run-benchmark.ps1
```

The measured build used VS2019 BuildTools MSVC 14.29. For a different installation path, pass `build.ps1 -VcVars <vcvars64.bat>`. Use MSVC compatible with CUDA 12.6. Setup refuses to overwrite an existing packages directory; run it once in a fresh checkout.

For another listed card, use the same selection for build, metadata preflight and measurement:

```powershell
.\benchmark\build.ps1 -Card 3090ti
.\benchmark\run-benchmark.ps1 -Card 3090ti -PreflightOnly
.\benchmark\run-benchmark.ps1 -Card 3090ti
```

The runner requires the selected executable's hash in the build manifest exactly
once, before executing even its metadata check. A different card's build manifest
does not authenticate a stale executable. Rebuild for the selected card if needed.
The manifest contains that card's source/executable/link artifacts and shared DLLs;
building another card replaces the active manifest, so rebuild when switching back.
Rebuild existing binaries to obtain the four-mode protocol; the updated runner
rejects old three-mode executables before workload execution.

Save and close ComfyUI, video, recording and other GPU workloads before measuring. Start limits are GPU utilization at most 5%, VRAM at most 2048 MiB and temperature at most 65°C. The runner records three runs in a new timestamped `benchmark/results/` directory. It does not forcibly terminate other processes.

Use the same executable, driver, power limit, overclock and temperature conditions before and after. This tool does not change GPU settings. FP16/BF16 Tensor throughput gains do not imply the same speedup in complete games, video tasks or ComfyUI workflows. See [the measured case and its limits](RESULTS.en.md).

## Offline before/after comparison

New runner results include `comparison-context.json`: hashes of the benchmark
build artifacts, CUDA/cuBLAS inspection, GPU model/PCI location, driver version,
power limit and maximum SM/memory clocks. The runner checks these GPU settings
after every trial. `comparison-complete.json` is written only after successful
completion and binds the context and all three raw trial files by SHA256.
These hashes detect accidental edits; they are not signed hardware evidence.

Compare two completed runs from this updated runner, using Python 3.9+:

```powershell
python .\benchmark\compare_results.py .\benchmark\results\BEFORE .\benchmark\results\AFTER --same-machine-confirmed
```

Use `--same-machine-confirmed` only after confirming the same physical machine,
GPU and OS, unchanged manual tuning and comparable workload/temperature
conditions. Model names and PCI locations alone do not establish machine identity.
Recorded maximum clocks do not capture actual clocks or all overclock settings;
review the saved telemetry as well. This command only reads saved files and
prints JSON; it runs no GPU work and writes no files.

The comparison requires matching artifacts/settings/library metadata, the exact
4096/30-sample/two-second-warmup protocol, checked results and three complete
trials. Missing provenance, partial runs, edited files, unavailable power/clock
limits, non-finite values or protocol mismatches are refused with exit code 2.
Old results without the new context/completion records cannot be compared by
this tool; do not manufacture those records to upgrade historical evidence.

Context schema 2 records the exact four-mode list and protocol/environment
confirmation from inspection and every trial. Existing schema-1 three-mode
results with valid provenance remain readable and comparable to each other.
Four-mode results can only be compared with matching four-mode results; mixing
old/new protocols or different mode sets within a run is refused. Historical
results do not acquire a TF32 measurement.

The offline reader supports these same three GPU names and requires each model's
own executable hash. It rejects comparisons between different models and does not
add EFI support for other boards, BIOS versions or display routes.

Output reports median-of-three TFLOPS, each trial, ratios and the FP32 control
change. It does not identify locked/unlocked state or prove that a throughput
change was caused by unlock. Existing numerical checks still inspect only the
first and last matrix elements. No stability or application-speed claim follows.
