# Stage 1 console-splitter pilot

Research source and host-only tests for one bounded UEFI console-splitter
disconnect on the pinned MSI X570 / ASUS RTX 3080 Ti configuration. **This
pilot is hardware-untested.** Passing these tests does not establish firmware
compatibility, hardware safety, recovery, or an unlock.

No firmware, captured machine code, raw captures, EFI image, deployment
package, installer changes or firmware build script is included here. The
normal unlock profiles remain unchanged. This is not a new supported profile.

## Scope

Stage 1 authenticates the target identity, ownership graph, driver metadata
and code hashes. After explicit confirmation it permits exactly one
`DisconnectController(physical display, console splitter, NULL)` call. It
checks the resulting graph, physical text interface, configuration,
attributes, code hashes and fresh keyboard input, writes evidence, revokes
its authority and halts. It cannot advance to a graphics/NVIDIA disconnect,
PCI claim, decode restoration, unlock payload or Windows handoff.

The bounded policy accepts exactly raw PCI command bits `IO=0, MEM=1, BME=0`
and protocol attributes `0x200` or `0x300`. The latter pair covers a protocol
IO-attribute discrepancy without accepting raw IO enablement. It grants no
general release or decode permit. This is an experimental policy decision,
not evidence that the discrepancy is harmless on physical firmware.

The source includes two reviewed corrections:

* Recheck raw PCI state after PROM-buffer allocation, immediately before its
  first GPU read. Reject unsafe or changed state and free the buffer. Apply
  the corresponding guard at cold-register read boundaries.
* Preserve the first failure's phase, checkpoint, attempted/valid flags,
  status and available observations. A valid configuration snapshot contains
  all 64 words and the first differing offset/expected/observed values.
  Attributes are reported only when actually read successfully. Early
  identity failures identify their checkpoint without inventing observations
  from later checks.

## Run without private inputs

Use Linux or WSL with Python 3.9+, Clang with AddressSanitizer and
UndefinedBehaviorSanitizer, and `nm`. The EDK2 headers already in this
repository are sufficient. No network, Git history, firmware download,
capture path or binary fixture is required.

From the repository root, choose a new output directory outside the checkout:

```sh
python3 research/msi-release-pilot/test.py --output ../stage1-host-results
```

The runner refuses an existing output directory. It compiles ordinary host
executables and runs the real pilot entry, core, policy and diagnostic code
against simulated services. It does not compile, load or run an EFI image.
Generated headers, executables, logs and `results.json` remain in the chosen
output directory and must not be included in a source PR.

`synthetic.py` constructs a minimal PCI capability chain, role-based handle
graph and deterministic byte arrays. Python computes expected hashes;
the C tests independently fill buffers and use the real SHA-256 checker.
Code-looking buffers are data only. The test never invokes a driver Stop
pointer stored in those buffers. The ordinary compiled host shim models the
splitter's child and parent cleanup calls. The model cannot establish what
an actual MSI Stop implementation does.

Synthetic hashes require both `GU_PILOT_SYNTHETIC` and
`GU_PILOT_HOST_TEST` in a hosted compilation. Freestanding builds reject
them. The real metadata in `profile-data.h` is never replaced in production
profiles. Missing/wrong stages, production combinations and firmware use of
the host halt hook are rejected by explicit compile checks.

Coverage includes accepted/rejected raw-command and attribute pairs,
ownership and identity drift, changed code bytes, allocation-return drift,
partial/failed reads, exact failure checkpoints, full PCI diagnostics,
confirmation and fresh-key handling, partial Stop failures, logging failures,
TPL restoration, buffer cleanup, terminal authority and blocked later stages.
Virtual source overlays deliberately remove guards or corrupt reports.
Each control must fail at its expected assertion; a compile failure or an
unrelated test failure does not count as a detected regression.

## Limits and evidence interpretation

* Allocation-return drift is a software boundary counterexample. It shows
  what happens if a service returns after changing PCI state; it does not
  demonstrate that MSI firmware actually does this. Rechecks are snapshots,
  not an atomic guarantee against all asynchronous device/firmware activity.
* The minimal synthetic model is not a replay of a captured machine or a
  firmware emulator. Its handles, graph size, register values and data
  patterns do not prove a particular physical setup is compatible.
* A complete primary report and `REPORT-CLOSED.OK` can already exist when a
  later sidecar/directory/root close or final verification fails. The marker
  proves only the earlier primary-report closure. It is not proof of final
  success, restored display/input, or recoverability. This acknowledged
  late-close ambiguity remains.
* An artificial direct `PilotVerify` reentry during preflight logging can
  replace the first diagnostic before reporting freezes it. The original
  failure still prevents effects and success. The harness does not establish
  a real firmware callback route for that reentry. Freeze protection during
  diagnostic emission is tested; broader preflight reentry hardening remains
  separate work.
* No real allocation interleaving, splitter Stop, input continuity, filesystem
  durability, shutdown/recovery or hardware damage risk has been validated.
  A hardware run requires its own review and explicit authorisation.

## Source organisation and provenance

This focused extraction comes from local research revision
`9b10bfae5c05d5077a924e5cbec13a4ce1343df9`, rebased as a source-only contribution
on `e3f03cd9389195adcabda4743d8fc2922b32498c`. That research revision and any
private evidence are not prerequisites for building these tests.

`entry.c`, `core.c`, `stage1-policy.h` and `stage1-diagnostics.h` retain the
reviewed Stage 1 flow and guards. Other compiled pilot stages are excluded.
`read-state.c` extracts the read/identity helpers needed from the research
`full-release.c`; no general GPU-release/decode implementation is added.
It shares the existing console inspection helpers through `prepare-console.c`.
Future fixes to those extracted helpers should be compared with their source
to avoid divergence.

Only three existing files need hooks: `prepare-console.c` selects the isolated
profile and verifies console hashes for pilot builds; `port.h` selects that
profile for shared PCI checks; `checks.c` uses its target bus. Normal builds
retain bus 1 and their existing hardware/profile guards. Existing
`machine-profile.h`, `full-release.c`, installers and firmware assets are
unchanged. The upstream copyright notices and GPL-2.0-only licensing apply.

## Maintainer integration review (26 October 2026)

The integration branch includes upstream main `5c98f11` (diagnostic-hardening
PR #37). The pilot's incompatible-profile guard now also rejects the X870,
B660-PLUS, H610M, H610M observation and local-profile definitions. The public
guard runner covers the existing production profiles as well as these additions:
one positive control and 17 incompatible-build cases.

The Windows Clang 20/MSVC2019 host review passed 489 adversarial cases and
55 safety/evidence cases, with 564,693 assertions. Twelve production object
pairs across five published profiles and a synthetic local-profile definition
were identical after normalizing only the COFF timestamp. The host link map
also checked the expected entry/core symbols and absence of prohibited
capabilities; real-profile freestanding entry/core objects compiled.

The subsequent merge of `5c98f11` changes no files under `internal-boot` or
`research/msi-release-pilot` relative to that tested integration, apart from
this review note. Linux ASan/UBSan and the 15 VFS overlay controls were not
rerun in the Windows review. No physical Stage 1 run, EFI boot, keyboard
continuity, Windows recovery or hardware safety is established. The contribution
remains Draft pending the source-readiness and final merge decisions.
