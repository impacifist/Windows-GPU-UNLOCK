# Twenty-fifth candidate: bounded GSP Falcon baseline

[English](AD104-TWENTY-FIFTH-BOOT.en.md) | [한국어](AD104-TWENTY-FIFTH-BOOT.ko.md)

User explicitly accepted the disclosed new-MMIO risk and staging of this exact candidate. Built/host-tested and prepared for USB staging; physical test25 has NOT executed. Test24 originals are preserved before staging. The next boundary is18additional live BAR0 DWORD reads, including three previously untested addresses. This is a physical engine-baseline experiment, not another RAM-only integration test.

Repeat test24, then after PROM capture/save/free and before Shift+V make two RAM-only batches of nine reads:
`0, 0x110100, 0x110040, 0x110044, 0x11010c, 0x82381c, 0x823820, 0x625f04, 0`.
The three new addresses are GSP Falcon MAILBOX0, MAILBOX1 and DMACTL. [NVIDIA GSP map](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/main/src/common/inc/swref/published/ampere/ga102/dev_gsp.h) defines GSP base/mailboxes; [Falcon map](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/main/src/common/inc/swref/published/ampere/ga102/dev_falcon_v4.h) defines DMACTL+0x10c. These names identify intended registers, not a guarantee of safe AD104 reads.

Known chip bookends must match194000a1. CPUCTL/SS0/SS1/VGA must match the prior batch2 values; mailbox/DMA changes between new batches are informational. Record raw values and a stability flag. DMACTL bits or mailbox contents alone cannot establish idle, firmware completion, unlock or execution readiness. This baseline supplies evidence for later GSP execution design, but does not by itself resolve SS1/VGA admission.

Pin Mem.Read and full PCI/ownership/input state before/between/after batches and after logging. No logging, file callbacks or allocation inside a read batch. One attempt; any read error, all-ones/BADF observation, chip/known-state mismatch, method drift, log/state failure blocks handoff without retry. Sentinel rejection means inaccessible-or-unaccepted observation, not proof those literal mailbox contents are impossible. Log partial attempted results. The existing guarded decode-off/Windows path is reached only on success; failure can leave decoding enabled and hold indefinitely.

Success adds18Mem.Read calls: total262338, including262274PROM accesses. No new GPU writes, reset, mailbox clear, interrupt acknowledgment, DMA launch, BME/VGA enable, firmware upload/execution, reconnect, unlock or automatic rollback. New reads can still hang or have side effects. Display/input/Windows may fail; manual power-off/USB removal may be needed, and recovery is unproven. The user explicitly approved this exact staging boundary.

Native AMD64 EFI66,560bytes/noimports, SHA256:
`abf77fd008606705ab8e73e0bdc0bfe33b67cbacf35598b14615d4d6d2b11425`.
Host tests cover all18transport-failure positions, sentinel/chip/selector/method/state faults, intent/inter-batch/summary logging faults and informational mailbox drift. They simulate RAM callbacks and are not hardware proof.

Attended instructions after verified staging: save work/fullshutdown/wait10s/UEFIUSB/Shift+G once; wait10s then Shift+V every2s up to3minutes. NoWindows after10minutes: poweroff/removeUSB/normalBootManager. Run once; preserve allcaptures before restage.

[Actual result: guarded stop/manual recovery / 실제 결과: 보호 정지 및 수동 복구](AD104-TWENTY-FIFTH-RESULT.en.md).
