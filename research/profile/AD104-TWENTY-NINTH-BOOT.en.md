# Twenty-ninth candidate: guarded Windows recovery after one refused observation

[English](AD104-TWENTY-NINTH-BOOT.en.md) | [한국어](AD104-TWENTY-NINTH-BOOT.ko.md)

Built for review; user explicitly accepted the disclosed new failure-recovery risk and staging. Physical29 has NOT executed; test28 originals are preserved before verified staging. This changes handling of one optional read failure. It does not accept the failed register as valid or grant execution authority. The reviewed binary is staged only after USB/source/archive/hash verification.

## Offline finding

[NVIDIA gpu_access.c](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/main/src/nvidia/src/kernel/gpu/gpu_access.c) checks a PRI-error mask/code and dispatches a sanity-check handler for returned error-pattern data. This supports separating successful host/PCI read transport from usable device register data. Exact0xBADF5620 cause, protection level and harmlessness are NOT established by the searched public source. Prior25/28 captured returned reads followed by software guard holds; neither proves an MMIO-call hang. No attempt is made to alter privileges, clear errors or retry the refused register.

## Exact new branch

Repeat28. The original FalconObserve still returns EFI_NOT_READY on the sentinel. Only the FIRST optional batch's ninth read may qualify: offset0x111388, callbackstatus0, value exactly0xbadf5620, all nine statuses0, chip194000a1, FalconCPUCTL0x10 matching prior sample, and ENGINE0. All earlier values must have passed the existing checks. The original pinned Mem.Read and full PCI/ownership/input state must remain admitted after the partial-result log flush. Other addresses/values, all-ones, transport error, method drift or a second-batch refusal retain permanent hold.

Record original observation failure in DIAG and STATUS. Stop all remaining optional reads: no BCR_CTRL and no second optional batch. A separate one-shot recovery admission flushes intent, freshly verifies state, then makes two seven-DWORD REQUIRED-state batches:
`0, 0x110100, 0x1103c0, 0x82381c, 0x823820, 0x625f04, 0`.
Require both chip bookends194000a1, CPUCTL and selectors/VGA equal the prior sample, ENGINE0, original read pointer before/after every callback and fresh full state around all logging. No allocation/file/console callback occurs inside a read batch. Any further error or mismatch holds without decode-off or Windows.

Only after all14rechecks and a separately logged recovery admission, require a fresh Shift+V and the existing final-state checks. Then make the established ONE Attributes Disable(0x300), verify exact saved post-parent state, record decode-off outcome in STATUS and recheck fresh PCI/ownership state. No GPU MMIO after Disable. An observation remains incomplete/NOT_READY even if the recovery experiment becomes eligible for Windows; a successful recovery experiment is not successful core-state sampling. STATUS independently preserves refusal/recheck/decode-off records plus normal Windows markers. Witness write/flush/budget/ExitBootServices protections remain.

No new MMIO addresses, reset, core switch, start/stop, DMA launch, BME/VGA enable, firmware upload/execution, unlock, reconnect or automatic rollback. However, continuing after a rejected read is a NEW risk boundary: critical checks may not detect every hardware side effect. Display/input/Windows may fail or hang, manual power-off/USB removal may be needed, recovery remains unproven. Prior successful-path recovery approval does not authorize this branch; explicit approval for this exact branch was received.

Counts: normal complete observation path retains28additional reads/262348total. Exact refusal path uses9optional+14required=23additional/262343total, with no refused-register retry. Manifest separately flags CanRecoverKnownObservationRefusal=true, RecoveryRegisterReads14, RefusalPathAdditionalReads23; DMACTL/reset/firmwareexecution/unlock=false.

Host validation covers exact refusal and nonqualifying states, all14recovery transport failures, required-state/method/log/ownership/witness faults, freshV and pre/postDisable guards. Review caught logging-time read-pointer substitution; the helper now takes the ORIGINAL saved pointer, and a callable-reader substitution test proves zero recovery MMIO/Disable. Real witness-wrapper tests cover preserved NOT_READY, write/flush failures, record limit and no writes after ExitBootServices. These are RAM simulations, not recovery proof.

Native AMD64/noimports EFI69,632bytes SHA256:
`f6de8e330777bd344c229ac37ec8844ac3f66b77f487d7dd09e3d3ee079c41c0`.
Checklist remains15/26(58%); unlock and actual firmware execution are unproven.

After verified staging: savework/fullshutdown/wait10s/UEFIUSB/Shift+G once. Wait10s then Shift+V every2s up to3minutes. If noWindows10minutes,poweroff/removeUSB/normalBootManager. Runonce and preserve captures. A failed recovery guard still cannot resume with keys.

[Physical result: automatic Windows return](AD104-TWENTY-NINTH-RESULT.en.md). This document preserves the pre-run test contract; see the result for current evidence.
