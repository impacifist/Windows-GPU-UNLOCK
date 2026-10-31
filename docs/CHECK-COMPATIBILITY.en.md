# Check existing captures without running an unlock

**English** | [한국어](CHECK-COMPATIBILITY.ko.md) | [README](../README.md)

Want to include file fingerprints and your observed Windows outcome in one report? Use the [evidence reporter](EVIDENCE-REPORT.en.md), which wraps these same offline checks without running an experiment.

This offline checker explains whether **existing capture files** fit the automatic
local-profile route. It does not scan your running PC, collect a probe, run firmware,
generate a profile, build or stage an unlock, or change boot settings. Python 3.10+
is the only tool required for this check; a compiler and firmware parser executable
are not needed.

From the repository root, select the folder containing your `DIAGnn.TXT` files and
the corresponding latest `IMGnn.BIN` dumps and `PROM.BIN`:

```powershell
python -I research/profile/check-compatibility.py "D:\RTX3090-UNLOCK"
```

No captures yet? Read the [capture procedure](NEW-MACHINE.en.md#steps) first.
Obtaining captures is a separate firmware-boot operation; this command does not
perform it. A capture from a different machine does not describe your PC.

| Result | Meaning | Next step |
|---|---|---|
| `candidate` | Observations agree and pass the shared local-profile structural checks; latest driver code and PROM size/hash also pass | Follow [profile generation and host testing](NEW-MACHINE.en.md#steps) |
| `needs_data` | Captures, observations or required firmware files are missing | Obtain the missing data using the capture guide |
| `outside_scope` | The current automatic generator does not accept this GPU, console layout or firmware driver | Review the [exact supported configurations](COMPATIBILITY.en.md); this does **not** mean every project route is unsupported |
| `invalid_data` | A capture is malformed/inconsistent, records an unacceptable state, or exceeds an input limit | Review the stated reason and the original files locally; recapture as needed |
| `error` | The installed checker/envelope could not be loaded | Check the source installation |

Exit codes are `0` for a candidate, `2` for any other diagnostic result, and `3` for
an installation error. **Exit 0 is not permission to unlock.** The checker neither
reads nor issues write-build eligibility evidence; the existing build gates remain.
Fixed profiles and the automatic local-profile envelope are different scopes.

## Reports and language

```powershell
# Korean explanation
python -I research/profile/check-compatibility.py "D:\RTX3090-UNLOCK" --lang ko

# Machine-readable report, stdout only
python -I research/profile/check-compatibility.py "D:\RTX3090-UNLOCK" --json
```

The checker writes no files, including Python bytecode caches. Reports use
allowlisted reason codes/messages, standard `DIAGnn.TXT` filenames and typed
observations. Format 2 adds PCI model/subsystem/class IDs, PCI segment/bus/device/
function, display/GOP counts, EDID open-record counts, observed chip/memory values
and recognized console/EDID/nvgop binding versions and image sizes. These are
comparison inputs, not proof of support. Missing observations stay `null`, not zero;
an invalid graph has `available: false`, not an empty hardware inventory.

EDID `by_driver` counts records with exactly `EFI_OPEN_PROTOCOL_BY_DRIVER`; `active`
counts records other than passive attributes `1`, `2`, `4`. These are record counts,
not unique driver counts. Binding details stay unknown if the declared binding
snapshot is absent/incomplete. At most 16 adapters and 16 recognized binding records
are listed per capture; truncation is marked. The envelope digest identifies the
public checker configuration, not a device. Reports omit
input paths, raw records, serial numbers, memory addresses, PROM hashes and
firmware contents. No report or capture is uploaded. Review anything you choose
to share; do not attach raw firmware dumps to an issue.

Paste the reviewed JSON into the optional **Offline compatibility report** field
of the [compatibility issue form](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/new?template=compatibility.yml).
Keep board/BIOS, physical display connection, source/build and VBIOS history in the
other fields: this report cannot infer them. Reports in either language are welcome.
If you have no captures, the existing [Windows inventory collector](CONTRIBUTING.en.md)
or manually entered models are sufficient to submit an issue.

## Compare two reports

Choose new filenames if you want to save reports explicitly. For example:

```powershell
python -I research/profile/check-compatibility.py "D:\RTX3090-UNLOCK" --json | Out-File -LiteralPath .\compatibility-report-probe.json -Encoding utf8 -NoClobber
python -I research/profile/compare-compatibility.py .\compatibility-report-reference.json .\compatibility-report-probe.json
```

Saving is performed by your shell; the tool itself still writes no files.
`compatibility-report*.json` files are ignored by Git. Review reports before sharing.
The comparison reads two format-2 checker JSON reports (UTF-8 or BOM-marked UTF-16,
up to 1 MiB each), validates their typed schema and prints `same`, `different` or
`unavailable` for each field that differs or is unknown. `--json` includes all fields;
`--lang ko` changes the explanatory note. Duplicate keys, non-finite numbers and
invalid fields are refused. Different checker versions or envelope digests defer
comparison. Old format-1 reports should be regenerated from the same captures.

Capture numbers/repeated values do not identify a machine. The comparator compares
distinct observations per side, not boot history. It does not validate signatures or
source provenance and never reprints the reports' free-text messages. Differences
help investigate shared fixes; similarity does not make another profile safe.
Exit `0` means comparison completed, including when differences exist; exit `2`
means invalid reports or a deferred comparison. Windows inventory collector reports
use a different schema and are not accepted by this comparator.

## What remains unverified

- File contents do not establish their origin, separate cold boots, or the
  current hardware state. Even identical captures are not proof of separate boots.
- FWSEC parsing is not run. Profile generation still needs its trusted host parser.
- Required host replay tests, actual driver release, Windows recovery and hardware
  stability are not exercised. A candidate is not an additional verified machine.
- The latest capture's driver dumps are checked, matching the generator's policy.
  Earlier observations must agree on profile constants, PCI configuration and
  display inventory. Files are read into bounded immutable snapshots for each run.
- Limits: 32 `DIAGnn.TXT` files, 8 MiB per diagnostic, 4 MiB per required driver dump,
  exactly 1 MiB for a matching PROM, and 64 MiB of total input. Inputs must be regular
  files. Only the fixed capture filenames are read; no executable from the capture
  folder is launched.

Use the [new-machine guide](NEW-MACHINE.en.md) for the remaining stages. Never edit
a profile or firmware hash to force admission.
