# Captured AD104 ROM chain and offline EFI inspection

Update: [physical test21](AD104-TWENTY-FIRST-RESULT.en.md) exposes NVIDIA NPDE continuation beyond this two-image RAM buffer. The ordinary PCIR final bit below is accurate; the extended interpretation is separate.


English | [한국어](AD104-ROM-CHAIN.ko.md)

The [sanitized chain evidence](ad104-rom-chain-evidence.json) was produced by
`inspect-rom-chain.py` from the private third-boot buffer. The 150,016-byte
buffer contains exactly two bounded images, both identifying `10de:2782`:

| Offset | Bytes | Code type | Checksum modulo 256 | Final flag |
| --- | --- | --- | --- | --- |
| 0 | 64,512 | 0 (legacy) | 0 | clear |
| 64,512 | 85,504 | 3 (EFI) | 0 | set |

The final image ends exactly at the buffer boundary. This establishes chain
consistency within the captured buffer, not complete physical PROM coverage.
The EFI header declares signature `0x0ef1`, AMD64 (`0x8664`), boot-service
driver subsystem 11, compression type 1 and payload offset 80.

The 85,254-byte compressed stream was decompressed offline using
[TianoCore EDK II Decompress.c, edk2-stable202502](https://github.com/tianocore/edk2/blob/edk2-stable202502/BaseTools/Source/C/Common/Decompress.c),
with a local host type shim and bounded input/output allocations. Source SHA-256:
`bfd71b624dedf25bd43a470c56ae21373a262829e73d2b8dee81ff66c50c628d`.
The vendor source retains its BSD-2-Clause-Patent notice. The extracted image
was never executed, loaded into firmware or copied to boot media.

Decompressed file: 163,824 bytes, SHA-256
`1df6b4680a3ee3406e38e40239fb353966d1aa533a151068c20ed7ead2ae781e`.
Static PE inspection reports AMD64 PE32+, EFI boot-service subsystem 11,
image size `0x277a0`. Bounded strings include `NVIDIA GPU UEFI Driver`,
`nvgop-ad1xx` and `VER:0x70009`. These identify payload content; they do not
prove firmware selected or executed that payload. The live graph instead
links the GPU to the captured `BIOS[INT10] Video Driver` abstraction. No loaded
image of the decompressed payload's reported image size was found in this log;
size matching alone would not establish identity even if present.

Raw input, decompressed firmware, vendor parser source and local helper remain
private ignored artifacts. Offline chain parser tests and profile tests total
17 passing Python tests. No extracted firmware code was run.

The next [resource observation](AD104-FOURTH-BOOT.en.md) addresses BAR0 descriptor
shape, which the first boot's BME guard prevented checking. It does not switch
legacy/UEFI graphics mode or alter BIOS settings.
