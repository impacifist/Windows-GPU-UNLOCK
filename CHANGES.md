# Changes

## 2026-10-27 — Maxsun B760M / RTX 3080 LHR hardware-verified profile

- Added compile-time profile `b760m-3080lhr` for Maxsun MS-Terminator B760M D4
  BIOS H7.8G, i5-12490F (no iGPU) and RTX 3080 10 GB LHR (GA102, subsystem
  `7377:2000`). PCI/subsystem, BAR0, chip/VRAM, PROM, root-port, display path
  and all EFI driver fingerprints remain build-time constants; a user-editable
  file still cannot select a profile. The profile uses the default
  `GU_TARGET_BUS 1U` and keeps its AMI EDID runtime.
- Added `GU_TOPO_B760M` aggregate-console allowances: nvgop's display child
  uses open attribute `0x8`; the AMI EDID runtime binds through the NVIDIA
  private protocol; an optional AMI DevicePath consumer is detached at stage 4;
  the console text-out driver is identified by FFS GUID+size+version because
  its Stop offsets vary per boot. Existing profiles and their build hashes are
  unchanged.
- One cold boot on 2026-10-26 completed the unlock and direct Windows handoff
  with display recovery. Installed wrapper `64D887E4…` and child `1265F152…`
  match a clean rebuild from this source (clang/lld-link 23.1.4, not the
  pinned 20.1.8).
- Measured cuBLAS GEMM n=4096, FP32 accumulate: FP16 109.8 and BF16 113.2
  TFLOPS (locked 56.4/59.9); FP32 unchanged. One boot and one before/after
  pair; S3/resume, repeated automatic cold boots, games/ComfyUI and long-term
  stability remain unverified. This is not general RTX 3080 support.
- Added `tests/test_hardware_profiles.py` and source documentation. The
  Chinese-locale BCD parser and the local observation-registration script are
  separate/local and not part of this profile change.

## Unreleased — offline compatibility reports and comparison

- Add a Python-only, read-only checker for existing probe captures. Reuse the
  generator's structural, driver-code and PROM checks without profile generation,
  FWSEC execution, host-test evidence changes or hardware operations.
- Explain candidate, missing-data, out-of-scope and invalid-data results in English
  or Korean. Automatic local-profile scope is separate from exact fixed profiles;
  a candidate is not hardware verification or write-build eligibility.
- Format-2 JSON reports include typed model/topology observations for issue review,
  preserving unknown values and graph completeness. Exclude private paths, serials,
  memory addresses, PROM/capture hashes and firmware contents. Upload nothing.
- Add bounded, schema-validated comparison of two checker reports, distinguishing
  different observations from unavailable data and deferring across checker/policy
  versions. No report can authorize an unlock or cause a command to be executed.
- Connect the optional report to the existing compatibility issue form and EN/KO
  guides. Windows inventory and manually entered model reports remain welcome.

## Unreleased — local profiles: per-machine PCI bus, optional iGPU, optional EDID driver; MSI B550M / RTX 3090 evidence

- `GU_TARGET_BUS` (default `1U`) replaces the literal bus `1` in `GuCheckConfig`, `GuTargetConfig`,
  `full-release.c` and `prepare-console.c`. A non-default value compiles only with
  `GU_PROFILE_LOCAL`. For all five reviewed profiles, headless and manual builds, the objects of
  `full-release.c`, `prepare-console.c`, `checks.c`, `port.c` and `diagnostic.c` match the
  current main baseline byte for byte except for COFF creation timestamps.
- `make-profile.py` accepts any nonzero bus at segment 0, device 0, function 0 and writes
  `GU_TARGET_BUS`. It no longer requires an integrated GPU: none or one is accepted; with none the
  RTX must own the only GOP in the graph, and a capture set that gains or loses an iGPU between
  boots is refused. It accepts a graph in which no handle has an active `EdidDiscovered` opener and
  then writes `GU_NO_EDID_DRIVER 1` and no `GU_EDID_*` constants.
- `GU_NO_EDID_DRIVER` (local profiles on the non-aggregate headless build only; other builds refuse
  it at compile time): the release skips the EDID stage and stops only the NVIDIA GOP driver after
  the console detach. Any live driver on a display child stops the release before that stop. Such
  a profile's evidence gains a 20th job, a replay of the whole `GuReleaseGop` on its own graph,
  driver image, PROM and PCI config (`test_headless_full.c`). `make-console-fixture.py` takes
  `--no-igpu` and `--no-edid-driver`.
