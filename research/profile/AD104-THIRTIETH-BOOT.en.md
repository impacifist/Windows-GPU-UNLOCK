# Thirtieth candidate: isolate the remaining BCR observation

[English](AD104-THIRTIETH-BOOT.en.md) | [한국어](AD104-THIRTIETH-BOOT.ko.md)

[Physical29](AD104-TWENTY-NINTH-RESULT.en.md) confirmed automatic Windows return through the exact approved CPUCTL-refusal recovery. This candidate keeps29 reproducible and uses a strict subset of the already approved28 address list. It removes rejected RISC-V CPUCTL0x111388 entirely, allowing BCR_CTRL0x111668 to be reached for the first time. DMACTL0x11010c also remains excluded. No additional hardware permission is inferred from the successful recovery.

Repeat the existing five driver stops, explicit I/O+memory-only decode enable, two fixed22-register samples, full1MiB PROM capture/readback/hash and CPU-owned FWSEC audits. Then make two13-DWORD batches in this exact order:

`0, 0x110100, 0x110040, 0x110044, 0x110008, 0x1100f4, 0x110108, 0x1103c0, 0x111668, 0x82381c, 0x823820, 0x625f04, 0`.

Keep chip bookends194000a1, prior Falcon CPUCTL/selectors/VGA matches, original read-method pin and fresh full PCI/ownership/input checks around logging. Require ENGINE0 immediately before BCR. No file/console/allocation callbacks occur inside a read batch. BCR values remain observational: log raw value, VALID and configured-core fields; configured selection is qualified only when VALID=1. None establishes currently executing core or firmware-entry readiness. BCR changes alone are not silently promoted into a hardware admission policy.

The CPUCTL-specific29 recovery exception is compiled out. Builder and header reject combining BCR mode with that exception. Any BCR sentinel, including BADF5620, all-ones, transport error or required-state/method/log drift retains permanent hold. Do not retry a refused register. Only a complete admitted observation can proceed through fresh Shift+V, existing one Disable(0x300), exact saved post-parent checks and Windows handoff. No GPU MMIO follows Disable.

Counts:26 additional observations/262346 total Mem.Read, two fewer than a complete28. This does not introduce GPU control writes, reset, core switching, DMA launch, BME/VGA enable, firmware upload/execution, unlock, reconnect or automatic rollback. Display/input can disappear, firmware/Windows can fail or hang, manual power-off/USB removal can be needed; recovery after a new BCR error is not established and deliberately does not use29's exception.

Host checks cover exact order/count and exclusion of both refused addresses, every26 transport positions, four BCR error patterns, VALID/configured-core variations, ENGINE mismatch and inherited logging/method/state guards. An explicit negative compile test rejects BCR+REFUSAL. RAM mocks/build success do not prove physical30 success.

Native AMD64 EFI67,584bytes, no imports/IAT/delay imports, SHA256:
`d49aa002a509404d9bd8495005d8f022d977be5d315fabb0270b860ad9da349e`.

After USB identity/source/hash checks and preservation of29, save work, fully shut down, wait10seconds, select the UEFI USB. Press Shift+G once, wait10seconds, then press Shift+V every2seconds for up to3minutes. The blinking cursor is not progress: video output stops after driver release. If Windows has not started within10minutes, power off, remove USB and select normal Windows Boot Manager. Recovery is not guaranteed. Run once; preserve captures before another attempt. Checklist15/26(58%) remains unchanged.

[Actual result / 실제 결과](AD104-THIRTIETH-RESULT.en.md).
