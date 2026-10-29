# AD104 diagnostic and parser: host-only investigation

Source under study: external `gpu-unlock-ada` commit
`4e7fefcc4923f9e083d5324dc75a7d2f906af77f`.
Date: 2026-10-25. No GPU access or firmware execution is authorized by this work.

## Finding 1: the diagnostic is also an exact-platform sampler

`windows/ad104-msi/diagnostic/src/sample.c:CaptureGuard` requires the captured
card revision, PCI location `0000:01:00.0`, BAR0 `0x80000000`, a matching 16 MiB
resource descriptor and memory decode on with bus mastering off. It does not
discover arbitrary BDF/BAR locations before its MMIO reads. Consequently a
diagnostic capture may enumerate our matching GPU while refusing the deeper
register/PROM sample. This is a source-derived expectation, not an observed
rejection on our B550 host.

The host test suite already covers failed guards, early read failure and PCI
identity drift. PROM tests cover a bad signature and one mid-stream read failure.
Additional useful cases are failure/drift at the final identity check and ROM
signature changes during the copy. These can be simulated entirely in RAM.

## Finding 2: graph completeness is not evidence completeness

`diagnostic/src/platform.c:SamplePlatform` marks enumeration/API/budget failures
in `complete`. Driver-binding/image lookup failures and unavailable code details
are represented separately; a successful graph walk can still have missing code.
It always emits `atomic_snapshot:false` and `release_authorized:false`.
Consumers must check the required fields individually; neither `complete:true`
nor a code hash can authorize display release. Missing target/owner IDs and
malformed loaded-code cases were characterized with synthetic handles below.

## Execution boundary and status

The runner compiles ordinary Windows console executables with mocked UEFI
services and synthetic ROM arrays. It does not invoke either upstream
`Build.ps1`, produce an EFI, install/load a driver or use a device interface.
ROM patch tests modify synthetic process memory only. External
source remains pinned and unmodified. Build artifacts stay in an ignored local
`build/` directory; only research sources, summaries and hashes are committed.

The first commit recorded source inspection only. Subsequent execution checkpoints
and results follow, each committed locally without a remote push.

## Baseline execution checkpoint

The baseline has now run on Windows using MSVC 14.29.30133 (VS 2019 Build Tools).
[run_host_tests.py](run_host_tests.py) verifies 68 upstream file hashes against
[host-source-provenance.json](host-source-provenance.json), then compiles three
ordinary console programs. It does not invoke upstream build/install scripts.

| Program | Observed result |
|---|---|
| Diagnostic application with fake UEFI services | 5,530 checks passed |
| Platform graph and loaded-image parser mocks | 179 checks passed |
| VBIOS/FWSEC parser with synthetic fixture | Passed, including every length 0–4095, valid expansion-chain variants and malformed ranges |

[baseline-host-results.json](baseline-host-results.json) preserves the local
compiler version, results and executable hashes. Hashes identify this run's
artifacts, not a promise of reproducible MSVC binaries. Upstream's printed phrase
"full EFI application executed against host mocks" describes calling `UefiMain`
inside a normal console test; no EFI image was produced or executed by firmware.
The parser test's optional real-ROM argument was not supplied. Its patch routine
operates only on synthetic in-process arrays.

Reproduce with a separately obtained checkout of the pinned fork:

```powershell
python research/ada-4070ti/run_host_tests.py ../gpu-unlock-ada-research `
  --vcvars 'C:/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
