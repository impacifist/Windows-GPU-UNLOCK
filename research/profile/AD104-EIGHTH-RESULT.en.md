# Eighth AD104 boot: predicate false and callback owners identified

English | [한국어](AD104-EIGHTH-RESULT.ko.md)

The physical read-only observation passed its snapshot consistency checks.
Both native PCD queries for token `0x4ac` succeeded and returned false after
1,152 token-enumeration steps each. This is an actual false observation, not
a missing-token or failed-query default. It supports skipping the conditional
GOP SetMode branch under these observations. It does not establish the token's
semantic name or guarantee its value during a later Stop.

ConOut and StdErr share an interface inside the IMG14 loaded image. Their
OutputString callbacks are at RVA `0x34fc`; console SetMode is at `0x378c`.
Offline checks of the captured section tables place these callbacks in executable
`.text`. IMG14/15/16 are duplicate dumps of the same loaded image, not three
independent callback owners. These checks establish location, not safety after
the driver's internal state changes. Future text-detach code must avoid relying
on these output callbacks after an attempt.

The child GOP SetMode callback is at IMG29 RVA `0x1ea0`, also within its captured
executable `.text`. GOP mode is `2` of five modes, 1024×768, pixel format `1`,
stride 1024, framebuffer base `0xd0000000`, size `0x300000`. No framebuffer
contents were read. Console text mode is `0` of four, attribute `0x0f`, cursor
column 0/row 1 and hidden. These states agreed across the three snapshots.

PCD GetBool is at RVA `0x4f4` in one containing loaded image of `0x2e40` bytes.
That producer has no driver-binding image dump in this archive. Its firmware
file GUID is `80cf7257-87ab-47f9-a3fe-d50b76d89541`, but no producer code hash or
executable-section check can be derived from the available image dumps. The
query result does not substitute for a fingerprint of that implementation.

All 33 files were copied into a new private host archive and every USB/copy
SHA-256 matched. The log is 398,582 bytes, SHA-256
`8142dcfca55d2a9511c6f810d6036fd6414e4c3cc4eb43adf1d9946bd134a43c`.
USB EFI SHA-256 matches the prepared image:
`7f9cba828a34660723e389465ffd9a190ce8ead20dc08d11477c741d0d5946b1`.
See [sanitized evidence](ad104-eighth-boot-evidence.json). Raw captures and
absolute callback addresses remain private; USB originals were left unchanged.

After the user returned, Windows `nvidia-smi` reported RTX 4070 Ti at
`00000000:0B:00.0`, driver `610.88`, VBIOS `95.04.31.00.f0`. This establishes
current driver availability; the read-only diagnostic log does not trace the
final Windows handoff. Metadata baseline `capture-complete=0`/`EFI_NOT_READY`
remains expected. No text detach, display SetMode, PCD setter, GPU MMIO or unlock
ran. The successful seventh EDID release remains a separate experiment.

The next engineering task is an output-independent text-lifecycle contract,
including fresh predicate verification and display-state postconditions. The
missing PCD producer fingerprint must remain explicit. This capture does not
authorize text or PCI-owner release. No next EFI is staged and repeating this
unchanged observation is unnecessary.
