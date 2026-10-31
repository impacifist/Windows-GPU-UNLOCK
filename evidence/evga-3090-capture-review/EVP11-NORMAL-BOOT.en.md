# EVP11 cleanup confirmed; ordinary boot check

[한국어](EVP11-NORMAL-BOOT.ko.md)

The contributor reports successful separate cleanup of Boot7E00. The receipt binds the reviewed EVP11 log/receipt/runtime-marker hashes, preserves the original result and records no BootOrder writes or GPU access. Transaction, same-session benchmark and temporary-entry cleanup are demonstrated; the new package's later ordinary restart remains to be checked.

Administrator PowerShell, updated repository root:

```powershell
./research/profile/confirm-evga-profile-normal-boot.ps1
```

Wait for `phase: ready_for_normal_restart`. The script requires the successful evidence and matching cleanup receipt, checks the owned option is absent and BootNext clear, verifies the original normal entries and saves only a local Windows-boot-time baseline. On any error stop and send it.

Save work, keep USB unplugged and use one ordinary Windows Restart. Do not choose the USB, EFI test or change boot order. This does not reapply unlock, and does not establish persistence of unlocked performance. If Windows does not return, select the existing normal Limine/Windows entry; report manual recovery. Do not rerun an EFI.

After Windows, run the same command again and send its JSON. Expected `normal_restart_confirmed: true`, normal BootCurrent, absent temporary option, clear BootNext and unchanged primary entries. There are no script firmware writes, GPU access or automatic reboot. A local result receipt is saved. Mock tests pass in PowerShell 7 and 5.1 for baseline/new-boot acceptance, same-session refusal and temporary BootCurrent refusal. Actual normal restart and default rearm lifecycle remain separate evidence.
