# Finish the temporary SULK02 entry cleanup

[한국어](SULK02-CLEANUP.ko.md)

The contributor's 2026-10-30 comparison shows current BootOrder is exactly `0000,0008,000C,000D`. These Limine/Windows/USB entries still match the registration backup byte-for-byte, including attributes. Only generic fallback entries `000E` (CD/DVD), `000F` (removable) and `0010` (network) are absent. BootNext is clear and Boot7E00 exactly matches the owned SULK02 option. The reason the fallback entries disappeared is unknown. They will not be recreated.

Update the branch, then run one command from the repository root in administrator PowerShell:

```powershell
./research/profile/cleanup-shared-esp-second.ps1
```

Expected: `cleanup_completed: true`, `temporary_option_present: false`, `boot_next_clear: true`, `boot_order_written: false`, `firmware_writes: 1`. Send the JSON. No reboot, benchmark or EFI run is required by this step.

This accepts only the exact reviewed contraction above. It verifies the four remaining entries, absent fallbacks, clear BootNext and exact owned option repeatedly, saves an immutable `cleanup-sulk02.json` outside the ESP, then deletes only Boot7E00 with its saved attributes 7 and verifies readback. It does not change BootOrder, other entries, ESP files, GPU state or existing backups/receipts/reservations. An unexpected state still refuses. The new snapshot does not replace the original registration backup.

This is one firmware NVRAM deletion; firmware can reject or mishandle it and affect boot. Recovery is not guaranteed. Close other boot-management tools. If any error occurs, preserve all files/output and stop without retrying or guessing deletions; a failure after deletion may leave the owned entry absent. No automatic rollback/reboot occurs. If normal boot later fails, use the existing Limine/Windows entry or recovery media and report it.

Local validation: 21 RAM-only exact-state/failure checks cover changed order/entries, reappearing fallbacks, pending BootNext, changed option, backup failure, races, deletion rejection/readback failure and post-deletion drift. A real-entry RAM-native fixture verifies WhatIf, durable snapshot, deletion attributes 7, original-backup preservation and replay refusal. No native firmware API or GPU operation ran locally. Actual cleanup remains pending.

**Live result, 2026-10-30:** contributor output confirms cleanup completed, Boot7E00 absent, BootNext clear, no BootOrder write and one deletion. Do not rerun cleanup. [Final normal restart check](FINAL-NORMAL-BOOT.en.md).
