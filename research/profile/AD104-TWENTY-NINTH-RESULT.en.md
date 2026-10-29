# Twenty-ninth result: recovery after the exact refused observation

[English](AD104-TWENTY-NINTH-RESULT.en.md) | [한국어](AD104-TWENTY-NINTH-RESULT.ko.md)

Physical test29 on 2026-10-27 returned automatically to Windows, confirmed by the operator. The first optional batch stopped after nine callbacks: RISC-V CPUCTL0x111388 returned transport status0 and0xBADF5620. The observation remains failed/incomplete (EFI_NOT_READY); the value is not accepted as CPU state.

The separate approved recovery branch completed both seven-DWORD required-state batches. Both contained chip194000a1, Falcon CPUCTL10, ENGINE0, SS0=24020000, SS1=5, VGA=10e09 and chip194000a1. Recovery admission and fresh-input/final-state guards passed. One Disable(0x300) returned success; both postchecks recorded command0/attributes0/ownership graph2, followed by Windows eligibility and experiment success. STATUS independently records the failed observation, successful rechecks, successful decode-off, root/log close and StartImage-next0. The operator's automatic Windows return supplies the OS outcome that StartImage-next alone cannot prove.

No BCR_CTRL or second optional batch was read. Nine optional plus14 required recovery reads means262343 total Mem.Read. No MMIO after decode-off, reset, core switch, firmware upload/execution or unlock occurred. This establishes one successful recovery run for this exact refusal, not general recovery safety, register accessibility or firmware execution readiness.

All36 files were privately copied and source/copy hashes verified. DIAG433738bytes SHA256 d0384b5c23515b9937afc7c5614a796796a98b34aad822ddc5e796bdc14ab0af; STATUS471bytes SHA25643bc9bc4eed244c4c14624a71f7b1832fc07ca2b2c787e641b34b27b9a92e413. PROM remains accf2a054de0ec394078f933f74356ed7c4057ee0bd1ce6734cacda54ca9e1e1. Tested EFI f6de8e330777bd344c229ac37ec8844ac3f66b77f487d7dd09e3d3ee079c41c0.

Next: isolate BCR_CTRL using a strict subset of approved28 reads, omitting refused111388. Keep29 reproducible and its recovery exception unavailable to the BCR candidate. Checklist15/26(58%) is unchanged: recovery evidence does not complete firmware-entry qualification.
