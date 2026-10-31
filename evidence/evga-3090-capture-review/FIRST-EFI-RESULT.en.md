# First live EFI run: pre-call capture refusal and corrected test

[English](FIRST-EFI-RESULT.en.md) | [한국어](FIRST-EFI-RESULT.ko.md)

Movian reported waiting more than three minutes, manually power cycling, then normal Windows boot and display. The downloaded EVGA00.TXT is 408,651 bytes with SHA256 c0bc72185c59d24e685b875bb58b6701c01c84eaeb165aa848cad48b13d8cc43. Raw evidence is preserved locally outside tracked content; [sanitized findings](first-efi-run-result.json) retain its hash and outcomes.

The log has one incomplete reason: gop-info with EFI_NOT_READY (0x8000000000000006), followed by capture-complete=0 and handoff-stopped with the same status. It contains neither evga-text-stop-before nor evga-text-stop-return. The reviewed handoff source only enters GuEvgaTextStop after successful initial capture, so this run did not reach the driver Stop routine. This was a programmed pre-call refusal, not evidence the tested Stop hung. No lifecycle success or unlock is established.

## Fix

The generic probe had already recorded the pathless, framebufferless GOP with SizeOfInfo=0. The new EVGA build had omitted that known capture case and treated it as incomplete. The corrected diagnostic accepts zero mode-info size only for a zero-framebuffer/zero-size GOP whose device-path query returns EFI_UNSUPPORTED. It does not dereference an absent-size Info pointer. A physical GOP, nonzero short metadata, path presence/error or null Mode still fails. The live code/ownership gates and the one-call boundary are unchanged.

The new image also records a pre-Stop refusal and attempts the preloaded normal Windows entry if no Stop call started and logging/file closure succeeds. Once a Stop starts, failures still hold without retry or restoration. USB staging now chooses a free numbered backup if the original backup exists, preserving both the original capture EFI and failed-test EFI; an armed marker still refuses staging and must not be deleted.

Validation: 18 membership host checkpoints, nine actual diagnostic-path regression cases, 78 Python tests, and a fresh warnings-as-errors EFI build passed. PE verification reports AMD64 EFI application, no imports/IAT/delay imports, NX/dynamic base. Corrected local artifact: 19,456 bytes, SHA256 d9c1cb8dea987cf64ba6e1bab822cb048cb69117007b843a6d4b03d92eb427b9. Contributor toolchain hashes can differ; staging must match that contributor build's own manifest.

## Next run

Update the same branch in GitHub Desktop. Rebuild with the previous build command, then run stage-evga-text-stop.ps1 -Drive D again. Preserve EVGA00.TXT and all backups. If the helper reports an armed marker or another error, send that result instead of bypassing it. Use the same attended boot/Shift+G instructions and disclosed risk in [NEXT-EFI-TEST](NEXT-EFI-TEST.en.md). No extra confirmation reply is needed. The next free EVGAnn.TXT records the corrected run. Send it and report direct versus manual Windows arrival.
