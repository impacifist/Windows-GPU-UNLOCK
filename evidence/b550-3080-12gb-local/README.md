# GA102 RTX 3080 12 GB / ASUS TUF B550-PLUS LOCAL evidence

[한국어](README.ko.md)

Source: [issue #73](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/73),
reviewed 2026-10-29. This is a contributor report, not a hardware test of this
integration. No raw PROM, driver dumps, firmware assets or private identifiers
are distributed.

| Item | Reported value |
|---|---|
| GPU | Gigabyte RTX 3080 12 GB, GA102, PCI `10de:220a`, subsystem `1458:40a2` |
| Host | ASUS TUF GAMING B550-PLUS, AMI 3611; Ryzen 9 5900X, no iGPU |
| Windows / driver | Windows 11 Insider 26340 / NVIDIA 621.01 |
| VBIOS / memory | `94.02.85.00.d7` / `0x3000` MiB |
| GPU / BAR0 | `0000:08:00.0` / `0xfb000000`, 16 MiB |
| Chip | `0xb72000a1` |
| PROM SHA-256 | `86587a2931742c40d6385c49825cf8702be8fe68f22f052b462815930915a3ec` |
| NVIDIA GOP | `0x60010`, existing reviewed text hash `615b37a708b1894e18d7fa60cfcdacaa4d8b598863a10b8103d231a595847b2b` |
| Tested profile | `LOCAL-220a10de-40a21458-86587a29`; headless USB |
| Tested source | `0d2027e` + PR #49 head `b79cdd6`, local merge `b0ef291`, plus disclosed uncommitted probe/admission changes |
| Tested wrapper / child | `22ecc7dd285a8990e6abf1872407cb2d3ec9e4e5841b7a11667e63e858ca64f4` / `de07c5426fd8c93b97ca96b4b81e67a8ccb744bcef5fb4b27c02b2863579e541` |

Two separate full-shutdown captures agreed. The report describes one cold USB
transaction with selectors retained at SS0 `88888888` / SS1 `8`, one FLR,
quiescence, no retained mappings/faults, and final status 0 / clean 1. Windows
started directly and NVIDIA initialized. HDMI recovered immediately; the
DisplayPort monitor required power-cycling or replugging. The cause of that
monitor recovery requirement is not independently established.

Same-machine cuBLAS n=4096 medians, three processes, CUDA/cuBLAS 12.8 rather
than the pinned 12.6 protocol: FP16 69.30 → 114.03, BF16 69.11 → 117.32,
TF32 31.27 → 61.71, FP32 21.40 → 22.35 TFLOPS. They do not establish application
gains, repeated boots, resume or long-term stability.

## Integration scope

Only `220a10de` / GA102 `b72000a1` / `3000` MiB is added. Its NVIDIA GOP
must be the existing `60010` code above. Legacy 3090/3090 Ti memory/driver
combinations remain unchanged; 3080 10 GB/LHR, 3080 Ti and foreign chip
revisions remain refused. No firmware template, register sequence, ownership
rule, DMA, reset or cleanup implementation is changed.

Use the ordinary [LOCAL workflow](../../docs/NEW-MACHINE.en.md), not another
machine's header. Subsystem, PROM, ancestry, console/EDID code and BAR pins
still come from the owner's own captures; current host tests must pass on
exactly those bytes. This source integration has synthetic/compile and
RAM-only probe coverage, not another physical run. A fixed motherboard
selector would duplicate LOCAL while lacking the private fixtures needed
to reproduce this historical package.