- `proven.json` admits nvgop `0x60009` (`.text` `d045cee2...`), released cleanly three times on
  the machine in [evidence/b550m-3090-usb](evidence/b550m-3090-usb/README.md): MSI B550M PRO-VDH
  WIFI, Ryzen 7 5700X (no iGPU), MSI RTX 3090 at `0000:2b:00.0`, no EDID driver, headless USB.
  Locked to unlocked at n=4096: FP16 64.3 to 110.9, BF16 66.2 to 115.9 TFLOPS, FP32 unchanged.
- Preserve the merged strict probe-record parser, callback drift tests and PROM allocation-return
  checks. Benchmark selection stays at bus `01`; its unfinished bus option is not restored.
- Admission and fixture generation share strict graph completeness checks; target probe/graph BDFs
  must agree. No-EDID children require unique complete paths, replayed from their recorded bytes.
- Bind the no-EDID report flag and required release replay to one exact boolean header define.
  Reject missing/ambiguous defines, malformed observations and header/report disagreement.
- Capture-free generator and release regressions cover bus/iGPU/EDID variants and one/two children,
  IO off/on and both admitted VRAM sizes. These tests are not additional hardware evidence.

## Unreleased — probe: non-01 PCI bus and PciIo attribute record

- The generic probe takes its deep-read bus from `GU_PROBE_BUS` (`research/profile/build-probe.ps1
  -TargetBus`, default 1). The default keeps the same deep-read BDF check; a non-default bus compiles
  only into the read-only diagnostic build. Many AM4/B550 boards put the CPU x16 slot on another
  bus (for example `0x2b`), where the probe could only inventory the card.
- The checked probe records the PciIo attribute cache once, with `EfiPciIoAttributeOperationGet`,
  before its final raw-state recheck (`pci-attributes status= value=`). `make-profile.py` requires
  that record and refuses a capture whose IO/MEM/BME attribute bits differ from the PCI command
  register: `full-release.c` `CapturePermit` would stop the release there, after the console was
  already detached. Captures from an older probe must be taken again.
- Benchmark selection is unchanged: the native benchmark still requires bus `01`.
- Refuse malformed or ambiguous PciIo attribute records, including a damaged record beside a valid one.
- Host tests: the generic probe suite also runs at bus `0x2b`; a non-default bus is refused outside
  the diagnostic build; the attributes mock fails on any operation other than Get. Attribute
  callbacks that change MEM/BME/configuration/BDF/interface prevent all dependent reads; a
  failed Get with a plausible partial value retains its failure status. No reviewed
  profile, release path or generator admission rule other than the attribute record changes.

## Unreleased — recheck PROM read state after allocation

- Recheck the exact PCI interface, identity, all 64 configuration DWORDs and MEM1/BME0
  after PROM buffer allocation, before dependent reads. Preserve both IO-enabled and
  IO-disabled reference configurations. A later buffer-free error no longer replaces
  an earlier read/hash/configuration error.
- Extracted the shared safety idea from PR #53 without its MSI profile, proposed
  attribute policy or the #49 support expansion. No hardware support is added.
- `internal-boot/boot/test-prom-boundary.ps1` uses synthetic RAM and the real SHA-256
  implementation for all five reference profiles, manual and headless configurations.
  Tests cover service-return drift, partial reads and combined read/cleanup failures.

## Unreleased — harden GA106 observation and integrate the fifth profile

- Preserve current GA104/GA102 profiles, LOCAL gates, ownership checks and TF32 tools
  while adding the exact H610M GA106 headless USB profile. Keep the README GPU-based.
- Apply bounded BAR/MEM/BME and allocation-return identity checks to the new named
  probes and USB diagnostic path. Keep generic discovery inside its existing GA102 envelope.
- Refuse existing or unreadable image/PROM dump files. Observation staging requires an
  archived/empty capture set; EFI absence checks assume no concurrent writer and are not
  an atomic exclusive-create primitive.
- Validate typed observation capabilities, exact profile/defines, source and platform
  config digests and EFI hashes before staging. Diagnostic builds exclude generated
  unlock headers entirely. Use the shared bounded PE parser for offline fingerprints;
  missing/failed MMIO stays null, never a fabricated zero.
