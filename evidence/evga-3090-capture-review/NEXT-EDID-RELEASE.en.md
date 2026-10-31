# Next combined EFI: release the physical GOP EDID consumer

[English](NEXT-EDID-RELEASE.en.md) | [한국어](NEXT-EDID-RELEASE.ko.md)

[EVGA07](EIGHTH-EFI-RESULT.en.md) completed IMG13, IMG16 and IMG17 Stops and automatic Windows return. This next build repeats those stages with their postchecks, then makes exactly one new IMG07 Stop(binding, physicalGop, 0, NULL). It does not stop the two EDID-only siblings or the NVIDIA driver. Unlock remains unproven.

## Exact scope and review

Before the first Stop, after every stage and before Windows, require the same three EDID-discovered handles, with one BY_DRIVER open/count1 on each and the same owner. After the new Stop require only the physical GOP's active EDID open absent; require both siblings' ownership retained. Keep the prior GOP/interface/system-console checks. Select the actual active owner, not a matching file GUID. Pin binding version1, loaded image0x9000, Stop RVA0x14d4 and .text RVA0x1000/length0x37da/SHA256 a015d52f55fa6441414458e2b699ba2edcb53f03b001ac6367e45bcd908c418e. The hash helper now accepts an explicit code RVA because this PE differs from the earlier TE images; their existing pins are unchanged.

Static inspection of captured IMG07 finds the EDID-discovered GET_PROTOCOL at0x1514, then on success a driver-handle CloseProtocol at0x153e and a stored flag clear, returning without the other cleanup helpers. A failed initial OpenProtocol can reach simple-pointer/absolute-pointer cleanup and helper0x3080, including an indirect graphics-interface call and another service call. A prior successful graph query does not prove the internal OpenProtocol cannot fail; full indirect behavior is not qualified. No captured code was executed locally. The exact protocol GUID comes from the repository's EdidDiscovered.h, not the separate c7a7030c system-console protocol.

**New risk:** this invokes a new OEM callback and changes EDID ownership/global driver state. Alternate cleanup paths may affect input/display and invoke graphics services. Display/input may disappear, firmware or Windows may hang, or GPU initialization may fail. Manual power-off/USB removal may be needed; recovery remains unproven and there is no automatic restoration. The harness adds no GPU reset, direct MMIO, PCI attributes change, DMA launch, firmware upload/execution or unlock. Driver callbacks may have indirect hardware effects. Choose whether to run after reading this; no separate acknowledgement is needed.

## Build and run

Update the same branch through GitHub Desktop, then from the repository root:

```powershell
.\research\profile\build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:\Program Files\LLVM\bin\clang.exe" -LldLink "C:\Program Files\LLVM\bin\lld-link.exe" -ReleaseEdid
.\research\profile\stage-evga-text-stop.ps1 -Drive D
```

ReleaseEdid includes all three earlier stages. Keep all three old markers, captures and numbered backups. The new marker is EVGA-EDID-RELEASE.ARMED; staging requires the exact reviewed EVGA05, EVGA06 and EVGA07 hashes and current source/EFI/ESP pins. The staging mode checks now use explicit array membership rather than scalar comparison against an array. Stop on errors; do not bypass checks or remove markers.

Full shutdown → one-time UEFI USB boot → fresh Shift+G once after Live guards passed. No Shift+V. After G the display is not a progress indicator. If Windows has not started within three minutes, power off, remove USB and use normal Windows Boot Manager. Preserve evidence and do not repeat. Send EVGA08 (or next EVGAnn), build/staging output and automatic/manual Windows outcome. Saved-log checks will be done here.

Local verification: 61 host checkpoints/capture cases passed using mocked protocols, hashes and Stops, including fourth-Stop failure, missing physical release, wrong EDID inventory/owner/version, earlier failure blocking continuation and final verification with logging disabled. EFI warnings-as-errors compilation and PE verification passed: 23552 bytes, SHA256 c0cfbe2a4911551e85a05e8740c2da49057833ae1f908c85393b41fa154aeba0, AMD64, no imports/IAT/delay imports, NX/dynamic base. Built only, not staged or hardware executed here; contributor toolchain hash may differ. This is a new hardware test, not unlock or general recovery validation.
