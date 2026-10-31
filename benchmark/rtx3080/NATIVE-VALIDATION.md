# Native validation follow-up

Date: 2026-10-26 UTC.

**Current status: CUDA compilation, native metadata inspection and all four
precision modes in three GPU processes passed using the desktop entry profile.
The completed bundle and its publication copy both passed offline verification.**

The owner explicitly authorized continuing with the compatible compiler,
native build and ordinary GPU execution after the initial draft contribution.
The earlier [CPU-only validation record](VALIDATION.md) is retained as a
historical snapshot. This follow-up concerns ordinary CUDA computation.

## Toolchain and package preparation

Microsoft's versioned v142 component was added alongside the existing Build
Tools installation using the official signed Visual Studio installer:

- Component: `Microsoft.VisualStudio.Component.VC.14.29.16.11.x86.x64`.
- Installer exit code: 0; completed at 2026-10-26T14:04:08.3893412Z
  ([installation receipt](evidence/20261026-desktop/toolchain-install.json)).
- Automatic restart and forced application closure were disabled.
- Selected toolset: `14.29.30133`.
- Actual compiler file version: `19.29.30159.0`.
- Compiler SHA-256: `8e4352b523f4ff37b429aea1d340094be1e17bbdf4f687247fc0be92d6eb9440`.

All four pinned NVIDIA archives were downloaded from the official redistribution
server, checked against [packages-lock.json](packages-lock.json), extracted into
the project, and verified file-by-file. Setup completed with exit 0 and wrote its
[schema-2 dependency receipt](evidence/20261026-desktop/packages-manifest.json)
after verification. No NVIDIA driver installer ran.

The first native build used source commit
`1caa536a03c3a307ab40c0031d8dd862c617f0c5` with a clean working tree, NVCC
12.6.20, CUDA 12.6.0 and cuBLAS 12.6.0.22. Compilation and linking succeeded.
The build's manifest SHA-256 was
`d3bc6b4603d423add9ad9b250686f8c6d94081aaa837fa0285e10530c743e429`.

Native `--inspect-only` execution succeeded under a 10-second process deadline,
reported protocol 3 / CUDA 12060 / MSVC 1929, and performed no CUDA API calls.
The CPU numerical helper was rebuilt with v142 using
`/EHsc /std:c++17 /O2 /W4 /fp:precise`; all 15 groups passed.

The packaged cuBLAS header defines 12/6/0 with build suffix 22 and
`CUBLAS_VERSION=120600`. The CUDA runtime header defines `CUDART_VERSION=12060`.
The runtime guards therefore remain unchanged.

## Initial quiet-profile refusal

The original quiet-profile attempt at 2026-10-26T14:11:09.431Z was refused at
`pre_query` with `entry_not_quiet`, zero validated trials and no completion
record. No native GPU process was launched. The
[refusal record](evidence/20261026-desktop/initial-quiet-refusal.json) is retained.

A separate [read-only observation](evidence/20261026-desktop/entry-observation.json)
at 2026-10-26T14:12:58.877384Z reported:

| Field | Observed value |
|---|---:|
| GPU utilization | 0% |
| Allocated GPU memory | 2136 MiB |
| Total reported GPU memory | 10240 MiB |
| GPU temperature | 34 C |
| Reported power limit | 370 W |
| Operating SM / memory clocks | 210 / 405 MHz |

The allocation exceeded the original 2048 MiB entry ceiling. A low utilization
snapshot does not prove continuous inactivity. No applications or documents
were closed to obtain the measurement.

## Explicit entry profiles

The default quiet profile remains available. A separate desktop profile records
the less restrictive ambient allocation condition needed for this desktop:

| Entry requirement | Quiet | Desktop |
|---|---:|---:|
| Maximum GPU utilization | 5% | 5% |
| Maximum allocated GPU memory | 2048 MiB | 3072 MiB |
| Maximum temperature | 65 C | 65 C |
| Minimum available host RAM | 1 GiB | 1 GiB |

The desktop ceiling still leaves 7168 MiB of the reported total unallocated at
entry. Native free-memory and allocation-budget checks also apply. These are
measurement policies, not hardware ratings or proof of an idle machine.

The profile name and exact limits are included in context schema 2 and checked
by the offline reader. The new profile does not change numerical tolerances,
input generation, precision modes, sample counts, process deadlines, runtime
version requirements, or device settings. It must be identified explicitly
when reporting measurements; it is not the original quiet-profile condition.

The updated combined Python suite passed 112 tests (49 protocol and 63 runner)
in 33.137 seconds, exit 0. Tests cover both profiles' boundaries, preservation of
the default quiet policy, invalid profile names, and profile/limit tampering
before and after file-hash recomputation. Relabeling a desktop record at
2136 MiB as a quiet record is rejected.

## Completed GPU measurement

The profile change was committed before measurement as
`439291fbbbc50e410985113a3f252059f93c6044`. The working tree was clean when the
native program was rebuilt and executed. The second native build passed with
the same pinned toolchain. Its [build manifest](evidence/20261026-desktop/build-manifest.json)
has SHA-256
`6670f71b4dcf05284a8257ad688f8fff3b8927aa4c1879d09234056b49467e41`.
The measured source commit remains the provenance of these results; the later
evidence/documentation commit does not represent a new GPU run.

The authorized command was:

```powershell
py -3.11 .\benchmark\rtx3080\run.py --execute --profile desktop
```

