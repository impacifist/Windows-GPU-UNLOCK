# B760M-K / RTX 3060 12GB (GA106) headless USB evidence

## Provenance and build scope

These results come from one machine and were reported by the contributor on 2026-10-28/29. The
maintainer has not run a hardware test for them. The EFI hashes in [summary.json](summary.json)
are the files that were booted, built with
`research/headless/build-usb.ps1 -HardwareProfile b760mk-3060` from this PR's branch before it
was rebased onto main. The wrapper embeds the builder's Windows ESP GUID, so its hash differs on
other machines.

Hardware: ASUS PRIME B760M-K (BIOS 1663), Core i5-13500 (iGPU disabled), Zotac RTX 3060 12 GiB
(GA106, `10de:2504` / `19da:2630`, VBIOS 94.06.2f.40.e3). One monitor on the RTX.

BIOS: Secure Boot off · CSM off · iGPU off · Primary Display PEG · Fast Boot off.

## How the profile was derived

1. Three cold boots of the generic read-only probe. They gave the same identity, BAR0
   (`0x84000000`–`0x84ffffff`), PROM digest, display path (ADR `0x80010340` behind `Pci(1,0)`)
   and driver fingerprints.
2. The AMI console drivers were compared with ASUS TUF B660M-E D4 BIOS 4003 (PE diff plus a
   `Stop()` call-tree diff):
   - console_text is a different build, so the profile gets its own firmware entry,
     `GU_FIRMWARE_AMI_B760MK_1663`.
   - console_text's `Stop()` and its 8 direct callees match B660M, and so does console_gfx's
     `Stop()`.
   - combo's `Stop()` adds one FreePool error path.
3. Host tests:
   - `test_b760mk_capture.py` pins the profile to the capture.
   - `test-aggregate-graph-b760mk.ps1` replays the real single-child graph in WebAssembly.
   - `test-cold-precheck.ps1 -HardwareProfile b760mk-3060`.
   - `test-all.ps1` with this card's private PROM and capture fixtures.

   FWSEC-FRTS is at PROM `0x416bc` (`0xea00` bytes).

## Recorded hardware runs

| Run | Boot | Result |
|---|---|---|
| HLS000000 / AUTO000000 | full shutdown | `FINAL status=0 clean=1`, `SS0=0x88888888 SS1=0x8`, one FLR, Windows started directly with the display recovered |
| HLS000001 / AUTO000001 | full shutdown | same |

The 12 GiB layout puts FRTS at `0x2ffe00000` and WPR2 at `0x2ffe000`–`0x2ffee00` pages, the same as
Profile 5. See [report-excerpt.txt](report-excerpt.txt).

| Repository cuBLAS benchmark, n=4096, 170 W, TFLOPS (median) | Locked | Unlocked run 1 | Unlocked run 2 |
|---|---:|---:|---:|
| FP32 | 8.04 | 8.04 (1.00×) | 8.06 (1.00×) |
| TF32 | 12.92 | 23.74 (1.84×) | 24.85 (1.92×) |
| FP16 | 25.60 | 44.60 (1.74×) | 47.34 (1.85×) |
| BF16 | 25.54 | 47.70 (1.87×) | 48.32 (1.89×) |

The locked baseline was measured with a normal boot on the same machine, with the same driver
(617.14), power limit and script. Both unlocked runs come from the boot that wrote AUTO000000.

The PROM image, the raw probe and graph logs, driver image dumps and partition identifiers are
not included.
