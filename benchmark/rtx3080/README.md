# Ordinary CUDA baseline for the inventoried RTX 3080

**Validation status: native CUDA compilation, metadata inspection and three
GPU trials passed on the inventoried RTX 3080 using the desktop entry profile.**
The median results were 21.57 TFLOPS FP32, 60.44 FP16, 60.30 BF16 and 30.60 TF32.
See [NATIVE-VALIDATION.md](NATIVE-VALIDATION.md) for conditions and numerical
coverage, the [original measurement evidence](evidence/20261026-desktop/README.md),
and
[VALIDATION.md](VALIDATION.md) for the initial CPU-only checks.

This is a separate protocol-3 benchmark for the configuration documented in
[research PR #39](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/39).
It adds no firmware profile or boot functionality.

## Target and separation from existing benchmarks

The Python runner requires one NVIDIA GPU with all of these reported values:

| Field | Required value |
|---|---|
| Model | NVIDIA GeForce RTX 3080 |
| PCI device | `10de:2216` |
| PCI subsystem | `1458:403f` |
| Total memory reported by NVIDIA SMI | 10240 MiB |
| PCI location | `00000000:07:00.0` |
| Native CUDA capability | 8.6 |

The native program independently matches model, location and capability. Full
device/subsystem/memory admission belongs to the runner. A different location
or variant needs its own reviewed inventory and explicit code/protocol change.

The existing 3090, 3090 Ti and 3070 Ti scripts and protocol readers are unchanged.
Protocol 3 has different inputs, numerical checks and timing summaries; its
results must not be silently compared with the older protocols.

## Offline checks

From the repository root, with Python 3.11:

```powershell
py -3.11 -m unittest discover -s benchmark/rtx3080 -p "test_*.py" -v
.\benchmark\rtx3080\test-package-tools.ps1
.\benchmark\rtx3080\setup.ps1
.\benchmark\rtx3080\build.ps1 -CheckOnly
```

The Python tests use synthetic results, mocked NVIDIA queries and small owned
CPU-only child processes. Setup without `-Download` only reports package
presence. Build with `-CheckOnly` only reports compiler/package metadata.
The PowerShell tests use small synthetic ZIPs and restore process-local
environment values after testing compiler-option refusal.

The numerical helper is also a standalone C++ host test. In an x64 Visual
Studio developer terminal:

```powershell
New-Item -ItemType Directory -Force benchmark/rtx3080/build | Out-Null
cl.exe /EHsc /std:c++17 /O2 /W4 /fp:precise benchmark/rtx3080/test_numerics.cpp /Febenchmark/rtx3080/build/test_numerics.exe /Fobenchmark/rtx3080/build/test_numerics.obj
.\benchmark\rtx3080\build\test_numerics.exe
```

This test uses no CUDA headers, libraries or GPU calls. Its successful C++
compilation does not establish that NVCC can build the CUDA program.

## Dependencies and compilation

[packages-lock.json](packages-lock.json) pins the official CUDA 12.6.0 manifest
and four Windows archives by SHA-256 and size. The recorded versions are NVCC
12.6.20, CUDA runtime/CCCL 12.6.37 and cuBLAS 12.6.0.22. The archive download
total is approximately 466 MiB, before extraction.

The setup script requires the explicit `-Download` option to retrieve these
components. It verifies the pinned manifest and archive bytes before extraction,
then checks the complete extracted file/directory set and each file hash against
the archive. Extraction is staged before publishing a component; a rerun can
reuse a complete component only after verifying it again. The receipt is written
last. Licence files are retained, and no installer or driver change is involved.

The [CUDA 12.6 Windows compiler table](https://docs.nvidia.com/cuda/archive/12.6.0/cuda-installation-guide-microsoft-windows/index.html)
lists MSVC 192x/193x. The build script requires an existing x64 MSVC 14.2x/14.3x
environment, with optional `-VcVarsPath` and `-MsvcToolset` selection. It has
no unsupported-compiler override. The authorized native follow-up added toolset
14.29.30133 alongside the existing compiler and built successfully with compiler
19.29.30159.0. Select it explicitly with `-MsvcToolset 14.29`; `-VcVarsPath`
selects the intended installation. These scripts do not install compilers.

A successful build places the executable and two cuBLAS DLLs in `bin/`, then
writes `build-manifest.json` last. The manifest records source/artifact hashes,
compiler options, the resolved host compiler's version/hash, toolchain versions
and source commit/dirty state. Build rechecks extracted dependencies and refuses
ambient `CL`, `_CL_`, `NVCC_PREPEND_FLAGS` or `NVCC_APPEND_FLAGS` options. Their
values are not recorded. Build never launches the native output. Generated
packages, binaries, manifests and results are ignored by Git. A reviewed,
byte-preserving copy of the completed measurement and metadata is published
under `evidence/20261026-desktop/`; it contains no executable or toolkit binary.
Source hashes and the commit are checked before and after compilation so a
concurrent source edit prevents artifact publication.

## Inspection, later execution and saved-result verification

```powershell
py -3.11 .\benchmark\rtx3080\run.py
```

The default command verifies the exact artifact/source manifest offline. It
does not launch the native program, query NVIDIA SMI or create result files.
Before a build exists, it refuses because the required manifest is absent.

Only `run.py --execute` requests ordinary GPU computation. The native follow-up
was explicitly authorized by the owner. It first checks the manifest, allowed
environment and exact GPU identity, then applies the selected entry profile:

| Profile | Maximum utilization | Maximum allocated GPU memory | Maximum temperature |
|---|---:|---:|---:|
| `quiet` (default) | 5% | 2048 MiB | 65 C |
| `desktop` (`--execute --profile desktop`) | 5% | 3072 MiB | 65 C |

The desktop profile accommodates the inventoried desktop's ambient allocations.
Its name and exact limits are recorded in context schema 2 and verified offline;
identify the profile when publishing results. Immediately before each native
launch it also requires at least 1 GiB of available host RAM, measured through
the read-only Windows memory-status API. Those are test-policy thresholds,
not hardware ratings. Each trial has a pre/post GPU telemetry snapshot and a
timestamped prelaunch host-memory reading. Snapshots do not establish continuous
idle or factory stock configuration.

Execution uses three fresh sequential native processes, with 120 seconds per
process and 360 seconds overall. NVIDIA queries have their own short deadlines.
The runner bounds stdout/stderr, terminates only its own child on timeout or
cancellation, and performs no GPU reset. A deadline cannot guarantee recovery
from a driver hang.

### Offline bundle paths

After an authorized run, `run.py --verify-run <result-directory>` verifies the
saved completion record, file hashes, trial contract and recomputed summary
offline. It needs no GPU queries or installed CUDA artifacts. A partial result,
missing completion record or altered file is rejected.

The runner resolves the bundle argument as follows, independently of the
terminal's current directory:

- A bare run name resolves beneath `benchmark/rtx3080/results/`.
- A relative path with directory components resolves beneath
  `benchmark/rtx3080/`, the directory containing `run.py`.
- An absolute path is used directly.

For the published evidence, run this from the repository root:

```powershell
py -3.11 .\benchmark\rtx3080\run.py --verify-run evidence/20261026-desktop/run
```

Successful output contains `context.json`, three trial files, `summary.json`
and `complete.json`. The completion record is written last. Failed runs
remain incomplete. Results are never uploaded automatically.

## Numerical and timing contract

| Mode | A/B storage | Compute selection | C/output |
|---|---|---|---|
| FP32 control | FP32 | `CUBLAS_COMPUTE_32F_PEDANTIC` | FP32 |
| FP16 | FP16 | `CUBLAS_COMPUTE_32F` | FP32 |
| BF16 | BF16 | `CUBLAS_COMPUTE_32F` | FP32 |
| TF32 | FP32 | `CUBLAS_COMPUTE_32F_FAST_TF32` | FP32 |

All modes use `cublasGemmEx`, FP32 scalars `alpha=1`, `beta=0` and explicit
precision metadata. The non-pedantic modes disallow reduced-precision reduction.
Requested modes do not prove which instructions ran. See the
[cuBLAS API reference](https://docs.nvidia.com/cuda/archive/12.6.0/cublas/index.html).

Numerical coverage per mode is deliberately explicit:

- Five small cases check all 3783 output values, including signs, nonuniform
  inputs, rectangular shapes, transpose/stride handling, zero and identity.
- A 4096-square signed-diagonal known-answer case checks all 16777216 outputs.
- The **last timed random output** is scanned fully for non-finite values, with
  1024 prescribed stratified positions checked against FP64 CPU dot products.
  Arithmetic coverage of that random output is sampled; earlier timing samples
  do not each receive a separate full reference comparison.

FP16/BF16 references use the converted input values. Fixed tolerances, maximum
absolute/scaled errors and reference magnitudes are saved and checked. A failed
case cannot be relabelled as passing merely by setting a boolean.

Each process uses one explicit CUDA stream for cuBLAS and both timing events,
two seconds of warmup per mode and 30 timings in acquisition order. Transfers,
allocation and validation are outside the timed operation. Samples retain
elapsed milliseconds and repetition counts. The reader normalizes to time per
GEMM, averages the middle pair of the 30 sorted times, then derives throughput.
The aggregate is the median of the three process TFLOPS results.

```text
TFLOPS = 2 * M * N * K * repetitions / (elapsed_ms * 1e9)
```

Application-controlled GPU allocations are capped at 512 MiB; the current
matrix buffers require at most 192 MiB. The code separately checks free VRAM
with workspace headroom. Context/library allocations are outside that cap.

## Interpretation and publication

Call results an ordinary CUDA baseline **on current settings**. They do not
establish factory configuration, firmware state, unlock support, application
speedup or gaming FPS. Raw samples and hashes support reproducibility; JSON and
hashes are not hardware attestation.

Environment provenance is restricted to reviewed CUDA/cuBLAS settings. GPU
UUIDs and arbitrary environment text must not enter results. Metadata includes
model identifiers and relative artifact paths, not usernames, hostnames,
serials or personal paths. Review result files before publishing.

The repository's [licence](../../LICENSE) and
[third-party notices](../../THIRD_PARTY_NOTICES.md) remain in force. This source
contribution does not redistribute NVIDIA toolkit binaries.
