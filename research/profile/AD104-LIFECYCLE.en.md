# AD104 lifecycle investigation and fifth diagnostic

English | [한국어](AD104-LIFECYCLE.ko.md)

## Release-port mismatch

[Offline audit evidence](ad104-release-port-evidence.json) reproduces three
reasons the pinned external release code cannot be used unchanged:

| Property | Captured host | AD104 reference release |
| --- | --- | --- |
| PCI location | `0000:0b:00.0` | requires `0000:01:00.0` |
| PCI command | `0007`, BME enabled | rejects BME enabled |
| PCI owner version | `0x24` | `0x7000b` |
| Owner image bytes | 18,432 | 162,016 |
| Stop RVA | `0x7c4` | `0x6ec` |

Reference: [gpu-unlock-ada, pinned msi_release.c](https://github.com/JustAResearcher/gpu-unlock-ada/blob/4e7fefcc4923f9e083d5324dc75a7d2f906af77f/windows/ad104-msi/msi_release.c).
The local full-release implementation also pins machine-specific location,
MEM1/BME0, driver fingerprints and expected graph transitions. The native NVIDIA
EFI image found inside the ROM is not the observed PCI owner. Changing a few
constants is not a validated port.

## Captured Stop callback

Static inspection used the private IMG29 snapshot, SHA-256
`6978c561cfc119be29c3832d47fc51e471b02e86dc84b3a4eac6524a65de502c`.
Its recorded Stop is RVA `0x7c4`. In the zero-child branch, the disassembly
includes a call through PCI I/O offset `0x78` with operation 4, then masks the
returned value with `0x700`, ORs `0x18`, and calls that method with operation 3.
A compiled host ABI check against the bundled EDK II headers confirms offset
`0x78` is `Attributes`, operation 4 is Supported and operation 3 is Disable.
`0x700` covers I/O decode, memory decode and bus mastering; `0x18` covers VGA
memory and VGA I/O. Thus this branch can request changes to decoding and DMA
permission. This is a static path interpretation, not an observed Stop result.

The child-cleanup helper at RVA `0xcd0` includes protocol teardown and calls
consistent with legacy INT10 video services. This resembles
[TianoCore's archived BIOS-video lifecycle](https://github.com/tianocore/edk2-archive/blob/master/IntelFrameworkModulePkg/Csm/BiosThunk/VideoDxe/BiosVideo.c),
but source resemblance does not prove identical OEM behavior. A second binding
has a null image handle while its Stop RVA `0x2b38` lies inside IMG29. It must
not be treated as an unrelated or missing driver solely because its image
handle is null. No Stop callback or DisconnectController was executed here.

Reproduce the snapshot audit without hardware access:

```powershell
python research/profile/audit-release-port.py C:\private\DIAG00.TXT `
  --output C:\private\release-audit.json
```

The tool is specific to this capture family and never grants runtime authority.
Nineteen profile Python tests pass, including CRLF handling and ambiguous-input
rejection. This audit does not certify a release sequence or recovery.

## Fifth EFI test: query device-wide attributes

Before any mutation, collect `PciIo.Attributes(Get)` and
`PciIo.Attributes(Supported)`. The fourth boot's `GetBarAttributes` supports field
is BAR-specific and does not answer these device-wide queries. The new resource
diagnostic issues only Get/Supported, with zero attribute mask, and rechecks
interface/BDF/full PCI configuration before and after. It logs both statuses
and publishes masks only if both calls succeed and identity is unchanged.

Expected record: `ad104-pci-attributes ... current=... supported=... query-only=1`.
These values can refine the candidate mask in the static Stop path. They cannot
prove Stop reaches that branch, succeeds, clears BME or preserves Windows recovery.
No Set/Enable/Disable operation, GPU MMIO, ROM-buffer copy, driver release,
firmware upload or unlock is included. Existing BME admission is unchanged.

Build switches remain `-Ad104Msi -Ad104Metadata -Ad104Resources -WindowsMbr`.
Implementation `588323e`, 23,040-byte EFI SHA-256:
`2949d1f816b0b201c98320d0b762c931571cdf1749bee0c377f07a47292a91b7`.
AMD64 PE checks passed (no imports/IAT/delay imports, NX/dynamic base).
Host checks: 66 resource/attribute, 151 deep AD104, 24 metadata, 113 ROM-buffer,
66 MBR, four GPT, 20 rejected build combinations and 19 Python tests. This new
artifact has not been physically tested. `capture-complete=0` remains expected.

Preserve previous files before staging. With the capture USB connected, save work
and run `shutdown /s /t 0`. Wait ten seconds after power-off. Use ASUS Delete →
F8 → UEFI entry for the 8GB USB DISK 2.0; no test key. If Windows does not return
within four minutes, power off, remove USB and boot normal Windows Boot Manager.
Automatic return is not guaranteed. Preserve the new files before another boot.
Do not change BIOS graphics/CSM settings or infer permission to detach from a
successful query.
