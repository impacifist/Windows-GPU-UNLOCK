# Initial offline validation record for the ordinary RTX 3080 baseline

Date: 2026-10-26 UTC.

**Historical snapshot before native execution was authorized:** source
implementation with host-only verification. The later build and execution work
is recorded in [NATIVE-VALIDATION.md](NATIVE-VALIDATION.md).

This contribution starts from upstream commit `5c98f11eaafdcb7a2a06fb14fcdcde7297eb409e` and is separate from [research PR #39](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/39). That research does not establish unlock support.

## Verification scope

The checks below run on the Windows host. Python fixtures contain synthetic numbers and mocked device responses; those numbers are test data, not hardware measurements. The only C++ executable built and run is the CPU-only `test_numerics.cpp` helper. It has no CUDA headers, CUDA libraries or GPU calls.

| Check | Observed result |
|---|---|
| Combined Python suites | 101 tests passed in 25.072 seconds; exit 0 |
| Protocol reader subset | 49 tests |
| Runner and saved-bundle subset | 52 tests |
| CPU numerical helper | 15/15 test groups passed |
| Synthetic PowerShell package/compiler/source checks | 15 checks passed; exit 0 |
| PowerShell syntax parsing | All 4 scripts parsed without errors |
| Default setup / build readiness | Both passed; no download or compilation requested |
| Default runner with no build manifest | Expected refusal: exit 2, `required_file_unavailable` |
| Native CUDA build | Not attempted |
| Native benchmark / GPU workload | Not executed |
| NVIDIA redistributable archive download or installation | Not performed |
| Firmware, registers, power/clock settings or security settings | No changes made |

The CPU helper compiled with MSVC 19.44.35229 for x64 from toolset 14.44.35207, using `/EHsc /std:c++17 /O2 /W4 /fp:precise`. Host compilation success does not establish NVCC compatibility.

## Numerical helper coverage

The 15 CPU test groups cover golden FP16/BF16 rounding boundaries and exhaustive round trips of representable finite/infinite values, deterministic signed inputs, independent rectangular and transposed/strided products, references using converted inputs, interior corruption with unchanged endpoints, non-finite values, exact diagonal references, unique stratified positions, normalized medians/throughput arithmetic, PCI parsing and environment privacy.

These tests exercise the reference and validation machinery. Actual cuBLAS results, precision behavior, selected kernels and execution time remain untested.

The Python suites cover strict result contracts, malformed and non-finite data,
timing recomputation, incorrect numerical evidence, environment privacy, exact
artifact/source sets, compiler metadata, runtime version gates, host-memory
preflight, bounded CPU child processes, completion-last writing and saved-bundle
corruption. NVIDIA queries, CUDA output and host-memory readings are mocked.
Only the bounded subprocess tests launch real, small CPU Python children.

The PowerShell checks use small synthetic ZIPs to exercise staged extraction,
verified reuse, interrupted state, altered/missing/extra files and directories,
and atomic receipt creation/replacement. They also check compiler family/path
validation, ambient-option refusal and source/commit changes. Temporary files
stay under the ignored build directory, and process-local environment values
are restored. No NVIDIA archive is needed for these tests.

## Reproduction commands

From the repository root, with Python 3.11:

```powershell
py -3.11 -m unittest discover -s benchmark/rtx3080 -p "test_*.py" -q
.\benchmark\rtx3080\test-package-tools.ps1
.\benchmark\rtx3080\setup.ps1
.\benchmark\rtx3080\build.ps1 -CheckOnly
```

In an x64 Visual Studio developer terminal:

```powershell
New-Item -ItemType Directory -Force benchmark/rtx3080/build | Out-Null
cl.exe /EHsc /std:c++17 /O2 /W4 /fp:precise benchmark/rtx3080/test_numerics.cpp /Febenchmark/rtx3080/build/test_numerics.exe /Fobenchmark/rtx3080/build/test_numerics.obj
.\benchmark\rtx3080\build\test_numerics.exe
```

Default setup and build `-CheckOnly` are metadata-only checks. Default `run.py`
verifies saved source/artifact hashes without launching the native program or
querying NVIDIA SMI; refusal is expected when no build manifest exists.
`run.py --verify-run` reads an existing completed bundle offline.

## Dependency and compiler evidence

The small official CUDA 12.6.0 redistribution manifest was retrieved and hashed:

- URL: https://developer.download.nvidia.com/compute/cuda/redist/redistrib_12.6.0.json
- SHA-256: `87740b01676b3d18982982ab96ec7fa1a626d03a96df070a6b0f258d01ff5fab`

The four locked archive sizes, paths, versions and hashes come from that manifest.
The archives themselves have not been downloaded, installed or compiled against.
Synthetic archive tests do not verify NVIDIA's real archive layout or runtime
DLL behavior.

The initial host compiler inventory listed only MSVC 14.44.35207. The
readiness check found the compiler setup script, no accepted 14.2x/14.3x
toolsets, no project CUDA packages and no ambient compiler-option overrides. The
[CUDA 12.6.0 Windows guide](https://docs.nvidia.com/cuda/archive/12.6.0/cuda-installation-guide-microsoft-windows/index.html)
lists MSVC 192x/193x; [Microsoft's compiler table](https://learn.microsoft.com/en-us/cpp/overview/compiler-versions?view=msvc-170)
maps MSVC 14.44 to 1944. The build script provides no unsupported-compiler override.
A supported x64 compiler configuration must be established before validating
a native build.

## Evidence gates at the initial checkpoint

1. Verify the real pinned package extraction and build with a documented compatible host compiler, retaining exact build provenance.
2. Review the built artifacts and execution policy before requesting the separately authorized ordinary GPU run required by the adopted contribution guide. No such execution authorization is inferred from source-work approval.
3. During that authorized run, verify all four numerical modes, actual emitted protocol-3 JSON, three complete fresh processes, resource/deadline behavior and offline bundle verification.
4. Review saved telemetry and raw samples before publishing measurements or making any stability, throughput or application-performance claim.

The known-answer case checks all outputs, while arithmetic validation of the last timed random output checks 1024 positions. A full finite-value scan does not prove all-element arithmetic correctness. Recorded hashes and process metadata check consistency; they are not signed execution or hardware evidence.
