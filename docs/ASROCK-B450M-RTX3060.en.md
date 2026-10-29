# Experimental ASRock B450M Pro4-F / RTX 3060 Windows port

Implemented and tested on 2026-10-27; documented on 2026-10-28.

## Scope and provenance

This is an exact-machine extension of
[impacifist/Windows-GPU-UNLOCK](https://github.com/impacifist/Windows-GPU-UNLOCK),
based on commit `20c44fecac13cdf601b6a9357a46d8fea29f2678`.
On 2026-10-29 the PR branch was integrated with upstream
`23cc028512acbb873cff8de38acdd63c66c98ea9`. The hardware observations below
remain tied to the original development tree; the integrated revision has no new
physical boot or performance measurement.
The retained unlock core comes from [Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock).
[PR #30](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/30), the upstream
H610M/GA106 RTX 3060 port, informed the diagnostic and headless USB approach.
The original algorithms, architecture and upstream contributions are not authored
by this port. Local implementation and analysis were developed with Codex assistance;
the machine operator performed the physical boots and supplied hardware evidence.

## Recorded machine

| Component | Recorded configuration |
|---|---|
| Motherboard / BIOS | ASRock B450M Pro4-F / P10.31 |
| CPU / system memory | AMD Ryzen 5 3600 / 16 GB |
| GPU | NVIDIA GeForce RTX 3060, GA106, 12 GB |
| PCI / subsystem | `10de:2503` / `1458:4074` |
| VBIOS | `94.06.14.40.95` |
| PCI location / BAR0 | `0000:07:00.0` / `0xfb000000`–`0xfbffffff` |
| Windows / NVIDIA driver | Windows 11 Home build 26300 / 617.42 |
| Firmware NVIDIA GOP | `nvgop-ga1xx`, version `0x6000b` |
| Boot route | UEFI, FAT32 USB, headless unlock, direct Windows handoff |
| Recorded prerequisites | Secure Boot disabled; C: fully decrypted |

These values describe one recorded system, not every board or card of these models.
The profile also pins PROM and driver-code hashes; model names alone do not match it.

## Boot flow

1. Start the USB wrapper from the firmware boot menu; it identifies the USB launch
   volume and the locally configured Windows EFI partition.
2. Validate exact PCI identity, resource bounds, power-on state and display ownership.
3. Release the boot console and the pinned NVIDIA display driver ownership chain.
4. Load the hash-bound child and authenticated, locally prepared firmware assets.
5. Run the retained transaction, target SS0 `0x88888888` and SS1 `0x8`, perform FLR,
   and verify containment, cleanup and the final device state.
6. Start Windows Boot Manager directly, without a reboot between unlock and Windows.

The procedure does not flash VBIOS or replace the Windows NVIDIA driver.
It changes volatile device state. A full power-off followed by ordinary Windows
boot without the USB route is the intended return path. Restart/sleep/hibernate
behavior and sustained unattended operation are not established here.

## Port changes

| Files | Contribution |
|---|---|
| `internal-boot/boot/asrock-probe.h`, `probe.c` | Narrow diagnostic deep-read admission to the observed identity, BAR and bus; preserve resource and state checks |
| `research/profile/build-probe.ps1`, `build-asrock-diagnostic.ps1` | Build the machine-targeted read-only diagnostic and its manifest |
| `internal-boot/unlock/asrock-b450m-3060.h`, `machine-profile.h` | Exact-machine constants, bus 7 and headless USB restriction |
| `internal-boot/unlock/asrock-evidence.ps1` | Require complete source-, fixture- and log-bound host results before build/staging |
| `internal-boot/unlock/build.ps1`, `local-profile.ps1`, `research/headless/build-usb.ps1` | Build/package recognition, stale-evidence checks and USB manifest binding |
| `internal-boot/boot/test_asrock_probe.c`, `test_asrock_release.c`, `test_full.c` | Admission refusals and real captured graph/code/PROM replay with simulated EFI calls |
| `research/profile/test-asrock-host.ps1` | Fourteen host jobs, transaction and application variants, release and pre-check tests |
| `research/profile/inspect-prom.py`, `parser-host-log.c` | Run the production VBIOS parser on a local PROM through a freestanding host DLL |
| `research/profile/review-asrock-profile.py` | Derive a research-only candidate and compare two captures without widening `proven.json` |
| `research/profile/prepare-host-assets.py` | Python 3.14 Zstandard extraction adapter; retain pinned upstream generation and hash checks |
| `benchmark/pytorch-benchmark.py`, `Run-PyTorch-Test.cmd` | Repeatable four-mode GEMM measurements with numerical checks and JSON output |

The ASRock profile is a separate experimental path. This port does not add its
`2503` identity to generic capture or automatic local-profile admission. A research candidate is not
used as write-build evidence. The profile's historical hardware-untested comments
and build-manifest flags describe the pre-test artifact; subsequent hardware
observations are recorded separately in the evidence directory.

## Development and validation

The initial generic inventory deliberately refused deep reads on device `2503`.
A separate targeted read-only probe was then implemented, with compile-time
rejection of unlock/release combinations and exact identity/resource admission.
Two full diagnostic captures agreed on PCI configuration, PROM, cold state and
driver fingerprints. The display ADR endpoint varied between boots while its PCI
ancestry remained identical; upstream `DisplayRoute` discovers and latches that
endpoint at runtime. No identity or driver-code checks were removed to accept it.

The production VBIOS parser found FWSEC-FRTS at offset `0x434e0`, size `0xe700`,
with three signatures and signature-version mask `7`.

Recorded host validation includes:

- 575 console replay checks against a capture with 489 handles / 126 active records.
- 112 production-core/compatibility/UEFI transaction scenarios with hardware effects simulated.
- 4,604 ASRock release checks, including injected failures.
- 197 read-only cold pre-check cases.
- Fourteen passing host jobs covering manual/automatic/headless USB application paths,
  PCI checks, port/write locks, master attributes, selector locks, release and full flow.
- Freestanding EFI compiler/PE validation and rejection of non-USB or mixed-profile builds.

These are software checks, not fourteen hardware boots. One successful hardware
transaction and Windows return were subsequently recorded. See
[public evidence](../evidence/asrock-b450m-3060/README.md).

### Integration checks, 2026-10-29

Against integrated upstream `23cc028512acbb873cff8de38acdd63c66c98ea9`:

- All 14 source-bound ASRock host jobs passed, including 575 console replay checks,
  112 transaction scenarios, 4,624 release checks and 207 cold pre-check cases.
- Ten family/profile test methods and 37 diagnostic CLI/build/resume cases passed,
  including ASRock bus-7 admission and wrong/missing-bus refusals.
- Eight Python regression suites passed 84 test methods. Native/Python GPU-path
  comparison passed 5,078 native checks and 10,024 matching decisions.
- Ten synthetic aggregate LOCAL configurations passed preparation, release,
  cold-state and foreign-profile refusal checks.
- Both USB child and wrapper compiled and passed PE verification with synthetic
  partition services and a dummy Windows ESP GUID. These artifacts must not be
  booted or distributed as a usable package.

All these checks are host simulations/builds. No GPU transaction, new Windows
handoff, lifecycle validation or benchmark was performed on this integrated revision.

## Reproduction boundaries

This is a source/research publication, not a turnkey installation.
Obtain the five assets using the pinned upstream process in [ASSETS.en.md](ASSETS.en.md).
The local adapter expects the firmware source checkout at commit
`0ebdb1c6308b6a2072be5bf04513472942541956` in `firmware-source/`.
Private captures, PROM, generated partition configuration, compiler downloads and
test evidence are intentionally absent from the publication package.

The development scripts expect local `research/results/asrock-targeted-*` and
`asrock-targeted-second-*` captures and derived fingerprint/FWSEC/candidate reports.
Another user must collect and verify their own fixtures. Matching board and GPU
names do not authorize reusing this profile or substituting its hashes.

Once the exact local inputs exist, the host test entry point is:

```powershell
.\research\profile\test-asrock-host.ps1 -Clang 'C:\TOOLS\llvm-mingw\bin\clang.exe'
```

The recorded firmware build used official clang/lld-link 20.1.8. Host tests used
LLVM-MinGW `20261020` UCRT x86-64; the firmware assets matched all five upstream
sizes and SHA-256 values. The source-only build entry point is:

```powershell
.\research\headless\build-usb.ps1 `
  -WindowsEspGuid 'YOUR-OWN-WINDOWS-ESP-GUID' `
  -HardwareProfile asrock-b450m-3060 `
  -Clang 'C:\TOOLS\LLVM20\bin\clang.exe' `
  -LldLink 'C:\TOOLS\LLVM20\bin\lld-link.exe'
```

It refuses missing/stale host evidence. Build and staging must use the same source
tree as the tests. Never copy someone else's partition-specific EFI executable.
The upstream staging script was run with Windows PowerShell 5.1: its strict
manifest integer checks reject PowerShell 7's Int64 JSON parsing. That compatibility
restriction remains documented rather than silently bypassed.

## Recovery and limits

Use a one-off firmware boot-menu choice; no persistent boot-order change was made.
If Windows/display recovery fails, fully power off, remove the USB and choose
Windows Boot Manager. Initial wrapper Esc skips the unlock; the child's later Esc
window is a different stage and is not a promise to restore the display.

The evidence does not establish long-term stability, repeated cold-boot reliability,
restart/sleep/hibernate handling, general RTX 3060 compatibility or performance in
games, ComfyUI and arbitrary LLM workloads. The observed Ollama greeting tests were
too short and uncontrolled to establish an application-level unlock speedup.

## Attribution and distribution

Keep [LICENSE](../LICENSE), [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md),
`LICENSES/` and upstream history/attribution. The inherited unlock/port code is
GPL-2.0-only; EDK2 and other components retain their own notices. This port does
not grant redistribution rights for NVIDIA firmware. The prepared publication
contains source and selected textual/numeric evidence only, not firmware or EFI
binaries. Preserve upstream attribution and component-specific license notices.
