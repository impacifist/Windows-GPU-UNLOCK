# Next EFI: NVIDIA teardown preflight

[English](NEXT-NVIDIA-AUDIT.en.md) | [한국어](NEXT-NVIDIA-AUDIT.ko.md)

[EVGA09](TENTH-EFI-RESULT.en.md) is a great consumer-release result: six successful Stops, all three EDID active owners removed, both GOPs/all549 handles retained, two complete captures and operator-reported automatic Windows return. The consumer sequence is now demonstrated on this machine. NVIDIA child/PCI release and unlock remain unproven; this is a milestone, not a numerical prediction of unlock success.

## Why this next test is needed

The exact NVIDIA image is IMG45, not another machine's IMG29: loaded size0x2b700, binding version0x60009, Stop RVA0x698, executable section RVA0x280/length0x23d82/SHA256 d045cee2113e461c7c678ec21542b1d0f18356765c0fc6a0bf2dcf824088e011. Static inspection shows a nonzero child count calls helper0x1620 before returning. That helper first examines the parent-private stored child at offset0x88, can uninstall its GOP and invoke helper0x3f0c, then can run EDID-active cleanup0x3088 before iterating the requested child list through0x13f0. The latter uninstalls a private child protocol, frees allocations and tail-calls0x2894. Therefore requesting only an EDID sibling is not proof that the physical GOP remains untouched. Complete indirect/helper effects remain unqualified; no captured code was executed locally.

The new mode repeats the six completed Stops. Before G, after the six stages and during final no-log verification, it checks the exact NVIDIA PCI owner/code and three matching BY_CHILD parent-device-path opens. It queries the installed parent-private protocol fe47349a-7f0d-4641-822b-34baa28ecdd0, reads ONLY its stored handle at0x88 (8CPU bytes) and flag at0x98 (1CPU byte), and logs a class/flag, not addresses. Class0=null,1=physicalGOP,2=EDID-only sibling,4=unknown. Unknown classes/flags outside0..1 refuse; the raw handle and flag must stay identical. This establishes which stored-child path is present after consumer release and whether its flag changes. It does not establish safety of the future Stop. The saved captures do not contain these private fields, so this evidence cannot be produced by offline replay.

**New risk:** these are new CPU reads through a private driver interface; its allocation size is not exposed by UEFI. A bad/private pointer could fault or hang firmware. Existing six-callback display/input/Windows risks remain; manual power-off/USB removal may be needed and recovery is unproven. No NVIDIA Stop/private method, reset, GPU MMIO/PCI attribute write, DMA launch, firmware upload/execution or unlock is added. Choose whether to run after reading this; no separate acknowledgement needed.

Update the same branch via GitHub Desktop, then:

```powershell
.\research\profile\build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:\Program Files\LLVM\bin\clang.exe" -LldLink "C:\Program Files\LLVM\bin\lld-link.exe" -AuditNvidia
.\research\profile\stage-evga-text-stop.ps1 -Drive D
```

New marker EVGA-NVIDIA-AUDIT.ARMED; preserve all five older markers/logs/backups. Staging requires reviewed EVGA05 through EVGA09 and current source/EFI/ESP/USB identity. Stop on errors. Full shutdown → one-time UEFI USB → fresh Shift+G once after Live guards passed. No Shift+V. If it returns before G, send the log rather than racing keys. If no Windows within three minutes after G: power off/remove USB/normal Windows Boot Manager. Do not repeat or delete the marker. Send EVGA10/next log, build/stage output and automatic/manual Windows outcome. Offline analysis is done here.

Validation:85 host checks pass (prior74 plus11 NVIDIA audit checkpoints), mocked protocols/hashes/Stops. New cases cover exact owner/code/children, private interface refusal, unknown child/flag, changed flag at final verification, marker and earlier stage failure; no NVIDIA callback executed. Warnings-as-errors EFI build/PE verification pass:26112bytes SHA256 ac600b84cb64c1bda6ced86db87a07cd5ea82d4eb8441444502545593c59e358, AMD64/no imports/NX/dynamic base. Built only, not staged or run locally; contributor toolchain hash can differ. No hardware result for this audit is claimed.