```

Builds use `/W4 /WX`. The final runner clears inherited `CL`, `_CL_`, `LINK` and
`_LINK_` option variables and explicitly undefines `NDEBUG` to keep assertions
enabled. It verifies each binary is an x64 Windows console PE (subsystem 3)
before executing it. The Linux parser fixture needs explicit
MSVC warning exemptions for byte narrowing (4244), unsigned checksum negation
(4146) and an optional integration variable shadow (4456). The diagnostic tests
do not use these exemptions. VS 18's setup script failed on this installation;
the working VS 2019 environment was used without changing either installation.
Initial compiler-environment quoting was corrected before the successful run.

The runner writes artifacts/capture/logs only under this folder's ignored
`build/` tree. All services in the application test are fake function pointers;
no Windows device handles, driver loading, MMIO or PCI access is performed.

## Additional capture and parser cases

The runner now also compiles three local harnesses against the pinned upstream
fixtures, without changing the external checkout. The local files include the
upstream test sources to reuse their fake protocol callbacks; renamed upstream
`main` functions are not called by the extra harnesses.
[extended-host-results.json](extended-host-results.json) records all six targets.

| Additional harness | Result and observation |
|---|---|
| [host_capture_cases.c](host_capture_cases.c) | 10 capture-failure scenarios, 2,832 assertions, all pass |
| [host_platform_cases.c](host_platform_cases.c) | 5 graph cases, 13 malformed PE cases and 1 overlap characterization; 103 assertions, all pass |
| [host_parser_cases.c](host_parser_cases.c) | 14 extra signature/descriptor checks, all pass |

The 10 capture cases inject final PCI failure/drift, final chip failure/drift and
a changed PROM signature during copying; register cases use zero/all-ones chip,
a later `badf` sentinel and failed/changed final chip. Every incomplete PROM
emits `sha256:null` and `hex:null`; attempts stay bounded at 258/259. Register
failures stop at 1, 2 or 23 reads as expected. All cases retain
`unlock_eligibility:false`. This validates containment against these fake
callbacks, not MMIO read safety or bus behavior on real hardware.

### Characterized observation limits

The five graph cases reproduce `complete:true` while respectively:

- The target has a retrievable path but is absent from the enumerated handles.
- Driver binding is unavailable.
- Loaded image is unavailable.
- The image has an unsupported machine type, yielding `code:null`.
- An ownership agent is outside the enumerated inventory, yielding `agent:null`.

The graph still reports `atomic_snapshot:false` and `release_authorized:false`.
These passing characterization assertions document existing behavior, not a fix
or a successful hardware qualification. A consumer needing an exact ownership
proof must check resolved target/edge identities and required driver code fields
in addition to graph enumeration status. An API returning success is insufficient.

The PE boundary tests reject invalid machine/magic, empty/oversized executable
sections, inconsistent relocation directory pairs and malformed block sizes.
One deliberately unusual **loaded-memory** fixture places an eight-byte DIR64
relocation at RVA `0x1fc`, crossing code start `0x200`. `LoadedCodeView` accepts
it and counts zero code relocations because its test considers the relocation's
starting address. Thus `relocations_in_code:0` means no counted relocation starts
in code, not proof that no relocation overlaps code. This test does not show an
exploitable path, alter a driver or apply a relocation; no production parser was
changed. Any future consumer needing overlap exclusion should validate the
whole relocation span and supported relocation type.

The ROM tests cover signature version 15, unsupported version 16, missing mask
bits, oversized masks, zero signature size, null output and multiplication
overflow. A synthetic descriptor with two signatures parses successfully while
its image geometry differs from the exact hardware profile. Parsing validates
structure, not authenticity, execution safety or `GuAd104Prepare` admission.
No board-owned ROM was read or supplied. No sanitizer or real-firmware coverage
is claimed by these assertion-based checks.

## Python analyzer cross-check and final rerun

[check_host_analyzer.py](check_host_analyzer.py) verifies the same upstream input
hashes before importing the reviewed pure-JSON `analyze.py`. It feeds the host
application's synthetic capture and the 15 new capture/graph outputs to that
analyzer. No GPU module or upstream command-line entry point is imported/run.
Eight test methods pass, with additional subcases for corrupted/short PROM,
wrong sample counts/offsets/final chip, malformed code/graph edges, mismatched
build IDs, target counts and claimed release/write authority.

The 10 partial captures remain explicitly incomplete. All five limited graph
captures are accepted as `platform_graph_complete:true`, including the absent
target case. Four have no verified code capture; the absent-target case has one.
All keep `unlock_ready:false` and `hardware_support_validated:false`.
Their `stable_fingerprint` values are identical because that field covers the
target inventory subset, not the platform ownership graph. It must not be
interpreted as proof that ownership is stable across captures.

[host-analyzer-results.json](host-analyzer-results.json) records these cases.
[final-host-results.json](final-host-results.json) supersedes the earlier local
execution snapshots for the six-target checkpoint; all six console targets passed again
after the compiler-option review correction. The source hashes embedded in those
results bind the local harness versions. No hardware support is established.
The later seven-target run is documented in [OFFLINE-FOLLOWUP.md](OFFLINE-FOLLOWUP.md).

After the runner command above, execute:

```powershell
python research/ada-4070ti/check_host_analyzer.py ../gpu-unlock-ada-research
```

### Consequence for a future capture plan

A proposed capture consumer should separately report target resolution, edge
resolution, required driver-code availability, register/PROM completeness and
identity stability. Keep these as observations with no write eligibility.
If exact relocation-free code is required, validate relocation span/type rather
than only counting starting addresses. A future generic sampler also needs
reviewed resource discovery before changing the present exact BDF/BAR guard.
These are implementation requirements for later work, not a reason to boot the
current probe or relax its admission checks.

Compiler-option isolation was applied and all affected targets were rerun.
