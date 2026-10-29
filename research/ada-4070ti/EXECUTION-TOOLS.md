# Tool scope and review boundaries

The saved reports describe the contributor's experiments. Source hashes and
offline arithmetic checks establish consistency with those reports, not
independent authentication of the experiment or permission to run a tool.
This directory contains several different kinds of executable code:

| Kind | Files | Effects and review boundary |
|---|---|---|
| Offline evidence/observation | Result checkers other than `check_host_analyzer.py`, `capture_observations.py`, `test_*.py`, `run_observation_cases.py` | Read local inputs, run CPU checks and possibly write local summaries; inspect imports and the exact entry point first. `check_public_evidence.py` reads a pinned external checkout as data. |
| External Python analyzer tests | `check_host_analyzer.py` | Imports and executes a pinned external Python module against synthetic JSON; inspect that module before execution. This is different from treating a checkout only as data. |
| Host C research | `run_host_tests.py`, `host_*_cases.c` | Compile and execute external C sources with synthetic services; review the exact pinned inputs and compiler before running. A console PE header does not establish benign code. |
| Package preparation | `prepare_binary_tools.py`, `prepare_cublas.py` | Network downloads, hash checks and local extraction; the latter also updates a tracked provenance JSON. These are not read-only operations. |
| Binary preparation | `gpu_binary_probe.py prepare` | Executes ptxas/nvdisasm and writes generated files, disassembly and a tracked manifest. |
| Stock GPU computation | `gpu_smoke.py`, `gpu_tensor_probe.py`, `gpu_accumulation_probe.py`, `gpu_precision_probe.py`, `gpu_cublas_baseline.py`, `gpu_binary_probe.py run`, `gpu_binary_sweep.py` | Load CUDA/user libraries, create GPU contexts and run workloads. Do not run during an unrelated GPU job. These are not offline evidence checks. |
| NVAPI observation | `gpu_nvapi_probe.py` | Loads vendor libraries and queries a live GPU; separate from the offline result checkers. |
| Driver control | `gpu_driver_restart.ps1`, `gpu_restart_recovery.ps1` | The controller's Execute mode restarts a driver; its recovery worker can enable a disabled device. Administrative hardware operations require separate review. Do not directly invoke the worker on untrusted state files. |

Known boundaries requiring review before reuse:

- Some telemetry invokes PATH-resolved nvidia-smi. A supplied manifest does not
  bind that executable or authenticate a local PATH entry.
- The recovery worker uses caller-provided state and controller marker files.
  Those files are not an independent authorization mechanism.
- Included package/tool hashes need comparison with authoritative provenance;
  a matching hash in a contributor's JSON alone is not an authenticity guarantee.
- Historical GPU results concern one machine and particular numerical fixtures.
  They do not authorize selector changes, firmware boot, driver control or an
  unlock on a matching model.

Research evidence and operational tools may be reviewed and accepted separately.
Correcting this documentation does not resolve the executable tools' remaining
reuse and maintenance questions.
