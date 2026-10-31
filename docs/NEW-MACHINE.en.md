# Bringing up a new machine (local profile)

**English** | [한국어](NEW-MACHINE.ko.md) | [README](../README.md) | [Headless USB guide](HEADLESS-USB.en.md) | [Portability](PORTABILITY.en.md)

The six fixed profiles in `internal-boot/unlock/machine-profile.h` cover their
recorded machines; other successful setups may use generated LOCAL profiles.
For any other motherboard/BIOS/card, this pipeline builds a
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
| RTX 3090 / 3090 Ti `220410de` / `220310de`, RTX 3060 12GB `248710de` / `250410de`, RTX 3060 Ti 8GB `24c910de`, RTX 3070 8GB `248410de`, or RTX 3080 12GB `220a10de` | exact reviewed PCI-ID/chip/VRAM combinations are required |
| GA102 `b72000a1` with `220410de` / `220310de` + `6000` / `5ff4` MiB or `220a10de` + `3000` MiB; GA104 `b74000a1` with `248710de` + `3000` MiB or `248410de` / `24c910de` + `2000` MiB; GA106 `b76000a1` with `250410de` + `3000` MiB | these are coupled tuples, not independent choices; class/revision remains `030000a1` |
| exactly one NVIDIA display controller, at a captured 16-bit PCI segment, device 0, function 0 on a nonzero PCI bus (reviewed profiles: segment `0000`, bus `01`) | PCI identity and the captured segment/bus remain independently verified; other segments are experimental |
| ACPI PNP0A03 or PNP0A08 root, a captured root UID, 1–16 legal PCI path nodes ending at `Pci(0,0)`, one ADR and an end node | LOCAL pins the complete ancestry; display paths are bounded to 120 bytes and the PCI parent must consume exactly the controller prefix |
| at most one integrated (non-NVIDIA) display controller; the RTX owns the only physical GOP | a recognized pathless, zero-framebuffer AMI aggregate GOP is allowed; a second physical GOP is refused |
| non-aggregate `ConOut` is exactly the RTX GOP, or the complete reviewed AMI aggregate ownership graph is present | aggregate profiles require distinct NVIDIA/Combo/EDID/Gfx/Out roles and one or two unique NVIDIA display children |
| AMI console text/GOP splitter drivers and `nvgop-ga1xx`; the AMI EDID driver is optional | the release path in `full-release.c` (see below) |
| `nvgop-ga1xx` `.text` hash already released cleanly on hardware | another build has never been released |
| GPU in its power-on state at every capture | the unlock never runs from anything else |
| PciIo attribute cache recorded and equal to the PCI command register's IO/MEM bits, no bus mastering | `CapturePermit` stops the release otherwise, after the console was detached; captures from an older probe are refused |
| at least two complete captures from separate full shutdowns, all agreeing | load-address- or boot-dependent values are refused |
| PROM.BIN matches the captured hash; the production parser finds FWSEC-FRTS within tested bounds | FWSEC pin and transaction tests |

