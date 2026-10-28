# Windows GPU UNLOCK

**English** | [한국어](README.ko.md)

Experimental UEFI GPU unlock and direct Windows handoff, expanded through
community contributions.

[![Join our Discord community](https://img.shields.io/badge/Join%20Discord-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/WjKsjEqxF)
[![View compatibility and supported configurations](https://img.shields.io/badge/Compatibility-455A64?style=for-the-badge)](docs/COMPATIBILITY.en.md)
[![Contribute research and improvements](https://img.shields.io/badge/Contribute-37474F?style=for-the-badge&logo=github&logoColor=white)](docs/CONTRIBUTING.en.md)

## Community

Join our [Discord community](https://discord.gg/WjKsjEqxF) for conversations,
work-in-progress findings, and collaborative development and research.
Keep substantial research results, reproducible test reports, code reviews,
and project decisions in [GitHub issues](https://github.com/impacifist/Windows-GPU-UNLOCK/issues)
and pull requests so everyone can find and review them. Please read the
[community rules](docs/DISCORD-RULES.en.md).

## GPU support

**Success reports apply to specific cards and systems, not every card of the same
GPU model.** Check the linked motherboard, BIOS and card requirements before building.

| GPU | Current status | Details |
|---|---|---|
| **RTX 3060 12GB (GA104 / GA106)** | Hardware success reported; headless USB only | [GA104](docs/COMPATIBILITY.en.md#profile-4-asus-prime-b660-plus-d4--rtx-3060-12gb-headless-usb) · [GA106](docs/COMPATIBILITY.en.md#profile-5-gigabyte-h610m-h--rtx-3060-12gb-ga106-headless-usb) · [GA104 B550](docs/COMPATIBILITY.en.md#profile-7-gigabyte-b550-aorus-elite-v2--rtx-3060-12gb-ga104-headless-usb) |
| **RTX 3070 Ti 8GB** | Hardware success reported; internal headless | [Supported configuration](docs/COMPATIBILITY.en.md#profile-2-asus-tuf-b660m-e-d4--rtx-3070-ti-aggregate-console) |
| **RTX 3080** | No dedicated profile; Windows display-routing research available | [Research observations](research/rtx3080/README.md) · [Project origins](THIRD_PARTY_NOTICES.md) |
| **RTX 3090 FE 24GB** | Hardware success recorded; internal headless; default profile | [Supported configuration](docs/COMPATIBILITY.en.md#profile-1-asus-tuf-b650m--rtx-3090-fe-original) |
| **RTX 3090 Ti 24GB** | Hardware success reported; route depends on exact system | [X870 USB](docs/COMPATIBILITY.en.md#profile-3-gigabyte-x870-aorus-elite-wifi7--rtx-3090-ti-headless-usb) · [B660M internal](docs/COMPATIBILITY.en.md#profile-6-asus-tuf-b660m-e-d4--colorful-rtx-3090-ti-internal) |

The [compatibility guide](docs/COMPATIBILITY.en.md) lists exact build profiles and
distinguishes maintainer tests from contributor reports. Results concern their
recorded builds; later main-branch hardening has host/build checks, **not a new
physical boot or recovery test**.

The latest tagged release, **v0.3.1, is a source-only pre-release**. This source
tree has changes beyond that tag, including the USB route and X870 profile; those
are not a newly published release. Build locally with your own partition GUIDs.
Firmware assets and private, partition-specific EFI binaries are not bundled.

## Choose the correct boot path

| Path | Display and input | Instructions |
|---|---|---|
| **Internal headless** | B650M/3090 FE or B660M/3070 Ti / 3090 Ti profile; monitors on RTX; display temporarily removed; no T/U/Enter | [Internal headless guide](docs/HEADLESS.en.md) |
| **Headless USB** | Reported RTX 3060 / 3090 Ti configurations; profile-specific wrapper and child on a FAT32 USB stick | [Headless USB guide](docs/HEADLESS-USB.en.md) |
| Internal AMD-display manual / Auto | Original B650M profile, motherboard/iGPU display preserved; manual variant requires U | [Existing internal guide](docs/INTERNAL-BOOT.en.md) |
| Legacy USB manual | Original AMD/iGPU display profile; U once, then Boot Override to Windows without reset | [Legacy USB guide](docs/GUIDE.en.md) |

X870, B660-PLUS and H610M are not wired into `Configure.ps1` or `Headless.ps1`; use the USB guide.
USB packages can also be built for the first two profiles, but their recorded
internal boots do not validate those USB routes. The legacy USB EFI retains its
AMD display requirement. BIOS priority `USB → Windows` alone does not implement
direct handoff.

## Internal headless quick start

**This example is for the default B650M / RTX 3090 FE profile.** For B660M,
select `-HardwareProfile b660m-3070ti` in both configuration and build as described
in [compatibility](docs/COMPATIBILITY.en.md). X870 uses the separate USB route.

Read the [full guide and recovery procedure](docs/HEADLESS.en.md) before changing
boot settings. Use the exact supported hardware, Secure Boot disabled, a fully
decrypted Windows C: with BitLocker protection off, and a connected keyboard.
The tools do not disable encryption.

Preserve the **existing Windows ESP and recovery partition**. Prepare a separate
GPT FAT32 **EFI System Partition of at least 1 GiB**, with a letter such as U:.
Tools do not create, shrink, delete or format partitions. An ordinary FAT32 data
partition alone is not the required ESP.

You need Python 3.9+, clang/lld-link **20.1.8**, the pinned repository dependencies
and five locally obtained [firmware assets](docs/ASSETS.en.md).
Host integration tests additionally need MSVC/Windows SDK and private fixtures.

From an administrator PowerShell in the repository root:

```powershell
# Replace the GUID and tool paths with your own values.
.\internal-boot\Configure.ps1 -UnlockDriveLetter U `
  -WindowsEspGuid 'YOUR-WINDOWS-ESP-GUID'
.\research\headless\build-unlock.ps1 `
  -Clang 'C:\TOOLS\LLVM20\bin\clang.exe' `
  -LldLink 'C:\TOOLS\LLVM20\bin\lld-link.exe'

# Stage and append a separate F8 test entry; retain existing boot priority.
.\internal-boot\Headless.ps1 -Assets '.\assets' -Register
```

Without an action switch, `Headless.ps1 -Assets '.\assets'` only stages files.
Configuration and build do not register a boot entry.

1. Save work and fully power off. On the tested ASUS firmware, select
   **F8 → GPU Headless UNLOCK - EXPERIMENTAL**.
2. **Wait without T, U or Enter.** Initial Esc within five seconds skips release
   and unlock and boots Windows directly.
3. The screen becomes black while the RTX GOP is removed. The child attempts
   once and the wrapper starts Windows directly. Do not reset between unlock and
   Windows. Confirm display recovery and [measure](docs/BENCHMARK.en.md).
4. Only after that F8 test succeeds, enable default automatic boot:

   ```powershell
   .\internal-boot\Headless.ps1 -EnableAutomatic -ConfirmedSuccess
   ```

That explicit action places headless first and Windows Boot Manager second and
disables Windows Fast Startup. Test registration alone does not. Publishing this
release does not change the research PC's current default boot selection.

**The two Esc windows differ:** initial wrapper Esc boots Windows without release;
the later three-second child Esc window is after release and cancels into a stop.
It does not restore the display or continue to Windows. On failure or if Windows
does not return, fully power off and choose **F8 → Windows Boot Manager**.

```powershell
.\internal-boot\Headless.ps1 -RestoreWindowsFirst
```

Windows-first recovery is independent of experimental EFI/build artifacts and
still works without saved state. Restoring the prior Fast Startup setting needs
that state; retain it and backups. [Recovery details](docs/HEADLESS.en.md#recovery).

## Original B650M results and limits

The original B650M hardware run's successful child reported SS0 `0x88888888`, SS1 `0x8`, BME disabled, zero
remaining mappings, completed cleanup and one FLR. The wrapper verified the released
topology before Windows entry. Windows display recovery was observed by the operator.

| CUDA 12.6 / cuBLAS 12.6, 4096 × 4096 | Three-run median |
|---|---:|
| FP32 pedantic | 22.85 TFLOPS |
| FP16 Tensor / FP32 accumulation | 124.97 TFLOPS |
| BF16 Tensor / FP32 accumulation | 128.93 TFLOPS |

Future runs of the [benchmark tool](docs/BENCHMARK.en.md) also measure TF32 separately.
The historical measurements above predate that addition and have no TF32 result.

There is **no contemporary controlled locked baseline** for this headless boot,
so these values do not establish an unlock percentage. They do not establish game
or ComfyUI gains, long-term stability, sleep/hibernate resume or repeated automatic
cold-boot reliability. [Sanitized measurements](evidence/headless-handoff/summary.json)
and [earlier results](docs/RESULTS.en.md) describe separate experiments.

For the other machines' measurements, see [B660M Profile 2](docs/COMPATIBILITY.en.md),
[X870 USB evidence](evidence/x870-3090ti-usb/README.md) and
[B660-PLUS / 3060 USB evidence](evidence/b660plus-3060-usb/README.md) (controlled locked
baseline: FP16 1.88–1.92×, BF16 1.92–1.95×, TF32 1.93–1.94× over two boots). Those results are tied
to their reported builds; a later successful build or host test does not extend
the physical test record.

## Check existing captures

An [offline compatibility checker](docs/CHECK-COMPATIBILITY.en.md) explains whether
existing probe captures fit the automatic local-profile route and what is missing.
It needs only Python 3.10+ and runs no probe or unlock:

```powershell
python -I research/profile/check-compatibility.py "D:\RTX3090-UNLOCK"
```

A candidate still needs profile generation, host tests and hardware validation.
An out-of-scope result does not rule out an exact existing fixed profile.
Use `--json` for a reviewed-shareable report with limited model/topology observations;
paste it into the optional offline report field in a [compatibility issue](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/new?template=compatibility.yml).
The guide also covers comparing two reports to investigate common differences.

## Implementation and boundaries

The [GPU family layer](docs/GPU-FAMILIES.en.md) shares reviewed GA102/GA104/GA106
traits and identical firmware templates, while exact card/platform pins remain
separate. Existing profile names and support scope are unchanged.

Headless is a separate compile-time profile. Exact GPU/PROM/PCI state, firmware
driver code, ownership, cold state, immutable selectors and cleanup checks remain.
The child logs to files only and rejects physical GOP reappearance or changed
console/input interfaces. Ordinary builds retain their AMD display guards.
Do not remove checks or substitute a PROM hash to force another machine through.

| Path | Purpose |
|---|---|
| `internal-boot/`, `research/headless/` | Internal/headless source, local build, installation and host tests |
| `unlock/`, `release/` | Preserved legacy USB manual implementation and historical artifacts |
| `benchmark/`, `evidence/` | Measurement tools and selected public evidence |
| `docs/` | Compatibility, assets, boot guides and limitations |

No VBIOS flashing or Windows driver replacement is part of the procedure.
Driver 617.14 was unchanged in the documented run. Host mock tests do not add hardware
support. New systems require [compatibility reporting](docs/CONTRIBUTING.en.md) and
[portability validation](docs/PORTABILITY.en.md). A machine inside the hardware-proven envelope
can build and test its own local profile with the [new-machine pipeline](docs/NEW-MACHINE.en.md).
This experimental workflow inventories NVIDIA display devices, but deep capture is
limited to one RTX 3090 / 3090 Ti in the reviewed PCI/BAR state. A generated local
profile does not add another verified machine profile. USB write builds require all
19 host jobs (20 when the firmware binds no EDID driver) and unchanged profile,
fixtures and build inputs; hardware validation is still required. Within this envelope
the bus is pinned per machine, and single-RTX systems may have no iGPU or separate
EDID driver. Aggregate consoles and multiple NVIDIA GPUs are not generated.
Staging the unlock package needs `-StageUnlock`.

## License and attribution

The port and reused core are **GPL-2.0-only**; EDK2 and separately attributed
components retain their licenses. Based on
[rough2515-hub/rtx3080-unlock-public](https://github.com/rough2515-hub/rtx3080-unlock-public/tree/a08254d269af30ace9e57237abd3d8e6c261e8f5)
and [Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock/tree/0ebdb1c6308b6a2072be5bf04513472942541956).
Preserve [LICENSE](LICENSE), [third-party notices](THIRD_PARTY_NOTICES.md) and
[change history](CHANGES.md). NVIDIA firmware, PROM/BIOS images, drivers and CUDA
DLLs are not redistributed in this source release.
