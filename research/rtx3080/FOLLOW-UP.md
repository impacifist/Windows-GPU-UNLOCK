# Follow-up research: hardware, evidence and build readiness

**Historical research snapshot:** the compiler/build-readiness discussion below
predates the completed ordinary CUDA contribution in
[PR #45](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/45).
See its [native validation report](https://github.com/danhol86/Windows-GPU-UNLOCK/blob/a04471a8c2cec047c25383315d122f1d6513462c/benchmark/rtx3080/NATIVE-VALIDATION.md)
for the later compiler installation, build and successful GPU measurement.
The subsequent [Windows display query](display-topology/README.md) and
[support review](SUPPORT-REVIEW.md) supplement the observations below.

Research date: **2026-10-26 (UTC)**. Inspected upstream revision remains
[`5c98f11eaafdcb7a2a06fb14fcdcde7297eb409e`](https://github.com/impacifist/Windows-GPU-UNLOCK/tree/5c98f11eaafdcb7a2a06fb14fcdcde7297eb409e).
This supplements the original [inventory](inventory.json) and
[collection record](observations.json); it does not replace their historical
observations. No ordinary GPU workload or experimental firmware was executed.

## Hardware conclusions from primary sources

| Question | Finding | Evidence and limit |
|---|---|---|
| Does the installed CPU provide integrated graphics? | **No.** | [AMD's Ryzen 7 3700X specification](https://www.amd.com/en/support/downloads/drivers.html/processors/ryzen/ryzen-3000-series/amd-ryzen-7-3700x.html) requires a discrete graphics card. This is a specification finding for the inventoried CPU, not a new adapter probe. |
| Are the motherboard display sockets a fallback with this CPU? | **No integrated display output is available with the 3700X.** | [ASUS's X570-F specification](https://rog.asus.com/motherboards/rog-strix/rog-strix-x570-f-gaming-model/spec/) ties its HDMI/DisplayPort outputs to processors with integrated graphics. Another installed or spare display adapter and actual cable routing remain unknown. |
| Is device `2216` a desktop RTX 3080? | NVIDIA lists both `2206` and `2216` as GeForce RTX 3080. | [NVIDIA's published device table](https://download.nvidia.com/XFree86/Linux-x86_64/495.44/README/supportedchips.html) distinguishes laptop models. This archived Linux-driver table is used only for model identification. |
| Which memory variant was observed? | **10240 MiB / 10 GiB**, from NVIDIA metadata. | [NVIDIA's family specifications](https://www.nvidia.com/en-gb/geforce/graphics-cards/30-series/rtx-3080-3080ti/) include both 10 GB and 12 GB RTX 3080 models. The model name alone cannot select the variant. |
| Is the model CUDA-capable? | NVIDIA lists RTX 3080 at compute capability **8.6**. | [NVIDIA's compute-capability table](https://developer.nvidia.com/cuda/gpus). This establishes a model capability, not a successful local workload or project-specific support. |

Microsoft defines [`Win32_VideoController.AdapterRAM`](https://learn.microsoft.com/en-us/windows/win32/cimwin32prov/win32-videocontroller)
as an unsigned 32-bit byte count. Its numeric range cannot represent 10 GiB.
The published VRAM finding therefore uses NVIDIA metadata. No particular WMI
wraparound or erroneous value was observed or inferred in this collection.

[Microsoft's PCI identifier documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/identifiers-for-pci-devices)
explains the device/subsystem fields. Those reported values and the VBIOS
version string do not establish the physical retail model, PCB revision or
cross-flash history. No authoritative exact retail-SKU mapping was established.

A software display flag is also insufficient to establish physical cabling:
[NVIDIA documents](https://docs.nvidia.com/deploy/nvidia-smi/index.html)
that a display can be active with no attached monitor. Physical monitor ports
and availability of another display GPU still need owner confirmation.

## Related reports: keep each machine separate

The comparison in [README.md](README.md#comparison-with-current-documentation)
remains applicable. Further review found these interpretation limits:

- [Issue #13](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/13):
  the original X570-P / Ryzen 5900X submission is inventory evidence. A later
  success comment about a different PNY system must not be attributed to the
  original machine, even though the thread contains both.
- [Issue #20](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/20):
  the FE card and Intel platform differ from this inventory; its diagnostic
  and measurement material does not establish support here.
- [Issue #28](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/28):
  a contributor reports one local boot and one before/after pair on a different
  Maxsun/Intel system. This is not independent replication, repeated reliability,
  sleep/resume validation or an application-performance result for this PC.

Neither an issue's existence nor acceptance of research documentation creates
a supported hardware profile. The inspected support table still has no exact
profile for this RTX 3080 / X570-F / BIOS 5043 / Ryzen 3700X configuration.

## Ordinary benchmark build readiness

Read-only checks at **2026-10-26T13:11:16Z** found:

| Item | Local observation |
|---|---|
| C++ build tools | Visual Studio Build Tools 2022 `17.14.37806.7` |
| Enumerated MSVC toolset | `14.44.35207` |
| Default compiler path expected by `benchmark/build.ps1` | VS 2019 Build Tools path absent |
| Project-local CUDA packages | `benchmark/packages` absent |
| Project-local benchmark binaries | `benchmark/bin` absent |
| RAM snapshot | 31.91 GiB total; 8.50 GiB available |
| Workspace-drive free space snapshot | 235.19 GiB |

The packages/binary observations concern this checkout, not all software
installed elsewhere. Resource availability can change before any later run.

The [build script](../../benchmark/build.ps1) selects NVCC `12.6.20` and
cuBLAS `12.6.0.22`, matching the
[CUDA 12.6 GA component table](https://docs.nvidia.com/cuda/archive/12.6.0/cuda-toolkit-release-notes/index.html).
NVIDIA's [CUDA 12.6 Windows compiler table](https://docs.nvidia.com/cuda/archive/12.6.0/cuda-installation-guide-microsoft-windows/index.html)
lists MSVC `193x` for VS 2022. Microsoft's
[version mapping](https://learn.microsoft.com/en-us/cpp/overview/compiler-versions?view=msvc-170)
maps VS 17.14 / MSVC 14.44 to compiler family `1944`.

**A documented compatible host-compiler configuration has not been established.**
The installed compiler is newer than the family listed for this toolkit.
No CUDA compilation was attempted: this is a support-table gap, not an observed
compiler rejection. Selecting a different compiler path alone would not settle it.

The reported driver `610.88` exceeds the release notes' Windows thresholds
`528.33` for CUDA 12.x minor compatibility and `560.76` for CUDA 12.6 GA.
That version comparison does not test runtime operation, the current Windows
build, or the benchmark. No driver, compiler or toolkit was installed or changed.

The selected GPU telemetry snapshot reported 5% utilization, 1994 MiB memory
used and 32 C. It is one observation, not proof of sustained idle. Reported power
limit and maximum clocks are preserved in [research-validation.json](research-validation.json)
with units. Maximum clocks are not measured operating clocks, and unchanged
settings do not prove factory stock settings.

## Offline validation actually performed

| Check | Result | Meaning |
|---|---|---|
| Original collector suite | 9 tests passed, exit 0 | Previously recorded in `observations.json`; synthetic collector checks |
| Benchmark comparison suite | **28 tests passed, exit 0** | Synthetic JSON, hashes, identity/protocol consistency and incomplete-result rejection |
| PowerShell runner integration suite | **Incomplete, exit 1; zero scenarios completed** | Windows denied access when copying the generated managed stub executable, before the first runner invocation |

The comparison command was:

```powershell
py -3.11 -m unittest discover -s benchmark -p test_compare_results.py -v
```

The runner suite was attempted in Windows PowerShell 5.1 with a process-local
`python` function forwarding to `py -3.11`; its exact wrapper and source hashes
are recorded in `research-validation.json`. It is designed to use a generated
managed executable and mocked NVIDIA queries. The local attempt failed at
`test-runner.ps1:82`, copying `stub.exe`. The cause of the access denial is
unconfirmed; no antivirus exception, permission change or security-setting
change was attempted. The suite's 60 planned scenarios must not be reported
as passing.

These are host-software checks, not CUDA numerical results, GPU stability tests
or firmware compatibility evidence. The proposed ordinary computation work is
described separately in [BENCHMARK-PLAN.md](BENCHMARK-PLAN.md).
