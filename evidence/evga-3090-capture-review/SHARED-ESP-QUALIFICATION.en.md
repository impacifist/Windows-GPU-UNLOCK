# QUAL01: shared-ESP GPU release and resources qualification

[한국어](SHARED-ESP-QUALIFICATION.ko.md)

INTG01's received marker and raw log were reviewed: Prepare, retained-assets release, capture completion/result and ready-Windows all recorded success. Windows boot and USB restoration were reported. This next attended test crosses the remaining **GPU release/decode/ownership boundary on the shared ESP**. It runs the existing production release-only/resources sequence through the real shared-ESP handoff entry. It performs the six consumer Stops, NVIDIA child/parent Stops, temporary memory-decode enable, bounded state/PROM/platform reads, exclusive PCI ownership claim/close and decode restoration. The five existing assets are verified through the production resource loader before release. They are not executed. **No unlock transaction, vendor firmware execution, DMA or FLR is performed.** This combines the established USB qualification phases on the shared ESP; it does not repeat the observation-only test.

**Risk:** this test changes live driver ownership and GPU PCI decode. The display can go black after the first Stop; later success is established from the log and Windows boot, not a final EFI screen. Failure can leave the display/drivers detached or decode enabled and require manual power-off. There is no blind rollback or retry. Filesystem writes continue on the active shared ESP, so interruption, firmware bugs or filesystem failure can damage boot/data. Back up ESP and USB first. Recovery is not guaranteed. Do not run this on another machine or substitute another card/EFI. Never delete/reset markers to retry.

From the updated repository root in administrator PowerShell, use your working compiler arguments if needed. Replace `D` with the verified USB letter. Existing assets and previous receipts/logs must remain intact. Staging requires the exact reviewed INTG01 marker/log hashes (`e2fe1103...955c2` / `f73b4001...64d6c`), unchanged Windows/Limine hashes and 16 MiB free reserve.

```powershell
$esp='c36c3e86-02aa-4f76-af1b-96e327f9d1c1'
$usb='D'
./research/profile/build-shared-esp-qualification-launcher.ps1 -WindowsEspGuid $esp
./research/profile/test-shared-esp-qualification.ps1
./research/profile/test-shared-esp-qualification-gate.ps1
./research/profile/test-shared-esp-qualification-prepare.ps1
./research/profile/test-shared-esp-qualification-launcher.ps1
./research/profile/stage-shared-esp-qualification.ps1 -WindowsEspGuid $esp -WhatIf
./research/profile/stage-shared-esp-qualification.ps1 -WindowsEspGuid $esp
$original=(Get-FileHash "$usb`:\EFI\BOOT\BOOTX64.EFI").Hash
$original # Record outside USB before proceeding.
./research/profile/stage-shared-esp-qualification-launcher.ps1 -Drive $usb -WindowsEspGuid $esp -ReuseExisting -WhatIf
./research/profile/stage-shared-esp-qualification-launcher.ps1 -Drive $usb -WindowsEspGuid $esp -ReuseExisting
```

New ESP files are `EFI/EVGA-SHARED-QUALIFICATION.EFI`, entry gate `RTX3090-UNLOCK/QUAL01.ARMED`, runtime log `QUAL01.TXT`, and the separate post-keypress `QUAL01-GPU.ARMED` marker in that directory. All older files remain. The new USB backup is `EFI/BOOT/BOOTX64.before-shared-qualification.efi`. Existing new-test files cause refusal; preserve them. The entry gate changes from PENDING to STARTED before Prepare and stays STARTED even after success or cancellation. It is not a completion receipt. `-WhatIf` skips writes and the boot hash checks inside the write block. Scripts never change boot entries, format or reboot.

Boot USB **once** using the same firmware selection. Wait for `Live guards passed. NOW press fresh Shift+G once`; photograph that prompt. Only then press **Shift+G once** to accept the described driver/decode test. Esc cancels before release. If guards refuse or an error appears, preserve it; do not repeat. Screen loss after the first Stop is expected. Allow up to three minutes for Windows. If Windows does not arrive, power off, remove USB and use ordinary Limine/Windows as before. This recovery is unproven. Do not reset a marker or rerun the test after cancellation/failure.

After Windows returns, collect evidence and restore USB. Choose any **new local output folder outside the ESP**; `G:\Movian\QUAL01-results` is only an example matching your custom desktop location.

```powershell
./research/profile/read-shared-esp-qualification.ps1 -WindowsEspGuid $esp -OutputDirectory 'G:\Movian\QUAL01-results'
./research/profile/restore-shared-esp-qualification-launcher.ps1 -Drive $usb -OriginalSha256 $original
```

Send the JSON, `QUAL01.ARMED`, `QUAL01.TXT`, `QUAL01-GPU.ARMED` if created, prompt photo, Windows result and restore output privately. Logs contain device paths/addresses. No firmware blobs are requested. Preserve ESP files. Remove USB and confirm ordinary Windows/CachyOS Limine boot. A new PowerShell session needs the saved original hash instead of an unset `$original`.

Qualification requires successful Stop results, resource claim/close, state/PROM/platform checks, memory disable and restored decode checks, two complete captures, `evga-text-stop-ready-windows` success, and normal Windows boot. A partial capture, STARTED gate, keypress marker or a successful callback alone is not qualification. Failures before a driver call may take normal Windows fallback; failures after a Stop intentionally hold without automatic rollback. Even full qualification is **not an unlock** and does not prove vendor transaction safety, automatic boot or recovery.

Local-only validation on 2026-10-28: QUAL01 release/resources wrapper, eight Prepare/logger/Windows cases, six gate/readback/repeat-refusal cases, ten launcher cases, stage/prior-hash/free-space/no-overwrite/evidence-copy and USB backup/restore fixtures passed. The existing broad EVGA suite also passed Stop/decode/state/platform/resource/ownership and cleanup mock regressions; hardware, code hashes and Stop callbacks were simulated. PE checks passed AMD64/no imports/NX/dynamic base. Child EFI 39,424 bytes SHA-256 `0cd58b055b66dd7c242381f2831ef23436c3e201d66274d2d124f21b17efd092`; launcher 6,144 bytes SHA-256 `7960bfc37a2b449573f6046c07ec21d62a62a85bf0cd24e4ffe32699dc5471e8`. Toolchains may differ. Hardware execution pending.
