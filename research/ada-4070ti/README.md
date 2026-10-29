# RTX 4070 Ti: portability research and stock-compute experiments

Date: 2026-10-25. Source baseline: `849f996` (current main when cloned).
Status: **inventory collected; unlock not attempted; Ada support not established**.
This is a research contribution, not a new supported hardware profile.
The initial inventory was read-only; later experiments include GPU workloads
and a separately authorized driver restart. Executable research tools are
included. Read [the tool scope and review boundaries](EXECUTION-TOOLS.md)
before considering any execution; offline evidence checks do not approve the
hardware tools or establish their safety on another machine.

**Follow-up:** [an existing external AD104 Windows reference](EXTERNAL-AD104.md)
uses the same GPU/subsystem IDs on a different platform and VBIOS. Its published
evidence was checked offline. The source constraints below describe this
Windows-GPU-UNLOCK baseline, not an absence of Ada work elsewhere.

The [host-only follow-up](HOST-TESTS.md) reproduces the diagnostic/parser mock
tests and adds synthetic failure/topology cases. It characterizes incomplete
graph/code observations without booting firmware or reading this card's ROM.

[Further offline execution](OFFLINE-FOLLOWUP.md) adds guard-backed deterministic
ROM mutations and records their bounded CPU-only results.

**Actual GPU execution:** [GPU-FINDINGS.md](GPU-FINDINGS.md) records stock CUDA
tests and read-only telemetry on this 4070 Ti after the owner's explicit GPU-use
authorization. These measurements do not establish unlock support.

[Live driver investigation](LIVE-DRIVER.md) records documented NVAPI queries and
the separately controlled restart/recovery investigation.

[Exact-binary tensor characterization](BINARY-TENSOR-RESULT.md) adds inspected
machine instructions, resource queries and a three-process batched control
with a measured 1.957–1.964x accumulation-rate ratio on the stock driver.

## What this machine contributes

[inventory.json](inventory.json) is the existing Windows inventory collector's
sanitized output. Collection completed with exit code 0, no missing sections,
`assessment.status = not-assessed` and `can_unlock = false`.

| Item | Observed value |
|---|---|
| GPU | NVIDIA GeForce RTX 4070 Ti |
| PCI vendor:device | `10de:2782` |
| Subsystem vendor:device | `1462:5132` |
| Board / BIOS | ASUS ROG STRIX B550-F GAMING / 3205 |
| CPU | AMD Ryzen 5 5600X |
| Windows | Windows 11 Pro, 10.0.26200, build 26200, 64-bit |
| CIM driver version | `32.0.16.1088` |
| NVIDIA driver version | `610.88` |
| Reported VBIOS version | `95.04.31.00.f0` |
| Reported memory total | 12282 MiB (driver-visible; not a pre-boot layout measurement) |
| Reported CUDA compute capability | 8.9 |

The last four rows came from this separate metadata-only query:

```powershell
nvidia-smi --query-gpu=name,pci.device_id,pci.sub_device_id,driver_version,vbios_version,memory.total,compute_cap --format=csv
```

It returned device `0x278210DE` and subsystem `0x51321462`. Those packed
representations agree with the collector's vendor/device fields. NVIDIA and
CIM driver versions are retained separately rather than compared numerically.
Querying the VBIOS version string did not dump the VBIOS/PROM image.
Installed Windows driver `610.88` is not evidence that the transaction's
`610.43.03` firmware assets are compatible with this card.

Only one display adapter was returned by CIM. This does not establish physical
monitor routing or the complete pre-boot GOP graph. AMD lists the 5600X as
requiring discrete graphics [3]; a motherboard display connector is therefore
not an independent iGPU recovery route on this CPU. Exact connected monitors,
firmware console topology and availability of another display GPU remain unknown.

## Findings from the current source

The internal tree now includes 3090 FE, 3070 Ti and 3090 Ti machine profiles.
The older statement in `docs/PORTABILITY.en.md` that the repository only contains
the GA102/3090 transaction should not be used as the current compatibility matrix.
Use [COMPATIBILITY.en.md](../../docs/COMPATIBILITY.en.md) and
[machine-profile.h](../../internal-boot/unlock/machine-profile.h) together.

