# Reusable attended EVGA installer and rearm profiles

**Current acceptance:** SharedEsp is demonstrated on the exact captured machine through EVP11/EVP12. [Final status and handoff](EVP12-RESULT.en.md). The first-install commands below are historical setup instructions; do not rerun Install on the existing installation or reuse consumed slots. Use full shutdown (Shift+Shut down), not Restart, before an attended unlock attempt. Maintainer approval and live reusable Usb/SeparateEsp qualification remain pending.

[한국어](REUSABLE-PROFILE.ko.md)

The installer/package implementation is available for the **exact captured EVGA RTX 3090/Z790 system**, PCI `10de:2204`, subsystem `3842:3387`, PROM SHA-256 `f1c7c6b205d9a5c0626af4f82e28cc377aa16cde8c25c61c77b4b5160b26e71f`. GPU/BAR/PROM, console graph and driver-code pins remain compiled into the existing transaction adapter. The JSON files select a boot layout; they do not make arbitrary GPUs eligible. Do not register this profile with the generic automatic/headless installer.

| Profile file under `research/profile/profiles/` | Layout | Status |
| --- | --- | --- |
| `evga-z790-3090-shared-esp.json` | Unlock/logs/assets on the existing Windows ESP | Earlier attended SULK02 path has live evidence; this reusable packaging requires its own live test |
| `evga-z790-3090-usb.json` | Unlock/logs/assets on USB; Windows ESP is only read | Earlier attended USB path has live evidence; new package builds/mocks pass |
| `evga-z790-3090-separate-esp.json` | Unlock/logs/assets on a distinct existing internal FAT32 ESP | Build/host validated only; no dedicated-ESP hardware result claimed |

None claims permanent/unattended unlock or general compatibility. Every EFI requires fresh **Shift+G after live guards**. The controller never formats, repartitions, writes BootOrder, changes Secure Boot, automatically reboots, resets markers or stops Windows applications.

## Movian: first shared-ESP package test

Update this branch. Connect the original capture USB as D:. Keep the existing ESP assets and all SULK02 evidence. In administrator PowerShell from the repo root, run **one command**:

```powershell
./research/profile/manage-evga-profile.ps1
```

Defaults: `Install`, `SharedEsp`, reviewed Windows ESP GUID `c36c3e86-02aa-4f76-af1b-96e327f9d1c1`, new state directory `E:\UNLOCK\EVGA-PROFILE`, LLVM from `C:/Program Files/LLVM/bin`. The controller verifies the reviewed slot-9 log and original USB history, chooses **slot 10 / EVP10**, builds and verifies a fresh EFI/package, saves an immutable journal, reserves the slot, stages assets/EFI/512-byte receipt and sets one-shot BootNext last. Existing Windows/Limine files and prior evidence are retained.

Expected final JSON: `prepared: true`, `slot: 10`, `run: EVP10`, `requires_fresh_shift_g: true`, `boot_next_selected: true`, `boot_order_written: false`, `automatic_reboot: false`. On any error, **do not reboot or rerun Install**; preserve all state and send the error. No automatic rollback is promised. Do not delete the state directory to bypass a refusal.

Only after successful preparation, safely remove storage USBs, save work and restart normally once. This package launches directly, so the old SULK02 launcher banner/countdown is not expected. Wait for `Live guards passed. NOW press fresh Shift+G once` and press Shift+G once. After display loss, allow up to three minutes for Windows; do not tap Shift+V. If Windows does not return, power off and choose the existing Limine/Windows entry with USB absent. Recovery is not guaranteed; do not repeat the attempt.

After Windows returns, use:

```powershell
./research/profile/manage-evga-profile.ps1 -Action Collect
```

Send its JSON and ZIP, direct/manual Windows return and whether USB was absent. Collection copies the receipt/log/runtime marker and package metadata, verifies the transaction/capture evidence, and cleans only the exact owned temporary option. Firmware backups stay private and are not included in the ZIP. A failure remains recorded; it is not converted into success or silently rearmed.

## Future successful-run rearm

After a successful collected run and cleanup, the next attempt is one command:

```powershell
./research/profile/manage-evga-profile.ps1 -Action Rearm
```

