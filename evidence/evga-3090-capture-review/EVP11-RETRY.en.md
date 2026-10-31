# Reviewed fresh-slot EVP11 retry

[한국어](EVP11-RETRY.ko.md)

Movian reported successful reviewed EVP10 cleanup: USB entry preserved, failed evidence preserved, no BootOrder write or GPU access. That report does not turn EVP10 into a success. The successful SULK02 log entered with SS1=7; EVP10 entered with SS1=0. The cause remains unknown.

The explicit `-ReviewedEvp10Retry` switch permits only the next slot 11 after this exact reviewed failure and cleanup. It verifies all three failed-evidence hashes, the failed/cancelled state, cleanup receipt, original volume identities and absence of the old temporary option. Generic failed attempts still refuse. This switch cannot skip a failed slot 11. Every admission/runtime check, including SS1=7 and fresh Shift+G, is unchanged. No automatic rollback, reboot or retry is added.

Update the branch and connect the original USB as D:. Administrator PowerShell, repository root:

```powershell
./research/profile/manage-evga-profile.ps1 -Action Rearm -ReviewedEvp10Retry
```

Expected `prepared: true`, `slot: 11`, `run: EVP11`. On error stop and send it. Do not reset state or receipts. After preparation remove storage USBs, save work and perform a full shutdown with Windows Fast Startup bypassed (hold Shift when selecting Shut down), then power on. This is a test of a possible restart/full-shutdown difference, not an established fix. Existing BootNext selects the new attempt. If the normal boot menu appears instead, report it rather than changing firmware settings.

Wait for live guards and press fresh Shift+G once. Allow up to three minutes for Windows. If it remains stopped, power off and select the normal Limine/Windows entry; recovery is not guaranteed. It repeats the existing driver-release, decode, MMIO, DMA/firmware and reset transaction if admitted, and can hang or prevent Windows/GPU initialization. If SS1 is still 0 it will refuse and hold again. Do not press Shift+V or repeat the EFI.

After Windows, run `./research/profile/manage-evga-profile.ps1 -Action Collect` and send its JSON/ZIP, direct/manual return and full-shutdown/USB-absence confirmation. The next test remains attended; no permanent/unattended or Secure Boot support is claimed.

Local controller tests pass in PowerShell 7 and Windows PowerShell 5.1 using mocked firmware/storage/hashes. They cover the explicit slot11 path, preservation of failed result10, and generic/modified/repeated refusal. The EFI itself is built and PE-verified; no live slot11 success is claimed.
