# Next hardware test: production shared-ESP handoff and capture

[한국어](SHARED-ESP-INTEGRATION.ko.md)

The contributor supplied the RETAIN01 COMPLETE photo and reported normal Windows boot, a COMPLETE receipt (SHA-256 `f8b09577c3919b8eba2627e0df6be22d6a1814674b6a69f78894afc86954d2c6`) and verified original USB restoration. INTG01 now combines the actual `handoff.c` shared-ESP `Prepare`, file logger/Flush and `Windows` close-and-start path with the production `GuCaptureGraphOnVolume` graph capture. It also retains, rehashes and frees all five assets. It reads protocol metadata and PCI configuration to capture the graph; it performs no driver Stop/disconnect, GPU/register/PCI writes, firmware-asset execution, DMA, FLR, NVRAM writes or unlock. EVGA Stop/transaction/vendor code is not linked.

**Risk:** UEFI creates a new log (graph capped at 8 MiB, plus status lines) and writes a dedicated 512-byte marker on the active shared ESP. It also reads firmware/GPU interfaces and retains about 85 MB of memory. Firmware bugs, filesystem errors, interruption, exhaustion or hangs can affect data and boot; recovery is not guaranteed. Back up ESP and USB first. Old assets, receipts, captures, test EFI files and backups are retained. Never delete/reset a marker to retry. Staging refuses existing INTG01 files and requires the exact COMPLETE RETAIN01 receipt. Runtime consumes the pending marker before `Prepare`, and refuses an existing log rather than selecting a later slot. Firmware CREATE is not atomic create-new; concurrent external writers remain a risk.

Run from the updated repository root in administrator PowerShell. Replace USB `D` with its verified actual letter; keep the working compiler arguments if needed. No asset copy is needed. Check the prior USB restoration succeeded before proceeding.

```powershell
$esp='c36c3e86-02aa-4f76-af1b-96e327f9d1c1'
$usb='D'
./research/profile/build-shared-esp-integration-launcher.ps1 -WindowsEspGuid $esp
./research/profile/test-shared-esp-integration.ps1
./research/profile/test-shared-esp-integration-lifetime.ps1
./research/profile/test-shared-esp-integration-launcher.ps1
./research/profile/stage-shared-esp-integration.ps1 -WindowsEspGuid $esp -WhatIf
./research/profile/stage-shared-esp-integration.ps1 -WindowsEspGuid $esp
$original=(Get-FileHash "$usb`:\EFI\BOOT\BOOTX64.EFI").Hash
$original # Save outside USB before continuing.
./research/profile/stage-shared-esp-integration-launcher.ps1 -Drive $usb -WindowsEspGuid $esp -ReuseExisting -WhatIf
./research/profile/stage-shared-esp-integration-launcher.ps1 -Drive $usb -WindowsEspGuid $esp -ReuseExisting
```

New ESP paths are `EFI/EVGA-SHARED-INTEGRATION.EFI`, `RTX3090-UNLOCK/INTG01.ARMED` and, at runtime, `RTX3090-UNLOCK/INTG01.TXT`. The USB backup is `EFI/BOOT/BOOTX64.before-shared-integration.efi`, separate from earlier backups. Existing Windows/Limine hash pins and 16 MiB free reserve are checked. `-WhatIf` skips writes and boot-file hash checks inside the write block. Any error: stop, preserve files/output and report. Scripts do not change boot entries or reboot.

Boot USB **once** using the previous successful firmware selection. Photograph `INTG01 ENTERED` and the final `INTG01 COMPLETE` or `INTG01 FAILED` screen, and report Windows boot. COMPLETE means this invocation's Prepare, retained asset checks/releases, graph capture and pre-handoff status writes succeeded. The final display pauses ten seconds before the production Windows path closes log/root and attempts StartImage. A marker can say COMPLETE before a later log/close/handoff failure; require the photo, log and boot report together. A partial capture never authorizes unlock. On failure after successful Prepare, Windows is attempted only if the marker write/close succeeded; an earlier failure can return to firmware. If no Windows within three minutes, power off, remove USB and use ordinary Limine/Windows. Do not repeat a failed run.

In Windows, collect just the two small test evidence files, then restore USB:

```powershell
./research/profile/read-shared-esp-integration.ps1 -WindowsEspGuid $esp -OutputDirectory "$env:USERPROFILE\Desktop\INTG01-results"
./research/profile/restore-shared-esp-integration-launcher.ps1 -Drive $usb -OriginalSha256 $original
```

The read script refuses an existing output directory and never writes the ESP. Send its JSON output, `INTG01.ARMED`, `INTG01.TXT`, both photos, restore output and boot results privately; logs contain device paths/addresses. Firmware assets are not requested. Preserve ESP files. Remove USB and confirm ordinary Windows/CachyOS Limine boot. In a new PowerShell session, replace `$original` with the hash saved outside USB.

Local validation on 2026-10-28: eight direct production Prepare/logger/Windows cases, nine integrated retention/marker cases, ten launcher cases and staging/backup/restore/evidence fixtures passed. Production diagnostic host suite passed 56 checks; its existing probe/profile regressions also passed using the installed VS2019 BuildTools environment (the script's default VS2022 path is absent). Host mocks are not firmware proof. PE checks passed AMD64/no imports/NX/dynamic base. EFI 18,432 bytes SHA-256 `642f1952b6910eca36e8392d2aecd46bf87c54da4e1f7997337d8b0caa5c7ff3`; launcher 7,168 bytes SHA-256 `c8c644a24dd6d81ce56bcd83899d82e287aaef886ae3957bdba6905e6c616e85`. Toolchains may differ. Hardware execution pending. This is integration evidence for an observation mode, not internal unlock, production transaction-marker continuity, automatic boot or recovery proof.
