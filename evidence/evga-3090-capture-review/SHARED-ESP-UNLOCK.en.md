# SULK01: first attended shared-ESP actual unlock attempt

[한국어](SHARED-ESP-UNLOCK.ko.md)

QUAL01's raw marker/log were independently inspected: consumer/NVIDIA Stops, ownership claim/close, platform/state/PROM checks, restored decode and two complete captures succeeded. Windows return and original USB restoration were reported. Their exact three hashes are pinned in `research/profile/shared-unlock-qualification.json`; staging refuses unapproved or differing evidence. **This next test runs the actual existing EVGA transaction from the shared ESP**, rather than another qualification. The exact-machine vendor/transaction implementation is unchanged. Shared-volume admission, new log/gate names and cross-volume attempt history are the integration changes. This is the first shared-ESP transaction, not proof it works on hardware or an automatic boot installation.

**Risk:** after fresh Shift+G, this releases the display/NVIDIA drivers, performs GPU MMIO/PCI writes, enables bus mastering, maps DMA buffers, uploads/executes firmware, writes selectors and performs one PCIe FLR. Screen loss is expected. Failure may leave live resources, DMA mappings or GPU state retained and require manual power-off. It intentionally holds on failure rather than blindly restoring or starting Windows. Containment and recovery are not guaranteed. Active ESP and original USB are written, so interruption, firmware/filesystem errors or hangs can damage boot/data. Back up both first. Use only this exact EVGA machine, original capture USB and pinned ESP. Secure Boot must already be disabled; scripts never change it. Do not delete/reset any marker or repeat after failure/cancellation.

The original USB history determines the next global attempt number; do not guess it or hard-code the example build slot. Stage verifies the prior USB capture pins and latest successful slot/transaction/cleanup, snapshots its markers/latest log into ESP `RTX3090-UNLOCK/USB-HISTORY-SULK01`, then creates **a reservation** named `EVGA-UNLOCK<N>.ARMED` on the original USB before creating a bootable ESP candidate. This USB reservation is not evidence execution occurred. It deliberately prevents the older USB workflow from advancing without review of the new shared attempt. On the ESP, the numbered runtime marker is created separately only after fresh Shift+G. A fixed SULK01 entry gate is consumed before Prepare, including cancellation/refusal. Neither gate nor reservation is reset automatically.


## One-command preparation

Instead of entering the preparation commands individually, run the helper below from administrator PowerShell. It discovers the slot, builds and runs all three host tests, and only with `-Stage` runs both existing guarded staging scripts. It prints the original USB boot hash and slot before staging; record both outside the USB for the post-run collector. It never reboots or performs the transaction. Without `-Stage` it only builds and tests. The same risks, backup, consent, failure and no-retry instructions below apply; if staging fails, preserve partial files and markers.

```powershell
./research/profile/prepare-shared-esp-unlock.ps1 -WindowsEspGuid 'c36c3e86-02aa-4f76-af1b-96e327f9d1c1' -Drive D -Clang 'C:/Program Files/LLVM/bin/clang.exe' -LldLink 'C:/Program Files/LLVM/bin/lld-link.exe' -Stage
```

After the attended boot, the collector/restore helper combines both commands. Supply the recorded slot and original hash, and use a new output folder outside the ESP:

```powershell
./research/profile/finish-shared-esp-unlock.ps1 -WindowsEspGuid 'c36c3e86-02aa-4f76-af1b-96e327f9d1c1' -Drive D -AttemptSlot <slot> -OriginalSha256 '<hash>' -OutputDirectory 'G:/Movian/SULK01-results'
```

If evidence collection fails, the helper stops before USB restoration; preserve the files and report the error. The original individual commands below remain available.

From the updated repository root in administrator PowerShell, replace `D` with the verified **original capture USB** letter. Preserve its captures/markers and the existing ESP assets. Use working compiler arguments if needed.