- Contributor physical evidence remains pinned to original PR #30 `4b748f5`.
  Current integration has host/build verification only, with no new EFI or GPU execution.

## Unreleased: Gigabyte H610M H / RTX 3060 12GB GA106 machine profile (headless USB)

- Added `-DGU_PROFILE_H610M_3060` (`-HardwareProfile h610m-3060` in `unlock/build.ps1`,
  `research/headless/build-usb.ps1`, `boot/test-cold-precheck.ps1`, `boot/build-diagnostic.ps1`
  and `unlock/test-all.ps1`) for Gigabyte H610M H BIOS FC, Core i5-14400 and Gigabyte RTX 3060
  12GB (GA106, `10de:2504`/`1458:4096`). It uses the non-aggregate single-RTX console and is the
  first GA106 entry. It pins the 12 GiB layout (`frts=0x2ffe00000`) and FWSEC-FRTS at PROM
  `0x416bc` (`0xea00` bytes).
- Added a read-only probe path for new machines. `GU_PROFILE_PROBE_H610M_3060` pins only the PCI
  identity and refuses to compile with any release or unlock define.
  `build-diagnostic.ps1 -UsbMedia [-DumpProm]` and `research/headless/build-observe-usb.ps1` /
  `stage-observe-usb.ps1` build and stage the observation EFI on a USB stick. A USB diagnostic
  returns to the firmware BootOrder on failure, because it performs no driver release or GPU writes.
  `research/headless/fingerprint_probe.py` derives profile candidates offline.
- `test-all.ps1` takes `-HardwareProfile` and `-PeBase`. The defaults are unchanged.
- Contributor-reported hardware result: one clean USB transaction, with direct Windows start
  and both displays recovered. torch.mm n=8192 medians: BF16 27.2 → 46.3 and FP16 27.1 → 46.6
  TFLOPS. [Evidence](evidence/h610m-3060-usb/README.md).

## Unreleased — include TF32 in future benchmark runs

- Add explicit TF32 Tensor GEMM with FP32 input/output buffers and FP32 accumulation
  to the 3090 / 3090 Ti / 3070 Ti measurement sources, after the existing three modes.
  Preserve 4096 matrices, two-second warmups and 30 samples per mode.
- Require protocol-2 metadata and four distinct result modes in the runner. Refuse
  nonempty NVIDIA_TF32_OVERRIDE rather than silently measuring overridden FP32 work.
- Preserve reading/comparing completed legacy three-mode results; compare only matching
  three-mode pairs or matching four-mode pairs, with existing provenance checks intact.
- Update English/Korean benchmark guidance. This change prepares future measurements;
  no new GEMM run or TF32 throughput result is claimed.

## Unreleased — integrate the fourth exact-machine profile

- Preserve the local-profile evidence gates while adding the reviewed B660-PLUS /
  GA104 RTX 3060 identity to USB staging. Its write builds require the headless USB route.
- Require exactly one held exclusive PCI ownership record in aggregate decode-restore
  checks, matching the existing direct-console policy. Reject missing/duplicate/drifted
  records before the restore gate; this corrects an existing aggregate-path omission.
- Keep generic deep capture and local profile generation within their existing GA102
  envelope; this exact GA104 profile does not widen those paths.
- Retain contributor measurements and original build hashes at PR #24 `60ae7e4`.
  Current-main integration has no new physical boot claim. Refresh both READMEs,
  compatibility tables and USB guidance with that boundary.

## Unreleased — profile: ASUS PRIME B660-PLUS D4 / RTX 3060 12GB (two-child aggregate console)

- New profile `-DGU_PROFILE_B660PLUS_3060` (`-HardwareProfile b660plus-3060`,
  `ASUS-PRIME-B660-PLUS-D4-3010-RTX3060`): GA104 RTX 3060 12GB `10de:2487` subsystem `10de:1530`,
  12 GiB (`0x3000`), BAR0 `0x82000000`, FWSEC-FRTS at `0x416b4`/`0xea00` (VBIOS 94.04.71.00.87),
  ASUS PRIME B660-PLUS D4 BIOS 3010. Console, combo-splitter, EDID and nvgop fingerprints are
  identical to the B660M / 3070 Ti profile; constants derived from three agreeing read-only probe
  captures, the last in the normal multi-monitor boot state. Headless USB only. Requires CSM
  disabled (with CSM on, the BIOS INT10 driver owns the card and BAR0 moves, which the checks
  refuse).
