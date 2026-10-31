# Default rearm acceptance — EVP12

[한국어](EVP12-REARM.ko.md)

After EVP11 cleanup, the contributor reported an ordinary restart through Boot0000 with the temporary option absent, BootNext clear and primary entries unchanged. This completes EVP11's attended transaction, benchmark, cleanup and ordinary-boot sequence. It does not establish persistent unlock across that restart.

The remaining reusable lifecycle check is the default rearm path without a reviewed-failure exception. Connect the original USB as D:, update the branch, then administrator PowerShell at the repo root:

```powershell
./research/profile/manage-evga-profile.ps1 -Action Rearm
```

Expected `prepared: true`, `slot: 12`, `run: EVP12`, `full_shutdown_required: true`. The controller verifies EVP11's successful evidence and separate cleanup receipt, preserves the original result and allocates a new slot. Do not use `-ReviewedEvp10Retry`; it applies only to the prior failure. On error stop and send it. No state deletion or automatic retry.

After successful preparation remove storage USBs, save work, hold Shift when selecting Windows Shut down, then power on. Wait for live guards and press fresh Shift+G once. SS1=7 remains required; full shutdown is the tested procedure, not a proven universal remedy. This repeats the existing GPU/PCI writes, driver release, DMA/firmware execution and reset if admitted. It may hang or prevent Windows/GPU initialization; recovery is not guaranteed. If still stopped after three minutes, power off and use the normal Limine/Windows entry, then report manual recovery. Do not repeat the EFI.

After Windows run `./research/profile/manage-evga-profile.ps1 -Action Collect` and send JSON/ZIP plus direct/manual return, full-shutdown and USB-absence confirmation. Do not reboot before evidence review in case a same-session check is needed. There is no automatic reboot, rollback, Secure Boot change or unattended unlock.

Local EVP12 build: 88064-byte AMD64 EFI, no imports/IAT/delay imports, NX/dynamic base; SHA256 `2d0db39cd798516adccb696b084708d68baf4221485c4fdd3ac2c7de3c214a98`. Host media/receipt gates pass. Mocked controller tests in PS7/5.1 cover default progression through slot12, preservation of previous results, correct power guidance and pending-slot refusal. Live EVP12 acceptance remains pending.
