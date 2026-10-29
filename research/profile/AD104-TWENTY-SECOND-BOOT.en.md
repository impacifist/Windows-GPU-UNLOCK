# Twenty-second attended candidate: observation-only FWSEC RAM audit

English | [한국어](AD104-TWENTY-SECOND-BOOT.ko.md)

Built/host-tested; not physically executed. Reuses the exact GPU operations of
[test21](AD104-TWENTY-FIRST-BOOT.en.md):262,274 PROM accesses,2chip bookends,
44register reads, existing five stops and Enable/Disable(0x300). No new GPU
address, write, firmware upload/execution, reset, selector latch or unlock.

After complete capture and before saving the file, hash-match the exact test21
PROM. Only that known ROM is passed to the production parser on CPU-owned RAM.
All16 descriptor fields and independent image/signature/DMEM bounds must match.
The image and768-byte signature-region hashes must match their observed pins.
This is byte-hash equality, not cryptographic signature authentication.

The wrapper does not use Gu.profile_verified/prom_verified or any transaction
context. Results are separate observations. The audit summary explicitly states
firmware-authenticated=0, cold-entry-admitted=0 and execution-authority=0.
An audit mismatch alone is informational: preserve PROM.BIN and follow the
existing capture/state/decode-off recovery contract. Logging, transport, file,
cleanup or hardware-state failures still hold. No failed audit can enable a
transaction. File readback remains bound to the capture digest computed before
these callbacks; state is freshly checked after audit logging and later I/O.

Parser diagnostic macros are suppressed only in this wrapper translation unit,
so parsing makes no console/file callbacks. Minimal freestanding errno constants
and RAM memcpy/memset declarations link to existing efi_runtime intrinsics; no
OS services or transaction driver is linked. Unreferenced parser patch helpers
are discarded by function-section linking. Native map was checked for absence of
patch_frts, sig_offset, GuWrite, GuAd104Prepare and unlock_transaction symbols.

Host validation includes16 field mutations, range/null/unknown-ROM rejection,
a positive audit of the actual private test21PROM, source-buffer immutability,
and recovery with an audit mismatch. Audit-log failure/BME/ownership drift cases
hold. The real capture passed the32-check positive host run. This already proves
host parsing; the next boot checks EFI integration and same-boot reproducibility,
not firmware discovery, authentication or cold-entry readiness.

The audited ROM pin is accf2a05…a9e1e1, image0x45480/0x10300, signature0x45180/768.
Reference PROMc81eb48f…3ad3fb4 and offsets0x45488/0x45188 remain different. Reference
payload bytes were unavailable locally for independent equality comparison.
SS1/VGA_BASE/BAR differences remain unresolved. No gate is weakened.

Native61,952-byte AMD64 PE/noimports, SHA-256:
`7e10d34969bd4ae6a2f791599bb0601f62dcb5506a65c868008d643a93194a86`.
Manifest: observation-only audit=true, firmware execution=false, unlock=false.
Builder requires FWSEC-audit→full-PROM→header→delayed→decode-off→read.

Use the same attended timing as21: fully shut down, wait10seconds, boot UEFI USB,
Shift+G once. Wait10seconds and tap Shift+V every2seconds up to3minutes. The actual
fresh-V poll lasts60seconds after capture checks; the operator period is not an
internal3minute timer. No Windows after10minutes: power off, remove USB, select
normal Windows Boot Manager. Callbacks may hang; recovery is unproven for this
run. Run once and preserve all captures before retry.

[Physical result / 실제 결과](AD104-TWENTY-SECOND-RESULT.en.md).
