# Next EFI: one NVIDIA three-child Stop

[English](NEXT-NVIDIA-CHILDREN.en.md) | [한국어](NEXT-NVIDIA-CHILDREN.ko.md)

[EVGA10](ELEVENTH-EFI-RESULT.en.md) validates exact NVIDIA code/ownership and a stable stored physical-GOP child with parent flag98=0 before/after six consumer Stops. The new mode repeats that sequence and preflight, then calls IMG45 Stop(binding, PCI parent, 3, children) exactly once. Child order is physical GOP then the two verified EDID-only siblings. No NVIDIA zero-child/parent Stop follows; PCI ownership is retained. No unlock is attempted.

## Review and expected result

The nonzero-child branch at0x6b7 calls helper0x1620 and returns without entering the zero-child parent cleanup at0x6e1. Helper0x1620 can remove the stored GOP regardless of the requested list; its GOP cleanup0x3f0c frees optional buffers/object, EDID-active cleanup0x3088/0x303c uninstalls/frees, then0x13f0 removes each child's EDID-discovered interface and buffers before0x2894 closes the parent device-path child open, uninstalls the child path and frees path storage via0x259c. Failed uninstallation can reopen an edge or leave partial cleanup. These exact paths were inspected statically; not every driver behavior or firmware-service implementation is qualified. No captured firmware was executed locally.

Fresh preflight requires the observed class1/flag0 and exact code pin from EVGA10, all three same child paths/owners and prior consumer invariants. After the call require: only the original virtual GOP remains with zero framebuffer; no EDID-discovered handles; none of the original three child handles in the bounded handle inventory; no active parent-device-path opens; exact NVIDIA PCI owner/code still present; private stored child NULL/flag0; original system-console text and virtual text-interface absence retained. Repeat the postcheck with logging disabled after files close, before preloaded Windows. Do not query/dereference removed physical GOP/child interfaces. Any callback/postcheck failure holds; no retry, parent Stop, reset or automatic rollback.

**New risk:** the first NVIDIA Stop removes the sole physical GOP, EDID and device-path interfaces and frees driver-owned objects. Display/input may disappear; firmware may hang, service calls may fail partway, or Windows/GPU startup may fail. Manual power-off/USB removal may be needed; recovery remains unproven and there is no automatic restoration. The harness adds no direct GPU MMIO/PCI attribute write, DMA launch, reset, firmware upload/execution or unlock. This does not exclude indirect callback/service effects. Choose whether to run after reading this; no separate acknowledgement needed.

Update the same branch via GitHub Desktop, then:

```powershell
.\research\profile\build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:\Program Files\LLVM\bin\clang.exe" -LldLink "C:\Program Files\LLVM\bin\lld-link.exe" -StopNvidiaChildren
.\research\profile\stage-evga-text-stop.ps1 -Drive D
```

Staging requires reviewed EVGA05..10 hashes/current source/EFI/ESP/USB pins and preserves all older evidence/backups. New marker EVGA-NVIDIA-CHILDREN.ARMED; retain all six old markers. Stop on errors; do not delete markers to retry.

Full shutdown → one-time UEFI USB → fresh Shift+G once after Live guards passed. No Shift+V. Display loss after G is expected but is not progress proof. If no Windows within three minutes: power off/remove USB/normal Windows Boot Manager. Do not repeat. Send EVGA11/next log, build/stage output and automatic/manual Windows outcome. Offline checks stay here.

Local validation:96 host cases pass (85 existing +11 NVIDIA child cases) using mocked hashes/protocols/Stops. New cases cover one exact three-child call, success/final no-log check, callback failure, retained physical GOP rejection, earlier failure blocking NVIDIA, flag/owner/hash/marker refusal and final drift. An initial host compile typo was corrected before the passing run. EFI Werror/PE verification passes:28160bytes SHA256615b224e567684fa98765f0b2543ef200b3999a269023f892364753cef1bb29d, AMD64/no imports/NX/dynamic base. Staging syntax/new-marker selection and diff checks pass. Built only, not staged/run locally; contributor toolchain output may differ. New NVIDIA path is hardware untested.
