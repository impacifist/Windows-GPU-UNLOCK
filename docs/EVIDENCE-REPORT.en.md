# One report from existing capture files

**English** · [한국어](EVIDENCE-REPORT.ko.md) · [Offline checker](CHECK-COMPATIBILITY.en.md)

Bundle the existing offline check, file fingerprints and your observed boot outcome into one JSON report. **No new boot or unlock is needed.** The tool reads files and prints to the terminal; it does not execute firmware, run a native parser, generate a profile, stage files or upload anything.

## Run one command

From the extracted project folder, with Python 3.10+, replace the example folder with your existing captures:

```powershell
python -I .\research\profile\report-evidence.py "D:\RTX3090-UNLOCK" --windows-outcome direct-usable --boot-kind full-shutdown
```

Choose values describing **the existing run**, not the expected result. For captures with different outcomes, keep separate reports and explain the per-capture history in the issue. A single selected outcome applies to the supplied set as an operator statement; the tool cannot assign outcomes to individual boots.

| `--windows-outcome` | Meaning |
| :--- | :--- |
| `unknown` (default) | Outcome was not recorded or cannot be recalled |
| `not-run` | You did not perform an EFI run |
| `direct-usable` | Windows started directly with a usable display |
| `display-intervention` | Windows started directly but the display needed intervention |
| `manual-recovery` | Manual power-off/reboot or USB removal was needed |
| `failed` | Windows did not start or firmware hung |

`--boot-kind` accepts `unknown` (default), `full-shutdown`, `restart`, `sleep` or `hibernate`. `not-run` must use `unknown`. Unknown is valid: omit either option if unsure. Logs and checker success never fill these fields automatically.

## Read the result

| Field | What it means |
| :--- | :--- |
| `compatibility` | The existing format-2 offline checker result, including typed hardware observations and refusal reasons |
| `files_read` | Standard filenames, byte counts and SHA-256 of the exact immutable bytes consumed by this run |
| `installed_tool_sources` | Fingerprints of this reporting/checker/parser code and the installed envelope; not a source-tree attestation |
| `source_revision` | Optional full revision supplied by the operator; always marked unverified |
| `operator` | Your selected outcome and boot kind; always marked operator-reported and unverified |
| `build` | Optional comparison of a local generic-probe build manifest with local EFI bytes |
| `evidence_gaps` | Missing declarations or invalid build metadata; a candidate may still have gaps |

`status` retains the checker status unless optional build metadata is invalid, which produces `invalid_data`. Exit `0` means an offline candidate, `2` means missing/refused/invalid data, and `3` means invalid options or an installation error. **None certifies a complete evidence package, hardware support or recovery.** `hardware_tested`, `write_eligible` and `unlock_proven` remain false even when you report usable Windows.

The file list is not a full directory manifest: earlier driver dumps or unrelated files may not be read. It records only successful bounded reads used by the checker, including partial evidence on a refusal. A missing, oversized or unreadable file may therefore be absent. File hashes identify bytes, not the originating machine or separate cold boots. The checker rejects disagreements it knows how to detect; it cannot detect every mixed capture set.

## Optional · Compare your existing build files

If you already have the generic diagnostic build's `manifest.json` and matching `BOOTX64.EFI`, provide both. Do not build or boot another EFI merely to fill this section.

```powershell
python -I .\research\profile\report-evidence.py "D:\RTX3090-UNLOCK" --build-manifest ".\internal-boot\boot\build\probe-generic\manifest.json" --efi ".\internal-boot\boot\build\probe-generic\BOOTX64.EFI" --source-revision YOUR_FULL_COMMIT_HASH
```

Replace `YOUR_FULL_COMMIT_HASH` with the 40- or 64-character lowercase hexadecimal revision used for that build, or omit the option if unknown. This is a declaration, not Git verification. Local changes must still be described separately.

The reporter checks the manifest's generic diagnostic mode, false capability flags, bounded source-row structure and EFI hash. `matching-local-efi` means only that the supplied bytes match the supplied manifest. It does **not** verify source-file contents, PE validity, the manifest's authenticity, capture-to-EFI provenance, source revision or actual diagnostic behavior. Existing build/staging validators remain authoritative. Unsupported manifest types are refused; an unlock manifest is not a generic diagnostic manifest.

## Review, then share

Copy the reviewed JSON into a text attachment or fenced JSON block on your existing issue. It is an **evidence report**, not the format-2 input accepted by `compare-compatibility.py`; use the original checker for that comparison workflow.

> [!IMPORTANT]
> The report omits raw logs, firmware contents, input paths and free-text build fields. It includes file hashes and hardware model/topology identifiers, which can correlate reports. Review them before posting. Do not attach PROM/IMG/EFI files, private manifests, serials, UUIDs, keys or personal paths. Nothing is uploaded automatically.

The diagnostic producer format, existing checker CLI/format, profile gates and qualification policy are unchanged. No native FWSEC parser or hardware tests are run. Sharing this report does not authorize another hardware experiment.
