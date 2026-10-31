# Sixth EFI result: first completed IMG13 membership Stop

Movian reports that the prompt remained visible, fresh Shift+G cleared the display, and Windows started automatically. A new ARMED file was created. Contributor EFI: 22528 bytes, SHA256 a10a74bbf528638c745c3873216399b5aff810439b27a5de161ba13e93f10046. EVGA05.TXT: 821126 bytes, SHA256 cbbfad6aadb858cdc6311f656c819e6707e52f274e8e0959df4348c51fbbec1a.

The log records exactly one successful Stop return, successful post-graph checks, two complete captures and ready-windows. The operator's report establishes Windows arrival; ready-windows alone would not. Fresh live ownership/code checks passed before and after the callback.

Independent offline comparison of the complete captured graph finds the two selected IMG13 SimpleTextOut opens removed. Both GOP handles and every other recorded active open are preserved; no handles were added or removed. One additional protocol GUID disappeared from the virtual GOP handle: d3b36f2c-d551-11d4-9a46-0090273fc14d. Its semantics/cause are not qualified here. The effect was therefore broader than just deleting two open records, even though the targeted guard passed. This must remain visible in subsequent lifecycle review.

No IMG16/17, EDID or NVIDIA Stop, GPU MMIO, PCI attribute change, firmware execution or unlock was attempted by this harness. This is one completed console membership test on this machine, not full firmware release or unlock proof, repeated reliability or general recovery qualification.

## Next concrete test: independent saved-result replay

Update via GitHub Desktop, then run from the repo root:

```powershell
python -I research/profile/inspect-evga3090-stop-result.py "D:\RTX3090-UNLOCK\EVGA05.TXT"
```

This only reads the saved log. Expected: two active opens removed, two GOPs preserved, all other active opens preserved, and the additional removed GUID reported. Send the JSON. Preserve the ARMED marker and backups; do not repeat the EFI or delete the marker. Before another hardware stage, review IMG16 and the additional protocol-removal effect. No new EFI stage is authorized by this offline result.

The checker passed against the actual uploaded log and rejected three mutations removing success/completeness evidence. [Sanitized result](sixth-efi-run-result.json).
