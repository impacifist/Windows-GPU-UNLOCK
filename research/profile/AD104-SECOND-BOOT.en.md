# Second AD104 test: firmware ROM metadata

English | [한국어](AD104-SECOND-BOOT.ko.md)

This user-present test asks whether the exact MSI AD104 device exposes a
firmware-provided PCI I/O ROM buffer pointer and size. It records only pointer
presence, never the pointer address or its contents. Existing ownership/image
capture still runs and can show the legacy-video protocol graph.

Build with `-Ad104Msi -Ad104Metadata -WindowsMbr` in `build-probe.ps1`.
Output: `internal-boot/boot/build/probe-ad104-metadata/BOOTX64.EFI`.
Native artifact built from the implementation at `6dec9ce`: 20,992 bytes,
SHA-256 `92e038ddf621a227ac37c7afba2d589272d0a39ad431ad769c5f1f8730f6792d`.
PE verification passed: AMD64 EFI application, no imports/IAT/delay imports,
NX and dynamic base enabled. The manifest explicitly marks metadata-only mode
and GPU MMIO disabled. This is build evidence, not a physical test result.

The exact PCI/subsystem/revision is checked before labeling metadata. The mode
returns before BAR resource inspection, PROM allocation, GPU MMIO or ROM-pointer
dereference. It cannot detach drivers, clear BME, upload firmware or unlock.
It intentionally logs `ad104-metadata-only-no-mmio`, `capture-complete=0` and
`EFI_NOT_READY`; those are expected, not a request to bypass admission. A zero
BAR0 is still rejected by the caller and would leave ROM availability unknown.
Reported pointer/size are unvalidated firmware fields and establish neither
readability, complete ROM contents nor equivalence to physical PROM.

Host tests passed: 151 original AD104 checks, 24 metadata checks (including
invalid pointer value 1, maximum reported size and wrong identity), 66 MBR,
four GPT and 17 rejected compile combinations. The 15 Python tests also passed.

## Before and after boot

Archive all first-boot files and the old EFI before replacing it. Do not format
the USB or alter internal boot settings. The active `RTX3090-UNLOCK` directory
must be empty because existing dumps are never overwritten. Its legacy name
does not select a 3090 profile.

Save work, leave the capture USB connected, then run `shutdown /s /t 0`.
After full power-off, wait ten seconds. Enter ASUS BIOS with Delete, open the
F8 boot menu and select the UEFI entry for the 8GB USB DISK 2.0. No test key is
required. The diagnostic attempts the pinned Windows Boot Manager afterward.
If Windows does not return within four minutes, power off, remove this USB,
then boot the normal Windows Boot Manager. Automatic return is not guaranteed.

After returning, do not boot the stick again. Preserve `DIAG00.TXT` and all new
images. Look for `ad104-metadata-only=1 rom-pointer-present=... rom-bytes=...`.
Both zero, inconsistent fields, or absent observations are valid findings; do
not infer that missing metadata means the physical ROM is absent. A nonzero
pointer and plausible size would justify designing a separately bounded buffer
capture, not running an unlock. No ROM-buffer copy is implemented in this test.