The runner exited with code 0. Its first prelaunch GPU snapshot was at
2026-10-26T14:20:13.782Z and its completion record was written at
2026-10-26T14:20:55.411Z. The three native processes took 12.797, 12.078 and
12.031 seconds respectively. This was one short, three-process measurement
session on the current configuration.

Each mode uses a 4096 x 4096 x 4096 GEMM, two seconds of warmup and 30 timed
samples per process. A process result derives throughput from its median
normalized GEMM time; the aggregate is the median of the three process
throughputs. These are requested cuBLAS precision modes, not instruction-trace
evidence.

| Mode | Process 1 TFLOPS | Process 2 TFLOPS | Process 3 TFLOPS | Median TFLOPS |
|---|---:|---:|---:|---:|
| FP32 pedantic | 21.5663 | 21.6620 | 21.4766 | **21.5663** |
| FP16, FP32 accumulation | 59.7719 | 60.4448 | 60.7320 | **60.4448** |
| BF16, FP32 accumulation | 60.7870 | 59.9588 | 60.2955 | **60.2955** |
| TF32, FP32 accumulation | 30.5979 | 31.0725 | 30.3901 | **30.5979** |

The [original six-file result bundle](evidence/20261026-desktop/run/) retains
all timings and full-precision summaries. Independent review recomputed all
four aggregates from the 360 recorded timing samples and matched the saved
summary. No speedup or unlocked/locked ratio is claimed because there is no
matched before/after firmware experiment.

## Numerical results

Every mode passed all numerical checks in all three processes. The following
table shows the largest observed errors across the three processes. Scaled
error at most 1 passes the fixed tolerances; the tolerances were not changed
for the native execution.

| Mode | Small cases: max scaled error | Large known answer: max absolute error | Timed output: max absolute error | Timed output: max scaled error |
|---|---:|---:|---:|---:|
| FP32 pedantic | 0.0225879 | 0 | 0.000112907 | 0.0178774 |
| FP16, FP32 accumulation | 0.0258477 | 0 | 0.000675572 | 0.0728850 |
| BF16, FP32 accumulation | 0.0108252 | 0 | 0.000500575 | 0.0468711 |
| TF32, FP32 accumulation | 0.0000903514 | 0 | 0.0183262 | 0.2518374 |

Per mode and process, all 3783 outputs across five small cases were checked,
as were all 16777216 outputs of the large signed-diagonal known-answer case.
The final timed random output was scanned for non-finite values across all
16777216 elements, with arithmetic reference comparisons at 1024 prescribed
stratified positions. The arithmetic check is sampled for the random output;
earlier timed outputs were not each independently reference-checked.
FP16/BF16 references use the converted inputs. TF32 references use the
original FP32 inputs.

## Resource and telemetry observations

| Observation | Result |
|---|---|
| GPU identity | GeForce RTX 3080; `10de:2216` / `1458:403f`; 10240 MiB; capability 8.6 |
| PCI location | `00000000:07:00.0` |
| NVIDIA driver | `610.88` |
| Runtime-reported versions | CUDA runtime `12060`; cuBLAS `120600`; CUDA driver API `13030` |
| Entry profile | `desktop`; limits 5% utilization, 3072 MiB allocated, 65 C |
| Prelaunch GPU utilization | 0% in all three snapshots |
| GPU memory used | 2138 MiB in every pre/post snapshot |
| Peak application-controlled GPU allocation | 201326592 bytes (192 MiB) in each process |
| Minimum available host RAM before a process | 6244286464 bytes (about 5.82 GiB) |
| Reported power limit | 370 W in all six snapshots |
| Reported maximum SM / memory clocks | 2115 / 9501 MHz in all six snapshots |

| Process | Temperature before | Temperature after | Operating SM / memory MHz before | Operating SM / memory MHz after |
|---|---:|---:|---|---|
| 1 | 35 C | 47 C | 210 / 405 | 1935 / 9501 |
| 2 | 46 C | 53 C | 1935 / 9501 | 1950 / 9251 |
| 3 | 49 C | 51 C | 1950 / 9501 | 1950 / 9501 |

The highest recorded pre/post temperature was 53 C. These are snapshots, so
53 C is not a measured peak during the workload. The 192 MiB figure covers
application-controlled allocations; CUDA context, driver and library allocations
are outside that budget. The recorded power limit is not measured power draw.
Maximum clocks and power-limit metadata do not establish a factory stock
configuration.

No driver, firmware, clock, power-limit or security setting was changed for
this run. Ordinary computation does not establish the current firmware state,
compatibility with an unlock profile, long-term stability, application speedup
or gaming performance.

## Verification and evidence

The Python suite passed 112 tests, the C++ numerical helper passed 15 groups
with the selected v142 compiler, and the initial 15 synthetic PowerShell checks
remain documented in [VALIDATION.md](VALIDATION.md). Native compilation,
metadata inspection, all three GPU processes and saved-result verification
also passed.

The original saved bundle and the byte-for-byte publication copy were each
validated offline. Their reader recomputes the timing summaries, checks the
numeric contract, entry profile and completion hashes, and rejects incomplete
or inconsistent records. The publication copy's build-manifest digest and all
eight recorded source-file hashes were independently matched to the measured
source. Publication review found no personal paths, hostnames, GPU UUIDs,
credentials or serial numbers in the saved JSON. See the
[evidence index](evidence/20261026-desktop/README.md) for the preserved files
and an offline verification command.

Hash and schema checks establish internal consistency and traceability. They
are not hardware attestation or independent proof of which instructions ran.
