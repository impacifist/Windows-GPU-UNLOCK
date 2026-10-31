# EVGA internal boot: read-only storage inventory

[English](INTERNAL-STORAGE-INVENTORY.en.md) | [한국어](INTERNAL-STORAGE-INVENTORY.ko.md)

The validated EVGA profile currently uses a USB. The separate internal launcher belongs to a different machine and cannot be used unchanged. Before designing an EVGA internal path, this inventory identifies storage layout and possible existing dedicated EFI partitions without changing boot settings, mounting partitions, formatting or writing to disks.

From the updated branch's repository root, run:

```powershell
powershell -NoProfile -File ./research/profile/inspect-internal-storage.ps1
```

The JSON contains disk numbers, bus types, partition sizes/types, drive letters and file systems. It omits disk serial numbers and partition GUIDs; check it before sharing. Send the output or the error. A `candidate_dedicated_esp` is only a possible existing non-USB FAT32 GPT EFI partition, at least 1 GiB, with a drive letter, that Windows does not mark system or boot. It does not certify that the partition is unused, safe to write, firmware-accessible, or suitable for the EVGA profile. An EFI partition without a drive letter will not be identified as a candidate by this check. No candidate does not authorize partition changes.

Local verification: mocked system/dedicated/USB examples and read-only inventory on the research machine passed. There is no live EVGA internal boot implementation or installation in this step.

Movian's supplied inventory reports one Windows system EFI partition of approximately 10.9 GiB, but no separate candidate meeting the current criteria. The system EFI has no drive letter or reported filesystem in this inventory, so its contents and free space remain unknown. The Linux data partition is not an EFI candidate. The current separate-partition internal boot code refuses a launch when its own volume is also the Windows partition, and the EVGA build currently requires USB media. The size of the system partition alone does not make it available for this profile; no partition change or installation is implied.

Movian reports that the ESP was expanded for dual-boot Windows and CachyOS and prefers a separate entry on the existing ESP. A shared-ESP route would need an explicit EVGA-specific design: pin and verify the actual loaded partition, keep launcher, assets, captures and markers in distinct paths, avoid overwriting existing Windows/Linux files, and verify the selected handoff and boot-entry recovery without assuming that existing separate-ESP checks cover this topology. The current code preloads the pinned Windows Boot Manager; that does not by itself preserve selection of CachyOS after the unlock. The earlier EFI partition inventory and one USB success do not establish free space, contents, firmware policy, shared-ESP safety or unattended operation. No repartitioning, installation, firmware boot-entry change or hardware test has been performed for this route.
