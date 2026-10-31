# Next EFI: combined IMG13 membership and IMG16 text release

[English](NEXT-TEXT-RELEASE.en.md) | [한국어](NEXT-TEXT-RELEASE.ko.md)

Built locally; not staged or hardware tested. This combines the successful EVGA05 first Stop with one new driver Stop in the same boot. Unlock remains unproven.

## Reviewed change

IMG13's extra protocol removal is consistent with its statically reviewed UninstallMultipleProtocolInterfaces call using GUID d3b36f2c-d551-11d4-9a46-0090273fc14d. EVGA05 directly observed that removal. The combined test requires that GUID absent before the second Stop; it does not infer that no other indirect state changed.

The captured IMG16 Stop at RVA 0xb2c has no nested local-helper calls on the inspected path. It obtains the controller's text interface, closes a text reference, uninstalls SimpleTextOut plus e1e4a857-c970-4075-a4da-e9c41b69adfc, branches on uninstall failure, closes GOP and frees three allocations. The service-table global at RVA0x3820 was previously traced to BootServices initialization. Individual CloseProtocol/FreePool statuses are not all checked by the OEM routine, so fresh state verification remains required. It does not branch on child count in this path; the harness passes zero children and NULL.

## Exact sequence

1. Complete before-capture, exact EVGA identity, two-GOP shape, console membership, owner and executable hash guards; fresh Shift+G.
2. Flush a new EVGA-TEXT-RELEASE.ARMED marker. Preserve the old EVGA-TEXT-STOP.ARMED marker; it is neither deleted nor reused.
3. Repeat the proven single IMG13 child Stop, check the selected text opens are absent and both GOPs/other selected owners/code pins remain.
4. Require the observed removed GUID absent and IMG16's two text-interface GUIDs present on the virtual GOP. Directly call pinned IMG16 Stop(binding, virtualGop, 0, NULL) once.
5. Require its GOP active open absent, both text-interface GUIDs absent, both original GOP handles/shape and physical GOP ownership retained, system console text present and code pins unchanged. No stale freed text interface is dereferenced.
6. Complete after-capture, close files, repeat final verification without logging and attempt preloaded Windows Boot Manager. A failed check after either Stop holds; no retry/rollback/reset is added.

Only these two Stops are included. No IMG17, EDID or NVIDIA Stop; no new GPU MMIO/PCI attribute writes, reset, firmware upload/execution or unlock. PCI config inventory reads remain. Driver callbacks can have indirect effects.

## Risk and operation

The new IMG16 call uninstalls interfaces and frees driver allocations. Display/input may disappear; firmware or Windows may hang or fail GPU initialization. Manual power-off/USB removal may be needed; recovery is unproven and there is no automatic restoration. Read this and choose whether to run; no separate confirmation message is required.

Update the same branch through GitHub Desktop. From the repo root:

```powershell
.\research\profile\build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:\Program Files\LLVM\bin\clang.exe" -LldLink "C:\Program Files\LLVM\bin\lld-link.exe" -ReleaseVirtualText
.\research\profile\stage-evga-text-stop.ps1 -Drive D
```

The stage helper requires the reviewed EVGA05 hash, checks this build's mode/source/hash/ESP, preserves backups/captures and refuses an existing new-stage marker. Do not bypass any failure. The old marker remains valid evidence and does not prevent this distinct experiment.

Save work, full shutdown, one-time UEFI USB boot. Fresh Shift+G once only after Live guards passed; no Shift+V. After G, display is not a progress indicator. If Windows has not started within three minutes, power off, remove USB and use normal Windows Boot Manager. Preserve the new marker; do not repeat. Send the next EVGAnn.TXT (expected EVGA06), build/stage output and automatic/manual Windows outcome. No duplicate offline replay is needed; we can inspect the supplied files locally.

## Local verification

25 original membership checkpoints, seven combined-mode checkpoints and nine capture regressions passed. Combined cases cover success/final no-log verification, second Stop failure, missing post-state transition, first Stop failure preventing the second, existing marker and bad code hash. EFI warnings-as-errors build and PE verification passed: 22528 bytes, SHA25633a2564514d2c6867746162aefd73e1ff40cb19a5670068843bd303273d21868, AMD64 EFI, no imports/IAT/delay imports, NX/dynamic base. These are mocked/build checks, not execution of captured firmware or hardware proof. Contributor toolchain hashes may differ; staging must match his own build.
