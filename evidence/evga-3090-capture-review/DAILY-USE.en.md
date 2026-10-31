# Attended SharedEsp daily use

[한국어](DAILY-USE.ko.md)

Save these instructions and keep the repository, `E:\UNLOCK\EVGA-PROFILE`, original USB and existing ESP assets. Do not move/delete the state or copy only the launcher files out of the repository. This is the exact-machine attended profile accepted in EVP11/EVP12; it is not permanent or unattended unlock.

## Double-click preparation

After updating, create a desktop shortcut to `research\profile\prepare-evga-unlock.cmd` inside the repository. Keep the target there. When you want an unlocked session:

1. Connect the original USB as D: and double-click that shortcut. Accept the normal UAC prompt. It runs the existing guarded `Rearm` controller, builds/verifies a fresh attempt, reserves the slot and selects one boot. It never reboots automatically. Double-click launch/UAC is a new convenience wrapper; live behavior is not yet qualified separately from the proven controller.
2. Only after `prepared: true`, remove storage USBs and save work. Hold Shift while selecting Windows Shut down, then power on. Wait for live guards and press fresh Shift+G once.
3. After Windows, double-click `research\profile\collect-evga-unlock.cmd` (or a desktop shortcut to it). Keep the output/ZIP. Do not blindly repeat if an error occurs.

Preparation writes local/ESP files and one-shot firmware boot variables. EFI execution retains the existing GPU/PCI writes, DMA/firmware and reset risks; hangs or Windows/GPU failure may require manual power-off and normal boot selection. Recovery is not guaranteed. The wrapper preserves all guards, UAC and execution policy; it does not bypass them. On refusal preserve state and report the error. No consumed slot reuse or automatic retry.

## Is the USB still needed?

**Not for EFI execution or Windows boot. It is still required during preparation.** The current controller stores a permanent reservation on that original stick, preventing the old USB workflow and the new ESP workflow from reusing the same attempt number. Remove it after preparation; this is what the accepted USB-free EFI tests demonstrated.

Eliminating even this preparation-time dependency needs a separate deliberate migration of the shared reservation history to local storage, with legacy-launch coexistence handled. Copying files or removing the USB check is not that migration. No migration, key enrollment or Secure Boot support is included in these launchers. There is nothing to run now merely to re-prove the completed acceptance sequence.
