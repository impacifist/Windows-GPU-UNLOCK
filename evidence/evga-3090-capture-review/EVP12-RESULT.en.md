# Reusable SharedEsp profile acceptance complete

**Final handoff verified, 2026-10-30:** the contributor confirmed Shift+Shut down and continuous USB absence for EVP12. Returned acceptance ZIP SHA256 `f112c781dc6c8878ed5425a7295388cf04fe05a32d2e96a62716d8ceb3d58221` contains exactly the seven intended summary/profile/instruction files. Receipt log/gate/runtime-marker/package hashes match the original EVP12 upload; the exporter confirms temporary option absent and BootNext clear. No contributor test remains for this acceptance sequence. Maintainer approval remains pending.

For future attended use, connect the original USB as D: for reservation, run `./research/profile/manage-evga-profile.ps1 -Action Rearm`, then after successful preparation remove storage USBs, save work, Shift+Shut down and power on. Press fresh Shift+G only after guards, then run the same controller with `-Action Collect` in Windows. On refusal/error preserve state and report; do not delete markers, use the historical retry flag or repeat a consumed EFI. Run `-Action Cleanup` only if a reviewed successful run needs separate cleanup. Secure Boot remains disabled for this qualified workflow. This is future-use guidance, not a request to run again now.

[한국어](EVP12-RESULT.ko.md)

EVP12 demonstrates default `Rearm` without a reviewed-failure exception, a fresh slot/receipt, successful attended transaction, direct Windows return, collection and cleanup. The returned log/receipt/runtime marker match the reported hashes. Two complete captures, pre-FLR SS0=88888888/SS1=8, result=0, fault=0, retained=0, maps=0 and FLR=1 are recorded, followed by successful close, asset release and Windows-ready. The collection reports CleanupCompleted=true with no evidence or cleanup error.

EVP11 separately established the same-session tensor benchmark, scoped cleanup and a subsequent normal Boot0000 restart with clear BootNext, absent temporary option and unchanged primary entries. EVP12 does not claim a new benchmark or another ordinary-restart result. Full-shutdown/USB-absence confirmation for EVP12 is requested in the handoff; its successful outcome does not prove why EVP10 differed.

The **SharedEsp reusable package workflow is demonstrated on this exact captured machine**, with full shutdown and fresh Shift+G required. The shared profile's historical hardware-tested flag records this qualification; every newly built package remains HardwareTested=false until that artifact is actually run. Permanent/unattended unlock, Secure Boot support and general hardware compatibility remain unproven. Usb and SeparateEsp profiles are provided but their new reusable packaging remains build/mock-only; older underlying USB evidence is distinct. Maintainer approval is still pending.

## Final contributor handoff — no reboot or GPU test

Update the branch, then administrator PowerShell at the repo root:

```powershell
./research/profile/export-evga-profile-handoff.ps1
```

Send the acceptance ZIP and confirm full shutdown and USB absence for EVP12. The script verifies the exact successful EVP12 evidence, existing EVP11 normal-boot receipt and live absence of the temporary option/BootNext. It saves a sanitized summary, profile, instructions and previously reviewed benchmark summary. No firmware backups, raw dumps, keys or executable firmware assets are included. It makes local files only; no staging, firmware write, GPU access, rearm or reboot. On error send it and preserve state. No further EFI run is requested for this acceptance sequence.

Preserve all original USB reservations, failed EVP10 evidence, per-attempt receipts and private recovery backups. Future reviewed use is `manage-evga-profile.ps1 -Action Rearm`, followed by full shutdown and the attended flow; it is not a persistent automatic installer. Do not erase history to reclaim consumed slots.

Authoritative hashes and checks: [EVP12 evidence](evp12-result.json), [EVP11 benchmark](evp11-benchmark.json), [technical review](REUSABLE-PROFILE-REVIEW.en.md). The PR is ready for maintainer review with these bounded claims; this does not represent maintainer approval.