- Contributor-reported hardware verification from a USB stick, two full-shutdown boots: first
  with the original contribution's build (on the unmerged per-machine pipeline branch), then
  with the original PR #24 build at `60ae7e4` (wrapper `809f483f...` with cold-precheck hardening, which passed;
  child `be538381...`, byte-identical in both). Both: clean transaction (`SS0=0x88888888 SS1=0x8`,
  one FLR, `clean=1`), Windows started directly with all monitors restored. PyTorch/cuBLAS GEMM
  n=4096, FP32 accumulate, 30-sample median, locked (full shutdown, no stick) → original PR
  build: FP16 26.12 → 49.07, BF16 25.77 → 49.41, TF32 12.98 → 25.06, FP32 7.99 → 7.98 TFLOPS
  (first build: 50.04 / 50.23 / 25.24 / 8.03).
- `GU_CONSOLE_MULTI_CHILD` / `GU_CONSOLE_CHILD_COUNT`: generalize the aggregate console to a GPU
  that drives more than one display child — one active console GOP child plus EDID-only siblings.
  `prepare-console.c` and `full-release.c` pin every child by path and require each sibling to be
  EDID-only (no GOP or TextOut) from the same EdidAgent; the existing per-child EDID detach loop
  then releases all children before nvgop. Existing single-child and direct-console layouts
  are retained; the aggregate ownership gate is hardened above. Aggregate-GOP identity hardening (`AggCandidate`/`AggDriver`)
  is preserved. Based on the three-child Z690 work in the `GinzaTech/codex/rtx3060ti-windows` fork,
  generalized to a profile-driven child count.
- Host tests: `research/profile/test-b660plus-3060.ps1` (16 jobs: preflight/port/transaction
  against this card's real PROM and the stock assets, all EFI/MMIO/DMA simulated) and
  `internal-boot/boot/test-aggregate-graph-b660plus.ps1` (WebAssembly replay of the real two-child
  console graph through the compiled `Consumers`/`FullConsumers`/`FullExpected`, plus eight
  fail-closed mutations). The contributor reports all passed; the 16 private-fixture jobs
  were not independently rerun during integration. Mock/replay only, no GPU or firmware access. The PROM and
  capture fixtures stay local and git-ignored under `research/profile/local/`.

## Unreleased — harden the per-machine pipeline before integration

- Bind all diagnostic dumps to the wrapper's validated launch root. Preserve fixed-profile
  hash-only PROM diagnostics; generic PROM dumping is limited to one reviewed GA102 PCI
  identity, MEM1/BME0, BDF and BAR resource envelope with a final identity/config recheck.
  Complete the MMIO batch before logging or file-service calls.
- Reject malformed/truncated loaded PE headers, sections, relocation blocks and code/Stop
  ranges; bound runtime code hashing as well as offline generation.
- Require format-2 evidence with exactly 19 expected jobs, four fixtures, profile report
  identity and an ordered build-input manifest including EFI runtime and EDK2 headers.
- Archive earlier local captures when resuming. Describe the experimental local workflow
  in both READMEs without adding a fourth hardware-verified profile.
- The byte-identical wrapper claim below describes the original contribution's private
  capture rebuild, not this hardened integration. No new physical boot is claimed.

## 2026-10-25 — per-machine pipeline: probe → local profile → tests → stick

- `research/profile/new-machine.ps1`: one resumable command for a machine without a reviewed
  profile. Stages the generic read-only probe on a USB stick, then (after two probe boots)
  generates a local profile, runs its host tests, builds the headless USB package, and with
  `-StageUnlock` stages it. Files only; booting is always manual. See
  [docs/NEW-MACHINE.en.md](docs/NEW-MACHINE.en.md).
- Generic probe `-DGU_PROBE_GENERIC` (`research/profile/build-probe.ps1`): records every NVIDIA
  display controller instead of one known card and dumps the PROM to `PROM.BIN`; compiles only
  into a read-only diagnostic build. `-DGU_USB_PROBE` lets the diagnostic log to the USB stick
  it booted from, like `GU_USB_MEDIA`.
- `research/profile/make-profile.py` derives every `machine-profile.h` constant from the
  captures' own ownership graph, driver dumps and PROM (via `fwsec-locate`, the production
  VBIOS parser on the host), and refuses captures outside `research/profile/proven.json` or
  that disagree between boots. On the X870 / RTX 3090 Ti captures it reproduces all 25 reviewed
  constants and both host fixtures exactly, and the resulting USB pair is byte-identical to the
  hardware-verified one (wrapper `19a145a2...`, child `6ffe6165...`).
