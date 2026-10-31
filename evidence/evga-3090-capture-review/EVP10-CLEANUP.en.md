# Reviewed EVP10 cleanup test

[한국어](EVP10-CLEANUP.ko.md)

From administrator PowerShell at the updated repository root, run `./research/profile/cleanup-reviewed-evp10.ps1`. No reboot is involved. Optional `-WhatIf` performs only the checks.

This is a firmware-writing cleanup test, not another unlock attempt. It requires the exact failed EVP10 log hash and package/journal, clear BootNext, unchanged original ordered entries and exactly one appended Boot000D with the reviewed attributes and SHA256. It preserves Boot000D, the entire BootOrder, files, receipts and failed result. It backs up the reviewed current firmware snapshot, then calls the existing race-checked cleanup helper to delete only the exact owned Boot7Exx option. It never sets BootNext, accesses GPU registers, rearms EVP10 or reboots. Unexpected state refuses cleanup. Firmware API failure may leave the option present; do not manually delete entries to bypass a refusal.

Send the final JSON. Expected `cleanup_completed`, `failed_attempt_preserved` and `usb_entry_preserved` are true; `rearm_authorized`, `boot_order_written` and `gpu_accessed` are false. The original failed result is deliberately not rewritten as a success. No new EFI is staged by this test.

Local verification covers real SHA256, mocked reviewed-entry admission and four refusal mutations, plus syntax in PowerShell 7 and Windows PowerShell 5.1. Existing mocked cleanup race/ownership tests also pass. This does not establish successful live deletion. Entry SS1 differed from SULK02 (7) to EVP10 (0); its cause still needs investigation before another unlock attempt.
