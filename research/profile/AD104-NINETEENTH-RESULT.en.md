# Nineteenth result: delayed reads and Windows recovery repeated

English | [한국어](AD104-NINETEENTH-RESULT.ko.md)

On2026-10-27 the delayed stability test completed on the MSI RTX4070Ti AD104.
The requested Stall(1000000) returned success; it is not a measured wall-clock
interval. Full fresh state admission passed after delay/logging, both22-read
batches matched exactly, and the approved Disable(0x300) restored the saved
post-parent state (command0, attributes0, two pointer relationships). Final
checks and both main/root file closes succeeded. The user explicitly confirmed
Windows started directly, without manual power-off.

This repeats run18's capture-and-return result with a nominal one-second batch
gap. SS0 remained0x24020000, SS1=5 and VGA_BASE=0x10e09. The latter two still
differ from reference requirements1/1; they were still5/0x10e09 at the post-delay sample.
Transient intermediate values were not observed. Stability and recovery are not unlock admission.
No new GPU register, PROM read, firmware upload, reset or unlock was performed.

All35 files, including STATUS.TXT, were copied privately and SHA-256 checked.
[Sanitized evidence](ad104-nineteenth-boot-evidence.json) records the log hash and
register values. Firmware and platform identifiers remain private. The executed
53,760-byte EFI hash was
`f63b358ef0c42e314103ab07655d40f9c5d81a385c6685520a0e228a48734600`.

Next is a proposed512-byte physical BAR PROM-prefix observation requiring
separate approval.
It adds new MMIO addresses and must not be mistaken for an unchanged repeat.
