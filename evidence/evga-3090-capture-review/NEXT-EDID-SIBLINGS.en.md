# Next combined EFI: both EDID siblings

[English](NEXT-EDID-SIBLINGS.en.md) | [한국어](NEXT-EDID-SIBLINGS.ko.md)

[EVGA08](NINTH-EFI-RESULT.en.md) confirms four successful Stops and automatic Windows return. This build repeats those four stages, then invokes the same pinned IMG07 Stop(binding, sibling, 0, NULL) once on each of the two EDID-only siblings. Six Stops total, with fresh graph/code checks after each. The two new callbacks are combined in one attended boot. Failure or unexpected state stops continuation; no retry or rollback.

All three EDID handles must retain their exact captured device-path bytes: the common ACPI root/PCI parent and distinct ADR values 0x80010320 (physical GOP), 0x80010340 and 0x80010370 (siblings). Node headers are checked before payloads, with fixed 36-byte scope. The same handles and owner must remain throughout; each completed handle must have no active EDID open, and not-yet-stopped handles must retain the exact owner/count1. After both new Stops all three active EDID opens must be absent, with both GOPs and earlier console/interface invariants retained. The IMG07 version/image/Stop/.text pins from the physical test remain unchanged. No new executable image is called.

**New risk:** these are two new controller contexts for IMG07, including release of its last EDID ownership. The normal EDID-present path closes the selected open and clears a global flag; an internal query failure can reach alternate input cleanup and indirect graphics services. Earlier success on the physical controller does not qualify sibling or last-owner behavior. Display/input loss, firmware/Windows hang or failed GPU initialization may require power-off/USB removal. Recovery remains unproven, with no automatic restoration. No NVIDIA child/parent Stop, GPU reset, direct MMIO/PCI attribute change, DMA launch, firmware upload/execution or unlock is added. Choose whether to run after reading this; no separate acknowledgement required.

Update the same branch via GitHub Desktop, then:

```powershell
.\research\profile\build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:\Program Files\LLVM\bin\clang.exe" -LldLink "C:\Program Files\LLVM\bin\lld-link.exe" -ReleaseEdidSiblings
.\research\profile\stage-evga-text-stop.ps1 -Drive D
```

The switch includes all four previous stages. Staging requires reviewed EVGA05/06/07/08 hashes and current source/EFI/ESP/USB checks. New one-shot marker: EVGA-EDID-SIBLINGS.ARMED. Preserve all four old markers, logs and numbered backups. Stop on errors; never delete a marker to retry.

Full shutdown → one-time UEFI USB boot → fresh Shift+G once after Live guards passed. No Shift+V. The display after G is not progress feedback. If no Windows within three minutes: power off, remove USB, use normal Windows Boot Manager. Do not repeat. Send EVGA09/next log and automatic/manual Windows outcome plus build/staging output; saved-log analysis is done here.

Local verification: 74 host checks passed (25 membership, 7 text, 8 graphics, 12 physical EDID, 13 sibling and 9 capture), using mocked protocols/hashes/Stops. Sibling cases cover each callback failure, each missing release, earlier failure blocking both, wrong inventory/path, marker/hash refusal and final no-log verification. Staging syntax and five mode-to-marker selections pass. Warnings-as-errors build/PE passes: 25088 bytes SHA256 476c6564ad62e0e31c2bd6181f19d2a55d4418ef7e40219afb7ffa05e8e42a0e, AMD64/no imports/NX/dynamic base. Built only, not staged/run locally; contributor toolchain output may differ. No hardware success for this new mode or unlock is claimed.
