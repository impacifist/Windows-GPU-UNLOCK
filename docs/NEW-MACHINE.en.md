# Bringing up a new machine (local profile)

**English** | [README](../README.md) | [Headless USB guide](HEADLESS-USB.en.md) | [Portability](PORTABILITY.en.md)

A reviewed profile in `internal-boot/unlock/machine-profile.h` exists for each
hardware-verified machine. For any other motherboard/BIOS/card, this pipeline builds a
**local profile** for your machine only: the same compile-time constants as a reviewed
profile, derived from read-only captures of your own machine, never committed, and
write-eligible only after your machine's own host tests pass against exactly those bytes.

It does **not** add hardware support. A local profile is mock-tested on your fixtures; it is
not hardware evidence until you have run it and reported it (see the end of this page).

## What it accepts

The generator refuses anything outside `research/profile/proven.json`, the envelope that
reviewed profiles have proven on hardware. There is no override flag. Today that means:

| Requirement | Why |
|---|---|
| RTX 3090 / 3090 Ti PCI identity `220410de` / `220310de` | only these identities have the reviewed GA102 deep-capture envelope |
| GA102 (`chip=0xb72000a1`, class/revision `030000a1`) with `0x6000` or `0x5ff4` MiB | the only chip/memory layouts that completed the transaction on hardware |
| exactly one NVIDIA display controller, at segment 0, device 0, function 0 on a nonzero PCI bus (reviewed profiles: bus `01`), behind one root port | `GuCheckConfig` / `GuDisplayPath`; the bus is the per-machine constant `GU_TARGET_BUS` |
| at most one integrated (non-NVIDIA) display controller; with none, the RTX owns the only GOP in the whole graph | the single-RTX console path never uses a second adapter and refuses any AMD GOP |
| `ConOut` is exactly the RTX GOP (non-aggregate console) | aggregate splitter consoles are not generated |
| AMI console text/GOP splitter drivers and `nvgop-ga1xx`; the AMI EDID driver is optional | the release path in `full-release.c` (see below) |
| `nvgop-ga1xx` `.text` hash already released cleanly on hardware | another build has never been released |
| GPU in its power-on state at every capture | the unlock never runs from anything else |
| PciIo attribute cache recorded and equal to the PCI command register's IO/MEM bits, no bus mastering | `CapturePermit` stops the release otherwise, after the console was detached; captures from an older probe are refused |
| at least two complete captures from separate full shutdowns, all agreeing | load-address- or boot-dependent values are refused |
| PROM.BIN matches the captured hash; the production parser finds FWSEC-FRTS within tested bounds | FWSEC pin and transaction tests |

A RTX 3090 or 3090 Ti on an AMI BIOS board, with or without an iGPU, is the likely fit. Anything
else is refused with the reason. Widening the envelope needs a reviewed change that cites new
hardware evidence, normally a new reviewed profile (PORTABILITY stage 4).

**Bus, iGPU and EDID.** The generator accepts a card on another PCI bus, a machine without an
iGPU and a firmware that binds no EDID driver. One machine covers all three on hardware
([evidence/b550m-3090-usb](../evidence/b550m-3090-usb/README.md)); anything else about such a
machine (chip, memory, nvgop build) must still be inside the envelope. When no handle in the
capture has an active `EdidDiscovered` opener the profile sets `GU_NO_EDID_DRIVER`: the release
skips the EDID stage and only stops the NVIDIA GOP driver after the console detach. Every display
child of the RTX must still carry the `EdidDiscovered` interface, and any live driver holding a
display child, or an EDID driver appearing later, stops the release before the NVIDIA stop. Such a
profile must also pass a replay of the whole driver release on its own graph, driver image, PROM
and PCI config (a 20th job). Only the non-aggregate headless build supports the flag; other builds
refuse it at compile time. The probe needs `build-probe.ps1 -TargetBus` for a card off bus `01`.

Admission and replay share a strict parser: incomplete/ambiguous handle, protocol,
open and PCI snapshots cannot establish absence of an adapter or EDID owner. The
no-EDID path admits one or two unique NVIDIA-owned display children. The generated
header explicitly records `GU_NO_EDID_DRIVER 0/1`; the report must contain the same
boolean, and this header-bound value selects the required release replay. Regenerate
older local profiles that lack this flag from their existing valid captures.

Maintainers can run capture-free regression checks with
`python -I research/profile/test_local_topology.py` and
`research/profile/test-local-release.ps1`. These use synthetic data only and do not
replace the actual machine's required capture/asset replay or hardware evidence.

## Check existing captures first

The [offline compatibility checker](CHECK-COMPATIBILITY.en.md) reports missing data
and local-profile refusal reasons without generating a profile, launching an external
firmware parser or touching hardware. It is optional and does not replace any step or evidence gate
below. Passing this check is not proof of hardware compatibility.

## Steps

Needs: LLVM (`clang`, `lld-link`), MSVC build tools, Python 3.10+, the five firmware assets
(`docs/ASSETS.en.md`) and a FAT32 USB stick. Fast Startup off. **CSM disabled** in the BIOS: with
CSM on, AMI CsmVideo (`29CF55F8-B675-4F5D-8F2F-B87A3ECFD063`) owns the RTX instead of `nvgop-ga1xx`
and firmware may leave bus mastering on. The probe then refuses the deep read
(`probe-unreviewed-envelope`, PCI command `0x0007`). See
[evidence/b550-aorus-3090-usb](../evidence/b550-aorus-3090-usb/README.md).

Run the scripts in **Windows PowerShell 5.1** (`powershell.exe`). PowerShell 7 reads JSON integers
as `Int64`, and the local evidence checks (`-isnot [int]`) then refuse a valid
`profile.json` with "Generated profile report identity is invalid".

