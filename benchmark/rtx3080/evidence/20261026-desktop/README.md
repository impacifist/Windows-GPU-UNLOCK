# RTX 3080 measurement evidence with the desktop entry profile

This directory preserves the completed ordinary CUDA measurement from
2026-10-26. Read [NATIVE-VALIDATION.md](../../NATIVE-VALIDATION.md) for its
results, conditions, numerical coverage and limitations.

The measured source commit is
`439291fbbbc50e410985113a3f252059f93c6044`, with `source_dirty=false`.
The command was `run.py --execute --profile desktop`. The runner completed
three native processes, passed all four precision modes and exited with code 0.

## Preserved records

| Record | Purpose |
|---|---|
| [run/complete.json](run/complete.json) | Completion marker and SHA-256 values for the other five run files |
| [run/context.json](run/context.json) | Source/build provenance, desktop entry limits, pre/post GPU snapshots, host-memory readings and process durations |
| [run/trial-1.json](run/trial-1.json), [run/trial-2.json](run/trial-2.json), [run/trial-3.json](run/trial-3.json) | Original native trial records, numerical errors and timing samples |
| [run/summary.json](run/summary.json) | Recomputed trial and aggregate summaries |
| [build-manifest.json](build-manifest.json) | Measured source, compiler configuration and artifact hashes |
| [packages-manifest.json](packages-manifest.json) | Verified official dependency archives and extracted-tree receipt |
| [toolchain-install.json](toolchain-install.json) | Versioned compiler component installation receipt |
| [initial-quiet-refusal.json](initial-quiet-refusal.json) | Earlier default-profile refusal before native execution |
| [entry-observation.json](entry-observation.json) | Separate read-only observation of the desktop's ambient GPU allocation |

The JSON files are copied byte for byte from the original records. They have
not been reformatted. Every publication copy was matched to its original with
SHA-256 before committing. The `run/` subdirectory contains exactly the six
files accepted by the offline verifier; supporting records are kept outside it.
Git attributes preserve these bytes across checkouts and mark the JSON as
generated data for review.

Build-manifest SHA-256:
`6670f71b4dcf05284a8257ad688f8fff3b8927aa4c1879d09234056b49467e41`.

Completion-record SHA-256:
`e6067bd6ef57536f842c2c8326a7b7fcfc05c064305f0241b599622eee92292a`.

## Offline verification

From the repository root, with Python 3.11:

```powershell
py -3.11 .\benchmark\rtx3080\run.py --verify-run evidence/20261026-desktop/run
```

The relative bundle path is resolved beneath `benchmark/rtx3080/`, where
`run.py` lives, regardless of the terminal's current directory. Do not repeat
that prefix in the argument. An absolute bundle path is also accepted. See
[the harness path rules](../../README.md#offline-bundle-paths) for run names
and other saved bundles.

This checks the saved six-file bundle without GPU queries, CUDA installation
or native process execution. The original and copied bundles both returned
`offline-run-verified`. The copied build manifest was separately checked
against the digest in the run provenance and all eight recorded source hashes
were matched to the measured files.

The executable and NVIDIA/Microsoft binary dependencies are not redistributed
here. Artifact hashes support comparison with the actual measured local build;
the saved records alone do not reproduce its executable.

The reviewed JSON contains model/PCI identifiers, relative artifact paths,
versions, hashes and measurement data. It contains no personal filesystem
paths, hostnames, GPU UUIDs, serial numbers or credentials. These records are
evidence of a short ordinary CUDA baseline on current settings, not evidence
of factory configuration, unlock compatibility or a performance gain from
firmware changes.
