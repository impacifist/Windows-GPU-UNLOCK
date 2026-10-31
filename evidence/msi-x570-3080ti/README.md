# MSI X570 Gaming Plus / ASUS RTX 3080 Ti: one private-build result

This records one successful automated USB unlock transaction on MSI MPG X570
GAMING PLUS, BIOS A.N0, with an ASUS RTX 3080 Ti 12 GB (10de:2208,
subsystem 1043:8803). The RTX drove the display; this firmware had no separate
EDID-driver owner. It follows the earlier investigation in issue #7 and the
cache-correction reports in #102.

## What ran

The hardware run used a separate private build at source
e1adcc5b11223e1803008e23c2644c48d3ee85cb, not upstream main or the research
adapter proposed alongside this record. Raw captures, PROM/driver code,
firmware assets, EFI images, USB contents and unique machine/partition
identifiers are not distributed.

| Tested image | SHA-256 |
|---|---|
| Launcher | dd6e46c2fa4f1e161c0ab83db1b356bd9650b17e2557f66cfb6402c0405dea07 |
| Child | 685747d2c1b6692def6b522fa6fe8f76c2549676f486cf40f56044fcd651727d |

The launcher recorded raw GPU command 0x0002 with cached PciIo attributes
0x300. One direct Attributes(Disable, IO) returned success and corrected
the cache to 0x200; no initial IO-enable request was used. Complete GPU and
parent PCI snapshots remained unchanged through the reviewed gate. Parent
discovery enumerated PciIo independently of active ownership edges because
the parent had only passive references in the preserved captures.

The console and NVIDIA GOP disconnects succeeded. The observed post-Stop
raw/cache pair was zero/zero; memory-only decode was then restored. The child
reported target selectors SS0 0x88888888 and SS1 8 before and after one
PCIe function-level reset, successful quiescence, BME off, no retained DMA
mappings/resources and clean success. The launcher recorded a successful
child return and preclose Windows verification. The operator observed a normal
Windows return with no observable boot issue.

The final log marker precedes file closure, fresh verification and StartImage.
It is not by itself proof those later actions succeeded; the operator's
Windows observation is separate supporting evidence.

## Windows compute measurement

A native Windows CUDA/cuBLAS GEMM used the same saved executable/DLLs,
NVIDIA driver 610.88, CUDA runtime 13.2 and cuBLAS 13.4.0 in both states.
Each mode used 4096 matrices, two seconds warmup and 30 timed samples; values
below are medians across three trials. FP16/BF16/TF32 use FP32 accumulation
and output; TF32 is not pure FP32 arithmetic.

| Mode | Normal-boot baseline TFLOPS | Unlocked-session TFLOPS | Observed change |
|---|---:|---:|---:|
| FP32 pedantic | 23.99 | 24.08 | +0.4% |
| FP16 Tensor | 69.01 | 121.31 | +75.8% |
| BF16 Tensor | 68.97 | 125.51 | +82.0% |
| TF32 Tensor | 35.15 | 63.24 | +79.9% |

The unlocked session was measured first. After full shutdown and normal boot
without the USB, the expected locked baseline was measured. No live Windows
selector query established the baseline label, and this was not a randomised
on/off/on study. Driver, reported power/max-clock settings, workload and
library hashes matched; manual GPU settings were not changed.

Admission differed: the unlocked run passed a single <=5% utilisation check;
the baseline used an explicitly recorded low-power P8/210 MHz desktop screen
with repeated NVIDIA and Windows GPU-engine observations. This screen is
separate local benchmark work, not part of this cache-profile patch.
Its criteria are not universal definitions of idle.

An initial warmer baseline had one post-trial thermal-slowdown flag and was
retained. One further baseline after natural cooldown is reported above.
It started at 44 C versus 47 C for the unlocked run; post-trial temperatures
were 64/65/66 C versus 64/67/68 C. Neither run showed an active thermal flag at
its post-trial snapshots. This is not continuous no-throttling evidence.

## Scope

The result supports a substantial Tensor GEMM throughput difference on this
one setup, with a nearly unchanged FP32 control. It does not establish game,
video or application speedup, extended stability, sleep/resume behaviour,
repeated cold-boot reliability, reboot persistence, general compatibility or
guaranteed recovery/safety.

The accompanying code adds only an exact PCI snapshot envelope and synthetic
tests to upstream's existing research cache core/adapter. It does not publish
the private launcher, add a production profile, broaden family/firmware or LOCAL
eligibility, modify proven.json, or connect the adapter to a boot image.
The public candidate itself has not been tested on hardware.
