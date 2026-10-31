# Triage compatibility reports

**English** | [한국어](COMPATIBILITY-TRIAGE.ko.md)

Use the [compatibility form](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/new?template=compatibility.yml) and [contributor guide](CONTRIBUTING.en.md). Inventory-only reports are welcome. Ask for existing evidence; do not ask someone to boot, change firmware settings or repeat a failed experiment merely to complete intake.

## Review in order

1. Search existing issues and PRs. Match the reporter, physical GPU, board/BIOS, source/build and display route before treating reports as duplicates. Link related cases; preserve the original evidence and identify changed settings or builds.
2. Record exact board/CPU/GPU, reported PCI/subsystem IDs, BIOS, Windows/driver versions and physical monitor connection where known. Keep physical card identity separate from software identity or cross-flashed VBIOS. Unknown remains unknown.
3. For an existing run, bind each capture to the source revision/local changes, generated profile, EFI SHA-256, build command/options and compiler. Ask for sanitized excerpts or hashes, not firmware assets, PROM/IMG dumps or personal system reports.
4. Record the operator's Windows outcome separately: direct usable display, display intervention, manual power cycle, failure, or unknown. An end-of-log marker cannot fill this field.
5. Review measurements independently: matched precision/accumulation, matrix size, library/driver versions, run counts and boot lifecycle. A successful transaction is not proof of a performance gain.

## Keep separate evidence states

| State | What it establishes | What remains unproven |
|---|---|---|
| Report received | Contributor supplied observations | Capture validity and support |
| Capture validated | Reviewed captures and identity/build relationships satisfy the applicable checker | Hardware transaction and Windows recovery |
| Transaction reported | Existing logs report transaction/cleanup results | Independent qualification, Windows outcome and performance |
| Windows outcome reported | Operator describes the actual handoff/display result | General compatibility and measured gain |
| Hardware qualified | Maintainer accepts the exact configuration against the project's qualification requirements | Other machines, firmware versions or display routes |

These are review descriptions, not automatic labels or an ordered success counter. Host checks may pass while hardware fails. Record each result and its source separately using the [validation record](VALIDATION.en.md); consult the [compatibility matrix](COMPATIBILITY.en.md) and [confirmed setup evidence](CONFIRMED-SETUPS.en.md) before claiming existing coverage.

## Follow up and resolve

Bundle missing information into one specific request. Say which existing artifact or observation answers each gap, and accept unavailable evidence. Keep failures and contradictory results visible; a later successful run must name its changed build/settings rather than overwrite the earlier outcome.

For a supported LOCAL configuration, explain whether it needs an owner-generated private profile instead of a new hardcoded motherboard profile. Do not copy another machine's addresses, hashes or ownership pins. Requests to expand runtime admission or qualify new hardware need core-owner review.

Only use a PR closing keyword such as `Resolves #123` when the PR actually addresses that issue's request. Acknowledging a report or adding it to a table does not by itself resolve an outstanding failure. Use a non-closing reference for partial work. Check GitHub's linked issues before merge; even incidental wording such as `Closed #123` can be interpreted as a closing keyword. Review labels describe work type and are not support certifications.

This guide defines intake and evidence handling. It does not replace maintainer qualification policy or authorize hardware execution.
