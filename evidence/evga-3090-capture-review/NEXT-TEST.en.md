# Next test: offline console-chain replay

[English](NEXT-TEST.en.md) | [한국어](NEXT-TEST.ko.md)

This test requires no reboot, firmware assets, BIOS change or new EFI. It reads the saved captures and tests whether the second GOP belongs to the observed firmware console chain. It does not generate a profile or change the existing topology admission gate.

On the PR96 branch, run from the repository root:

```powershell
python -I research/profile/inspect-evga3090-topology.py "D:\RTX3090-UNLOCK"
```

Expected for these submitted files: each capture reports expected_console_chain_observed=true, with write_eligible=false, unlock_proven=false and hardware_accessed=false. Exit0 means this observational chain test matched, not that an unlock build is eligible. If any check differs, preserve/send the JSON output rather than changing BIOS settings or forcing a profile. Missing, malformed or ambiguous records fail the test.

## What the existing files establish

Both DIAG00/01 match nine checks: one PCI display controller; two GOP handles; exactly one GOP under the RTX PCI path; a nonzero framebuffer on that GOP; no device path and zero framebuffer on the other GOP; a count1 graphics-console BY_DRIVER/BY_CHILD relationship connecting the two; a count1 text-console relationship from the second GOP to SystemTable ConsoleOut; and text output but no GOP on that final console handle.

The connecting binding identities are55987e62-1df6-41d1-9062-334fcac3e54d (graphics-console role) and628a497d-2bf6-4264-8741-069dbd3399d6 (text-console role). These are identities recorded in the captures, not authenticated driver-code qualification. The pathless/framebufferless GOP and shared-agent child relationship are evidence for a firmware virtual-console layer; the raw count of two does not identify a second physical GPU or an active iGPU. No firmware producer/consumer is disconnected by this test.

[Saved replay results](topology-test-results.json) contain the capture SHA256 values and boolean findings without raw handles/framebuffer addresses. The result refines the original topology_scope refusal; it does not remove it. ConOut variable absence and this aggregate console remain outside the current automatic route. Any support change needs separate implementation/replay review. Distinct file hashes still do not independently prove separate cold boots or Windows arrival.

Validation:10 synthetic regression tests passed, including wrong/shared driver, ownership count, binding identity, missing console/framebuffer, incomplete input and CRLF handling. Real strict-parser replay passed on both uploaded logs. No new hardware test was run. Keep BIOS unchanged while reviewing this captured baseline.