A matching card on an AMI BIOS board, with or without an iGPU, is a candidate. Anything
else is refused with the reason. The RTX 3060 tuples require nvgop `60010` with the exact
reviewed `.text` hash in `proven.json`. The RTX 3080 12GB `220a` tuple also
requires this `60010` code; [issue #73 evidence and limitations](../evidence/b550-3080-12gb-local/README.md). The GA104 evidence is an aggregate B660-PLUS
machine. LOCAL aggregate and non-aggregate combinations still need their own replay
and physical validation; generation is not another success report. The GA106
evidence is one non-aggregate H610M success. Other boards and current builds still
need their own host replay and physical validation. RTX 3070 `2484` / `2000` MiB uses
the existing `60009` code hash, based on the [contributor's exact build and success report](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/113).
GA104 requires a complete `pci_nvgop` map: 3060 `2487` remains `60010`-only and 3070
`2484` is `60009`-only. RTX 3060 Ti `24c9` / `2000` MiB is `60015`-only,
with `.text` SHA-256 `645a2f416940211e09023838ac03f4b45453f95f457eab2de49628d57e9deb95`,
based on [issue #105 and its reviewed source patch](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/105).
That contributor reports restart-based captures and one successful transaction;
separate full-shutdown captures and current-main physical qualification remain required.
A missing map, another SKU's driver or a different hash is refused.
Widening the envelope needs a reviewed change that cites new
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
and PCI config. Every LOCAL profile now requires a full driver-release replay, whether
the EDID driver exists or not. Only the non-aggregate headless build supports the flag; other builds
refuse it at compile time. The generic probe automatically selects the bus of the single
eligible NVIDIA display controller; you do not need a motherboard-specific bus option.
LOCAL profile generation still requires device 0, function 0 and a nonzero bus.

The generic read-only probe can also observe a unique reviewed NVIDIA target on
segments `0001`–`ffff`. It pins both segment and bus through the final identity
recheck; segment drift or multiple NVIDIA display controllers prevents deep reads.
`-TargetBus` constrains the bus, regardless of the observed segment. This collects
porting evidence. LOCAL generation pins the segment in `GU_TARGET_SEGMENT` and
`Replay.Segment`; both cold-boot captures and the header/report must agree. Prepare,
release, cold reads and PCI transactions require that exact segment, including when
another device has the same bus/device/function on a different segment. Legacy local
headers and named profiles retain segment zero. Nonzero-segment support has synthetic
host replay coverage, not physical compatibility evidence; it requires the same complete
host-test evidence and per-machine physical validation as other experimental layouts.

Admission and replay share a strict parser: incomplete/ambiguous handle, protocol,
open and PCI snapshots cannot establish absence of an adapter or EDID owner. The
non-aggregate no-EDID path admits one to four unique NVIDIA-owned display children;
EDID-driver and aggregate paths remain limited to one or two. The generated
header explicitly records `GU_NO_EDID_DRIVER 0/1`; the report must contain the same
boolean, and this header-bound value selects the required release replay. Regenerate
older local profiles that lack this flag from their existing valid captures.

Three/four-child no-EDID generation uses the existing runtime capacity of four;
it does not enlarge the driver's release permissions. The replay fixture carries
every captured sibling handle/path and checks faults on the final child, including
late path changes, new EDID/other consumers and a fifth valid-route child. Header,
report and host evidence must agree on the recorded count. The non-aggregate runtime
continues to discover and verify its bounded current-boot children; this metadata
is not a new exact runtime count pin. Four-child coverage is synthetic host evidence.
[Report #102](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/102) motivated
the three-monitor case, but its separate IO-cache correction and new nvgop `6000a`
entry are not admitted by this change. It does not qualify that whole machine on main.

**Aggregate console.** The generator records `GU_LOCAL_CONSOLE_AGGREGATE` and
`GU_LOCAL_CHILD_COUNT` together, plus the observed Combo driver metadata. The report
must match these values exactly. Aggregate preparation preserves the console and
performs zero detach calls; the later release stops EDID and NVIDIA drivers. Its
required host jobs use the actual captured ownership graph, loaded driver images,
PCI configuration, BAR descriptor, cold observations, GOP mode and console handles.
An ordinary non-aggregate release result cannot replace the aggregate replay.
All display child paths must agree across cold captures. Aggregate profiles require
an EDID driver. Capture with the current probe if older records lack pixel format,
GOP info size or the raw BAR0 resource descriptor. Synthetic host integration covers
generation and prepare/release but is not physical validation or a vendor transaction test.

Captured PCI path nodes must contain legal device/function numbers. LOCAL console
selection uses the captured NVIDIA ancestry and independently checks its PCI identity;
a root-port number that matches a historical AMD route does not identify an AMD GPU.
The bounded C/Python path decoder is checked on identical synthetic inputs as part of
the generator regressions. LOCAL generation accepts this bounded bridge-chain shape
for aggregate, non-aggregate and no-EDID consoles. Root HID/UID, every PCI hop and all
display child paths must agree across captures. The runtime checks the complete
ancestry and exact PCI-parent consumption again before release. This extends the
experimental discovery route; it does not add a hardware success report or widen the
GPU/driver-code proof envelope.

Maintainers can run capture-free regression checks with
`python -I research/profile/test_local_topology.py` and
`research/profile/test-local-release.ps1`. These use synthetic data only and do not
replace the actual machine's required capture/asset replay or hardware evidence.

`research/profile/test-pci-layout.ps1` checks the production PCI save/reset/restore
path against RAM-only services, including late PCIe v1/v2 capability layouts and
failure containment. `test-capture-model.ps1` checks that captured capabilities and
IO/MEM attributes reach the transaction model. Neither runs on a GPU or replaces
the full firmware-asset replay required for a local profile.

## Check existing captures first

The [offline compatibility checker](CHECK-COMPATIBILITY.en.md) reports missing data
and local-profile refusal reasons without generating a profile, launching an external
firmware parser or touching hardware. It is optional and does not replace any step or evidence gate
below. Passing this check is not proof of hardware compatibility.

## Steps

Start with the [capture-only setup guide](CAPTURE-SETUP.en.md)
([한국어](CAPTURE-SETUP.ko.md)) to check tools and collect diagnostics without firmware
assets or MSVC. Use `-CaptureOnly` to keep repeated runs at the collection stage.
Continuing to profile generation and host replay needs MSVC build tools and the
five firmware assets (`docs/ASSETS.en.md`) in addition to LLVM, Python 3.10+ and
the FAT32 USB stick. Fast Startup off.

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
   loaded driver images, and for a single eligible RTX 3090 / 3090 Ti, RTX 3060 12GB, RTX 3060 Ti `24c9` or RTX 3070 `2484` identity,
   16 BAR0 registers and the 1 MiB PROM window. The deep read requires MEM1/BME0,
   the reviewed class/revision, the selected captured segment/bus at device 0/function 0, a 32-bit aligned 16 MiB BAR resource
   and unchanged handle/interface/config after resource queries and allocation. The bus is
   discovered automatically and the generator records it as `GU_TARGET_BUS`. An optional
   `-TargetBus 0x2b` on `new-machine.ps1` or `build-probe.ps1` constrains discovery to that
   exact bus, without fallback. On resume, `new-machine.ps1` rejects existing captures
   with a different or ambiguous target before copying them or generating a package.
   The probe also records the PciIo
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

The generic read-only probe now attempts an optional parent PCI baseline. It logs
`parent-baseline` status/availability and distinct `parent-baseline-device` /
`parent-baseline-config` records, without adding another target config snapshot.
This captures up to 15 parent bridges through exact controller prefixes. Current
observation scope is command `2` and the research 2204/2203/2484/2487/2504 IDs;
command `3`, 24c9 and unsupported paths can remain `available=0` while the ordinary
diagnostic still completes. Parent availability never grants profile or correction
approval. Target/capability drift instead stops before further GPU deep reads.
Named diagnostic profiles do not request this extra collection.

Generic build/staging validates the exact diagnostic flags, bus constraint, EFI hash
and source manifest, including the six external read-only collector files. Those
files are also bound into LOCAL input hashes. Older source-bound evidence becomes
stale; all 22 actual-input LOCAL jobs remain required. The generic image links the
read collector and pure predicates, never the correction core or ownership adapter.
These integrations have host/PE evidence only, with no new physical EFI result.

Host fixture setup derives IO/MEM/BME attributes once from the captured PCI
command. Command `0x0002` therefore starts with attributes `0x200`, while command `0x0003`
starts with `0x300`; a later injected mismatch is not repaired. The SHA host test
also uses the selected profile's PROM hash. These fixes address the test issues
reported in [#129](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/129#issuecomment-6482780948)
and [#105](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/105).
They do not reconcile real PCI attributes or authorize the research pre-step.

The asset-free fixture regression can be run from an MSVC developer environment:

```powershell
python -I research/profile/test_capture_fixtures.py --clang C:\TOOLS\LLVM20\bin\clang.exe --mutations --output research/profile/build/your-new-fixture-run
```

Use a new output directory. It runs setup/port mocks and synthetic SHA inputs,
without invoking the app entry point or a vendor transaction. It writes no LOCAL
evidence and cannot replace the required actual PROM/assets tests below.

A write build of `-HardwareProfile local` (`internal-boot/unlock/build.ps1`, `build-usb.ps1`,
and `stage-usb.ps1` again before copying) is refused unless `test-results.json`:

- uses evidence format 2 with boolean pass/no-hardware flags and the exact 22
  host results (including exit 1 for the two required rejection jobs and the full
  driver-release replay selected by the profile's EDID state),
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

`machine-profile.h` accepts `profile-local.h` only with the generator's marker and
valid topology metadata. Never commit it, `research/profile/local/` or any capture: they
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
