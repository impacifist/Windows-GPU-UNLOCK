# Start with capture-only setup

**English** | [한국어](CAPTURE-SETUP.ko.md) | [Full local-profile workflow](NEW-MACHINE.en.md)

For a new contributor, collect diagnostics before considering profile generation or
unlock. This workflow does not require the five firmware assets or MSVC. It needs
Windows PowerShell, Python 3.10+, LLVM `clang`/`lld-link`, and an existing FAT32 USB.
The script checks the USB target; it does not format or partition it.

## New to this? Start here

1. Download the repository ZIP from GitHub's **Code → Download ZIP** menu and use
   Windows **Extract All**. Do not run files from inside the ZIP. Git is not needed.
2. Open the extracted folder and double-click **Start-Capture.cmd**. A text window
   stays open so you can read the result. This is capture setup, not an unlock button.
3. Follow the tool checks. If something is missing, the window gives the official
   Python/LLVM download links. Install it yourself, then close and reopen setup.
   Existing LLVM installations in the standard folder are found even with a stale PATH.
4. Setup detects a supported Windows EFI partition automatically. Select your USB
   from its label, model, size and drive letter. Unusable choices explain why.
   Back up the USB first. Type **PREPARE** only after checking the displayed target;
   pressing Enter cancels. No formatting, automatic restart or firmware setting
   changes are performed.
5. Read **Attended collection and return** below before booting. After each return
   to Windows, run the same launcher and choose the same USB. It stops at capture
   collection and cannot progress into unlock.

If Windows blocks the downloaded script, stop and check that you downloaded the
intended repository revision. Ask a maintainer for help; do not disable system
security or execution policy. The launcher still requires installed Python and
LLVM: it is guided setup, not a dependency installer. Unsupported/ambiguous boot
layouts need maintainer help rather than guessed partition IDs. The current text
prompts are in English; this page also has a Korean counterpart.

## Check host tools

From the repository root in a normal terminal:

```powershell
python .\tools\check_prerequisites.py --phase capture
```

Exit 0 means discovery/version checks passed, not that a build or hardware test
passed. The JSON records the actual executable paths and versions; LLVM version
output is checked for identity, not against a newly certified version range.
If Python cannot start, install Python 3.10+ first. If tools are missing after
installation, reopen the terminal. Explicit paths avoid relying on a stale PATH:

```powershell
python .\tools\check_prerequisites.py --clang 'C:\Program Files\LLVM\bin\clang.exe' --lld-link 'C:\Program Files\LLVM\bin\lld-link.exe'
```

This runs version commands only. It does not install tools, persist environment
changes, run a compiler build or access the GPU. `--phase profile` additionally
discovers `vcvars64.bat` (or accepts `--vcvars`); discovery does not validate MSVC
linking or execute the batch file. Check the [asset guide](ASSETS.en.md) separately
before continuing to host replay.

## Stage the capture probe explicitly

Identify your Windows system EFI partition and USB drive letter; do not copy
another person's GUID or assume their drive letter applies:

```powershell
Get-Partition | Where-Object IsSystem | Select-Object DiskNumber,PartitionNumber,Guid,Type
Get-Volume | Select-Object DriveLetter,FileSystemLabel,FileSystem,Size
```

For a GPT/EFI Windows setup, substitute your own values:

```powershell
.\research\profile\new-machine.ps1 -Drive D -WindowsEspGuid 'YOUR-WINDOWS-ESP-GUID' -CaptureOnly
```

Pass the same `-Clang` and `-LldLink` paths here if needed; the doctor does not save
them. `-Python` selects the Python executable for the build verification script.
The script builds the generic diagnostic EFI and writes it to the chosen USB,
preserving the previous loader and older capture files through the existing backup
path. It does not reboot the PC. CaptureOnly cannot be combined with `-Assets` or
`-StageUnlock`.

## Attended collection and return

After reviewing the [probe scope and boot guidance](NEW-MACHINE.en.md#steps), shut
down fully, boot this USB from the firmware boot menu, and let the probe attempt
Windows handoff. Repeat from a separate full shutdown for the required captures.
Diagnostic reads can still hang or fail to return to Windows. If necessary, power
off, remove the USB and select your normal Windows Boot Manager; recovery is not
guaranteed. Do not change firmware security settings merely to complete this guide.

Rerun the same CaptureOnly command. Once enough complete logs exist, it reports
collection complete and exits **without** generating a profile, replaying assets
or building/staging unlock. File completeness is not a compatibility verdict.
Keep original `DIAGnn.TXT`, `IMGnn.BIN` and `PROM.BIN` under `RTX3090-UNLOCK` for
local analysis; that is the existing directory name. Review and sanitise reports
before sharing; do not publicly attach firmware dumps.

Only when explicitly continuing beyond capture, follow the full workflow with
`-Assets` and its existing admission/replay gates. Omitting assets can still stage
the initial probe for older commands, but now stops before profile generation
when captures are ready. Unlock staging still requires `-StageUnlock` and assets.
