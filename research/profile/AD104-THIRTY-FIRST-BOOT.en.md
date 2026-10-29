# Thirty-first candidate: measured entry-input audit without refused registers

[English](AD104-THIRTY-FIRST-BOOT.en.md) | [한국어](AD104-THIRTY-FIRST-BOOT.ko.md)

Prepared after [test30's deliberate BCR hold](AD104-THIRTIETH-RESULT.en.md). This is a strict subset of approved28 operations plus CPU arithmetic. It excludes DMACTL0x11010c, RISC-V CPUCTL0x111388 and BCR_CTRL0x111668 entirely. It does not expand test29's refusal-recovery exception: that exception is compiled out. No new hardware approval boundary is introduced; historical modes remain reproducible.

Repeat the existing five stops, I/O+memory-only decode, matching delayed22-register samples, PROM capture/file readback/hash and CPU-owned FWSEC audits. Then make two12-DWORD batches:

`0, 0x110100, 0x110040, 0x110044, 0x110008, 0x1100f4, 0x110108, 0x1103c0, 0x82381c, 0x823820, 0x625f04, 0`.

Require chip bookends, prior CPUCTL/selectors/VGA matches, ENGINE0, pinned read method and fresh PCI/ownership/input state. Any transport/sentinel/state/method/log failure still holds. There are24additional observations/262344total Mem.Read. No file/console/allocation callbacks occur inside each read batch.

After successful sampling, a CPU-only audit consumes the actual pre-PROM22-register sample, independently checking all22 offsets, statuses, sentinel rejection and chip bookends. It computes FB bytes, decoded VGA workspace, the existing reference algorithm's VGA fallback, decoded MMU range/rounded top and its top-lock predicate. It logs SS1/VGA equality to the existing reference values1/1 as observations, without changing admission. Fresh state and the original saved read-method pin are checked before the audit and after its log flush. No new MMIO reads occur in the audit.

The saved physical input vector yields FB0x2ffa00000, raw VGA0x10e0000, reference-algorithm VGA0x2ff9e0000, MMU low0x10000000/high0x2ef9ff000/rounded top0x2efa00000. That top is256MiB below FB; top-lock=false. SS1=5 and VGA=0x10e09 do not match the reference's1/1 gates. These are algorithmic consequences, not proof of the AD104 reservation policy, safe placement or permission to weaken a gate. The audit explicitly logs assets-qualified=0/full-layout-unavailable=1/profile-admitted=0/execution-authority=0. Full layout requires pinned parsed Booter descriptor/GSP image spans; raw firmware file sizes cannot substitute. No allocation, reservation or GPU address programming is attempted.

Only a successful admitted capture reaches fresh Shift+V, the existing one Disable(0x300), exact saved post-parent checks and Windows handoff. No MMIO after Disable. No GPU control write, reset, core switch, DMA launch, BME/VGA enable, firmware upload/execution, unlock, reconnect or automatic rollback is added. Firmware/display/input/Windows may fail; manual power-off/USB removal may be needed and recovery is not guaranteed.

Host validation covers exact subset/counts, all24 optional transport positions, ENGINE/required-state faults, audit log/method/ownership faults, forbidden mode combinations, and103 independent CPU arithmetic/input rejection checks. Full native mock suite plus focused final-source tests passed;73Python and67evidence checks are separate from physical proof. Candidate31 has not run. Checklist15/26(58%) unchanged. Incremental value is actual sample→CPU audit→recovery integration; the arithmetic is already reproducible offline.

Native AMD64 EFI69,120bytes, no imports/IAT/delay imports. SHA256:
`b01e5e2cf0f4274c2dcf9511d1e292942dac154306886f37c05da2da12de090d`.

After verified staging/preservation of30: save work, fully shut down, wait10seconds, boot UEFI USB. Shift+G once; wait10seconds, then Shift+V every2seconds for up to3minutes. Video stops updating after driver release; a blinking cursor is not progress. If no Windows within10minutes, power off, remove USB and choose normal Windows Boot Manager. Run once; preserve captures before another attempt. Failed guards cannot resume with keys.
