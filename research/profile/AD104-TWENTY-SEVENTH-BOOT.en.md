# Twenty-seventh candidate: GSP capability/status observations

[English](AD104-TWENTY-SEVENTH-BOOT.en.md) | [한국어](AD104-TWENTY-SEVENTH-BOOT.ko.md)

Built/host-tested for review. User explicitly accepted the disclosed new-MMIO risk and staging of this exact candidate. Physical test27 completed successfully; see the linked result. Test26 originals/old EFI are preserved before verified staging. This extends the successful mailbox-only baseline with three published read-only register locations, leaving DMACTL excluded.

Two eleven-DWORD batches:
`0, 0x110100, 0x110040, 0x110044, 0x110008, 0x1100f4, 0x110108, 0x82381c, 0x823820, 0x625f04, 0`.
New addresses: GSP IRQSTAT0x110008, HWCFG2=0x1100f4, HWCFG=0x110108. [NVIDIA Falcon header](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/main/src/common/inc/swref/published/ampere/ga102/dev_falcon_v4.h) marks offsets+8/+f4/+108 read-only; the GSP base is0x110000. Published Ampere register identification does not guarantee safe AD104 access.

Record raw values, HWCFG2 RISCV field(bit10), memory-scrubbing field(bit12) and HWCFG IMEM-size field(bits8:0). Do not infer current CPU execution mode from a capability field, convert IMEM units to bytes without independent qualification, or treat cleared/stable values as idle/ready/authenticated. New fields may change between batches without rejection. The purpose is to characterize the engine interface before any firmware-execution design, not to bypass the exact-machine gates.

Existing chip/CPUCTL/SS0/SS1/VGA equality, all-ones/BADF, transport, read-method, ownership/input/PCI/log guards remain. Read-only batches use RAM without file/console/allocator callbacks. Full fresh state is checked before/between/after logging; one attempt, no retry. Failure remains a permanent hold and may leave decoding enabled. No fallback to Windows after a failed guard.

Success performs22additional reads instead of26's16 (262342total Mem.Read), including the three new addresses twice each. No new GPU writes, reset, interrupt acknowledgment, DMA launch, BME/VGA enable, firmware upload/execution, reconnect, unlock or automatic rollback. New reads may have side effects or hang; display/input/Windows may fail and manual power-off/USB removal may be needed. Recovery is unproven. Explicit approval for this exact staging boundary was received.

Host validation covers original25/reduced26/new27 modes, all22transport positions, all-ones/BADF at every new register, existing method/critical-state/ownership/log guards, and positive IRQ/HWCFG drift cases. Native AMD64/noimports EFI67,072bytes, SHA256:
`c945a05fadc7fa86f5aca5bf7ea8f8464dd4703326489d13130872620ffeeecb`.
Manifest capability-observation=true, additionalreads22, DMACTL/firmwareexecution/unlock=false.

## Offline layout interpretation from existing samples

No additional hardware access was needed for this calculation. In vendor/unlock_610.c: usableFB=0x2ffa00000; raw VGA workspace=0x10e0000, below usableFB-1MiB, so its existing layout policy selects usableFB-128KiB=0x2ff9e0000. Decoded MMU low=0x10000000, high=0x2ef9ff000, rounded exclusive top=0x2efa00000, exactly256MiB below usableFB. Therefore that code's top_lock predicate is false: this is not its top-of-FB reservation. These are consequences of the existing algorithm, not approval to use that policy on AD104. Assets, actual allocations/reservations, overlap/scrub constraints and exact-machine admission still need qualification. No numeric candidate is written to hardware and no full transaction layout is claimed valid. Checklist remains15/26(58%).

After verified staging: save work/fullshutdown/wait10s/UEFIUSB/Shift+G once. Wait10s then Shift+V every2s up to3minutes. If noWindows after10minutes, poweroff/removeUSB/select normalBootManager. A guarded hold cannot resume with keys. Run once and preserve captures; recovery is unproven.

[Actual result / 실제 결과](AD104-TWENTY-SEVENTH-RESULT.en.md).
