# Sixth test: live topology preflight, no release

English | [한국어](AD104-SIXTH-BOOT.ko.md)

This test runs newly implemented capture-specific discovery against live
firmware. It does not replay saved addresses or merely repeat attribute queries.
It discovers four driver bindings by loaded code hash, size, code type, version
and Stop RVA, plus the null-image binding whose Stop lies inside IMG29.
IMG08 is correctly pinned as runtime-services code; the other roles use
boot-services code. Across all five saved captures these code hashes are stable
and the hashed text ranges contain no relocation entries.

It checks eight expected active relationships and rejects additional active
opens on the GPU, its display child or console, or by the relevant driver agents
elsewhere. Enumeration is bounded to 1,024 handles, 256 protocols per handle and
512 open entries per protocol. Passive observations are excluded. A second fresh
collection must agree, including input handles/callback pointers and the shared
IMG08 byte/count observations (`1`, `1`, `1`). These data values remain unnamed
loaded-state observations; their full operational meaning is not established.

Expected record: `ad104-runtime-preflight topology-status=0x0000000000000000`.
`roles=0xf`, a present null binding and eight matched edges explain discovery.
Refusal records distinguish driver discovery (step 1) from graph checking (step
2). A zero step is not an overall authorization. A failure is useful evidence,
not a reason to loosen fingerprints or retry repeatedly.

**Success means topology/fingerprint consistency only.** Resource admission is
reported separately. It does not prove working keyboard input, an atomic
snapshot, cold GPU state, safe Stop behavior or eligibility to release/unlock.
`release-authority=0`, `cold-state-known=0` and `input-execution-tested=0` stay
explicit. The metadata path still reports `capture-complete=0`/`EFI_NOT_READY`.
No driver Stop, DisconnectController, attribute writes, GPU MMIO, ROM copying,
firmware execution or unlock is included. Loaded-image pointers and allocations
are trusted firmware protocol contracts, as in the existing image dumper.

Build with `-Ad104Msi -Ad104Metadata -Ad104Resources -Ad104Preflight -WindowsMbr`.
Artifact built from implementation `8c22c7e`: 28,672 bytes, SHA-256
`df8245799c9c4ca010e56e71d7a21c486ac6933862eed6fd271b68bba0d7a169`.
Native AMD64 PE checks passed: no imports/IAT/delay imports, NX/dynamic base.
Host checks passed: 18 new discovery/graph checks, 151 deep AD104, 24 metadata,
113 ROM-buffer, 66 resource, 66 MBR, four GPT, 21 rejected build combinations,
and 31 Python tests. The new C checks use RAM firmware mocks and substitute
fixture hashes; native build checks are separate from physical execution.

Preserve previous captures and EFI files before staging. Save work, leave the
capture USB connected, run `shutdown /s /t 0`, and wait ten seconds after power
off. Use ASUS Delete → F8 → UEFI entry for the 8GB USB DISK 2.0. No test key is
required. If Windows has not returned within four minutes, power off, remove USB
and boot normal Windows Boot Manager. Automatic return is not guaranteed.
Preserve the new capture before another boot. No BIOS settings need changing.
