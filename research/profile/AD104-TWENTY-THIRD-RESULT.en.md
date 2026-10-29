# Twenty-third result: RAM rehearsal and Windows recovery passed

[English](AD104-TWENTY-THIRD-RESULT.en.md) | [한국어](AD104-TWENTY-THIRD-RESULT.ko.md)

The attended test23 on 2026-10-27 passed the FWSEC scratch-patch integration. The log records source-unchanged=1, patch-match=1, selector-matrix=1 and scratch-intact=1. Exactly nine bytes changed within the permitted 48-byte footprint. The patched RAM SHA-256 matches the host rehearsal:
`0bca65c5d5f41cec855acd8009340a86a1e08d117cb9c1a716cb444f3d0a2d52`.

The captured 1MiB PROM remains identical to tests21/22:
`accf2a054de0ec394078f933f74356ed7c4057ee0bd1ce6734cacda54ca9e1e1`.
Transport, free, final state, decode-off and Windows eligibility all returned success. STATUS records successful root/log closes and StartImage-next; the user confirms automatic Windows return. All36files were privately copied and each source/copy SHA-256 matched. A subsequent Windows inventory sees RTX4070Ti with driver610.88; no workload was run this turn.

This validates EFI RAM preparation and recovery integration. The FRTS address was synthetic; no live fuse was read, no firmware uploaded/executed, and authority remained zero. Signature-index results are hypothetical and do not establish authentication. Checklist remains15/26 (58%).

The next useful work is offline investigation of SS1/VGA/BAR differences and exact-machine admission, plus asset qualification. Another identical boot would not resolve those blockers. Firmware execution remains a separate, unapproved hardware boundary. No next EFI is staged by this result update; the USB still contains the executed test23 and its original captures. Do not rerun it before capture archival/restaging.
