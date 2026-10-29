# Seventh AD104 boot: one EDID disconnect

English | [한국어](AD104-SEVENTH-BOOT.ko.md)

This is the first lifecycle mutation test. It is not an unlock test. The
operator explicitly accepted possible lost display/input, firmware hang, manual
power-off and failure to return to Windows. Recovery is unproven; this PC has
no integrated GPU. See [the complete contract](AD104-EDID-EXPERIMENT.en.md).

The EFI first captures a baseline, then performs new exact identity, code,
eight-edge graph, shared-state, full PCI configuration, BAR and attribute checks.
Prepared Windows boot-image bytes are pinned and verified by the MBR handoff.
At the first prompt, press **Shift+E** (uppercase `E`) to authorize one
`DisconnectController` on the discovered GOP child using only IMG08's binding.
Any other key or input timeout stops without invoking it. There is no automatic
Windows fallback in this test, even if it stopped before the disconnect.

The intent record must be flushed before the one call. Regardless of its
returned status, two independent postcollections follow. Expected: only the
EDID relationship disappears (eight edges become seven), shared byte `1→0`,
pointer counts remain `1/1`, and all other pinned state stays unchanged.
Only if these checks and callback status pass will the next prompt ask for
**Shift+V** (uppercase `V`). This verifies actual keyboard input after the change.
Another fresh postcheck and successful result logging/file closes are required
before attempting Windows. There is no retry, reconnect, owner release, GPU
MMIO, ROM copy, attribute write, NVRAM change or unlock transaction.

Each key prompt has a 60-second polling budget; firmware callbacks may hang.
Do not hold keys or type ahead. If the display is lost, do not blindly type or
repeat the boot. If it stops, hangs or Windows has not returned within five
minutes, power off, remove the USB and select normal Windows Boot Manager.
This is a recovery attempt, not a guarantee. Preserve the USB files before
another EFI run; do not run this test twice to chase a missing result.

Save work, leave the capture USB connected, run `shutdown /s /t 0`, and wait
ten seconds after power-off. Power on with ASUS Delete, then F8 and select
**UEFI: USB DISK 2.0** (8GB). Do not change BIOS settings.

Build implementation `b45ddd6`, using `-Ad104Msi -Ad104Metadata -Ad104Resources
-Ad104Preflight -Ad104EdidTest -WindowsMbr`. EFI: 33,280 bytes, SHA-256
`5503b5fc3cda5cac2d9bc852d669ec509890f94800a205510da54b099b9d69f7`.
Manifest intentionally reports `CanDetach=true`, `CanUnlock=false`,
`CanReadGpuMmio=false`, `CanDereferenceRomImage=false` and `HardwareTested=false`.
Native AMD64 PE verification passed: no imports/IAT/delay imports, NX/dynamic
base. RAM-only checks passed: 84 lifecycle, 27 topology, 15 handoff, 151 deep
AD104, 24 metadata, 113 ROM-buffer, 66 resources, 66 MBR, four GPT, 22 rejected
compile combinations and 31 Python tests. These do not prove physical recovery.

Before staging, the sixth capture and previous EFI/manifest must remain in
verified host and USB archives. The legacy `RTX3090-UNLOCK` folder still names
the diagnostic output directory for this RTX 4070 Ti; no 3090 profile is used.
