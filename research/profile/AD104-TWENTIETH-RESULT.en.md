# Twentieth result: physical PROM prefix captured, Windows returned

English | [한국어](AD104-TWENTIETH-RESULT.ko.md)

On2026-10-27 test20 captured all128DWORDs/512bytes of the physical BAR0 PROM
prefix on the MSI RTX4070Ti AD104. Signature/chip readbacks, bounded PCIR parsing,
state checks, decode-off and handoff eligibility passed. The user confirmed
Windows started automatically. Both root/main-log Close returned success.

PCIR is at offset368, vendor10de/device2782; the first image declares64,512bytes.
The captured512-byte prefix is byte-identical to the first512bytes of the earlier
150,016-byte firmware RAM ROM. Prefix SHA-256:
`7860719945ae7f43fc7a565551c97788176d6113384068f2ea0c3e52beb130ee`.
This is prefix equivalence only, not full-ROM identity, checksum/chain validation,
FWSEC validation, unlock admission or execution. The signature reread does not
prove every prefix byte was temporally stable.

All35 source/copy files were SHA-256 verified and retained privately. The prefix
was reconstructed only after checking all128 logged offsets were sequential.
[Sanitized evidence](ad104-twentieth-boot-evidence.json) contains hashes/metadata;
raw ROM bytes and platform identifiers remain private.

Next proposed combined test captures a fixed1MiB window, checks same-boot prefix,
signature/chip/state, and verifies PROM.BIN by file readback/hash before the known
decode-off recovery. It requires separate expanded-MMIO approval. Offline ROM
chain and FWSEC parsing will follow capture; neither is claimed from the prefix.
