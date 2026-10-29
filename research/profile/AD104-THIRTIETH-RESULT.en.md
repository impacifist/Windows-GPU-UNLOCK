# Thirtieth result: BCR refusal and deliberate hold

[English](AD104-THIRTIETH-RESULT.en.md) | [한국어](AD104-THIRTIETH-RESULT.ko.md)

Physical30 on2026-10-27 completed prior register/PROM/RAM checks, then attempted9 optional reads. GSP ENGINE0x1103c0 returned0; BCR_CTRL0x111668 returned callback status0/value0xBADF5620. All9 callbacks returned0. The unchanged data guard returned EFI_NOT_READY and subsequent logs explicitly record manual-power-off-required-no-key-can-resume/handoff-stopped. STATUS independently records handoff-stopped-manual-power-off. This is a returned read followed by deliberate software hold, not proof that an MMIO call hung.

The operator recalls getting stuck but is unsure of the last run. Logs identify the30 binary and stopped path; the exact manual recovery sequence remains unconfirmed. No second optional batch, freshV, decode-off or Windows StartImage was reached. There were9additional/262329total Mem.Read. No reset, core switch, GPU control write, firmware upload/execution or unlock occurred. Decoding was not explicitly disabled before hold.

All36files were privately copied/hash-verified; original USB files remain until verified restaging. DIAG427897bytes SHA256084bfa4e0c94c651b48523cc485acb454dda03d84c7af0778eb688f7d7a77a83. EFI SHA256d49aa002a509404d9bd8495005d8f022d977be5d315fabb0270b860ad9da349e. PROM stillaccf2a054de0ec394078f933f74356ed7c4057ee0bd1ce6734cacda54ca9e1e1.

DMACTL(test25), RISC-V CPUCTL(test28/29) and BCR(test30) all returned BADF5620 despite transport success. A common cause, exact permission level and harmlessness are not established. Test29's separately approved CPUCTL recovery is not general BCR recovery authority. Do not accept rejected data as core state or retry those registers.

Next31 excludes all three refused addresses, retains proven engine/capability observations and adds a CPU-only audit of actual previously sampled FB/VGA/MMU/SS1 inputs. It explains existing reference-policy mismatches without waiving them. Full layout still needs pinned parsed firmware assets; file sizes are not parsed image sizes. Checklist15/26(58%) unchanged. [Test31](AD104-THIRTY-FIRST-BOOT.en.md).
