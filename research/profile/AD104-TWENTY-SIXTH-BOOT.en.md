# Twenty-sixth attended candidate: mailbox-only baseline

[English](AD104-TWENTY-SIXTH-BOOT.en.md) | [한국어](AD104-TWENTY-SIXTH-BOOT.ko.md)

Built/host-tested; physical test26 pending. This is a strict subset of the accepted test25 read set. DMACTL0x11010c is excluded entirely, not read and accepted despite BADF. Its access restriction remains unexplained. Both original test25 mode and its sentinel stop remain reproducible.

Repeat test24's established capture/RAM assembly, then two eight-DWORD batches:
`0, 0x110100, 0x110040, 0x110044, 0x82381c, 0x823820, 0x625f04, 0`.
These are chip bookends, CPUCTL, MAILBOX0/1, SS0/SS1 and VGA_BASE. No new GPU address or operation beyond approved25. Added16reads instead of18; total262336Mem.Read callbacks on success. Expected purpose: complete mailbox reproducibility and post-PROM selector stability observation, then test the existing decode-off/Windows path without the rejected DMACTL access. This does not prove engine readiness or resolve SS1/VGA admission.

All transport/all-ones/BADF/chip/critical-state/method/log/ownership guards remain. CPUCTL/SS0/SS1/VGA must equal the earlier22-read batch2. Mailbox changes alone remain informational. Fresh full state admission surrounds both RAM-only batches and logging. No allocation/file callbacks within batches. No reset, interrupt acknowledgement, DMA launch, GPU control writes, BME/VGA enable, firmware upload/execution, unlock or automatic rollback is added. Failure still holds; a new failure can still require manual power-off. Existing approval covers the smaller read subset; recovery is not guaranteed.

Regression mocks exercise both nine-word25 and eight-word26 modes, exact address order/count, every transport failure point, literalBADF5620 mailbox rejection, all-ones, critical-state/method/ownership/log faults, and permitted mailbox drift. No sentinel rule was weakened. Native AMD64/noimports EFI66,560bytes, SHA256:
`9a0a77857d8453e294a6fce8b01565b5ac26bd6d10ad578acde770e3afb9c6cc`.
Manifest mailbox-only=true, additionalreads16, CanReadDmaControl=false, firmwareexecution/unlock=false.

Save work, fully shut down, wait10seconds, bootUEFIUSB, Shift+G once. Wait10seconds then tap Shift+V every2seconds up to3minutes. A deliberate guard hold cannot be resumed with keys. If noWindows after10minutes, poweroff/removeUSB/select normalWindowsBootManager. Recovery remains unproven for this run. Run once; preserve captures before retry. Test25 originals/oldEFI must be archived and verified before staging.

[Physical result / 실제 결과](AD104-TWENTY-SIXTH-RESULT.en.md).
