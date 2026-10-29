# Twenty-third candidate: RAM-only FWSEC preparation rehearsal

[English](AD104-TWENTY-THIRD-BOOT.en.md) | [한국어](AD104-TWENTY-THIRD-BOOT.ko.md)

Built and host-tested before staging; physical result pending.

Repeat test22's existing five stops, Enable(0x300), 44 register reads, 262,274 PROM reads and two chip bookends. Add no GPU address, GPU write, reset, fuse read, firmware upload or execution. Decode-off and the existing Windows handoff remain unchanged.

After the exact PROM/parser/geometry/payload audit, copy only the 66,304-byte FWSEC image into the already allocated second 1MiB readback buffer. Require disjoint, bounded buffers. Put 16-byte canaries before/after the scratch image. Call the production FRTS patch helper with synthetic offset 0x100000000: this is an encoding test vector, never an allocated/admitted device address.

Independently compare every image byte against the original and expected command contents. The only allowed write locations are 44 bytes at image offset 0x10240 and four bytes at 0x1000c (DMAP mapper+44). Record changed-byte count and patched-image hash. Verify canaries and hash the source PROM again against its pre-callback digest. Patched bytes are overwritten by normal file readback; they are never saved as executable assets, uploaded or executed.

Combine signature-selection checks for hypothetical fuse values 0..15: versions=3 with 384-byte signatures permits offsets 0 and 384 for hypothetical values 0/1; values 2..15 must reject without changing the output. This does not identify the live fuse, authenticate a signature or authorize execution. Host tests additionally reject invalid fuse/version/size/null/overflow and unaligned/out-of-range patch offsets.

A patch/selector audit mismatch alone is informational and retains PROM/recovery like22. Source mutation, scratch-canary failure, logging or hardware-state failure blocks handoff. No transaction context, profile verification or selector latch is set. Production patch/signature helper symbols are intentionally linked for RAM-only use; device transaction helpers remain absent.

Host tests on the actual captured PROM establish the transformation without another boot. This EFI checks scratch integration with capture/recovery; it does not resolve SS1/VGA/BAR/reference differences or finish the execution-readiness milestone. Checklist stays15/26 (58%).

Attended instructions: save work, fully shut down, wait10seconds, boot UEFI USB, press Shift+G once. Wait10seconds, tap Shift+V every2seconds up to3minutes. If Windows has not started after10minutes, power off, remove USB and select normal Windows Boot Manager. Recovery remains unproven for this run; run once and preserve captures.

Native EFI: 64,000 bytes, AMD64, no imports. SHA-256:
`1b0ab6ba12c8d270693d1faa4b6c0d013b9a82572c21baa93a59ff13cd53f508`.

Actual-PROM host rehearsal: 57 checks passed, 9 bytes changed; patched RAM hash
`0bca65c5d5f41cec855acd8009340a86a1e08d117cb9c1a716cb444f3d0a2d52`.
The native build excludes test-fault hooks and device transaction helpers.

[Physical result / 실제 결과](AD104-TWENTY-THIRD-RESULT.en.md).
