# Twenty-eighth result: RISC-V CPUCTL sentinel and manual recovery

[English](AD104-TWENTY-EIGHTH-RESULT.en.md) | [한국어](AD104-TWENTY-EIGHTH-RESULT.ko.md)

Physical test28 on2026-10-27 completed the prior PROM/RAM checks, then attempted nine reads in the first14-word batch. The first seven reproduce test27: chip194000a1, FalconCPUCTL10, mailbox0/0, IRQ10, HWCFG247b7, HWCFG80420100. New GSP ENGINE0x1103c0 returned0; RISC-V CPUCTL0x111388 returned0xbadf5620. All nine read callbacks returned status0.

The unchanged BADF guard returned EFI_NOT_READY(0x8000000000000006). Logs subsequently record experiment-result/manual-power-off-required-no-key-can-resume/handoff-stopped; STATUS independently records stopped-manual-power-off. This proves a returned read followed by deliberate hold, not a CPU/MMIO call hang. BCR_CTRL0x111668, remaining first-batch reads, second batch, Shift+V, decode-off and Windows StartImage were not reached. Only9of28extra reads occurred (262329total Mem.Read). Decoding was not explicitly disabled before the hold.

User reports forced shutdown/manualWindowsboot, so automatic recovery did not succeed. Subsequent Windows inventory sees RTX4070Ti/driver610.88; no compute or stress verification was performed. ENGINE0 alone is not evidence of readiness; rejected CPUCTL does not disclose active/halted state. BCR/core selection remains unobserved. No control/reset/core-switch/start/stop/write/upload/execution/unlock occurred.

All36files privately copied with matching source/copy SHA256. DIAG429249bytes SHA2567a454d2ce9f4053f4cb2ff443d0c439fc156960d23da101f1ff38d6dfdb840d9. EFI matches staged5cb2e6d75b1ff7dda0872b903077bd06349544eec9ae3999f4fdf65fde3a0316. Original USB files remain intact and output occupied. Do not rerun. No new EFI staged by this update.

The DMACTL rejection in25 and RISC-V CPUCTL rejection in28 warrant offline investigation of register access semantics before another attempt. Neither is accepted as valid state and no guard is weakened. A common root cause is not established. Exact-machine admission, SS1/VGA policy and firmware execution remain unresolved; checklist15/26(58%) unchanged.
