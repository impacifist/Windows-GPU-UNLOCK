# Third AD104 test: bounded firmware ROM buffer

English | [한국어](AD104-THIRD-BOOT.ko.md)

The second boot reported a non-null PCI I/O ROM buffer of `0x24a00` (150,016)
bytes. This test asks whether that exact firmware-provided buffer can be read
consistently and has a bounded first option-ROM/PCIR header for `10de:2782`.
It does not read the physical PROM through GPU BARs or establish unlock support.

Build switches: `-Ad104Msi -Ad104Metadata -Ad104FirmwareRom -WindowsMbr`.
Output: `internal-boot/boot/build/probe-ad104-firmrom/BOOTX64.EFI`.
Implementation commit `e0b91bf`, 24,064 bytes; SHA-256:
`45be1100b3042d980e56da1c7553842537b302157117d92dac1585953d2dfe53`.
Native AMD64 EFI PE verification passed: no imports/IAT/delay imports, NX and
dynamic base enabled. Physical execution of this artifact remains untested.

## Admission and expected results

Exact PCI/subsystem/revision and a single NVIDIA display target remain required.
The buffer size must equal the second boot's observation. The whole pointer
extent must fit one non-runtime, non-read-protected Loader/Boot Services RAM
descriptor in a valid memory map bounded to 64 KiB. Reserved, MMIO, overlapping,
split, malformed or insufficient mappings are refused without reading the ROM
pointer. This restriction may prevent capture on a legacy firmware layout; that
is a useful refusal, not a reason to weaken memory admission.

Device handle/protocol, BDF, all 256 PCI configuration bytes and ROM pointer/size
are rechecked. A copied buffer must contain AA55, a bounded PCIR header with the
expected vendor/device and valid first-header lengths. A second memory-map check
and byte comparison test consistency. These observations are not an atomic
snapshot or a guarantee against arbitrary firmware bugs.

Only successful write, full length, flush and close produce the log record
`firmware-rom-buffer dump=FIRMROM.BIN ... sha256=... unlock-eligibility=0`.
Existing dumps are refused. On failure, inspect `firmrom-*` reasons; a partial
file is not accepted without its success record and matching hash. No ROM bytes
are executed. BME stays unchanged and no GPU MMIO method is called.

`capture-complete=0` and the final `EFI_NOT_READY` remain intentional: this mode
does not capture the GPU register/PROM evidence required for a complete profile.
Even a successful `FIRMROM.BIN` is a firmware buffer, not proven complete physical
ROM contents or a qualified firmware transaction.

Host checks passed: 113 firmware-buffer checks, 151 deep AD104 checks, 24 metadata
checks, 66 MBR checks, four GPT checks, 18 rejected compile combinations and 15
Python tests. The tests include unreadable pointers, memory admission, changing
metadata/content, header bounds and malformed output handles.

## Attended boot

Preserve earlier captures and boot files before staging. The active legacy-named
`RTX3090-UNLOCK` folder must be empty. No disk formatting or BootOrder changes
are needed. Save work, leave the capture USB connected and run
`shutdown /s /t 0`. Wait ten seconds after power-off. Power on, use Delete to
enter ASUS BIOS, then F8 and select the UEFI entry for the 8GB USB DISK 2.0.
No test key is required. The diagnostic then attempts the pinned Windows loader.

If Windows does not return within four minutes, power off, remove this USB,
and boot normal Windows Boot Manager. Automatic return is not guaranteed.
After returning, preserve the new log and all dumps before another boot. Do not
publish raw firmware. A refusal is an expected possible result; do not retry the
same image repeatedly or change BIOS settings to force it through.
