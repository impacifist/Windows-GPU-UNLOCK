# AD104 offline ownership findings

English | [한국어](AD104-OWNERSHIP.ko.md)

The first boot's private archive was analyzed without executing its firmware
images or touching the GPU. [Reproducible evidence](ad104-ownership-evidence.json)
contains image hashes and role-based joins; addresses and raw images are omitted.

## Established relationships

The exact AD104 PCI handle has a `BY_DRIVER` open on PCI I/O. That same agent
has a `BY_CHILD_CONTROLLER` open pointing to the handle exposing GOP. Its driver
binding maps to `IMG29.BIN`, 18,432 bytes, SHA-256
`6978c561cfc119be29c3832d47fc51e471b02e86dc84b3a4eac6524a65de502c`.
This establishes the observed PCI-to-GOP ownership relationship, not vendor
identity or a safe driver stop sequence. The image contains the UTF-16 name
`BIOS[INT10] Video Driver`, strongly suggesting a platform legacy-video
abstraction. It is not established as native NVIDIA GOP. NVIDIA legacy ROM
services could still sit underneath; absence of NVIDIA strings excludes nothing.

On the GOP child, a second agent opens Simple Text Output (`387477c2-69c7-11d2-8e39-00a0c969723b`)
`BY_DRIVER` and opens it `BY_CHILD_CONTROLLER` to the system console handle.
That binding maps to `IMG14.BIN`. Thus the display path includes a text-console
layer above the PCI-to-GOP owner. Other consumers remain in the graph; removing
one observed edge would not prove the device or console was fully released.

There are 32 image snapshots but 25 unique SHA-256 values. Duplicate snapshots
are expected where multiple bindings use the same loaded image. These are
loaded memory images, not necessarily original PE files or ROM contents.
The null-image binding and images without driver bindings are not covered by
this snapshot mechanism. No full firmware coverage is claimed.

## Reproduce offline

```powershell
python research/profile/analyze-ownership.py C:\private\capture\RTX3090-UNLOCK `
  --output C:\private\ownership.json
python -m unittest discover -s research/profile -p 'test_*.py'
```

The analyzer rejects duplicate handles/image indices, mismatched image lengths,
and missing or ambiguous AD104 targets. It never executes images, releases
drivers, or grants transaction eligibility. Hashes identify these particular
relocated snapshots, not a portable vendor firmware release.

## Capture changes and next experiment

The AD104 diagnostic now prints the sampled PCI command bits before admission.
The requirement for disabled bus mastering remains unchanged. RAM-only tests
explicitly verify that active BME produces zero register/PROM reads. Current
checks: 151 AD104, 66 MBR, four GPT, 16 rejected compile combinations and 15
Python tests. The first physical capture remains the earlier build; no new EFI
was built, copied to USB or booted for this analysis.

Another identical boot is unnecessary: its BME refusal is already explained.
The next useful capture design should record legacy-video/CSM protocol presence
and bounded component names, then report the firmware-provided PCI I/O
`RomImage`/`RomSize` metadata and, only after bounded format checks and dedicated
tests, consider copying that firmware memory buffer. This could supply ROM
evidence without BAR MMIO, command-bit writes or detachment. Availability and
equivalence to physical PROM are unknown; this is a proposed design, not an
implemented capture or authorization to bypass the existing guard. A complete
PROM/register capture and an unlock remain blocked.