```powershell
# Windows ESP GUID: Get-Partition | Where-Object IsSystem | Select Guid
.\research\profile\new-machine.ps1 -Drive D -WindowsEspGuid 'YOUR-WINDOWS-ESP-GUID' -Assets 'C:\LOCAL-FIRMWARE\assets'
```

Run the same command repeatedly; each run does the next step.

1. **Probe.** The first run builds the generic read-only probe
   (`research/profile/build-probe.ps1`, `-DGU_PROBE_GENERIC`) and copies it to the stick as
   `EFI\BOOT\BOOTX64.EFI`, keeping any previous loader in `RTX3090-UNLOCK\backup\` and earlier
   capture files in `RTX3090-UNLOCK\old-*\`. Shut down fully, choose the stick in the firmware
   boot menu, and let it start Windows. Do this twice. The probe only reads: PCI config,
   loaded driver images, and for a single eligible RTX 3090 / 3090 Ti only,
   16 BAR0 registers and the 1 MiB PROM window. The deep read requires MEM1/BME0,
   the reviewed class/revision, the selected segment-0/device-0/function-0 bus, a 32-bit aligned 16 MiB BAR resource
   and unchanged handle/interface/config after resource queries and allocation. A card on
   another bus can be captured with `build-probe.ps1 -TargetBus 0x2b` (diagnostic build only);
   the generator then records that bus as `GU_TARGET_BUS`. The probe also records the PciIo
   attribute cache (Get only), which the generator compares with the PCI command register.
   Unknown devices and multiple NVIDIA displays receive inventory only; they cannot
   produce an eligible local profile. It writes
   `DIAGnn.TXT`, `PROM.BIN` and `IMGnn.BIN` through the launch volume validated by the
   wrapper; it never searches other filesystems for a matching folder. Missing roots
   and dump write/flush/close failures invalidate the capture. It cannot compile with unlock code.
2. **Profile.** With two captures on the stick, the next run copies them to
   `research/profile/local/capture/`, builds `fwsec-locate` (the production VBIOS parser for
   the host) and runs `research/profile/make-profile.py`. It writes
   `internal-boot/unlock/profile-local.h`, `research/profile/local/profile.json` and the test
   fixtures, or stops with the refusal.
3. **Test.** `research/profile/test-profile.ps1` compiles everything with
   `-DGU_PROFILE_LOCAL` and runs: the generator's refusal cases on your captures, the
   config/port/write-lock/BME/runtime/profile-lock tests, a reviewed profile for another
   card rejecting your config and console graph, the R2 console replay of your real graph,
   the cold pre-check, and the production transaction against your real PROM and the stock
   assets with all EFI/MMIO/DMA simulated. Only a complete pass writes
   `research/profile/local/test-results.json`.
4. **Package.** `research/headless/build-usb.ps1 -HardwareProfile local` builds the headless
   USB wrapper and child. Then the run stops; nothing is staged.
5. **Stick.** Read this page and the [headless USB guide](HEADLESS-USB.en.md), then rerun with
   `-StageUnlock`. That runs `research/headless/stage-usb.ps1`. Booting it is the first
   time anything writes to your GPU, and it is never automatic: choose the stick from the boot
   menu after a full shutdown, supervised, exactly as in the USB guide.

## The evidence gate

A write build of `-HardwareProfile local` (`internal-boot/unlock/build.ps1`, `build-usb.ps1`,
and `stage-usb.ps1` again before copying) is refused unless `test-results.json`:

- uses evidence format 2 with boolean pass/no-hardware flags and the exact 19
  host results (including exit 1 for the two required rejection jobs; 20 for a firmware
  without an EDID driver, which adds the driver-release replay),
- was made against exactly the current `profile-local.h` bytes and PROM hash,
- matches exactly the four fixtures and the generated profile report byte for byte, and
- matches an ordered path/hash manifest and digest of the sources, tests, generator,
  envelope, headless build/staging scripts, EFI runtime and EDK2 headers (`Get-LocalTreeDigest` in
  `internal-boot/unlock/local-profile.ps1`).

Editing the header, a fixture or any source after the tests makes the build refuse until
`test-profile.ps1` passes again. A local profile only builds the headless USB write path; the
internal-ESP and manual paths refuse it. Diagnostic/preflight builds need no evidence.
The evidence is a local stale-input guard, not a signature or permission to bypass
review. It excludes generated build output and compiler binaries; use a trusted
toolchain. Existing format-1 evidence must be regenerated by rerunning the tests.
The USB manifest also pins the source digest and exact test-results file. Passing
tests again after a source change does not authorize an older package: rebuild it
before staging. Packaging rechecks evidence after compilation as well.

The generator validates complete AMD64 PE32+ loaded-image headers, code/Stop bounds,
section ranges and DIR64 relocations before hashing. Truncated ranges and any
relocation touching hashed code are refused. Resuming the workflow archives earlier
local captures instead of deleting them.

`machine-profile.h` accepts `profile-local.h` only with the generator's marker and a
non-aggregate topology. Never commit it, `research/profile/local/` or any capture: they
identify your hardware and contain your PROM. Both paths are in `.gitignore`.

## When something changes

Regenerate (start again from step 1) after a BIOS update, a different or reflashed card, a
slot change or a Windows reinstall (new ESP). The boot-time checks would refuse anyway, but the
profile no longer describes the machine.

## Reporting a result

After a supervised run, the per-boot log and before/after benchmarks are what a reviewed
profile needs. Open a compatibility report ([CONTRIBUTING](CONTRIBUTING.en.md)) with the
`profile.json` values, the boot log and benchmark results, never the PROM or firmware assets.
A reviewed profile then moves your constants into `machine-profile.h` with that evidence,
and `proven.json` can widen for the next machine.
