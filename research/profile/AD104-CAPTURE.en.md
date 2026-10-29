# MSI AD104 capture and a diagnostic MBR Windows handoff

English | [한국어](AD104-CAPTURE.ko.md)

Date: 2026-10-26. This adds a **capture-only** option for PCI identity
`10de:2782`, subsystem `1462:5132`, class/revision `030000a1`, and chip ID
`194000a1`. It does not add an unlock profile, widen `proven.json`, or make the
GA102 profile generator accept AD104 captures.

## The two preparation gaps

The generic diagnostic previously confined deep reads to reviewed 3090/3090 Ti
identities and bus 1. The added `-Ad104Msi` switch selects a separate read-only
capture implementation. It discovers a legal PCI location and checks that it
stays unchanged; Windows bus numbering is not substituted for a firmware capture.

The local Windows installation reports UEFI boot and EFI loader paths, but its
system boot partition is MBR/NTFS rather than a GPT ESP. `-WindowsMbr` identifies
that current system boot partition without converting, mounting, formatting or
modifying it. This option is confined to the diagnostic build.

These changes remove software preparation assumptions. The first physical
diagnostic boot on 2026-10-26 produced a partial capture and refused AD104
MMIO/PROM reads because bus mastering was enabled. See
[the first-boot findings](AD104-FIRST-BOOT.en.md) and the accompanying
[sanitized evidence](ad104-first-boot-evidence.json). Automatic Windows return
and complete GPU capture are not established by that log.

## AD104 read admission

Before MMIO, the probe requires exactly one NVIDIA display controller, exact
PCI/subsystem/revision values, memory decoding enabled, bus mastering disabled,
and a 32-bit aligned 16 MiB BAR0 with a matching resource descriptor. It does not
enable decoding or bus mastering. It latches the device handle, interface and
BDF, then rechecks all 256 PCI configuration bytes after allocation and resource
callbacks. Invalid segment/bus/device/function ranges are rejected.

The first device read must match the AD104 chip ID. The remaining fixed register
list follows the separately published AD104 diagnostic at
[gpu-unlock-ada commit 4e7fefcc](https://github.com/JustAResearcher/gpu-unlock-ada/blob/4e7fefcc4923f9e083d5324dc75a7d2f906af77f/windows/ad104-msi/diagnostic/src/sample.c).
It reads 22 register values in total and a bounded 1 MiB PROM window. Register
errors/sentinels stop the capture. PROM signature agreement, a final chip read,
and unchanged handle/interface/BDF/configuration are required before publishing
the complete observation. Short writes, existing dumps and file I/O failures
invalidate capture completion. Raw captures remain private.

Neither metadata nor complete capture authorizes a transaction. MMIO reads and
firmware callbacks still require a reviewed physical test; a diagnostic label
is not a guarantee against platform-specific side effects.

## MBR boot-manager identification

The build requires one current `IsSystem` partition on a non-USB MBR disk with
a nonzero disk signature, an active type-7 partition, and 512-byte logical
sectors. It records the partition number, start LBA and size, plus the actual
system boot volume's `EFI/Microsoft/Boot/bootmgfw.efi` size and SHA-256. The
system boot partition is distinguished from the Windows OS partition.

At runtime, exactly one filesystem device path must match that MBR signature
and extent. GPT signatures, USB/USB Class/USB WWID ancestry, repeated partition
nodes, malformed paths, extent overflow and ambiguous matches are rejected.
The disk signature is identification metadata, not authentication.

The diagnostic reads the fixed boot-manager path, validates the bounded file
metadata, closes the file/root, verifies its SHA-256, rechecks the volume path,
and passes those same verified bytes to `LoadImage`. It does not hash one file
and then reopen a different file for loading. Existing GPT selection remains
the default. An MBR diagnostic configuration cannot be used for unlock/release
builds. A Windows update changing the pinned image requires a new build.

The boot volume must actually be exposed by firmware through Simple File System
when booting the USB. Its availability from Windows does not prove that. A
missing volume or failed load remains a physical-test limitation; the diagnostic
does not install an NTFS driver or promise automatic recovery. The existing
observation-only USB failure path returns control to firmware; subsequent boot
behaviour must be verified on the machine.

## Local preparation

Build only, from a suitable elevated Windows PowerShell when access to the
current system boot volume requires it:

```powershell
.\research\profile\build-probe.ps1 -Ad104Msi -WindowsMbr `
  -Clang 'C:\TOOLS\LLVM\bin\clang.exe' `
  -LldLink 'C:\TOOLS\LLVM\bin\lld-link.exe'
```

Use installed trusted tool paths. No firmware assets are required for this
diagnostic. Generated output is
`internal-boot/boot/build/probe-ad104-msi/BOOTX64.EFI`; its adjacent manifest
records source hashes, local configuration hash and binary hash. The local
`platform-config.h` contains private partition/image pins and is ignored by Git.
Existing unrelated internal configuration is refused rather than overwritten.

The script does not copy an EFI to USB, edit boot variables, stage an unlock,
release drivers, change GPU state or reboot. Review the artifact and recovery
procedure before separately authorizing a user-present physical capture.
Preserve previous boot files when later preparing media.

Host-only checks:

```powershell
.\research\profile\test-capture-preparation.ps1 -VcVars 'C:\Path\to\vcvars64.bat'
.\internal-boot\boot\test-diagnostic.ps1 -VcVars 'C:\Path\to\vcvars64.bat'
```

## Verification recorded locally

- 148 RAM-only AD104 checks: exact admission, multiple targets, BDF bounds and
  drift, resource failures, every sampled register error/sentinel, partial PROM,
  changing PROM signature, final identity changes, and dump failures.
- 66 synthetic MBR volume/image checks: identity/extent mismatch, USB variants,
  malformed/repeated nodes, ambiguity, hash/length/metadata failures, close
  failures, allocation failure and the exact buffer supplied to mock `LoadImage`.
- Four default GPT matching regression checks.
- Sixteen incompatible/non-diagnostic compile combinations rejected.
- Existing diagnostic, four fixed-profile probe, checked GA106 and generic probe
  host suites passed before the final isolated AD104/MBR corrections; those
  corrections leave the legacy probe branch unchanged.
- A native AMD64 EFI application built with clang/lld 21.1.6 and passed the PE
  verifier (no imports/IAT/delay imports). Its local manifest marks hardware
  testing false. No generated EFI or proprietary boot image is committed.

The build manifest describes the artifact at build time; its hardware-testing
flag remains false and is not a live test ledger. Subsequently, the artifact
was staged on a separately authorized spare USB and booted by the user. The
first-boot report records that physical result separately from the host checks.
No unlock was attempted, and the capture admission guard remains unchanged.
