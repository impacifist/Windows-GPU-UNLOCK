# EVGA locked-state performance baseline

[English](NEXT-BENCHMARK.en.md) | [한국어](NEXT-BENCHMARK.ko.md)

Run this after EVGA12 and before any unlock attempt. Update the current branch with GitHub Desktop. Use a normal Windows boot, without booting the EFI USB. Keep BIOS, driver, power limit, clock/overclock settings and cooling unchanged for the later comparison. Close GPU workloads and allow the GPU to cool. Save other work first: the workload increases power/temperature and a driver/application crash remains possible. It changes no GPU settings, stages no EFI and performs no unlock.

Run each block separately in PowerShell; continue only if the previous block succeeds. Setup downloads hash-verified NVIDIA CUDA 12.6 components into this repository, without an installer. It also refreshes the two pinned upstream reference files in benchmark/. Preserve any local edits first. Run setup only for a fresh package directory; if it reports an existing/partial package directory, send the error rather than deleting it. No system CUDA installation is needed.

The second block discovers VS2019 C++ x64 tools across editions (including Community), avoiding a newer side-by-side installation. EVGA contributor reported VS2019 and VS2026 installed; the earlier unrestricted latest query selected an unsupported compiler. If setup already completed, skip the first block and resume at build; do not rerun setup. CUDA 12.6 may reject a newer compiler. If it does, send the complete error; do not change compiler guards or reinstall tools blindly. Preflight checks binary metadata without a GPU workload. Only the final block measures GPU performance.

```powershell
cd E:\UNLOCK\Windows-GPU-UNLOCK
powershell -NoProfile -File .\benchmark\setup.ps1
```

```powershell
powershell -NoProfile -File .\benchmark\build.ps1 -Card 3090
```

```powershell
powershell -NoProfile -File .\benchmark\run-benchmark.ps1 -Card 3090 -PreflightOnly
```

```powershell
powershell -NoProfile -File .\benchmark\run-benchmark.ps1 -Card 3090
```

The final command runs three trials of four correctness-checked modes: pedantic FP32, FP16 tensor/FP32 accumulation, BF16 tensor/FP32 accumulation and TF32 tensor/FP32 accumulation. It refuses a busy/hot GPU, unexpected identity or changed comparison settings.

Zip the newly created timestamp directory under `benchmark/results/` and send it, plus `benchmark/build-manifest.json` and `benchmark/runtime-verification.json`. A completed run must have `comparison-complete.json`; partial/error output is useful but is not a baseline. Keep `benchmark/bin/` and the build manifest unchanged for the post-attempt run. We will validate the results offline; no duplicate checker run is needed on your PC.

This establishes a pre-attempt performance baseline. The later identical run and provenance comparison can establish a measured performance change; parent release alone, or a single high score, does not prove unlock. Do not run another EFI until the baseline is reviewed and the next scope is supplied.
