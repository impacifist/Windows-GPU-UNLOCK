# Next EFI: NVIDIA PCI-parent Stop

[English](NEXT-NVIDIA-PARENT.en.md) | [한국어](NEXT-NVIDIA-PARENT.ko.md)

[EVGA11](TWELFTH-EFI-RESULT.en.md) is a great child-release result: exact three-child Stop success, passing postchecks, three handles removed, one virtual GOP/no EDID-discovered handles, retained NVIDIA PCI ownership and reported automatic Windows return after a longer delay. Unlock remains unproven. The locked-state benchmark is required after release testing and before step4's unlock attempt, followed by an identical post-attempt workload/settings comparison.

This mode repeats all seven completed callbacks and preflight, then directly calls IMG45 Stop(binding, PCI parent, 0, NULL) once. Fresh child-post checks immediately before it require no physical/EDID children, parent-path active opens absent, exact code/PCI owner, private stored child NULL and flag98=0. The NVIDIA zero-child path at0x6e1 queries its private parent object, conditionally calls an object method at0x731 if flag98 is nonzero, uninstalls the parent-private protocol, then calls0x135c. That helper frees an allocation, calls0x113c's indirect destructors for objects at private offsets0x48/0x40/0x38, closes PCI ownership and frees parent storage. We inspect static paths, not full indirect/service behavior. The observed flag0 skips the conditional method at0x731; it does not make the rest of cleanup harmless. No captured firmware is executed locally.

Postchecks retain prior virtual GOP/system-console/child-absence requirements but now demand NO active PCI owner and NO parent-private protocol in fresh ProtocolsPerHandle membership. Do not query/dereference the freed parent object. The NVIDIA binding/code must remain loaded/pinned. Read the already-used first16 PCI config DWORDs, require exact GPU/subsystem/class identity and log the command word. Do not assume decoding remains enabled or force a command value. Final no-log postchecks repeat after files close before preloaded Windows. Callback/postcheck failures hold; no retry, reconnect, decoding enable, GPU reset or automatic rollback.

**New risk:** this reaches NVIDIA's broader object/resource destructors and PCI ownership close. Indirect methods may change GPU state, decoding or mappings; complete hardware effects remain unqualified. Display/input loss, firmware hang, partial cleanup or failed Windows/GPU initialization may require manual power-off/USB removal. Recovery remains unproven, with no automatic restoration. The harness adds no direct MMIO/PCI attribute write, DMA launch, explicit reset, firmware upload/execution or unlock. This is not a guarantee against indirect callback effects. Choose whether to run after reading this; no separate acknowledgement needed.

Update the same branch via GitHub Desktop, then:

```powershell
.\research\profile\build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:\Program Files\LLVM\bin\clang.exe" -LldLink "C:\Program Files\LLVM\bin\lld-link.exe" -StopNvidiaParent
.\research\profile\stage-evga-text-stop.ps1 -Drive D
```

New marker EVGA-NVIDIA-PARENT.ARMED; preserve all seven older markers/logs/numbered backups. Staging requires exact reviewed EVGA05..11 plus current source/EFI/ESP/USB pins. Stop on errors; do not bypass or delete markers.

Full shutdown → one-time UEFI USB → fresh Shift+G once after Live guards passed. No Shift+V. After G the screen is not a progress indicator. If no Windows within three minutes: power off/remove USB/normal Windows Boot Manager. Do not repeat. Send EVGA12/next log, build/staging output and automatic/manual Windows outcome. Saved-log checks are done here. Benchmark remains a required gate before any future unlock transaction.

Validation:107 host cases pass (prior96+11 parent cases), mocked protocols/hashes/Stops; no captured firmware executed. Cases include success/final no-log, parent callback failure, retained PCI owner/private protocol, earlier child failure blocking parent, marker/hash refusal and final PCI identity drift. EFI Werror/PE pass:28672bytes SHA256aae2237f5463ad11133f154f3d3a07d9029d29f000312d4a530d6f210fe4acad, AMD64/no imports/NX/dynamic base. Staging syntax/new-marker and stageddiff checks pass. Built only, not staged/run locally; contributor toolchain hash may differ. New parent path hardware untested; no unlock proof.
