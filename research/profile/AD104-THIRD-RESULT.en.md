# Third AD104 boot: firmware ROM buffer captured

English | [한국어](AD104-THIRD-RESULT.ko.md)

On 2026-10-26 the third diagnostic saved `FIRMROM.BIN`, 150,016 bytes.
Its SHA-256 is
`0ec450dce01cfc8f8b599b165f2de00c0fc6e2d2683c1be491251078c6a3a9fd`,
matching the diagnostic's successful full-write/flush/close record. All 34
capture files were copied into a separate private host archive and each copy's
SHA-256 matched the USB source. USB originals remain unchanged. The raw ROM
buffer is excluded from Git.

The log reports ROM pointer present, size `0x24a00` and PCI command `0007`
(memory decoding and bus mastering enabled). The admitted RAM-buffer capture
path succeeded without GPU MMIO, BME modification or driver detachment.
`unlock-eligibility=0`, `capture-complete=0` and final `EFI_NOT_READY` remain
intentional: a successful firmware-buffer copy is not a complete GPU profile.

Initial offline header inspection confirms AA55, PCIR at offset 368, vendor
`10de`, device `2782`, PCIR length 24, first image length 64,512 bytes, code
type 0 and indicator 0. The first image therefore does not mark the end of the
chain. No conclusion about subsequent images or whole-chain validity follows
from this first-header inspection. The full buffer is not yet established as
complete physical ROM contents.

The diagnostic log is 395,912 bytes. Ownership analysis again finds the captured
PCI-to-GOP owner and text-console relationships. The log still ends before the
Windows start call; the user's return does not establish that exact automatic
handoff path.

Next: validate the complete option-ROM image chain offline, including bounds,
image types and checksums, then compare any EFI image against the observed
legacy-video ownership path. Do not execute extracted images or publish raw
firmware. No new EFI has been staged and another boot is unnecessary for this
offline analysis. No unlock has been demonstrated.
