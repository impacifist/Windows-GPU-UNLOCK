# B550 AORUS ELITE V2 / RTX 3060 12GB (GA104) headless USB evidence

## Provenance and build scope

These results come from one machine, reported by the contributor, on 2026-10-28. The
maintainer has not run a hardware test for them. The booted files were built with
`research/headless/build-usb.ps1 -HardwareProfile b550-3060` from commit `c26378c`; later
commits on the branch change only tests, documentation, header comments and the USB manifest
name list.

| File | SHA-256 |
|---|---|
| Wrapper `BOOTX64.EFI` (39,424 B) | `9045718fcd1b9a50b02f12d0c84c79252a965a14a265c68a90ca7f57735e48d4` |
| Child `UNLOCK-HEADLESS.EFI` (67,584 B) | `0f0e8015dc900eb9ac1abe87a03d5a8646748fc4f314d284b95f0e5473df1d63` |

The wrapper embeds the builder's Windows ESP GUID, so its hash differs on other machines.

## Machine

Gigabyte B550 AORUS ELITE V2, AMI BIOS F13, Ryzen 7 3700X (no iGPU), NVIDIA
reference-subsystem RTX 3060 12 GiB (GA104, `10de:2487` / `10de:2487`, VBIOS
94.04.71.00.a5, 170 W). Windows 11 Pro 26200, driver 610.88. **CSM disabled**; Above 4G
Decoding and Re-Size BAR disabled; Secure Boot enabled but not active (setup mode);
BitLocker and Fast Startup off.

The card sits on **bus 06** (CPU x16 slot behind root port `00:03.1`), unlike every other
fixed profile at bus 01. A second card (RTX 3050) was removed for the captures and the run.
One monitor is connected to the RTX over HDMI; nvgop produces one display child
(`AcpiAdr 0x80010370`), non-aggregate console.

This is the first profile with **nvgop `0x6000a`**; its AMI console and EDID drivers are also
new for BIOS F13. The fingerprints are in [summary.json](summary.json).

## How the profile was derived

1. Three cold boots of a read-only USB observation EFI built with
   `-DGU_PROFILE_PROBE_B550_3060` (PCI identity and bus only; fails to compile with any
   release or unlock define), each saving the PROM. All three agree on identity, BAR0, PROM
   digest, GOP path and driver fingerprints; only the entry SS0 differs
   (`0x76446145`, `0x77446145`, `0x76446145`; SS1 `0x3`).
2. `research/headless/fingerprint_probe.py` and the production VBIOS parser
   (`fwsec-locate`): FWSEC-FRTS at PROM `0x438b4`, `0xea00` bytes, three signatures.
3. Host tests, `research/profile/test-b550-3060.ps1`: 19 jobs including the cold pre-check,
   the transaction against this card's PROM and the five assets (112 scenarios), an R2 replay
   of the real captured console graph and a replay of the whole driver release (EDID stop,
   nvgop stop, IO/MEM decode restore, final verifier) on that graph with the captured
   EDID and nvgop images. All are simulations.

## Recorded hardware run

| Run | Boot | Result |
|---|---|---|
| HLS000000 / AUTO000000 | full shutdown, F12 to USB | pre-check passed (cold). Console, EDID and nvgop `0x6000a` detached and verified; `FINAL status=0 clean=1`, `SS0=0x88888888 SS1=0x8`, one FLR, Windows started directly with the display recovered |

FRTS is at `0x2ffe00000` and WPR2 at `0x2ffe000`–`0x2ffee00` pages, the same 12 GiB layout
as the other RTX 3060 profiles. See [report-excerpt.txt](report-excerpt.txt).

## Benchmark

| cuBLAS n=4096, FP32 accumulate, 170 W, TFLOPS (median of 30) | After AUTO000000 |
|---|---:|
| FP32 (no tensor) | 8.28 |
| TF32 tensor | 24.05 |
| FP16 tensor / FP32 acc. | 48.41 |
| BF16 tensor / FP32 acc. | 48.15 |

torch 2.11.0+cu128, driver 610.88. **No locked baseline was measured on this machine with this
script.** The method matches the B660-PLUS GA104 RTX 3060 evidence, whose locked values were
7.99 / 12.98 / 26.12 / 25.77; the unlocked values here are in line with its unlocked results.
In an application, one MOSS-image generation at 1808×1152 took about 8 minutes locked and
about 5 minutes unlocked (contributor's wall-clock observation). The card runs a few degrees
warmer when unlocked.

Excluded: the PROM image, raw probe and graph logs, driver image dumps and partition
identifiers. Not established: repeated cold-boot reliability (one successful transaction),
a locked GEMM baseline here, Windows restart, sleep/hibernate resume, long-term stability,
the internal-ESP route and configurations with a second NVIDIA card installed.
