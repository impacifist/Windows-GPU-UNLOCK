# Twelfth AD104 test: direct owner-child Stop

English | [한국어](AD104-TWELFTH-BOOT.ko.md)

Status: physically tested successfully on 2026-10-27; see [result](AD104-TWELFTH-RESULT.en.md). This adds a fourth
stage after the proven text/GOP-consumer/EDID sequence. It directly calls only
`IMG29 binding->Stop(binding, gpu, 1, &child)`, not DisconnectController, not
zero-child parent Stop. It never requests PCI attribute writes, GPU MMIO, reset,
firmware/NVRAM writes or unlock. Driver-internal legacy video effects remain possible.

Fresh exact code/binding/PCI/graph/PCD/input state must agree twice, and intent
must flush before the one direct call. The postcollector never queries the old
child's protocols or dereferences saved GOP/mode allocations, even after errors.
It requires only the child relationship to disappear: four relevant edges become
three, owner and both pointer edges remain, shared flag zero/counts1/1, command7,
attributes718/support1b77b and full PCI configuration unchanged. Independent PCD
checks remain. A fresh full handle enumeration must no longer contain the old
child; a surviving handle conservatively refuses advancement. Callback error or
partial/unexpected state blocks handoff. No retry/reconnect or automatic rollback.

New risk: this invokes the driver's Stop directly, bypassing the firmware
DisconnectController wrapper. The pinned child branch invokes legacy INT10 video
services and can remove/free the sole GOP child. That can lose display/input,
hang firmware or prevent Windows startup despite retained PCI ownership/BME.
Manual power-off/USB removal may be necessary; recovery is unproven. This extends
the prior consumer-only experiment and needs explicit acceptance before staging.

Validation: full preparation suite passed, including 193 direct-child lifecycle
checks and 33 real display/PCD collector checks; 31 Python tests passed. Faults
cover callback failure before/after partial change, surviving child/edge, lost
owner/pointer, changed PCI/input, log failure, binding drift after intent, wrong
final key and final input drift. No freed child protocol access is permitted.
The native AMD64 EFI is 46,592 bytes, no imports/IAT/delay imports, NX/dynamic base,
SHA-256 `bb82bd260c9b8df0142318692a090b8f28ecf8ecb8e3858593d2f145c8b20975`.
These host checks do not prove physical recovery.

After acceptance and confirmed staging only: save work; fully shut down, wait ten
seconds; ASUS Delete/F8 → UEFI USB DISK2.0. Tap Shift+G once to permit all four
stages, wait five seconds, then tap Shift+V every two seconds for up to one minute.
No second prompt is printed. Do not hold keys or repeat the boot. If stopped/hung
or no Windows after five minutes, power off, remove USB and select normal Windows
Boot Manager; this is an attempted recovery path, not a guarantee.

Success would establish child-only removal on this run, not parent release or
unlock. BME should still be enabled. Preserve every capture before any new test.
