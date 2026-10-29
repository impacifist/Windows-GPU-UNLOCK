# Twenty-second result: EFI FWSEC audit passed

[English](AD104-TWENTY-SECOND-RESULT.en.md) | [한국어](AD104-TWENTY-SECOND-RESULT.ko.md)

On 2026-10-27 test22 completed the production FWSEC parser and hash audit on CPU-owned captured bytes. PROM, descriptor geometry, image and signature-region matches were all 1. The observed image offset/size is 0x45480/0x10300 and signature offset/size 0x45180/768. No cryptographic authentication or firmware execution is established.

All 36 capture files were privately preserved and SHA-256 verified. PROM.BIN is byte-identical to test21 (1,048,576 bytes; SHA-256 accf2a054de0ec394078f933f74356ed7c4057ee0bd1ce6734cacda54ca9e1e1). Transport, cleanup, final state, decode-off and Windows eligibility all returned success. STATUS records successful root/log closes and StartImage-next; the user separately confirms automatic Windows return.

The parser reported firmware-authenticated=0, cold-entry-admitted=0, execution-authority=0. SS1/VGA/BAR and reference-ROM differences remain unresolved. Checklist completion remains 15/26 (58%); repeated capture and EFI parser integration do not complete another execution-readiness milestone.

Next is a combined CPU-only scratch-patch and hypothetical signature-index rehearsal. Host testing establishes the byte transformation; the EFI candidate tests its integration with capture and recovery. It adds no GPU access or firmware execution. [Test23 contract](AD104-TWENTY-THIRD-BOOT.en.md).
