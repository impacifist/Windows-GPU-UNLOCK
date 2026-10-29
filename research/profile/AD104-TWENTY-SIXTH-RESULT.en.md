# Twenty-sixth result: mailbox baseline and automatic recovery passed

[English](AD104-TWENTY-SIXTH-RESULT.en.md) | [한국어](AD104-TWENTY-SIXTH-RESULT.ko.md)

Physical test26 on2026-10-27 completed both eight-DWORD batches, all16callback statuses0. Both batches matched: chip194000a1, CPUCTL10, MAILBOX0=0, MAILBOX1=0, SS0=24020000, SS1=5, VGA_BASE=10e09. The known fields also matched the pre-PROM batch2 sample. Log mailbox-only-stable=1; DMACTL was excluded entirely.

Decode-off returned0, Windows eligibility/experiment0. STATUS records successful root/log closes and StartImage-next; user confirms automatic Windows return. Subsequent Windows inventory sees RTX4070Ti/driver610.88; no workload was executed this turn. This establishes one successful mailbox-only capture/recovery run, not general recovery safety, mailbox readiness or unlock.

All36files were privately copied and source/copy SHA256 verified. DIAG433891bytes SHA25644473ea84c3acb51ea731510e77d7e0130296509a8d96df9c15665cbf9483713. PROM1MiB remains accf2a054de0ec394078f933f74356ed7c4057ee0bd1ce6734cacda54ca9e1e1. USB EFI matches9a0a77857d8453e294a6fce8b01565b5ac26bd6d10ad578acde770e3afb9c6cc. Original USB captures remain intact; do not rerun occupied output before archival/restaging.

Relative to25, removing DMACTL permitted the two batches, post-PROM selector checks, verified decode-off and automatic Windows return to complete. This does not prove why DMACTL returned BADF5620, nor establish that any value would be safe to accept. Guards were unchanged. SS1=5/VGA layout and exact-machine execution admission remain unresolved; no firmware upload/execution/unlock occurred. Checklist stays15/26(58%). No replacement EFI was staged in this result update.
