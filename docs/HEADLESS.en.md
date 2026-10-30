# Internal headless unlock → Windows

**English** | [한국어](HEADLESS.ko.md) | [README](../README.md)

This v0.3.1 source pre-release documents one successful ASUS BIOS 3057 / RTX 3090
Founders Edition boot on 2026-10-24. All monitors were on RTX. The capture showed
one RTX physical GOP before release and none afterward; the automatic child
completed one guarded transaction and Windows display recovery was observed.
[Evidence](../evidence/headless-handoff/README.md).

The CPU was a Ryzen 7 7800X3D, which contains an iGPU. No active AMD GOP was used.
This does not verify CPUs without an iGPU or other systems.
[Exact compatibility](COMPATIBILITY.en.md) still applies. Headless USB is untested;
the [legacy USB guide](GUIDE.en.md) describes an AMD-display manual route.

## Prerequisites and storage

Main also contains contributor-reported B660M/3070 Ti and B660M/3090 Ti internal
profiles. For these later profiles, use main instead of the v0.3.1 checkout below
and pass the exact `-HardwareProfile` to both Configure and build-unlock. See the
[configuration and evidence limits](COMPATIBILITY.en.md). The default stays B650M.

- Exact documented GPU/PROM, board, BIOS and PCI profile; Windows 11 x64 UEFI,
  Secure Boot disabled, Windows C: fully decrypted and BitLocker protection off.
  The tools do not change encryption.
- Monitor(s) connected to RTX and a connected keyboard for Esc/F8 recovery.
  Check access to firmware Boot Override and Windows Boot Manager before testing.
- Separate existing GPT FAT32 EFI System Partition (ESP), at least **1 GiB**,
  with a drive letter such as U:. Keep the original Windows ESP and recovery
  partition. Never delete the existing 100 MiB Windows ESP to make room.
- Python 3.9+, clang/lld-link 20.1.8, repository-pinned dependencies, and the five
  locally obtained [assets](ASSETS.en.md). Firmware/PROM/BIOS images, drivers and
  private partition-specific EFI binaries are not bundled.
- MSVC/Windows SDK and private fixtures if running the host integration tests.

The scripts do not create, shrink, delete or format partitions. Prepare storage
separately after backup and identify it carefully. A FAT32 basic-data partition
does not satisfy the ESP type requirement. Do not change the type of Windows C:,
the system ESP or recovery partition.

Use read-only inspection to identify the dedicated ESP and existing system ESP:

```powershell
Get-Partition | Format-Table DiskNumber,PartitionNumber,DriveLetter,Size,GptType,Guid,IsSystem
Get-Volume -DriveLetter U | Format-List FileSystem,Size,Path
```

The Windows ESP GUID is the GUID of the current system EFI partition containing
`EFI\Microsoft\Boot\bootmgfw.efi`, not the new dedicated ESP's GUID.

## Configure and build

