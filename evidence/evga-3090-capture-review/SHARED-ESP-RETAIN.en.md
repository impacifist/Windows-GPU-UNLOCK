# Next test: retained assets and one-shot ESP receipt

[한국어](SHARED-ESP-RETAIN.ko.md)

The contributor's asset-preflight photo showed `FIVE ESP ASSETS VERIFIED`; Windows returned and USB restoration was reported. This next test exercises a new boundary: all five assets remain allocated simultaneously, are hashed again together, then freed, and a small receipt is written/flushed/read back on the same pinned ESP. It uses the proven audit admission/Windows handoff. It does not touch the GPU or execute firmware assets. It does not yet run the unlock handoff `Prepare`, its logger, capture graph or transaction markers, and successful receipt output does not qualify a shared-ESP unlock.

**Risk:** UEFI now writes an existing 512-byte test receipt on the active shared Windows/CachyOS ESP. Firmware filesystem writes, interruption, allocation exhaustion or hangs may damage data or affect boot. Back up the ESP and USB first; recovery is not guaranteed. No existing assets, logs, markers, EFI files or old USB backups are deliberately replaced. A new receipt/EFI already present causes staging refusal. Runtime accepts only the exact pending receipt and marks it STARTED before allocation; a second invocation refuses it. Never delete/reset the receipt to bypass refusal. There is no automatic cleanup of partial files.

From the updated repository root in administrator PowerShell, use the same compiler arguments as your last working build if needed. Replace `D` with the verified USB letter. Existing ESP assets from the successful preflight must remain in place; this test verifies them without copying them again.

```powershell
$esp = 'c36c3e86-02aa-4f76-af1b-96e327f9d1c1'
$usb = 'D'
./research/profile/build-shared-esp-retain-launcher.ps1 -WindowsEspGuid $esp
./research/profile/test-shared-esp-retain.ps1
./research/profile/test-shared-esp-retain-launcher.ps1
./research/profile/stage-shared-esp-retain.ps1 -WindowsEspGuid $esp -WhatIf
./research/profile/stage-shared-esp-retain.ps1 -WindowsEspGuid $esp
$original = (Get-FileHash "$usb`:\EFI\BOOT\BOOTX64.EFI").Hash
$original # Save this hash outside USB before continuing.
./research/profile/stage-shared-esp-retain-launcher.ps1 -Drive $usb -WindowsEspGuid $esp -ReuseExisting -WhatIf
./research/profile/stage-shared-esp-retain-launcher.ps1 -Drive $usb -WindowsEspGuid $esp -ReuseExisting
```

Stage creates `EFI/EVGA-SHARED-RETAIN.EFI` and `RTX3090-UNLOCK/RETAIN01.TXT` on ESP, with a 16 MiB free-space reserve. Existing Windows and Limine hashes must match the previously observed pins. The USB backup is `EFI/BOOT/BOOTX64.before-shared-retain.efi`, separate from prior backups. `-WhatIf` checks source/space/assets and skips writes and boot-file hash checks inside the write block. Stop on any error and preserve output and files. No script changes firmware entries or reboots.

Boot USB **once** through the same firmware selection as before. Photograph `RETAIN01 ENTERED` and the final `RETAIN01 COMPLETE` or `RETAIN01 FAILED OR ALREADY USED` screen. COMPLETE means the five allocations coexisted, passed a second digest check, were released, and the final receipt passed flush/readback and close checks. No unlock happened. After the ten-second display, Windows handoff is attempted even on a preflight failure. The USB launcher's old `AUDIT LOADED` text alone proves none of these steps. If no Windows within three minutes, power off, remove USB and use ordinary Limine/Windows as before. Do not repeat a failed run.

After Windows returns, read `RTX3090-UNLOCK/RETAIN01.TXT` without mounting or writing:

```powershell
./research/profile/read-shared-esp-retain.ps1 -WindowsEspGuid $esp
```

Send this JSON output. It is 512 bytes with NUL padding; the first line should be `COMPLETE RETAIN01 - FIVE ASSETS RETAINED REHASHED FREED - NO GPU`. STARTED, FAILED, PENDING, missing/truncated content, or any error is not success; preserve it and report. The receipt does not prove Windows handoff completed because it is written before that call. Keep the receipt and staged files for review.

```powershell
./research/profile/restore-shared-esp-retain-launcher.ps1 -Drive $usb -OriginalSha256 $original
```

Remove USB and confirm normal Limine Windows/CachyOS boot. Send build/stage/restore output, the two photos, the small receipt and boot results. Do not upload the firmware assets. If using a new PowerShell session, use the original hash recorded outside USB rather than an unset `$original`.

Local-only checks on 2026-10-28: nine mock cases cover simultaneous retention/release, partial allocation cleanup, second-pass corruption, pending receipt admission, write/flush/readback and repeat refusal. Ten launcher mocks passed. Staging fixtures passed low-space/WhatIf/no-overwrite and receipt-content checks, plus USB backup/reuse/restore and wrong-hash refusal; storage/boot hashes were mocked. PE checks passed AMD64, no imports, NX/dynamic base. EFI 10,240 bytes SHA-256 `3c445de01f565a09bf1c7b2eb010d4b021e1d57ed43de96843e2cd97553a57e3`; launcher 7,168 bytes SHA-256 `3f8cf75a74048367e7f9e3b6603930b6629d31feab87ea971640a077fc297019`. Toolchains may differ. Hardware execution pending.
