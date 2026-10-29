# Twenty-first result: full PROM captured, FWSEC located, Windows returned

English | [한국어](AD104-TWENTY-FIRST-RESULT.ko.md)

On2026-10-27 the approved combined test captured the fixed1MiB BAR PROM window,
matched its same-boot512-byte prefix, passed GPU/state/bookend and file readback/
EOF/hash/close/free/final-admission checks, then performed verified decode-off.
The user confirmed automatic Windows return. All36 files were privately copied
and SHA-256 checked. The actual1,048,576-byte PROM matches the EFI log hash:
`accf2a054de0ec394078f933f74356ed7c4057ee0bd1ce6734cacda54ca9e1e1`.
The complete earlier150,016-byte firmware RAM ROM equals this physical prefix.
[Sanitized evidence](ad104-twentyfirst-boot-evidence.json) records hashes/metadata;
raw firmware, platform identifiers and loaded code remain private.

## Extended chain and newly available material

The production parser and bounded offline inspection identify four images:

| Offset | Bytes | Data/code type | Effective final |
| --- | --- | --- | --- |
| 0 | 64,512 | PCIR/legacy0 | no |
| 64,512 | 85,504 | PCIR/EFI3 | no |
| 150,016 | 24,576 | NPDS/0xe0 | no |
| 174,592 | 451,584 | NPDS/0xe0 | yes |

The ordinary PCIR final bit of image2 is set, matching the earlier two-image RAM
report. Its NVIDIA NPDE revision0x100/length16 overrides effective final to false
in the production parser, revealing the extra images. This refines the earlier
bounded interpretation; the byte-identical RAM prefix was not the entire physical
ROM. Checksums/validity of every extended image are not claimed merely from this walk.

FWSEC-FRTS begins at0x45480 (283,776), beyond the RAM ROM prefix. The host production
parser returned0 and its output ranges were separately bounded against the capture.

| Field | Observed |
| --- | --- |
| Image offset/bytes | 0x45480 / 0x10300 (66,304) |
| IMEM PA/size/VA | 0 / 0xf500 / 0 |
| DMEM PA/data offset/bytes | 0 / 0xf500 / 0xd80 |
| PKC/interface offsets | 0xb24 / 0x1c |
| Microcode/engine | 9 / 0x400 |
| Signature count/versions | 2 / 3 |
| Signature-region offset/bytes | 0x45180 / 768 |

Image SHA-256:
`e604f9be77c4f45d14ccdd2e45caef480801342ec7833e8618c70bbf34c60ab9`.
Signature-region byte SHA-256:
`df4953bd74c6c97736ba433a78db9cb27487c78e332445604379dabe7027a4f2`.
A signature-region hash identifies bytes; it does not authenticate a signature.

## Reference differences and remaining gates

The pinned AD104 reference requires a different PROM hash(c81eb48f…3ad3fb4),
image offset0x45488 and signature offset0x45188: both eight bytes later.
The other14 descriptor fields match the listed geometry. Reference payload and
signature bytes are absent from the local checkout, so equal payload/signatures
cannot be inferred from similar geometry. Existing SS1=5 and VGA_BASE=0x10e09
remain different from its1/1 requirements, and PCI BAR layout also differs.
No reference gate is weakened and no reference profile is transferred.

Finding/parsing/bounding/hashing this material is progress in firmware evidence,
not firmware authentication, accepted cold-entry or execution authority. No FWSEC
or other captured firmware was executed, and no unlock transaction ran.
The next observation-only EFI repeats the same GPU operations and audits these
known captured bytes using the production RAM parser. Host positive testing has
already parsed this capture; another boot tests EFI integration/reproducibility,
not discovery of the descriptor or proof it can execute.