- `-DGU_PROFILE_LOCAL` / `-HardwareProfile local` (`unlock/build.ps1`, `build-usb.ps1`,
  `test-cold-precheck.ps1`): includes the generated, git-ignored `unlock/profile-local.h`.
  Write builds and `stage-usb.ps1` require `research/profile/local/test-results.json` from a
  passing `research/profile/test-profile.ps1` bound to the exact header, fixtures and source
  tree; headless USB only.
- `research/profile/test-profile.ps1`: the reviewed-profile host jobs made generic for the local
  profile (19 jobs incl. the production transaction against the card's real PROM), plus
  `test_make_profile.py` refusal cases built by mutating the machine's own captures.

## Unreleased — integrate the third profile with matched benchmark evidence

- Preserve the hardened pre-release checks while integrating the exact X870 /
  RTX 3090 Ti USB profile. Historical contributor measurements retain their
  original build provenance; the merged build has no new physical boot claim.
- Match benchmark GPU selection, executable hashes and offline comparison across
  3090 / 3090 Ti / 3070 Ti. Build manifests include only the selected model's
  artifacts and shared DLLs; missing or duplicate selected executable hashes
  reject before execution. The default 3090 protocol is unchanged.
- Refresh English/Korean README and compatibility entries with all three exact
  profiles, contributor evidence and the X870 USB-only installation boundary.

## 2026-10-25 — Gigabyte X870 / RTX 3090 Ti machine profile (headless USB)

- Added `-DGU_PROFILE_X870_3090TI` (`-HardwareProfile x870-3090ti` in `unlock/build.ps1`,
  `research/headless/build-usb.ps1`, `boot/test-cold-precheck.ps1`): Gigabyte X870 AORUS ELITE
  WIFI7 BIOS F13d, Ryzen 7 9800X3D, ASUS RTX 3090 Ti `10de:2203`/`1043:8870`. Non-aggregate
  single-RTX console. The card reports `0x5ff4` MiB, so the WPR layout sits 12 MiB lower than on
  the B650M (`frts=0x5ff200000`); FWSEC-FRTS is at PROM `0x41758` (`0xea00` bytes).
- Contributor-reported hardware verification from a USB stick: three clean transactions (two full shutdowns, one Windows
  restart), direct Windows start with display recovery, cuBLAS GEMM n=4096 medians BF16
  81.0 → 150.2 / FP16 80.4 → 149.3 TFLOPS. The original `7b578247` contribution reports
  byte-identical rebuilds of the measured pair (wrapper `19a145a2...`, child `6ffe6165...`).
  These hashes do not identify the later hardened merged build.
- Per-profile test pins for the X870 cold-state line and FWSEC-FRTS location.
- `benchmark/compute-benchmark-3090ti.cu`; `build.ps1`/`run-benchmark.ps1` take
  `-Card 3090|3090ti|3070ti` (default `3090`) and `build.ps1 -AllowUnsupportedCompiler` for MSVC
  14.4x with CUDA 12.6.

## Unreleased — harden the pre-release cold-state read

- Validate memory decoding, disabled bus mastering, the exact raw BAR0 and the
  existing BAR descriptor/end-tag/checksum policy before any cold-state MMIO.
- Re-resolve the original handle and require the same PciIo, BDF and complete
  configuration snapshot before and after the resource allocation/free calls.
  No release/unlock authority is created by this read-only pre-check.
- Expand fixture-free pre-check tests with zero-MMIO negative cases for invalid
  decode/resources, failed services and service-return identity/config drift.
  The wrapper skips release/unlock when prerequisites cannot be verified.
- These are software guards, not an atomicity or hardware-safety proof. Existing
  hardware reports predate this hardening and do not validate the changed build.

## 2026-10-25 — headless USB media and read-only cold pre-check

- `-DGU_USB_MEDIA` (`internal-boot/unlock/build.ps1 -UsbMedia`, `research/headless/build-usb.ps1`):
  the headless wrapper and child run from a FAT32 USB stick and accept only the USB
  device they were loaded from instead of a pinned internal ESP GUID. The Windows ESP
  GUID, child SHA256 contract and every exact-profile check are unchanged.
  `research/headless/stage-usb.ps1` copies the package with hash verification.
- All full-unlock wrappers now run a read-only cold-state pre-check (`GuPeekCold`)
  after the Esc window and before any driver release. A non-cold GPU (restart, already
  unlocked, initialized WPR2) takes the existing skip path to Windows instead of
  releasing the display and stopping at `VerifyCold`.
- USB builds return `EFI_NOT_STARTED` to the firmware boot order when a failure occurs
  before driver release, so a default-boot stick falls through to Windows Boot Manager.
- New fixture-free host tests: `internal-boot/boot/test-cold-precheck.ps1` (pre-check
  cases including zero-MMIO failure paths, plus full-flow gates) and a USB-media case in
  `test_headless.c` (internal-partition load refused before any hardware effect).
- See [docs/HEADLESS-USB.en.md](docs/HEADLESS-USB.en.md).

## Unreleased — issue #21 handoff and controlled benchmark comparison

- Inspired by Doug Sharp's same-machine before/after evidence in issue #21,
  preserve build/runtime/GPU-setting provenance with each benchmark run and
  hash the completed raw evidence. Check recorded GPU settings after each trial.
- Add an offline comparison tool with explicit same-machine confirmation,
  protocol/metadata checks, median-of-three ratios and the FP32 control change.
  Partial, mismatched and modified evidence is refused. Existing GPU workload
  and EFI/boot behavior are unchanged; no new hardware support is claimed.
- Record attachment provenance, source-review findings and missing launcher/
  firmware provenance in `docs/research/ISSUE21_HANDOFF.ko.md`. The contributed
  EFI sources remain a local research input, not an integrated executable path.

## 2026-10-24 — v0.3.1 variable headless display-child discovery

- Fixed preflight rejection when ASUS firmware exposes three RTX display
  children instead of exactly two. The wrapper now discovers and validates up
  to four unique RTX display children, then releases each guarded EDID
  connection before the NVIDIA GOP.
- The rebuilt wrapper completed automatic unlock and Windows handoff on the
  same system. The user observed the RTX display go dark during release and
  return after Windows started. No new performance benchmark was run for this
  build.
- Release remains source-only. Do not publish local EFI binaries, firmware,
  PROM captures, raw boot logs, partition identifiers or machine-local config.

## 2026-10-24 — GA104 RTX 3070 Ti machine profile (second verified machine)

- Added compile-time machine profiles in internal-boot/unlock/machine-profile.h:
  -DGU_PROFILE_B650M_3090FE (default, original verified machine) and
  -DGU_PROFILE_B660M_3070TI. PCI/subsystem, BAR0, chip/VRAM, PROM, root-port,
  display-path and all EFI driver fingerprints are profile constants; a build
  remains exact-machine and a user-editable file never selects them.
- New aggregate-console headless topology (GU_CONSOLE_AGGREGATE) for hosts
  without a second display GPU: nvgop owns the target PCI function, the AMI
  GOP combo splitter serves the console through an aggregate GOP, and the
  wrapper releases the nvgop/EDID/combo/text chain instead of the two-child
  nvgop console used on the B650M profile.
- Hardware verified on TUF GAMING B660M-E D4 BIOS 4003, i7-12700F, MSI
  RTX 3070 Ti: one headless unlock transaction, direct Windows start with
  display recovery, cuBLAS GEMM n=4096 medians FP16 88.7 / BF16 88.6 TFLOPS.
  S3 sleep still invalidates the unlock; no resume path exists.
- internal-boot/unlock/build.ps1, research/headless/build-unlock.ps1,
  Configure.ps1 and Headless.ps1 take -HardwareProfile (default b650m-3090fe);
  installer board/BIOS gates follow the built package profile. The B650M
  profile keeps its original code paths and constants behind the same checks.
- Added a read-only diagnostic probe (probe.c, -DGU_DIAGNOSTIC_PROBE) used to
  fingerprint new platforms; it performs no writes and is compiled into
  diagnostic/headless wrapper builds.

## 2026-10-24 — v0.3.0 headless internal handoff, public setup and recovery

- Verified one internal headless boot on the exact ASUS BIOS 3057 / RTX 3090 FE
  system: RTX GOP 1 → 0, one successful unlock transaction, direct Windows
  return and three Tensor benchmark trials. FP16/BF16 medians were
  124.969948/128.931534 TFLOPS. No active AMD display was required; the CPU
  still has an iGPU, and other boards/CPUs/headless USB are not validated.
- Added a separate headless compile profile: two reviewed EDID children, exact
  NVIDIA ownership release, zero-physical-GOP policy, file-only child logging,
  TPL-protected observations and original PROM/cold/containment/cleanup gates.
- Added public configuration/build provenance, stage/register/confirmed automatic
  promotion and Windows-first recovery. Registration preserves default priority;
  promotion explicitly orders Headless then Windows and saves Fast Startup
  recovery data before mutation. Source/include/config/asset contracts are rechecked.
- Rewrote English/Korean README and setup guides, retained distinct legacy USB
  and AMD-display paths, and published sanitized headless evidence. Firmware,
  PROM/BIOS, private partition configuration and generated EFI images stay local.
- Original manual child, automatic child and automatic wrapper EFI hashes remain
  identical. Public installer mutation paths are mock-tested; cross-machine
  registration, repeated cold-boot reliability and sleep/resume remain unverified.

## 2026-10-23 — v0.2.0 internal handoff and experimental automatic boot

- Added a separate `internal-boot/` source tree; original USB release source and EFI remain unchanged.
- Reviewed RTX console/EDID/GOP release, original IO/MEM restoration without BME, and direct Windows handoff. Original unlock transaction and card guards retained.
- Discovered display ADR per boot while pinning PCI ancestry, GPU identity and subsequent whole-path checks. Allowed only old CED/URD status clearing after successful unlock; other config changes still fail.
- Added precise handoff failure stages and config differences. Internal manual handoff and Tensor performance were verified on one BIOS 3057 system.
- Added a separate compile-time automatic child with two Esc windows, strict input failures, fresh hardware gates and rejection of unexpected owners. Automatic firmware boot remains unverified.
- Added local partition configuration, builds, stage/manual/automatic/recovery scripts and bilingual walkthroughs. Private partition GUIDs, firmware/PROM and raw boot graphs are excluded.
- Public-source builds reproduced all four locally installed EFI hashes. Public installer registration/promotion remains experimental; host and policy tests are not hardware evidence.

## 2026-10-23 — International documentation and compatibility inventory

- English default README and complete English USB, assets, build, compatibility, benchmark and results guides; Korean README and guides remain available through language links.
- Added a local read-only Windows model/version inventory and manual compatibility issue form. Private identifier fields and PnP instance suffixes are excluded; reports never authorize unlock.
- Added a staged portability design distinguishing host discovery, GPU policy, software tests and hardware evidence. This update adds no new write-enabled EFI profile or hardware support.
- USB package readme links both languages. Refreshed the public source inventory for these documentation/tool additions; successful EFI and its corresponding C/header hashes are unchanged.

## 2026-10-20 — Windows GPU UNLOCK public case study

- Base: rough2515-hub/rtx3080-unlock-public at a08254d269af30ace9e57237abd3d8e6c261e8f5; original core and licenses retained.
- Local changes made 2026-10-19/20: exact RTX3090 24GiB and AMD iGPU identity checks, memory layout, firmware ownership/PCI access checks, default read-only mode, explicit experimental profiles, immutable entry-selector latch and one SS1 write for entryLatchedSs1Full, phase guards, corresponding host mock tests.
- Modified upstream files: unlock/app.c, app_io.c, build.ps1, checks.c, checks.h, port.c, port.h, test-all.ps1, tests/test_app.c, test_checks.c, test_efi_transaction.c, test_master_attributes.c, test_pe.py, test_port.c, test_sha256.c, vendor/unlock_610.c. Additional RTX3090 tests are under unlock/tests/.
- Public packaging only: portable LLVM defaults, configurable MSVC environment and PE negative tests that inspect the selected profile rather than requiring a separate preflight build; no further GPU transaction changes. Success EFI is tied to hashes of the corresponding C sources and headers. Changed source files carry dated notices; C notices share an existing line to preserve diagnostic __LINE__ values and reproduce the successful EFI byte for byte.
- Added Korean USB/ASUS boot handoff guide, measured results and benchmark sources. Changed benchmark device selection to RTX3090, added inspect-only mode, and kept the original GEMM helper/measure path.
- The release deliberately excludes NVIDIA firmware, drivers, CUDA DLLs, original PROM dumps, private paths, credentials and unrelated repositories.
- Direct Windows chainload and internal disk automatic boot are not implemented.
