# EVGA15 runtime PCI/BAR result

[English](SIXTEENTH-EFI-RESULT.en.md) | [한국어](SIXTEENTH-EFI-RESULT.ko.md)

The exact-identity shared PCI validator accepts the full256-byte configuration after release with memory decoding enabled and BME off. BAR0 is0x84000000..0x84ffffff, length0x1000000. PM/MSI/PCIe offsets are0x60/0x68/0x78; the recorded MSI-X header at0xc8 is unlinked. BAR descriptor validation, stable state pairs, original PROM hash, memory-disable/restored-state checks, both captures and ready-Windows succeed. Normal Windows arrival is assumed under the reporting convention unless the contributor reports a failure; this run has no separate explicit arrival confirmation.

[Sanitized result](sixteenth-efi-run-result.json) retains the64 configuration DWORDs and source hash; raw topology log stays ignored. This qualifies the configuration/BAR query in the tested released state, not exclusive ownership, DMA, firmware execution or unlock. Next work combines EFI asset loading/hashing and shared exclusive PCI ownership acquisition/release without GPU firmware upload or DMA launch.
