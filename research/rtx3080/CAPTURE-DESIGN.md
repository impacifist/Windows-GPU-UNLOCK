# RTX 3080 10 GiB X570-F capture design

Status: source/design and host tests only. This document does not authorize or
perform a firmware boot, unlock transaction, GPU register write, driver release,
DMA operation, reset, boot-variable change or boot-media staging.

Inspected upstream revision:
`38b36297fc1fcf4ecca40c5c7e818f0cb7c94b44`.

## Target already established in merged research

The merged `research/rtx3080` inventory describes the target as an RTX 3080
10 GiB with PCI identity `10de:2216`, subsystem `1458:403f`, on ASUS ROG STRIX
X570-F GAMING BIOS 5043 with Ryzen 7 3700X. Windows observes the GPU at
`0000:07:00.0` and the active display path on that adapter. Those Windows
observations do not establish the UEFI bus, PROM, BAR layout or console graph.

## Why a capture is now a reasonable next test

Two independent public hardware reports narrow the uncertainty without proving
this machine:

- Issue #28 / PR #72 reports one successful cold-boot transaction on another
  RTX 3080 10 GiB with the same `10de:2216` device identity and captured GA102
  revision `b72000a1`. Its subsystem, motherboard, firmware fingerprints and
  console topology differ and must not be copied here.
- Issue #73 reports one clean cold-boot transaction for a different Gigabyte
  RTX 3080 on an AMD, no-iGPU host. Its GPU is the 12 GiB `220a` variant and its
  platform constants also do not apply here.

Together these reports establish that the shared GA102 transaction has worked
on RTX 3080 hardware and on an AMD/no-iGPU platform. They do not establish this
X570-F/card combination.

## Capture-only source boundary

Current main deliberately separates observation from write eligibility:

- `internal-boot/boot/probe.c` permits bounded deep reads only for explicitly
  admitted identities and compiles under `GU_DIAGNOSTIC_ONLY` /
  `GU_DIAGNOSTIC_PROBE` / `GU_PROBE_GENERIC`.
- `research/profile/proven.json`, `research/profile/make-profile.py` and
  `internal-boot/unlock/gpu-family.h` independently gate automatic local
  profile generation and write-enabled family/card combinations.

This branch adds only the exact `221610de / 403f1458` pair to the generic
probe's read-only deep-capture identity list. A different `2216` subsystem
remains inventory-only. It does **not** add `2216` to `proven.json` or the
GA102 write catalog. A compile-only regression explicitly requires GA102 +
`221610de` + `0x2800` MiB to remain refused by `gpu-family.h`.

The read-only probe still requires the existing envelope before deep reads:
display class/revision `030000a1`, PCI IO/MEM enabled with bus mastering clear,
a 16 MiB 32-bit BAR0 descriptor matching PCI config, stable PCI identity/config
before the bounded reads, and the existing segment/device/function constraints.
Unknown NVIDIA identities remain inventory-only.

## Required physical evidence

The useful evidence is **two complete captures from separate full shutdowns**.
They must agree on all values that the local-profile pipeline treats as stable.
For this machine the review should cover at least:

1. Firmware-selected PCI segment/bus/device/function and root-port relationship.
   The firmware bus must be compared with, not assumed from, Windows
   `0000:07:00.0`.
2. Full 256-byte PCI configuration snapshot, capability walk, BAR0 resource
   descriptor and cached `EFI_PCI_IO_PROTOCOL` attributes.
3. Cold-state register snapshot collected by the existing bounded probe reads.
4. PROM identity/hash and the captured PROM image needed for offline FWSEC-FRTS
   location. Raw PROM bytes remain local and must not be committed.
5. Complete GOP/ConOut/EDID/driver ownership graph and loaded driver-image
   fingerprints needed to distinguish the X570-F firmware release path from the
   Maxsun B760M and other reviewed machines.
6. Evidence that each capture completed without a changed inventory, unexpected
   bus mastering, malformed BAR descriptor or partial capture.

If the two cold captures disagree, the profile is not generated and no mutation
test should be attempted.

## Review gate before any boot

Before executing the diagnostic, record and review:

- exact source commit and `BOOTX64.EFI` SHA-256;
- manifest flags showing `CanUnlock=false`, `CanDetach=false`,
  `WritesFirmwareVariables=false`, `HardwareTested=false`;
- removable-media layout and the files the probe may create;
- the attended recovery procedure if the diagnostic does not hand off to
  Windows normally;
- physical display cabling and whether a separate spare display GPU is
  available.

The first physical operation, if separately approved, is observation only. It
does not authorize an unlock transaction.

## Decision after capture

Run `research/profile/check-compatibility.py` on the private captures and review
its result. `2216` remains outside the automatic write-profile envelope on this
branch, so a complete capture can still correctly report `outside_scope`.

Only captured values from this machine may then be used to propose an
exact-machine profile. The Maxsun PR's BAR, subsystem, PROM, firmware hashes,
console allowances and paths are comparison evidence, not reusable constants.
Before any later mutation, host tests must cover wrong subsystem/revision,
PROM/firmware mismatch, cold-state mismatch, PCI/BAR drift, ownership/release
failure, cleanup failure, and Windows handoff behavior.

## What would count as proof on this machine

A build, a matching GPU marketing name, or a successful capture is not proof of
unlock support. A later claim requires a separately reviewed exact-machine
profile, an attended transaction with recorded state/cleanup and successful
Windows recovery, followed by repeated matched before/after computation with
the existing numerical checks and an unchanged control.
