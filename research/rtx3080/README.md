# RTX 3080 research: X570-F GAMING / Ryzen 7 3700X

**Status: Windows inventory and read-only display routing; unlock support unestablished.**
This contribution records a distinct Windows desktop configuration and an active
display-path observation. No experimental EFI build or boot, or unlock was
performed for this contribution. The separate ordinary CUDA baseline has now
been measured and published in
[PR #45](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/45).

The [next decision and missing identification](SUPPORT-REVIEW.md#next-decision-and-missing-identification)
are now explicit: these contributions provide evidence, and do not themselves
implement or enable an RTX 3080 unlock.

Collected on **2026-10-26 (UTC)** against upstream
[`5c98f11eaafdcb7a2a06fb14fcdcde7297eb409e`](https://github.com/impacifist/Windows-GPU-UNLOCK/tree/5c98f11eaafdcb7a2a06fb14fcdcde7297eb409e).
The evidence belongs to this configuration; related contributors' observations
are not test results for this machine.

## Evidence files

- [inventory.json](inventory.json): reviewed output of the unchanged
  `tools/collect_compatibility.py` collector, preserving its schema and assessment.
- [observations.json](observations.json): source revision, collector/test hashes,
  selected NVIDIA metadata, commands/results and the original physical-detail
  snapshot.
- [FOLLOW-UP.md](FOLLOW-UP.md): primary-source hardware findings, evidence limits,
  build readiness and additional offline validation.
- [BENCHMARK-PLAN.md](BENCHMARK-PLAN.md): proposed ordinary CUDA baseline, including
  numerical validation, raw samples, provenance and resource budgets.
- [research-validation.json](research-validation.json): sanitized follow-up
  observations, host-test outcomes and source hashes.
- [display-topology/README.md](display-topology/README.md): direct Windows
  display-to-adapter association, reviewed read-only collector and observation.
- [SUPPORT-REVIEW.md](SUPPORT-REVIEW.md): refreshed upstream/report assessment
  and the boundary between ordinary CUDA results and unlock compatibility.

The collector completed with exit code 0 and no missing sections. Its
`assessment.status: not-assessed` and `can_unlock: false` values are intentionally
preserved: they are conservative collector outputs, not proof that an unlock is
technically impossible.

## Observed Windows inventory

| Field | Observation | Source |
|---|---|---|
| System class | Desktop (`PCSystemType=1`) | Windows CIM |
| GPU name | NVIDIA GeForce RTX 3080 | CIM and NVIDIA metadata |
| VRAM | 10240 MiB (10 GiB) | NVIDIA metadata |
| PCI vendor/device | `10de:2216` | Sanitized collector and NVIDIA metadata |
| PCI subsystem vendor/device | `1458:403f` | Sanitized collector and NVIDIA metadata |
| Motherboard | ASUSTeK COMPUTER INC. ROG STRIX X570-F GAMING | Collector |
| Motherboard BIOS | American Megatrends Inc., 5043 | Collector |
| CPU | AMD Ryzen 7 3700X 8-Core Processor | Collector |
| Windows | Microsoft Windows 11 Pro, 10.0.26200, 64-bit | Collector |
| Windows driver version | `32.0.16.1088` | Collector |
| NVIDIA driver version | `610.88` | NVIDIA metadata |
| Reported VBIOS version | `94.02.71.40.c4` | NVIDIA metadata |

The two driver strings use different numbering conventions. The VBIOS value is
a driver-reported version string; no firmware image was dumped. Software PCI
identity does not establish the physical card model, PCB or cross-flash history.
The collector enumerated one display adapter; that does not establish the
availability of disabled, disconnected or spare adapters.

## Active Windows display follow-up

A read-only query on 2026-10-26 at 14:46:54Z returned one active, available
external DisplayPort path. Windows' source and target adapter identifiers
resolved to the same DXGI adapter, reporting `10de:2216` / `1458:403f`.
This supplies a direct Windows routing observation for the inventoried GPU.
The source compiled with warnings treated as errors, and collection completed
with exit 0 in approximately 0.437 seconds under a 10-second deadline.

See the [display-path record and method](display-topology/README.md).
The query does not identify a specific physical socket or cable chain, prove
the absence of a spare GPU, or establish pre-boot firmware topology.

## Comparison with current documentation

At the inspected revision, the [README GPU support table](../../README.md#gpu-support)
lists no dedicated RTX 3080 profile. The five configurations in
[COMPATIBILITY.en.md](../../docs/COMPATIBILITY.en.md) cover RTX 3090 FE,
RTX 3070 Ti, RTX 3090 Ti and two RTX 3060 variants. None describes this
RTX 3080 / X570-F / BIOS 5043 / Ryzen 3700X combination.

[Existing RTX 3080 candidate notes](../RTX3080-CANDIDATES.ko.md) already discuss
other contributors' systems. This inventory adds a separate AMD X570
configuration; it does not promote those reports or this machine to supported
status.

| Related report | Relevant overlap | Evidence boundary |
|---|---|---|
| [Issue #13](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/13) | Same reported `10de:2216` and `1458:403f`; ASUS PRIME X570-P, BIOS 3603, Ryzen 9 5900X | Original report is inventory only. Different board, BIOS and CPU; identical reported GPU IDs do not prove the same physical card. |
| [Issue #28](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/28) | Same GPU device ID; Maxsun subsystem `7377:2000`, B760M and i5-12490F | Contributor reports a local experiment on a different system. Its performance and recovery observations are not evidence for this machine. |
| [Issue #20](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/20) | RTX 3080 FE with different device `10de:2206`; ASUS Z390 and i9-9900KF | Diagnostic/measurement report for another card and platform; not a supported profile for this inventory. |

These are links to related work, not borrowed hardware evidence.

## What was run

From the repository root, using Python 3.11.9:

```powershell
py -3.11 .\tools\collect_compatibility.py
py -3.11 -m unittest discover -s tools -p test_collect_compatibility.py -v
Get-CimInstance Win32_ComputerSystem -Property PCSystemType | Select-Object PCSystemType
nvidia-smi.exe --query-gpu=name,pci.bus_id,pci.device_id,pci.sub_device_id,memory.total,driver_version,vbios_version --format=csv,noheader
```

- Inventory: exit 0, all selected sections collected.
- NVIDIA metadata query: exit 0; selected identification fields only.
- Existing collector suite: **9 tests passed**, exit 0. These test parsing,
  privacy-field omission, incomplete-input handling and mocked collection
  failures. They do not exercise the GPU or validate an EFI profile.
- Source review: contributor guidance, compatibility table, RTX 3080 candidate
  notes and the benchmark guide at the pinned revision.
- Follow-up: **28 offline benchmark-comparison tests passed**. The separate
  mocked PowerShell runner suite stopped on an access-denied error before its
  first runner invocation; it is not a passing result. See
  [the detailed validation record](FOLLOW-UP.md#offline-validation-actually-performed).

The original compatibility collector, existing firmware/profile sources,
project licences and upstream credits are unchanged. The display follow-up
adds a standalone read-only Windows collector; it adds no boot or unlock support.

## Remaining information and useful next contribution

The following are deliberately **unknown**, pending owner confirmation:

- Physical card manufacturer/model and any VBIOS cross-flash history.
- Exact physical socket and any intermediate adapters/switches; the current
  Windows observation establishes an external DisplayPort target.
- Availability of another installed or spare display GPU.

The integrated-graphics question is now resolved from vendor specifications:
the inventoried Ryzen 7 3700X has no iGPU, so this motherboard's HDMI/DisplayPort
sockets do not provide an integrated display fallback with that CPU. The
[follow-up sources and interpretation](FOLLOW-UP.md#hardware-conclusions-from-primary-sources)
supplement the unchanged original collection record.

No pre-boot compatibility, boot recovery, sleep/resume or performance claim can
be made from this inventory. Read-only Windows metadata is the only hardware
observation in this contribution.

The separate ordinary CUDA contribution in
[PR #45](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/45) now provides
a measured baseline on this PC. Its
[completed validation report](https://github.com/danhol86/Windows-GPU-UNLOCK/blob/a04471a8c2cec047c25383315d122f1d6513462c/benchmark/rtx3080/NATIVE-VALIDATION.md)
records three successful processes, four precision modes and the explicit
desktop entry profile. The [original proposal](BENCHMARK-PLAN.md) is retained
as design history. Those ordinary compute results establish neither unlock
support nor a before/after performance gain.

The immediate next step is to fill in the physical details and let maintainers
review this independent inventory alongside existing reports. No experimental
boot is needed to complete an inventory report.

## Sharing and provenance

The collector's sanitized fields were reviewed before inclusion. No hostname,
username, serial number, device UUID, PnP device-instance suffix, personal
filesystem path, credential, recovery key, raw firmware/capture or downloaded
binary is included. Relative paths above are repository paths.

The hashes in `observations.json` identify the local collector and test source
bytes used on Windows. They help track provenance; they are not signed hardware
attestation or evidence of compatibility.
