# Fourth AD104 boot: BAR0 descriptor matches

English | [한국어](AD104-FOURTH-RESULT.ko.md)

The 2026-10-26 resource-only test returned success from `GetBarAttributes`.
The descriptor was valid and matched the existing envelope's descriptor shape:

| Field | Observed value |
| --- | --- |
| Resource type | 0 (memory) |
| Granularity | 32 bits |
| Minimum | `0xfb000000` |
| Maximum | `0xfbffffff` |
| Length | `0x01000000` (16 MiB) |
| Translation offset | 0 |
| Supported attributes | 0 |
| Descriptor valid / envelope shape | 1 / 1 |

The log reports PCI command `0007`, memory decoding enabled and BME enabled.
Therefore the descriptor uncertainty is resolved, but full deep-read admission
is still not satisfied. `envelope-shape=1` is not permission to perform MMIO.
The probe performed no GPU MMIO, ROM-buffer copy, BME change, driver release or
unlock; `unlock-eligibility=0` remains explicit. Metadata still reports a ROM
buffer of `0x24a00` bytes. `capture-complete=0` and final `EFI_NOT_READY` are
intentional for this observation-only mode.

All 33 files were copied to a separate private host archive and USB/copy
SHA-256 values matched. The diagnostic log is 396,085 bytes. The USB EFI hash
matches the staged artifact
`cf3bd58ef496604d653fc4d21da6030f27eb326dea49b5131ca590026d80014d`.
USB originals remain unchanged. Offline ownership analysis still resolves the
PCI-to-GOP and text-console relationships; this does not establish disconnect
safety. The log does not prove the final Windows start-call outcome.

No additional identical resource boot is needed. Further work should use the
existing ROM and ownership evidence to evaluate a separate, explicitly reviewed
ownership/lifecycle experiment. Do not infer that clearing BME or stopping the
legacy-video owner is safe from this descriptor result. No next EFI was staged
while checking this capture.
