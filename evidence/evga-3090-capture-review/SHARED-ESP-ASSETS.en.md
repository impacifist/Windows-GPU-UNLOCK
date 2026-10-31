# Shared ESP asset preflight â€” next hardware test

[í•œêµ­ì–´](SHARED-ESP-ASSETS.ko.md)

The visible audit entered the pinned ESP and Windows subsequently booted; the contributor also confirmed ordinary Windows/CachyOS Limine boot and USB restoration. This next test advances to firmware asset access. It reads and SHA-256 verifies all five firmware assets from `\RTX3090-UNLOCK\ASSETS` on that same ESP, frees each allocation, displays the result for ten seconds, and attempts the existing Windows handoff. It does not access the GPU, load the asset blobs as executable images, create logs/markers, change boot entries or perform an unlock. A failed asset check also attempts Windows after displaying failure. This is separate from the successful audit, which is retained.

**Risk:** staging writes about 85 MB to the active shared ESP. Back up the ESP and USB first. The script requires an additional 16 MiB free reserve and refuses existing asset directories, changed Windows/Limine boot hashes and existing preflight files. Do not resize the ESP or remove files to bypass refusal; send the output. Filesystem errors, interruption, firmware image refusal or a hang can still affect boot. Recovery is not guaranteed. No automatic cleanup removes partial files. Scripts check paths/hashes but cannot eliminate concurrent filesystem changes.

Run from the repository root in administrator PowerShell. Keep the same local compiler arguments used for the audit if needed. `D` below must be the existing FAT32 USB letter, verified by you. `$assets` must point to the five locally prepared assets plus `NVIDIA-LICENSE` and `NOTICE.md`; the example uses the established local preparation output. Firmware stays local; do not send its contents.

```powershell
$esp = 'c36c3e86-02aa-4f76-af1b-96e327f9d1c1'
$usb = 'D'
$assets = (Resolve-Path './research/profile/build/evga-assets/output').Path
./research/profile/build-shared-esp-assets-launcher.ps1 -WindowsEspGuid $esp
./research/profile/test-shared-esp-assets.ps1
./research/profile/test-shared-esp-assets-launcher.ps1
./research/profile/stage-shared-esp-assets.ps1 -WindowsEspGuid $esp -Assets $assets -WhatIf
./research/profile/stage-shared-esp-assets.ps1 -WindowsEspGuid $esp -Assets $assets
$original = (Get-FileHash "$usb`:\EFI\BOOT\BOOTX64.EFI").Hash
$original # Save this exact hash outside the USB before proceeding.
./research/profile/stage-shared-esp-assets-launcher.ps1 -Drive $usb -WindowsEspGuid $esp -ReuseExisting -WhatIf
./research/profile/stage-shared-esp-assets-launcher.ps1 -Drive $usb -WindowsEspGuid $esp -ReuseExisting
```

The new USB backup is `EFI/BOOT/BOOTX64.before-shared-assets.efi`; earlier audit backups remain intact. Stop on any error; preserve output and files. `-WhatIf` validates inventory/source assets and space but skips copying and the boot-file hash checks inside the write block. The stage never formats, reboots or registers firmware entries. It creates `EFI/EVGA-SHARED-ASSETS.EFI` and the new asset directory on the ESP; existing visible audit files are retained.

Boot the USB once using the same firmware selection used successfully for the audit. Photograph `ASSET PREFLIGHT ENTERED` followed by either `FIVE ESP ASSETS VERIFIED` or `ESP ASSET CHECK FAILED`, and report whether Windows boots. The launcher still says `AUDIT LOADED FROM PINNED ESP`; that alone does not prove asset verification. If Windows is not reached within three minutes, power off, remove the USB and use the normal Limine/Windows entry as before. Do not repeatedly boot a failure.

In Windows, restore the USB using the original hash saved above (replace the placeholder if opening a new PowerShell):

```powershell
./research/profile/restore-shared-esp-assets-launcher.ps1 -Drive D -OriginalSha256 $original
```

Remove USB and confirm normal Limine boot into Windows and CachyOS. Return the stage/restore output, the two photos, and boot results. Leave the staged ESP files and assets intact for review; send no firmware blobs. This test proves asset access only if the success message is observed. It does not prove retained simultaneous allocations, capture/marker writes, GPU ownership, internal unlock, automatic boot or unlock recovery.

Local validation on 2026-10-28: AMD64 EFI PE checks passed with no imports, NX and dynamic base; asset EFI 7,680 bytes SHA-256 `d69efcfa4d02ee8c171cf5fdec2c447336c55f238f9a4fd7c31b27bed17b52d8`, launcher 7,168 bytes SHA-256 `2faf50cf1011475eae7eab681fd0d83f744e7f4b84b55e9cc7e74e92d2516a73`. Toolchains may differ. Eight host cases cover success, missing file, read error, short/oversized file, wrong digest, close error and allocation failure with cleanup. Ten launcher selection/handoff mocks passed. Staging fixtures verified low-space refusal, WhatIf, copy/no-overwrite, USB backup/reuse/restoration and hash refusal with mocked inventory/boot-file hashes. No physical ESP/USB or GPU was touched by these checks. Hardware run pending.

## Hardware result

On 2026-10-28, the contributor supplied a photo showing `ASSET PREFLIGHT ENTERED` and `FIVE ESP ASSETS VERIFIED`. They reported Windows booted normally afterwards, and the USB restore script reported the original boot file restored and verified. The photographed success establishes the preflight reached its five-asset verification success path on this machine; it does not establish retained simultaneous allocations, an unlock or a logged Windows handoff. Normal Limine boot after this test is not yet confirmed.
