# Final check: one normal restart after cleanup

[한국어](FINAL-NORMAL-BOOT.ko.md)

The contributor confirmed SULK02 cleanup completed: Boot7E00 absent, BootNext clear, one firmware deletion and no BootOrder write. The four current entries were preserved. This completes cleanup evidence, not the post-cleanup normal-restart check.

Update the branch and run from the repository root in administrator PowerShell:

```powershell
./research/profile/confirm-shared-esp-normal-boot.ps1
```

When it prints `phase: ready_for_normal_restart`, save work, keep USB unplugged and restart normally once. Do not select an EFI test. After Windows returns, run **the same command again** and send the JSON. Expected: `normal_restart_confirmed: true`, `temporary_option_absent: true`, `boot_next_clear: true`, `primary_entries_unchanged: true`, `firmware_written: false`.

The script writes only a new local Windows-boot-time baseline before restart, reads firmware state and checks a newer Windows boot plus ordinary Limine/Windows BootCurrent afterward. It preserves the baseline, backups and all test evidence. It performs no GPU work, firmware writes, EFI launch or automatic restart. Generic fallback/USB order suffix changes are reported outside the primary-entry check rather than restored. This is a normal-boot check, not an unlock-persistence test; an ordinary restart may clear the current unlock state.

If it errors or normal Windows boot fails, stop and report it; do not retry the unlock or delete markers. Use the existing Limine/Windows boot-menu entry or recovery media if needed; recovery is not guaranteed. Existing normal boot behavior is expected but not yet verified after this cleanup.

Local fixtures passed in PowerShell 7 and Windows PowerShell 5.1: baseline creation, same-boot refusal, ordinary entry acceptance, test-entry refusal, pending-BootNext refusal and zero firmware writes. Native firmware and Windows boot time were mocked; no real restart or hardware test occurred locally. The fixtures also cover the real cleanup wrapper. A null BootNext JSON compatibility issue in the registration status output was corrected; this does not alter firmware mutation behavior.

**Live result, 2026-10-30:** the contributor confirmed normal Windows boot. The script reported a newer boot through Boot0000, temporary option absent, BootNext clear and unchanged primary entries. No further restart or unlock test is requested. These instructions are historical; do not repeat the completed test.
