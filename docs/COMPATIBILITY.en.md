# Verified hardware and route-specific requirements

**English** | [한국어](COMPATIBILITY.ko.md)

**2026-10-29 issue audit:** [reported setups and profile decisions](CONFIRMED-SETUPS.en.md) · [all 105 issues](ISSUE-AUDIT.en.md). The six fixed profiles below are not the full list of reported LOCAL successes.

The current source contains six exact-machine profiles with recorded hardware
results: three internal headless configurations and three headless USB configurations.
The original B650M run was performed by the maintainer; B660M, X870 and B660-PLUS evidence
comes from contributors ([PR #5](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/5),
[PR #10](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/10),
[PR #24](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/24),
[PR #30](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/30),
[PR #77](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/77)). Fixed-profile builds compile
for exactly one reviewed profile selected at build time in
[internal-boot/unlock/machine-profile.h](../internal-boot/unlock/machine-profile.h);
other GPUs or platforms cannot use those fixed profiles. Experimental LOCAL
generation and its separate contributor reports are described below.

| Build profile | Exact platform / CPU | Recorded headless route |
|---|---|---|
| `b650m-3090fe` | ASUS TUF GAMING B650M-PLUS WIFI BIOS 3057 / Ryzen 7 7800X3D / RTX 3090 FE | Internal; original maintainer's run |
| `b660m-3070ti` | ASUS TUF GAMING B660M-E D4 BIOS 4003 / i7-12700F (no iGPU) / MSI RTX 3070 Ti | Internal; contributor report |
| `x870-3090ti` | Gigabyte X870 AORUS ELITE WIFI7 BIOS F13d / Ryzen 7 9800X3D / ASUS RTX 3090 Ti | USB only; contributor report |
| `b660plus-3060` | ASUS PRIME B660-PLUS D4 BIOS 3010 / i5-12600KF (no iGPU) / GA104 RTX 3060 12GB | USB only, two display children, CSM off; contributor report |
| `h610m-3060` | Gigabyte H610M H BIOS FC / i5-14400 (iGPU enabled, no GOP) / Gigabyte GA106 RTX 3060 12GB | USB only, two display children; contributor report |
| `b660m-3090ti` | ASUS TUF GAMING B660M-E D4 BIOS 4003 / i7-12700F (no iGPU) / Colorful RTX 3090 Ti | Internal; one contributor-reported cold boot |

The latest tag is the source-only pre-release v0.3.1; this source tree includes
later work, including the USB route and X870 profile. Hardware reports describe
their recorded builds. Current cold-precheck hardening has host/build validation
but no new physical boot/recovery result; compiling a profile is not a new
hardware verification. See each report's provenance and limitations below.

## Profile 1: ASUS TUF B650M / RTX 3090 FE (original)

| Common GPU/platform item | Validated value |
|---|---|
| Card | NVIDIA RTX 3090 Founders Edition, 24 GiB |
| PCI identity | `10de:2204`, subsystem `10de:1454` |
| GPU BDF | `0000:01:00.0` |
| Chip / VRAM register | `0xb72000a1` / `0x6000` MiB |
| BAR0 | `0xf5000000`, 16 MiB |
| PROM SHA256 | `8ea297373e9e5d3600ff41ad9fae88f2d14c98c017d100669f7333981da673b4` |
| Motherboard / BIOS | ASUS TUF GAMING B650M-PLUS WIFI / 3057 |
| CPU | AMD Ryzen 7 7800X3D |
| Recorded Windows / driver | Windows 11 Home 26200 / NVIDIA 617.14 |
| UEFI / Secure Boot | x64 / disabled |
| Selector profile | `entryLatchedSs1Full`; targets SS0 `0x88888888`, SS1 `0x8` |

These are documented successful values, not an authorization list. Code also
checks configuration/capabilities, PROM, cold state, driver code, ownership,
DMA cleanup and failure containment. Internal EFI builds additionally contain
the user's own Windows/dedicated ESP GUID configuration.

| Route | Display policy and validation |
|---|---|
| Legacy USB / ordinary internal builds | Preserve AMD physical GOP, PCI `1002:164e`, subsystem `1043:8877`, BDF `0000:0c:00.0` and validated display ancestry |
| Internal headless profile | One RTX physical GOP before release, zero physical GOP afterward; stable console/input interfaces, protected ownership graph, file-only child logging |
| Headless USB (`GU_USB_MEDIA`) | Buildable for this profile; its recorded B650M headless run used the internal route. The contributor's USB hardware evidence is for Profile 3 below |

The headless test had all monitors on RTX and no active AMD GOP. The 7800X3D
still contains an iGPU; BIOS disablement is not established by a missing GOP.
The B650M profile was not re-verified after the profile split; its code path
is preserved behind the same build-time checks.

## Profile 3: ASUS TUF B660M-E D4 / RTX 3090 Ti + RTX 3070 Ti (dual, aggregate console)

Multi-card profile built with `-HardwareProfile b660m-dual`. The firmware
table contains both reviewed cards on this machine: the Colorful RTX 3090 Ti
(`10de:2203`, subsystem `1301:7377`, chip `0xb72000a1`, 0x5ff4 MiB, PROM
`22b77e84…`) and the MSI RTX 3070 Ti (`10de:2482`, subsystem `1462:5052`,
chip `0xb74000a1`, 0x2000 MiB, PROM `f429aa1d…`). The console GPU is the
RTX 3090 Ti in the PEG slot, so the wrapper fingerprints match the
B660M_3090TI entry.

Adaptive behaviour versus the single-card profiles:

- Cards are found by PCI enumeration and matched to the table by device ID;
  the bus number is recorded, no longer required to be 1.
- BAR0 is read from PCI configuration, not a fixed address.
- The card with BY_CHILD_CONTROLLER display children (the console GPU) is
  always unlocked last; every other card runs the full pipeline first.
- Each card keeps its own ownership/DMA/latch state; a card that completes
  is left unlocked and quiesced while later cards are processed.
- Per-card PROM SHA256, chip ID and VRAM size stay exact — unknown cards
  are skipped, never targeted.

Select with `internal-boot/unlock/build.ps1 -HardwareProfile b660m-dual`
plus the usual headless flags, or `research/headless/build-unlock.ps1
-HardwareProfile b660m-dual` for the wrapper package. Hardware validation
of the two-card sequence is still pending; treat the first run like any
new profile port (read the report, confirm both transactions).

## Profile 2: ASUS TUF B660M-E D4 / RTX 3070 Ti (aggregate console)

A contributor-reported successful internal headless profile
([PR #5](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/5)): Intel host **without an iGPU**,
so the NVIDIA driver owns the console GOP and the AMI GOP combo splitter
serves the system console on top of it. The wrapper releases the nvgop/EDID/
splitter ownership chain, runs one unlock transaction and chain-loads Windows
Boot Manager without reset.

| Common GPU/platform item | Validated value |
|---|---|
| Card | MSI RTX 3070 Ti, 8 GiB (GA104) |
| PCI identity | `10de:2482`, subsystem `1462:5052` |
| GPU BDF | `0000:01:00.0` |
| Chip / VRAM register | `0xb74000a1` / `0x2000` MiB |
| BAR0 | `0x82000000`, 16 MiB |
| PROM SHA256 | `f429aa1d5f0f18d11ed9a19503683a674fe3541ae77e959df8e732652c2b4df0` |
| Motherboard / BIOS | ASUS TUF GAMING B660M-E D4 / 4003 |
| CPU | Intel Core i7-12700F (no iGPU) |
| Recorded Windows / driver | Windows 11 Pro 22000 / NVIDIA 616.92 |
| UEFI / Secure Boot | x64 / disabled |
| Selector profile | `entryLatchedSs1Full`; targets SS0 `0x88888888`, SS1 `0x8` |

Measured on the same cuBLAS GEMM n=4096 protocol: FP16 88.7 TFLOPS and
BF16 88.6 TFLOPS medians (locked baseline ~43.7). S3 sleep invalidates the
unlock on this profile as well; there is no resume path.

Select this profile with `research/headless/build-unlock.ps1 -HardwareProfile b660m-3070ti`
and `internal-boot/Configure.ps1 -HardwareProfile b660m-3070ti`; the default
`b650m-3090fe` profile is unchanged.

## Profile 3: Gigabyte X870 AORUS ELITE WIFI7 / RTX 3090 Ti (headless USB)

A contributor-reported successful headless profile and the first using the USB route
([headless USB guide](HEADLESS-USB.en.md)). All monitors on the RTX; the Ryzen
iGPU is present on PCI without an active GOP, like the B650M case. Console
topology is the non-aggregate single-RTX one (`GU_CONSOLE_AGGREGATE 0`).

| Common GPU/platform item | Validated value |
|---|---|
| Card | ASUS RTX 3090 Ti, 24 GiB (GA102), VBIOS 94.02.a0.00.2c |
| PCI identity | `10de:2203`, subsystem `1043:8870` |
| GPU BDF | `0000:01:00.0` |
| Chip / VRAM register | `0xb72000a1` / `0x5ff4` MiB |
| BAR0 | `0xdd000000`, 16 MiB |
| PROM SHA256 | `943565975885ee71a7e861265984c785f564f35272c32395ee7410b353d9bbc4` |
| Motherboard / BIOS | Gigabyte X870 AORUS ELITE WIFI7 / F13d |
| CPU | AMD Ryzen 7 9800X3D (iGPU `1002:13c0`, no GOP) |
| Recorded Windows / driver | Windows 11 Pro 26300 / NVIDIA 621.01 |
| UEFI / Secure Boot | x64 / disabled |
| Selector profile | `entryLatchedSs1Full`; targets SS0 `0x88888888`, SS1 `0x8` |

The contributor reports three clean transactions (two full shutdowns, one Windows restart; this board
resets the GPU on restart). cuBLAS GEMM n=4096 medians: BF16 81.0 → 150.2 and
FP16 80.4 → 149.3 TFLOPS. Entry selectors differ per power-on (`0x40123070/3`,
`0x44123070/7`, `0/0`). [Evidence](../evidence/x870-3090ti-usb/README.md).

Build with `research/headless/build-usb.ps1 -HardwareProfile x870-3090ti`. The
internal-ESP installer (`Configure.ps1`, `Headless.ps1`) is not wired for this
profile.

The X870 evidence and byte-identical build statement refer to original PR #10
revision `7b578247a28ff66f2707bea5af0e3943aede7974`, before the current main's
cold-precheck hardening. The old wrapper/child hashes are historical provenance,
not expected hashes for a new build. The updated build has no new X870 physical
boot report. [Evidence provenance](../evidence/x870-3090ti-usb/README.md).

## Profile 4: ASUS PRIME B660-PLUS D4 / RTX 3060 12GB (headless USB)

[PR #24](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/24) contributes this
exact GA104 variant, PCI `10de:2487`, subsystem `10de:1530`, VBIOS 94.04.71.00.87,
12 GiB (`0x3000`), on BIOS 3010 with an i5-12600KF (no iGPU). It requires **CSM
disabled** and the reported aggregate console: one GOP child and one EDID-only
sibling, with both monitors connected as captured. Other RTX 3060 variants are not covered.

Use `research/headless/build-usb.ps1 -HardwareProfile b660plus-3060` and the
[USB guide](HEADLESS-USB.en.md). Internal ESP installers do not support this profile;
its write-build gate rejects non-USB builds. It does not make the fixed profile portable. The separate LOCAL generator now
admits reviewed GA102/GA104/GA106 tuples and derives per-machine pins from captures.

The contributor reports two full-shutdown USB boots with display restoration and
direct Windows start. On original revision `60ae7e4`, the same-machine locked →
unlocked PyTorch/cuBLAS n=4096 medians were FP16 26.12 → 49.07, BF16 25.77 → 49.41,
TF32 12.98 → 25.06 and FP32 7.99 → 7.98 TFLOPS. These are a different benchmark
protocol from the repository's native 3090 runner; they are not cross-card comparisons.

Current integration adds ownership and runtime/capture hardening; it has host/build
validation, not a new physical boot. Restart, sleep/resume and long-term reliability
remain unverified. [Build provenance and limits](../evidence/b660plus-3060-usb/README.md).

## Profile 5: Gigabyte H610M H / RTX 3060 12GB GA106 (headless USB)

[PR #30](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/30) contributes PCI
`10de:2504`, subsystem `1458:4096`, VBIOS 94.06.2f.40.73, 12 GiB (`0x3000`), on
Gigabyte H610M H BIOS FC with an i5-14400. The UHD 730 is enabled but has no GOP;
both monitors are on the RTX, producing two non-aggregate display children.
This differs from Profile 4's GA104 chip and aggregate console.

Build with `research/headless/build-usb.ps1 -HardwareProfile h610m-3060`. Only the
[headless USB route](HEADLESS-USB.en.md) is shipped; internal write builds are rejected.
The fixed profile does not make generic discovery or LOCAL admission unconditional;
the separate generator checks reviewed PCI/chip/memory and driver tuples.

The contributor reports one clean transaction, direct Windows entry and both displays
restored. An earlier boot without the RTX GOP was refused before release. Matched
torch.mm n=8192 medians were FP16 27.06 → 46.61, BF16 27.18 → 46.28, TF32 13.50 →
24.02, FP32 9.58 → 9.34 TFLOPS. This is not the repository's cuBLAS n=4096 protocol.
These reports and hashes identify original revision `4b748f5`, not a physical test
of the later hardened integration. [Evidence and limits](../evidence/h610m-3060-usb/README.md).

## Profile 6: ASUS TUF B660M-E D4 / Colorful RTX 3090 Ti (internal)

Build profile `b660m-3090ti` selects PCI `10de:2203`, subsystem `7377:1301`,
GA102 `0xb72000a1`, `0x5ff4` MiB and the exact PROM hash from the
[contributor's record](../evidence/b660m-3090ti-internal/README.md).
The board and BIOS 4003 share the console/EDID/nvgop fingerprints with Profile 2;
runtime ownership, code, cold-state and cleanup checks remain unchanged.

One internal-ESP cold boot reports a clean transaction, direct Windows entry and
display recovery. Repeated boots, restart, sleep, stability and a controlled
performance baseline are not established. This is contributor-reported evidence,
not a maintainer hardware test of the integrated build. The USB selector is not
offered for this profile because its USB route has no hardware record.

On main, pass `-HardwareProfile b660m-3090ti` to both `internal-boot/Configure.ps1`
and `research/headless/build-unlock.ps1`; the v0.3.1 tag does not contain it.
Cold/full-flow tests and compile regressions are available. Full private-fixture
transaction replay through `test-all.ps1` is not wired for this profile; the public
evidence does not provide its exact FWSEC image offset/size or private inputs.
Those tests remain blocked rather than accepting an inferred firmware layout.

## Reported local-profile success

[Issue #105](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/105) reports a
Gigabyte RTX 3060 Ti LHR (`10de:24c9`, subsystem `1458:40cd`) on X870 EAGLE WIFI7
BIOS F11 / Ryzen 9 9950X. The source patch was reviewed as text (SHA-256
`eb6bce2587dd0ed1145829267802aebdc4e4287f8448dfccd36313b82c458a8f`). Its firmware
constants admit a narrow common LOCAL candidate: GA104 `b74000a1`, `2000` MiB,
nvgop `60015`, image `26e00`, Stop `678`, `.text` `2106f` bytes with SHA-256
`645a2f416940211e09023838ac03f4b45453f95f457eab2de49628d57e9deb95`.
No additional motherboard profile or firmware template is installed. Other GA104
SKUs retain their own PCI-specific driver pins.

The contributor reports two complete restart-based captures, two incomplete
captures and one clean USB transaction with Windows/display recovery. Their
temporary LOCAL envelope change is absent from the attachment; binary equivalence
to its fixed profile is contributor-reported. This review did not independently
hash their private PROM/driver bytes or reproduce the physical transaction.
Current-source generation/replay uses synthetic inputs; it does not establish
repeatability, full-shutdown behavior, other 3060 Ti variants or the internal route.
New LOCAL use still requires separate full-shutdown captures, exact per-machine
pins and all 22 current host jobs. Existing historical test results cannot replace them.

[Issue #113](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/113) reports one
headless USB unlock, clean FLR/cleanup and Windows display recovery on a Gigabyte
B460M D2VX SI BIOS FE / i5-10400F / Zotac RTX 3070 8GB. The tested source is based on
`9692463`, with [public change 17b5e50](https://github.com/Wandraker/GPU-UNLOCK/commit/17b5e50b0a56ca593fffd764bd0fc85379b536b2).
Its core changes cover probe ID, GA104 card traits, admission and refusal tests;
unlock, ownership, DMA and FLR runtime code was unchanged. This is not a new
physical validation of current main or long-term reliability evidence.

Automatic LOCAL generation now admits `10de:2484` / chip `b74000a1` / `2000` MiB
with nvgop `60009` and its existing reviewed code hash. Subsystem, PROM, paths and
remaining firmware values still come from the owner's captures, and all 22 required
host jobs must pass. GA104 RTX 3060 `2487` / `3000` MiB remains bound to `60010` code.
No new named motherboard profile is added; use the [local-profile workflow](NEW-MACHINE.en.md).
Other 3070 PCI IDs, VBIOS/driver combinations and console layouts need separate validation.

## Scope beyond the recorded configurations

| Other configuration | Status |
|---|---|
| Original B650M, F8-selected internal headless automatic child | One unlock + Windows display recovery + Tensor measurement verified on 2026-10-24 |
| Default automatic startup across repeated cold boots | Not established by the limited supervised runs |
| Another RTX 3090, even a Founders Edition in another PC | Unverified; exact checks may reject |
| Another ASUS BIOS, AMD board/CPU or Intel host | Unverified; separate profile and validation required |
| CPU without iGPU (Intel host, aggregate console) | One contributor-reported profile above; still not generalized |
| Multiple discrete GPUs | `GU_PROFILE_B660M_DUAL` build exists; cards are matched by PCI ID anywhere on segment 0, non-console cards unlock first; still needs on-hardware validation |
| laptop/hybrid setup | Unverified |
| Other NVIDIA models/architectures | Separate GPU port and validation required |
| AMD Radeon / Intel discrete unlock target | No unlock implementation here |
| Games, ComfyUI, sleep/hibernate resume, long-term stability | Not established by GEMM/boot evidence |

See [internal headless instructions](HEADLESS.en.md),
[headless USB instructions](HEADLESS-USB.en.md), [legacy USB instructions](GUIDE.en.md)
and the [original B650M evidence](../evidence/headless-handoff/README.md).
The ordinary source guards remain in [root unlock](../unlock/app.c); the separate
headless implementation is in [internal unlock](../internal-boot/unlock/app.c),
[headless policy](../internal-boot/unlock/headless-policy.h) and
[wrapper release policy](../internal-boot/boot/full-release.c).

Changing constants, replacing a PROM hash or deleting a display check is not a
validated port. New support requires memory/WPR layout, firmware compatibility,
ownership, DMA cleanup and failure-path evidence.
[Portability criteria](PORTABILITY.en.md) / [compatibility reporting](CONTRIBUTING.en.md).
