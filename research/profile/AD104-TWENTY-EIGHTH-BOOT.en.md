# Twenty-eighth candidate: GSP core/boot-state observation

[English](AD104-TWENTY-EIGHTH-BOOT.en.md) | [한국어](AD104-TWENTY-EIGHTH-BOOT.ko.md)

Built/host-tested for review; user explicitly accepted the disclosed new-MMIO risk and staging. Physical28 executed and stopped at a sentinel guard; test27 originals are preserved before verified staging. Test27's RISCV capability field does not identify the currently active/selected core; this candidate records relevant control/status fields without changing them.

Two14-DWORD batches:
`0, 0x110100, 0x110040, 0x110044, 0x110008, 0x1100f4, 0x110108, 0x1103c0, 0x111388, 0x111668, 0x82381c, 0x823820, 0x625f04, 0`.
New addresses: GSP ENGINE0x1103c0, RISC-V CPUCTL0x111388 and BCR_CTRL0x111668, each twice. [NVIDIA GSP definition](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/main/src/common/inc/swref/published/ampere/ga102/dev_gsp.h) defines ENGINE/reset field0. [NVIDIA RISC-V definition](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/main/src/common/inc/swref/published/ampere/ga102/dev_riscv_pri.h) gives base0x111000 plus0x388/0x668, CPU active7/halted4, BCR valid0/configured-core4. The matching local610.43.03 headers were checked. Published names do not guarantee safe AD104 access.

Read raw values and decode fields only. BCR core-select is configured selection (0Falcon/1RISCV), must be qualified by VALID, and even VALID1 does not prove present execution. Active/halted/reset fields are sampled context, not readiness/authentication/unlock. New-register changes between batches remain informational. No reset assertion/release, start/stop, core switch, boot command or acknowledgment is written.

All existing chip/CPUCTL/SS0/SS1/VGA equality, transport/all-ones/BADF/read-method/PCI/input/ownership/log guards remain. No file/console/allocation callbacks inside batches; fresh state surrounds logging. One attempt, no retry. Failure permanently holds and may leave decoding enabled; Shift+V cannot resume it.

Success adds28reads instead of27's22 (262348total Mem.Read). DMACTL remains excluded. No newGPUwrite/reset/DMAlaunch/BMEVGAenable/upload/execution/reconnect/unlock/automaticrollback. NewMMIOreads can have side effects or hang; display/input/Windows may fail, manual power-off/USB removal may be needed, recovery unproven. Explicit approval for this exact staging boundary was received.

Host tests cover25/26/27/28modes, all28transport positions, each new register's all-ones/BADF rejection and positive changing-value cases, plus retained state/method/log guards. Native AMD64/noimports EFI67,584bytes SHA256:
`5cb2e6d75b1ff7dda0872b903077bd06349544eec9ae3999f4fdf65fde3a0316`.
Manifest core-state=true, additional28, reset/DMACTL/firmwareexecution/unlock=false. Checks are host evidence, not physical success. Checklist15/26(58%) unchanged.

After verified staging: savework/fullshutdown/wait10s/UEFIUSB/Shift+G once;wait10s then Shift+V every2s up to3minutes. NoWindows10minutes:poweroff/removeUSB/normalBootManager. Run once,preserveallcaptures;recovery unproven.

[Actual result / 실제 결과](AD104-TWENTY-EIGHTH-RESULT.en.md).
