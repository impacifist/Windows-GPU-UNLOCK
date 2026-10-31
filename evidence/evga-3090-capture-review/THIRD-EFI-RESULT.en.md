# Third EFI result: console-interface refusal

EVGA02 completed capture and passed the exact one-display/matching-card inventory. The initial graph refused at `evga-console-interfaces` with EFI_UNSUPPORTED before the G prompt or any Stop. Movian reports the same automatic Windows return as the preceding run. See [sanitized evidence](third-efi-run-result.json).

The next build logs the virtual SimpleTextOut query, system-console SimpleTextOut query, and system-console GOP query separately, including whether the last query returned a pointer. This identifies API status versus a null interface or unexpected GOP. No acceptance condition is relaxed and no extra driver Stop or GPU access is added. Existing risk and one-shot protections remain.

Local validation: 22 membership host checkpoints, including four console-interface refusal cases, and nine capture regressions passed. EFI compilation and PE verification passed. These are host/build checks; the driver Stop remains untested on this machine.
