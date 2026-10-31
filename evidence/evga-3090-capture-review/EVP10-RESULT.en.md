# EVP10 acceptance result — 2026-10-30

## ZIP review supersedes the initial console interpretation

Inspection archive SHA256: `1a1b1fbaa312e802e6e872cdbbc4fc817e293494f5376039bc1dc8039038f989`. The actual log contains transaction-return `0x800000000000000f` (EFI_ACCESS_DENIED), followed by handoff-stopped with the same status. The collector's "incomplete" wording means the required successful event is absent, not that the function never returned. Receipt is STARTED; one capture is complete.

The cold admission record reports SS0=0 and SS1=0. Current `evga-admission.c` requires SS1=7, so this observation is sufficient to refuse admission. It does not prove all other admission predicates passed. The refusal occurs before selector latching, `Gu.armed=TRUE` and the vendor unlock load/call. Earlier wrapper operations still occurred; do not equate this with no GPU access or no state changes. Preserve the consumed slot; do not weaken SS1 qualification or replay EVP10. Whether entry state depended on restart/full shutdown remains unproven.

The added ordered entry is Boot000D, description `UEFI:  USB, Partition 1`; all previously ordered entries compare unchanged. Its origin is not established. It is not an allowed generic fallback, so cleanup correctly refused; do not delete it based on its description. The source manifest is available in the inspection for subsequent provenance comparison.

The offline analyser now reports exact transaction return status and entry SS1, accepts byte-identical duplicated package records but refuses conflicting duplicates. Four tests pass, including this observed refusal shape. Local analysis needs no repeated contributor execution.

[한국어](EVP10-RESULT.ko.md)

The contributor reported live guards, Shift+G, a hang and forced power-cycle recovery. USB was absent during the attempt. Collection reports missing transaction-return evidence and refuses cleanup for an unreviewed additional boot entry. This reusable packaging is not yet hardware-qualified; earlier SULK02 success does not establish success here.

The supplied photograph ends at the fresh-input prompt. It cannot identify the last completed transaction stage. The reported built EFI hash is `19eaa39d564dbb2a341bbb57cfd3c1b0877f00fe7d13f5183ff93056331f8ef8`, different from the local package; toolchain/source provenance requires inspection before attributing the failure.

Preserve the failed slot and state. Do not rearm or relax cleanup guards. After Collect, run `./research/profile/inspect-evga-package-failure.ps1` and send its ZIP. This copies the existing results ZIP and reads boot-entry names, descriptions and hashes for comparison. It makes no firmware writes and does not access the GPU. Private raw firmware backups are excluded. The controller remains blocked until the log and boot-entry difference are reviewed.

The inspection passed a synthetic incomplete-transaction/additional-boot-entry case with fake Native storage and zero firmware writes. This is a diagnostic test, not a new EFI retry.

For offline triage, run `python -I research/profile/review-evga-failure-zip.py "path-to-inspection.zip"`. It reads the outer/nested ZIP without extraction, reports the last 35 log lines, receipt prefix, capture count and boot-entry changes, and never authorizes a retry. It rejects ambiguous duplicate records and bounded-size violations. Three synthetic tests cover partial-run reporting, duplicate rejection and expanded-size rejection. The generated ZIP must be attached for review; console output containing its local path does not transfer its contents.