This uses the saved layout/GUID/volume identities, verifies all three previous evidence hashes, allocates the next contiguous slot and rebuilds a new EFI. It never reuses a receipt. Pending, failed, cancelled, modified or uncollected attempts refuse rearm and need review. Slots are bounded by the existing 8–99 runtime contract; exhausted history is not reset. Do not switch layouts or run parallel installations against the same history.

`-Action Status` is read-only. `-Action Cancel` cancels the current prepared attempt and keeps its files/reservation; it does not make that slot reusable. Cancel is for before boot, not a required post-run command. Use Collect after a run. USB mode restores the previous USB fallback loader during collection/cancellation; internal modes delete only their exact temporary `Boot7Exx` option. Cleanup preserves current entries, permitting only verified missing/appended generic fallback descriptions; real entry/order changes still refuse. On cleanup error, send the JSON/ZIP and retain state; do not guess deletions.

## Non-shared profiles

Use only a **fresh, reviewed legacy slot-7 USB history** or the reviewed slot-9 SULK02 history as the migration seed. The USB/dedicated-ESP variants do not require a previous shared-ESP run: slot 7 can migrate directly to a fresh EVP08 package. These are still this exact machine's profiles, not new-machine qualification. Other machines must provide their own profile/evidence.

For a compatible setup that uses USB instead of sharing Windows ESP, select the USB profile at first installation:

```powershell
./research/profile/manage-evga-profile.ps1 -Layout Usb -StateDirectory 'E:\UNLOCK\EVGA-USB-PROFILE'
```

It backs up/replaces only the USB's fallback loader for one attended boot and makes **no firmware writes**. Boot the USB once from the firmware menu; assets/logs/receipts stay on USB. The Windows ESP is never a deployment target. Atomic loader replacement may be rejected by a particular filesystem; if staging errors, do not boot/retry and report the state.

For an existing dedicated internal unlock ESP, first installation selects:

```powershell
./research/profile/manage-evga-profile.ps1 -Layout SeparateEsp -TargetDrive U -StateDirectory 'E:\UNLOCK\EVGA-SEPARATE-PROFILE'
```

U: must already be a distinct internal GPT/FAT32 ESP, not the Windows system/boot partition. Its unique partition GUID is compiled into the EFI and checked again at runtime. The installer does not create/resize it. This route has host/build evidence only; it is not a demonstrated live configuration. The Windows ESP remains read-only. Optional `-Assets` and `-EvidenceDirectory` explicitly locate existing verified files. After installation, the saved layout is reused by the same Rearm/Collect/Status commands with that state directory.

## Package and source distribution

`build-evga-profile.ps1` can build a payload ZIP without staging or firmware access. It contains `UNLOCK.EFI`, the selected profile, five-asset contract, PE report, generated partition/receipt header, source manifest, GPL license and README/source pointer. Firmware asset binaries are **not bundled**. Installation/rearm uses the matching repository controller/toolchain; this is not a standalone Windows application. Hashes record integrity/provenance, not a publisher signature or independent attestation. Keep local modifications visible through `SourceTreeDirty`.

Five exact firmware assets plus `NVIDIA-LICENSE` and `NOTICE.md` are required. No downloads or extraction run implicitly. The existing asset source can be supplied explicitly with `-Assets`; installed assets are reused only when hashes/sizes match. Old captures, reservations and receipts remain intact. A permanent `EVGA-UNLOCK-RESERVED<slot>.ARMED` record deliberately blocks the legacy uncoordinated stager after migration; use the new stateful controller thereafter.

## Risk and recovery

Preparation writes local/target files and, for internal routes, one temporary NVRAM option plus BootNext. No BootOrder write occurs. EFI execution after Shift+G releases display/NVIDIA drivers, performs MMIO/PCI writes, bus mastering, DMA, firmware upload/execution, selector writes and FLR. Display/input loss, firmware/Windows hang, retained resources, boot/data damage or GPU damage are possible. Recovery/containment are not guaranteed. Back up data and keep recovery media available; close other boot/firmware tools. A failed guard holds rather than blindly resuming or rolling back. Another ordinary restart may clear unlock state; persistence is not claimed.

[Technical review and validation](REUSABLE-PROFILE-REVIEW.en.md). Hardware results from the older attended flow are not substituted for the first live test of this new installer/package.
