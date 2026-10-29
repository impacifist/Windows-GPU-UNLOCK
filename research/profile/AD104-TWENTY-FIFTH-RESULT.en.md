# Twenty-fifth result: DMACTL observation rejected; manual recovery

[English](AD104-TWENTY-FIFTH-RESULT.en.md) | [한국어](AD104-TWENTY-FIFTH-RESULT.ko.md)

Physical test25 on2026-10-27 completed the existing PROM and RAM-audit sequence, including successful PROM save/readback/free/final-state checks. The first new GSP batch then attempted five reads, each with callback status0:

| Offset | Value |
| --- | --- |
| 0 | 0x194000a1 |
| 0x110100 CPUCTL | 0x10 |
| 0x110040 MAILBOX0 | 0 |
| 0x110044 MAILBOX1 | 0 |
| 0x11010c DMACTL | 0xbadf5620 |

FalconBatch rejects BADF high16 and returned EFI_NOT_READY (0x8000000000000006). The log continued afterward: experiment-result, manual-power-off-required-no-key-can-resume and handoff-stopped. STATUS separately confirms handoff-stopped-manual-power-off. Thus this is evidence of a deliberate hold after a returned read, not proof of a CPU/MMIO hang. The flashing display does not identify an additional cause.

No remaining first-batch reads, second batch, Shift+V challenge, decode-off or Windows StartImage occurred. The intended successful path had18extra reads; this run made only5 (262325total Mem.Read callbacks). Decoding was not explicitly disabled by the experiment before the hold. User reports forced shutdown and manual Windows boot; automatic recovery failed for this run. Subsequent Windows inventory detects RTX4070Ti/driver610.88; no compute/recovery stress test was run.

All36files were privately copied and each source/copy SHA256 matched. DIAG427531bytes, SHA256bf2898dc283bf185f08b279a1ecad2c35fede58255f08938aad6a024741da075; STATUS114bytes, SHA2569190572a6f7cde16bc5e0172b9b878c43c456105f90625103a4ac9b959300b29. Original USB files remain intact. EFI hash matches staged abf77fd008606705ab8e73e0bdc0bfe33b67cbacf35598b14615d4d6d2b11425.

BADF5620 is rejected as an inaccessible-or-unaccepted observation; this capture does not identify its hardware meaning or prove DMACTL is always inaccessible. Mailbox zero does not establish readiness. No sentinel guard is weakened, no register control writes/reset/upload/execution/unlock occurred. Checklist remains15/26(58%). Do not rerun this occupied USB or treat Shift+V as a recovery command. Investigate DMACTL access requirements and design a separately reviewed next candidate; none staged by this result update.
