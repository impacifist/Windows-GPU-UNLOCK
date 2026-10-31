# First EVGA text membership test: contributor staging report

[English](FIRST-EFI-STATUS.en.md) | [한국어](FIRST-EFI-STATUS.ko.md)

On 27 October 2026 at 21:51, Movian reported a successful local build and USB staging in Discord. The build PE verifier reported 18,944 bytes, AMD64 EFI application, entry efi_main, zero imports/IAT/delay imports, NX and dynamic base. The reported build and staged-file SHA256 both equal:

`d76a85d10d9c045281b6430541d23dc4fd01eef14185fb8ceec0fe337b14b2c7`

The staging helper reported D:/EFI/BOOT/BOOTX64.EFI copied, the previous capture EFI preserved as BOOTX64.before-evga-text-stop.efi, and captures preserved. These are contributor-supplied outputs, not an independent inspection of his USB or binary. The differing local Swift-toolchain build hash is not itself evidence of a failure; the contributor used his LLVM installation. No byte-for-byte cross-toolchain equivalence is claimed.

Current evidence establishes build/staging reports only. There is no live Stop result or Windows return from this EFI yet. Next action is the single attended run in [the test instructions](NEXT-EFI-TEST.en.md), followed by the EVGAnn.TXT evidence and direct/manual Windows-arrival report. Do not widen the test or remove the one-shot marker based on staging success.

Movian uses GitHub Desktop to update his branch. Future chat instructions should omit CLI pull commands unless he asks for them. The build banner now matches the documented operator-choice workflow; this text-only script correction does not alter the already-staged EFI and does not require rebuilding it.
