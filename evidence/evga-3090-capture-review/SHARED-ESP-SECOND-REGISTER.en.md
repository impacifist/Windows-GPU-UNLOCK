# SULK02 registration without selecting a boot

[한국어](SHARED-ESP-SECOND-REGISTER.ko.md)

The contributor passed all 32 RAM-only state-machine checks on 2026-10-29. The new live wrapper connects registration to the exact staged launcher/child hashes, internal GPT/FAT32 ESP geometry, already-disabled Secure Boot, Limine-first order and active Windows entry. It saves and verifies a new recovery backup before creating one unused temporary Boot7Exx entry. It does not write BootOrder, BootNext or ESP files, arm a receipt/reservation, access the GPU or reboot. This is development registration, not a ready-to-boot profile.

Update the branch and run this one command from the repository root in administrator PowerShell. USB is unnecessary; do not rebuild or restage.

```powershell
./research/profile/register-shared-esp-second.ps1 -StateDirectory 'E:\UNLOCK\ESP-SECOND02'
```

Expected: `registered: true`, `boot_next: null`, `boot_order_written: false`, `limine_first_verified: true`, `windows_entry_verified: true`, `attempt_armed: false`, `boot_authorized: false`. Send output/error. Do not reboot for this test or select the new entry. Keep `E:\UNLOCK\ESP-SECOND02\firmware-backup.json` private and intact. The directory must be new; never delete/reuse it to retry registration.

Read-only status uses the same command plus `-Action Status`. To cancel:

```powershell
./research/profile/register-shared-esp-second.ps1 -Action Cleanup -StateDirectory 'E:\UNLOCK\ESP-SECOND02'
```

Cleanup verifies the exact saved boot order/entries and owned option before deleting it, using attributes 7 as in the successful prior cleanup. It never restores BootOrder or deletes a foreign pending selection. If registration or cleanup fails, preserve backup, output and partial state; do not retry, select the entry or guess firmware deletions. No automatic rollback occurs. Firmware can reject, normalize or mishandle writes and affect boot even with BootNext unset; recovery is not guaranteed. Close other boot-management tools. If normal boot later fails, use the firmware menu's existing Limine/Windows entry or recovery media and report the result.

Local validation ran the real wrapper against a RAM-only Native implementation and mocked storage inventory/hashes: registration, status, cleanup, occupied-slot preservation, WhatIf, existing-backup refusal, and armed-state, launcher/child hash, pending-BootNext, Secure-Boot, Limine-order and nonlocal-path refusals. The existing 32 state-machine checks also passed. These tests made no native firmware calls or GPU accesses. Live registration remains pending.

Receipt/slot continuity and BootNext-last arming remain the next integration; registration success alone does not prove USB-free unlock.