Extract the [v0.3.1 source ZIP](https://github.com/impacifist/Windows-GPU-UNLOCK/releases/tag/v0.3.1)
or clone the fixed release:

```powershell
git clone --branch v0.3.1 --depth 1 https://github.com/impacifist/Windows-GPU-UNLOCK.git
cd Windows-GPU-UNLOCK
```

If you do not yet have a dedicated ESP, follow only the storage preparation
section of the [legacy internal guide](INTERNAL-BOOT.ko.md#2-별도-파티션-준비),
then return here for the headless build/install commands. Storage preparation is manual.

For a new dedicated volume, back up first, then use Windows Disk Management to
shrink a selected volume by at least 1024 MiB and create a new FAT32 volume only
in that newly unallocated space. Assign an unused letter such as U:. Confirm
the volume's size, label and `IsSystem=False` with the read-only commands above.
Only for that newly created dedicated volume, if its GPT type is still Basic
Data, change it to ESP and inspect the result:

```powershell
Set-Partition -DriveLetter U -GptType '{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}'
Get-Partition -DriveLetter U | Format-List Guid,GptType,Size,IsSystem
```

Never apply this command to C:, the existing Windows ESP, a recovery partition
or a volume containing other data.

In an administrator PowerShell at the repository root:

```powershell
.\internal-boot\Configure.ps1 -UnlockDriveLetter U `
  -WindowsEspGuid 'YOUR-WINDOWS-ESP-GUID'
.\research\headless\build-unlock.ps1 `
  -Clang 'C:\TOOLS\LLVM20\bin\clang.exe' `
  -LldLink 'C:\TOOLS\LLVM20\bin\lld-link.exe'
```

Replace placeholders. Configuration writes ignored local files; build creates a
separate headless wrapper and child with a hash contract. Neither changes boot
settings. Each build is specific to your partitions; a successful local hash is
not a universal EFI image.

## Stage, register and test

```powershell
# Stage only: no boot entry, boot-order or Fast Startup change.
.\internal-boot\Headless.ps1 -Assets '.\assets'

# Stage and append a separate F8 entry, retaining existing boot priority.
.\internal-boot\Headless.ps1 -Assets '.\assets' -Register
```

Do not use the older `Install.ps1` or an AMD-display Auto EFI for this route.
The headless installation places `HEADLESS.EFI` and `UNLOCK-HEADLESS.EFI` in the
dedicated ESP, preserving existing Windows EFI files and the original manual EFI.
Keep local state/backups; they are recovery inputs, not public release files.

Save work, fully shut down, power on and select **F8 → GPU Headless UNLOCK -
EXPERIMENTAL** on the tested ASUS firmware. **Do not press T, U or Enter.**
Wait for the automatic path. Do not reset between unlock and Windows.

| Stage | Expected behavior / Esc |
|---|---|
| Initial wrapper, five seconds | Esc skips all driver release/unlock and boots Windows |
| Driver release | RTX output disappears; black screen is expected |
| Automatic child, later three seconds | Esc cancels into a stop after release; it does not restore display or boot Windows |
| Successful child + cleanup | Direct Windows Boot Manager start without reset; NVIDIA display recovery expected |

The wrapper preloads Windows and the verified child before release. Failures stop
instead of retrying or proceeding to Windows. A black screen by itself does not
prove success. Confirm Windows display recovery, inspect local logs and run the
[benchmark](BENCHMARK.en.md) before promoting the boot path.

## Enable default automatic boot

Only after personally confirming the F8 headless trial, run:

```powershell
.\internal-boot\Headless.ps1 -EnableAutomatic -ConfirmedSuccess
```

This changes firmware priority to headless first and Windows Boot Manager second,
and sets Fast Startup off. Registering the test entry alone does neither.
It does not reboot. Do not treat host test success as confirmation of your F8 trial.
Publishing v0.3.1 does not promote the research PC's current boot settings.

## Recovery

If the attempt stops, is cancelled in the child, or Windows does not return, fully
power off. On the next power-on use **F8 → Windows Boot Manager** to bypass unlock.
Do not try another unlock in the same firmware session.

From Windows administrator PowerShell:

```powershell
.\internal-boot\Headless.ps1 -RestoreWindowsFirst
```

Windows-first recovery works independently of experimental EFI/build files and
even without saved state. The previous Fast Startup setting is restored only if
its saved value is available. Keep local state at
`internal-boot/install/headless-state.json` and the backups. If recovery reports an error, inspect firmware
boot choices and use Windows Boot Manager directly; do not assume a partial
recovery restored every setting. No partition deletion or formatting is needed.

## Evidence and limits

The single measured Windows boot reached three-run medians **FP16 124.969948** and
**BF16 128.931534 TFLOPS**, with FP32 22.845572, CUDA/cuBLAS 12.6 and driver 617.14.
The child recorded one FLR, BME off, no remaining mappings and clean completion.
[Sanitized measurements](../evidence/headless-handoff/summary.json).

No contemporary controlled locked baseline establishes a percentage gain.
Repeated default automatic cold boots, long-term stability, games/ComfyUI,
sleep/hibernate resume, other CPUs/cards/BIOSes and headless USB remain unverified.
The successful F8-selected automatic child is distinct from proving repeated
firmware-default operation. Source guards are retained; host mocks are not hardware evidence.