```powershell
$esp='c36c3e86-02aa-4f76-af1b-96e327f9d1c1'
$usb='D'
$slot=./research/profile/get-shared-esp-unlock-slot.ps1 -Drive $usb
$slot # Record the returned attempt number.
./research/profile/build-shared-esp-unlock-launcher.ps1 -WindowsEspGuid $esp -AttemptSlot $slot
./research/profile/test-shared-esp-unlock-gate.ps1
./research/profile/test-shared-esp-unlock-prepare.ps1
./research/profile/test-shared-esp-unlock-launcher.ps1
./research/profile/stage-shared-esp-unlock.ps1 -WindowsEspGuid $esp -Drive $usb -WhatIf
./research/profile/stage-shared-esp-unlock.ps1 -WindowsEspGuid $esp -Drive $usb
$original=(Get-FileHash "$usb`:\EFI\BOOT\BOOTX64.EFI").Hash
$original # Save this hash and $slot outside USB before proceeding.
./research/profile/stage-shared-esp-unlock-launcher.ps1 -Drive $usb -WindowsEspGuid $esp -ReuseExisting -WhatIf
./research/profile/stage-shared-esp-unlock-launcher.ps1 -Drive $usb -WindowsEspGuid $esp -ReuseExisting
```

Stage refuses existing SULK01 files/history, any prior ESP transaction marker, changed source/asset/boot hashes, history gaps or insufficient space (32 MiB plus the snapshot). Do not resize/delete files to bypass refusal. New ESP files are `EFI/EVGA-SHARED-UNLOCK.EFI`, `RTX3090-UNLOCK/SULK01.ARMED`, runtime `SULK01.TXT` and, after keypress, `EVGA-UNLOCK<N>.ARMED`. Earlier files are retained. The USB boot backup is `EFI/BOOT/BOOTX64.before-shared-unlock.efi`. `-WhatIf` skips writes and boot hashes inside the write block. Any error or partial stage: stop, preserve everything and send output. A reservation left after partial failure must not be removed to retry. No boot entry, firmware setting, format or reboot is performed by the scripts.

Boot USB **once** using the established firmware selection. Wait for `Live guards passed. NOW press fresh Shift+G once`, photograph it, then press **Shift+G once** to authorize the actual transaction described above. Esc cancels before release and still consumes this test's entry gate/reservation. After the first Stop the screen may go black; no final EFI success display is expected. Allow up to three minutes for Windows. If it does not arrive, power off, remove USB and use ordinary Limine/Windows as before; recovery remains unproven. Do not automatically retry or reset markers.

In Windows, collect the new evidence, restore the USB boot file and preserve its reservation. Choose a new local output directory outside the ESP; this example uses your custom desktop location:

```powershell
./research/profile/read-shared-esp-unlock.ps1 -WindowsEspGuid $esp -AttemptSlot $slot -OutputDirectory 'G:\Movian\SULK01-results'
./research/profile/restore-shared-esp-unlock-launcher.ps1 -Drive $usb -OriginalSha256 $original
```

Send JSON, `SULK01.ARMED`, `SULK01.TXT`, the numbered **ESP runtime marker if present**, prompt photo, Windows result and restore output privately. Logs contain hardware paths/addresses. Preserve the ESP history snapshot and original USB reservation; do not upload firmware blobs. Remove USB and confirm normal Windows/CachyOS Limine boot. In a new PowerShell session, use the recorded slot/original hash, not empty variables. Only after successful log review should the existing [manual post-unlock benchmark](POST-UNLOCK-BENCHMARK.en.md) be used; Windows boot alone does not prove an unlock or throughput gain.

Success requires the expected slot, `evga-unlock-transaction-return` success, containment/cleanup checks, released assets, restored decode, two complete captures, ready-Windows success and reported normal Windows boot. A STARTED gate, USB reservation, runtime keypress marker, zero callback or Windows alone is insufficient. This test does not prove repeatability, persistent automatic boot, recovery, or benchmark improvement.

Local-only checks on 2026-10-28: child/launcher PE verification passed; six entry-gate, eight production Prepare/logger/Windows and ten launcher cases passed. Cross-volume fixtures tested unapproved/wrong QUAL evidence refusal, WhatIf, create-new USB reservation and ESP history snapshot, repeat/unsupported USB advancement refusal, evidence collection and USB backup/reuse/restore. Actual transaction-adapter success/refusal/one-shot/containment and broad release/retained-asset cleanup mocks passed; hardware/PROM/vendor transaction were simulated. Sample **slot 7** child 88,064 bytes SHA-256 `d8b9612685ec4f456524e39c21cfde5ccb33dd48b83c406f9ae32d75dcd875ff`; launcher 6,144 bytes SHA-256 `bf1088f1ead3ad6d8d2d1eda94e25ec0ee662cf84cb9a416d2a08c8b7c12b1d0`. Other slots/toolchains can differ. No shared-ESP actual unlock has yet been run by these checks.

## SULK01 hardware result

On 2026-10-28 the contributor supplied the slot 8 runtime marker and SULK01 log (SHA-256 `c0cb1b121fbe24610e84dfdaee52da512856af39a65768a8441012e4236e5f23`). The log records transaction result zero, FLR=1, quiesced=1, fault=0, retained=0 and maps=0, successful transaction return, asset release and decode restoration, two completed captures and ready-Windows success. The contributor ran the collector in Windows and reported the finish command succeeded. This is the first recorded successful transaction from the shared ESP; it does not establish a performance gain or persistent automatic boot. A same-binary post-attempt benchmark was requested before another shutdown or reboot.
