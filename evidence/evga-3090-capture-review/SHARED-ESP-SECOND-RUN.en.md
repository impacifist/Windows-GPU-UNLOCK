# SULK02: one attended USB-free unlock boot

[한국어](SHARED-ESP-SECOND-RUN.ko.md)

On 2026-10-29 the contributor registered the pinned second launcher as `Boot7E00`. The live result had BootNext clear, original boot order preserved, Limine/Windows checks true and no armed attempt. This proves registration only. SULK01 already demonstrated the ESP-hosted transaction and benchmark gain through a USB launch. SULK02 tests the full firmware-to-ESP unlock path without USB attached at boot. It remains an exact-machine development test, not a completed reusable profile.

## Scope and risk

The new script checks the registration backup/current firmware, exact ESP geometry, staged EFI/normal boot-file hashes, all five assets and reviewed cross-volume history. It writes a local arming journal, reserves global slot 9 on the original capture USB, creates the fixed 512-byte ESP receipt, verifies them, then sets BootNext to the already-registered option **last**. It never changes BootOrder, replaces EFI files, changes Secure Boot, automatically reboots or resets a marker. The USB is needed for reservation only and must be removed for the boot test.

After a fresh Shift+G, the staged EFI releases the display/NVIDIA drivers, writes GPU MMIO/PCI state, enables bus mastering, maps DMA, uploads/executes firmware, writes selectors and performs one FLR. Screen loss is expected. It holds on a failed guard rather than blindly continuing. A failure may leave resources or GPU state retained, hang firmware/Windows, damage boot/data through interrupted active-ESP writes or require manual power-off. Recovery and containment are not guaranteed. NVRAM selection is not atomic; close other firmware/boot tools. Use only the reviewed EVGA/Z790 machine with backups/recovery media available. Do not change hardware, firmware settings, assets or files between preparation and execution.

## Prepare and run once

Update the branch. Do not rebuild/restage the already pinned payloads. Connect the original capture USB as D:, save work and run from the repository root in administrator PowerShell:

```powershell
./research/profile/run-shared-esp-second.ps1
```

The script uses `E:\UNLOCK\ESP-SECOND02`. Expected: `armed: true`, `attempt_slot: 9`, `boot_next_selected: true`, `boot_order_written: false`, `gpu_executed: false`, `automatic_reboot: false`. On any error, **do not reboot or retry**; preserve all state and send the error. A partial arm can have files and/or BootNext present. Do not delete the journal, reservation or receipt to get past a refusal.

Only after successful output, safely remove storage USBs and restart normally **once**, without choosing a USB or manually changing boot order. Record `UNLOCK CHILD LOADED FROM PINNED ESP` and the live-guards prompt. Wait for `Live guards passed. NOW press fresh Shift+G once`, then press **Shift+G once**. Esc cancels but still consumes the entry gate. After display loss, wait up to three minutes for Windows; do not tap Shift+V or assume a blinking cursor indicates progress. No Windows: power off, leave USB removed, use the existing Limine/Windows entry. No retry; collect/report after manual recovery. BootNext is expected to be consumed by firmware, but that and recovery require actual evidence.

## Collect and clean up

After Windows returns, before another reboot, use the same script:

```powershell
./research/profile/run-shared-esp-second.ps1 -Action Collect
```

It copies SULK02 receipt/log and optional runtime marker to a new local results directory, verifies copies, creates a ZIP and attempts exact-owned firmware cleanup. Send the ZIP and console JSON, whether Windows returned directly or manually, and whether USB was absent at boot. Preserve private firmware backups; they are not included in the ZIP. Cleanup refusal still returns the ZIP path and `cleanup_error`; preserve state and report it. It does not overwrite changed BootOrder. No unlock claim is derived from Windows returning alone. Keep the same Windows session available for a follow-up benchmark; do not run another unlock or reboot unnecessarily.

To cancel **before boot**, use `./research/profile/run-shared-esp-second.ps1 -Action Cancel`. Read-only firmware status uses `-Action Status`. Cancellation preserves the receipt, reservations and journal and does not make the attempt reusable. If cancellation refuses, do not reboot or guess firmware deletions; send the error.

## Verification boundary

Local tests passed 26 ordering/failure/receipt checks, plus the real orchestration script against RAM-native and mocked storage/hash/history fixtures: registration, WhatIf, receipt/reservation, BootNext-last selection, replay refusal, ZIP collection, cancellation, successful cleanup and ZIP preservation after cleanup refusal. Existing registration mocks passed 32 checks. These tests did not execute EFI, call native firmware APIs or access GPU hardware. Live USB-free unlock, cleanup after that boot, and same-session benchmark remain pending.
