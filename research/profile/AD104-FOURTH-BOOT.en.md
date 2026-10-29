# Fourth AD104 test: BAR0 resource observation

English | [한국어](AD104-FOURTH-BOOT.ko.md)

Question: does the exact GPU's firmware-reported BAR0 descriptor match the shape
expected by the existing deep-capture envelope? The first boot stopped at BME
before it could query that descriptor. This separate metadata mode calls only
`GetBarAttributes` and PCI configuration/location queries, not GPU MMIO methods.
BME stays unchanged. ROM-buffer copying is disabled and cannot be combined with
resource mode at compile time. No driver release, firmware upload or unlock.

Build: `build-probe.ps1 -Ad104Msi -Ad104Metadata -Ad104Resources -WindowsMbr`.
Output `internal-boot/boot/build/probe-ad104-resources/BOOTX64.EFI`:
23,040 bytes, SHA-256
`cf3bd58ef496604d653fc4d21da6030f27eb326dea49b5131ca590026d80014d`.
Built from implementation `c671d99`; native AMD64 EFI PE verification passed
with no imports/IAT/delay imports, NX and dynamic base enabled. Not yet booted.

Exact identity and single-display checks precede the query. The probe checks
the same interface, BDF and full 256-byte PCI configuration before and after.
It records status, descriptor validity, granularity, minimum/maximum, length,
translation and support flags, then frees the returned pool allocation.
The UEFI protocol supplies no allocation length: the implementation trusts
firmware's descriptor-list allocation contract, as the existing envelope does.

`descriptor-valid=1` and `envelope-shape=1` describe the descriptor only. They
do not establish full MMIO admission: memory decoding, BME, alignment, stable
identity and other checks remain separate. Errors and missing descriptors are
valid negative observations. `capture-complete=0` and final `EFI_NOT_READY`
remain expected; no unlock eligibility follows from this test.

Checks passed: 40 resource tests, 151 deep AD104, 24 metadata, 113 firmware-buffer,
66 MBR, four GPT, 20 rejected compile combinations and 17 Python tests. Resource
tests cover BME enabled, shape mismatch, checksum, null descriptor, malformed
length/end tag, callback error, identity drift and wrong/multiple devices.

Earlier captures and EFI files must be preserved before staging; the active
`RTX3090-UNLOCK` folder must be empty. Save work, leave the capture USB connected,
run `shutdown /s /t 0`, wait ten seconds after power-off, then use ASUS Delete →
F8 → the UEFI entry for the 8GB USB DISK 2.0. No test key is required.
If Windows does not return within four minutes, power off, remove USB and boot
normal Windows Boot Manager. Automatic return is not guaranteed. On return,
preserve the new log/images before another boot. No BIOS mode changes are needed.