| Area | Source evidence at the baseline | Consequence for this machine |
|---|---|---|
| Legacy identity | [root port.h](../../unlock/port.h) fixes `GU_BOARD_ID` to `0x220410de` and the original subsystem | The 4070 Ti is outside this target; do not replace IDs to force acceptance |
| Internal profiles | [machine-profile.h](../../internal-boot/unlock/machine-profile.h) selects B650M/3090 FE, B660M/3070 Ti or X870/3090 Ti | No B550/4070 Ti profile; existing successful machines do not validate this combination |
| Firmware/signature | Both [legacy](../../unlock/vendor/unlock_610.c) and [internal](../../internal-boot/unlock/vendor/unlock_610.c) cores name GA102 booter/V67 assets, GA10x GSP and `.fwsignature_ga10x` | No demonstrated Ada firmware/signature path; similar CUDA support does not establish firmware compatibility |
| Memory/WPR | The cores' layout preparation includes GA102-specific heap/scrub assumptions and bounded protected-memory reservations | Driver-visible VRAM capacity cannot supply Ada layout constants; independent layout evidence is missing |
| State and reset | `ColdCheck`/`PromCheck` in the application preflight code (`unlock/app.c` and `internal-boot/unlock/app.c`) and the transaction's reset/rejoin paths impose additional invariants | A PCI model match or edited PROM digest cannot establish safe selector/reset/cleanup behavior |
| Display ownership | Internal profiles pin display device paths and console/GOP driver fingerprints; headless release is separately constrained | The no-iGPU B660M precedent is relevant, but its AMI aggregate-console graph cannot be assumed on B550 BIOS 3205 |
| Benchmark selection | [build.ps1](../../benchmark/build.ps1) accepts `3090`, `3090ti`, `3070ti`; the matching CUDA sources select a fixed model at `0000:01:00.0` | Existing benchmark executables are not a 4070 Ti baseline tool; no benchmark was built or run here |

These are source constraints, not a hardware observation that Ada is permanently
incompatible or that this machine needs an unlock. External evidence now exists
for an AD104 Windows reference; applicability to this particular VBIOS/platform
and any performance benefit on this machine remain **open questions**.

## CUDA benchmark portability is a separate question

NVIDIA lists the 4070 Ti at compute capability 8.9 and the listed RTX 30-series
cards at 8.6 [1]. The current build uses `-arch=sm_86`. NVIDIA explicitly permits
8.6 cubins on 8.9 devices [2], so that flag is not by itself an Ada execution
blocker. Native `sm_89` could be a future comparison; it is not required merely
to argue that the existing kernel target can run on Ada. This says nothing about
UEFI firmware compatibility or complete application/runtime correctness.

A future ordinary Windows baseline contribution could extend explicit model/device
selection without broadening EFI support. Preserve the current defaults and
reject absent or ambiguous targets; record the selected model, PCI location,
compute capability, driver, CUDA/cuBLAS versions and source/build hashes.
Use the existing 4096-square sizes, precision/accumulation policies, warmup and
sample counts for comparability. Document the existing first/last-element result
check limitation. Record clocks, power/thermal conditions and background load.
Do not interpret an ordinary 4070 Ti result versus someone else's 3090 result
as an unlock percentage. A causal uplift claim requires controlled same-card
before/after evidence, which is absent here.

The production benchmark and EFI implementation are unchanged by this
contribution. Separate research workloads and saved results are included;
see the follow-up reports above for their actual scope and limitations.

## Next research that fits the current boundary

1. Keep this inventory as an explicitly untested compatibility report. The
   existing [contribution instructions](../../docs/CONTRIBUTING.en.md) accept
   such reports without running an EFI. Nothing has been submitted remotely.
2. Compare public NVIDIA Ada/Ampere implementation and firmware documentation
   offline, with pinned source revisions. Establish whether an Ada mechanism
   exists before designing a profile. Do not infer it from CUDA capability.
3. Review a future benchmark-only change separately. A stock Windows baseline
   can be useful even if no Ada unlock path exists. The separate stock GPU
   experiments above are not a causal unlock comparison or support evidence.
4. For a future diagnostic tool, require an independently reviewed non-arming
   path that does not release GOP/driver ownership, write PCI/MMIO, enable bus
   mastering, allocate device DMA or reset hardware. Do not assume a script or
   EFI is safe merely because its name includes "probe" or "preflight".

The initial inventory/portability investigation changed no boot configuration,
ESP, partition, BIOS setting, Secure Boot/BitLocker state, driver installation,
firmware image, power/clock limit or GPU selector. It built, staged and booted
no EFI and ran no reset, PROM dump or unlock transaction. These initial-scope
statements do not describe the later stock compute and driver-restart
experiments: their effects, results and unresolved limits are documented in
GPU-FINDINGS.md, LIVE-DRIVER.md and BINARY-TENSOR-RESULT.md. None establishes
an Ada unlock profile, general hardware safety or long-term stability.

## Sources and reproducibility

- Repository observations refer to `849f996`; relative links above are source
  navigation, not independent hardware validation.
- The collector was inspected before execution. It uses selected CIM properties
  and removes device-instance suffixes; the saved report contains no serial,
  UUID, hostname or username. No raw `nvidia-smi` system report is included.
- [1] [NVIDIA CUDA compute-capability table](https://developer.nvidia.com/cuda/gpus),
  accessed 2026-10-25.
- [2] [NVIDIA Ada compatibility guide, sections 1.2 and 1.3](https://docs.nvidia.com/cuda/ada-compatibility-guide/),
  accessed 2026-10-25. CUDA binary compatibility is distinct from device firmware.
- [3] [AMD Ryzen 5 5600X specifications, Graphics Capabilities](https://www.amd.com/en/products/processors/desktops/ryzen/5000-series/amd-ryzen-5-5600x.html),
  accessed 2026-10-25.
