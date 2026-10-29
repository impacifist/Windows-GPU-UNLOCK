# Ninth AD104 boot: producer captured and text detach observed

English | [한국어](AD104-NINTH-RESULT.ko.md)

Both combined stages succeeded. `PCDPROD.BIN` is 11,840 bytes and its SHA-256
matches the firmware's stable-copy record:
`362a8b2d6e3b9d0d231d47b84cae709858a811b06e6a10e00d584a24154deeef`.
Its executable text range is RVA `0x2a0`, length `0x236d`, SHA-256
`cb64fe58020cf7512de99c86d8284baca00dd679109680ea1af371081e3b6367`.
Parsed relocation records contain no targets in that text range. GetBool at
RVA `0x4f4` calls helper `0x1120` with size one and reads the returned byte;
this supports the observed Boolean accessor shape, not a complete review of
the producer or its database helper.

The text disconnect returned success. Both postcollections and the final
postcheck matched six edges, IMG08 shared values `1/1/1`, unchanged PCI/BAR/
attributes and GOP mode. IMG14's list changed from one record to empty, and its
cached PCD pointer became the current interface. Both actual input challenges
passed. All four full 256-byte PCI configurations are identical. Command stays
`0007`, attributes `718`, support `1b77b`: no cold GPU state or unlock is shown.

All 34 files were preserved with matching USB/host hashes. The log is 404,006
bytes, SHA-256 `3f61d24ca55da4d79e58790c035db5ec848ad3220c5b8ecbd7a4d4834e6eecbb`.
USB EFI still matches `2e2cb3069992b076db3e84a0ff9ca266230ab4d107f0cd96866547907f18de7d`.
Windows readback after the user's return found driver `610.88`, VBIOS
`95.04.31.00.f0` and the expected GPU/BDF. This is current driver availability,
not a post-StartImage trace or a recovery guarantee for further removals.
See [sanitized evidence](ad104-ninth-boot-evidence.json); raw files stay private.

The next target is IMG17's GOP consumer, after text removal. Static Stop
inspection shows uninstall of SimpleTextOut and companion protocol
`e1e4a857-c970-4075-a4da-e9c41b69adfc`, closure of the GOP open and freeing the
text interface, its mode and another allocation. Several cleanup statuses are
ignored before success. No direct SetMode/Blt call appears in this Stop body,
but firmware-mediated effects remain possible. A future test must verify both
protocol removals and the six-to-five graph transition, and never dereference
the saved child text interface or its mode after the call. PCI owner and GOP
interface remain separate, retained targets.
