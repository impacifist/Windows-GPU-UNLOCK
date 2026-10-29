# Fifth AD104 boot: device-wide attributes

English | [한국어](AD104-FIFTH-RESULT.ko.md)

The 2026-10-26 diagnostic returned success for both read-only attribute queries:

```text
current=0x0000000000000718 supported=0x000000000001b77b
```

Current `0x718` includes I/O decoding (`0x100`), memory decoding (`0x200`),
bus mastering (`0x400`), VGA memory (`0x8`) and VGA I/O (`0x10`). PCI command
remained `0007`. The BAR0 descriptor again passed its separate shape check.
BAR-specific support flags were zero; these are distinct from the device-wide
supported attribute mask above.

Applying the arithmetic observed in IMG29's static Stop path gives:
`(0x1b77b & 0x700) | 0x18 = 0x718`. That candidate Disable mask covers all
currently reported attributes. If the driver reaches that path and the
operation succeeds, it could disable both memory decoding and BME, along with
the other listed attributes. This is a prediction from static code plus query
results, not an observed Stop effect. Branch selection, callback success,
partial-failure behavior and Windows recovery remain untested.

No attribute-changing operation, DisconnectController, GPU MMIO, ROM-buffer
copy, driver release or unlock was performed. The log's `query-only=1` and
`unlock-eligibility=0` are explicit. `capture-complete=0` and final
`EFI_NOT_READY` remain expected for this metadata mode.

All 33 capture files were copied into a separate private host archive and
USB/copy SHA-256 values matched. The diagnostic log is 396,263 bytes. USB EFI
SHA-256 matches the prepared artifact
`2949d1f816b0b201c98320d0b762c931571cdf1749bee0c377f07a47292a91b7`.
USB originals remain unchanged. The offline release audit still reports the
location, BME and driver-fingerprint mismatches. No identical query repeat is
needed and no new EFI was staged while inspecting this result.

Next work must define a capture-specific lifecycle and decode-restoration
contract before a mutation test: expected child/parent closures, which decode
changes are acceptable, partial-stop handling and how Windows can boot after
successful verification. Do not simply clear BME or reuse another board's
release routine. This result does not qualify an unlock.
