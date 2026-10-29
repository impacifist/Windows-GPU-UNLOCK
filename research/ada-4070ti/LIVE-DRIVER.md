# Live Windows driver investigation

Date: 2026-10-25. Desktop/application disruption was explicitly authorized;
the execution boundary remains no planned PC reboot, firmware upload or direct selector write.
No remote push is authorized. Earlier ordinary-compute evidence is in
[CUBLAS-BASELINE.md](CUBLAS-BASELINE.md).

## Documented NVAPI queries on the actual card

[gpu_nvapi_probe.py](gpu_nvapi_probe.py) uses only documented read interfaces
from the installed `System32/nvapi64.dll`. Public interface IDs, structures and
version fields were checked against NVIDIA's SDK at commit
`70d337db9186e968eab622f7e786de7e437faf3d`.
[nvapi-source-provenance.json](nvapi-source-provenance.json) pins those headers;
[gpu-nvapi-result.json](gpu-nvapi-result.json) records returned values and hashes.
No unique board serial, UUID, raw firmware or device-instance suffix is published.

| Query | Actual returned value |
|---|---|
| Architecture / implementation / revision | `0x190` / `0x4` / `0xa1` |
| Interpretation of architecture/implementation | AD100 family / AD104 |
| GPU core count | 7680 |
| NVAPI shader-subpipe count, raw | **30** |
| CUDA multiprocessor count, separate query | **60** |
| Bus / raw slot | 11 / 0 |
| GSP feature query | Status `-104` (not supported); no version returned |

The revision is retained raw. Although the public header lists named revision
enums, the returned `0xa1` must not be silently translated to a different enum.
Similarly, the shader-subpipe counter is not interchangeable with CUDA SM count
on this observation. Treating 30 as the SM count would mischaracterize the card.
GSP `-104` does not distinguish unsupported reporting from GSP not running.

The public SDK header search did not expose the SM issue-rate getter used by the
external Linux experiment. Its `read_sm_rates.py` uses `/dev/nvidiactl`, Linux
ioctl encoding and RM object allocation; that ABI cannot simply be submitted to
a Windows device. No guessed private Windows interface or raw register mapping
was invoked. Read-only driver metadata does not establish effective selectors.

Reproduce:

```powershell
python research/ada-4070ti/gpu_nvapi_probe.py
```

## Driver restart path under investigation

The installed `nvidia-smi --help` explicitly describes `--gpu-reset` on Windows
as an administrator-only display-driver restart, with `-i` selecting one GPU.
This is distinct from the reference's pre-driver UEFI transaction. The running
adapter is healthy before testing, but a restart is not a guarantee that every
possible driver/hardware fault can be recovered without a PC reboot.

Before execution, the plan is to record exact private PnP identity and enabled
state, start an independent elevated recovery worker, issue one targeted vendor
restart, and verify PnP/kernel-driver/NVML health followed by fresh CUDA output
checks. Recovery must not repeatedly reset devices, remove drivers or reboot.
Only an originally enabled adapter found explicitly disabled may be re-enabled.
An enabled-but-hung GPU may remain unrecoverable by this fallback; uncertain
outcomes must be recorded as such rather than called recovery success.

Sources: [NVIDIA NVAPI GPU interfaces](https://docs.nvidia.com/nvapi/group__gpu.html),
[NVIDIA SMI restart documentation](https://docs.nvidia.com/deploy/nvidia-smi/index.html).
The completed local test is recorded below.

### Prepared recovery controls

[gpu_driver_restart.ps1](gpu_driver_restart.ps1) defaults to a read-only
preflight. It passed the exact PnP/PCI/initial-health checks on this machine.
`-Execute` requires an elevated process and permits one targeted vendor restart.
A create-new lock prevents concurrent tests and remains after an uncertain
outcome for manual inspection. Exact PnP instance identifiers remain private
under ignored `build/`; published evidence omits their unique suffixes.

[gpu_restart_recovery.ps1](gpu_restart_recovery.ps1) runs independently from the
desktop/controller and confirms elevation before the request. It exits if the
controller cancels or finishes. After 45 seconds it may enable the exact
originally enabled device only if the device is explicitly disabled (problem
code 22) and the original restart process is no longer present. It does not
overlap an enable with a still-running restart. There are no automatic reset
retries, driver removal/installation or reboot actions.

JSON state/readiness publication is atomic. Command completion, observed device
health and subsequent CUDA correctness are distinct outcomes. A timed-out
request is uncertain until the original process exits, even if queries look
healthy. Poll attempts are limited, but individual Windows PnP/CIM calls do not
have a hard timeout; this is not a guaranteed recovery mechanism for a hung
kernel/hardware state.

## Completed targeted restart and compute follow-up

[gpu-driver-restart-result.json](gpu-driver-restart-result.json) records one
targeted request on this card. The vendor tool printed `Driver restarted
successfully` and `All done.` The independent worker was ready beforehand;
its later check found problem code 0 and performed no enable action. Windows
boot-session comparison was unchanged. No firmware or selector write was made.

The first controller attempt aborted before issuing a restart because Windows
PowerShell 5.1 converted a null backup-path argument for `File.Replace` into an
invalid path. Commit `eae6ced` corrected this with `NullString.Value`; isolated
file replacement and read-only GPU preflight passed before the actual request.

The actual run exposed a separate process-reporting limitation: its unretained
process handles produced null exit codes. Consequently the original controller
recorded `HealthRecovered=false` even though PnP was OK and the kernel driver
was running. That result and the missing restart exit code remain in the
published evidence. Commit `f306a93` retains handles before waiting; Windows
PowerShell tests verified exit codes 0 and 7 and a real read-only NVIDIA identity
query. The GPU restart was not repeated to test this reporting correction.

Separate follow-up checks recorded two healthy PnP/kernel/NVML snapshots, both
with NVIDIA query exit code 0. The original request process had exited before
the retained lock was manually cleared. Fresh ordinary processes then produced:

| Check | Result |
|---|---|
| CUDA smoke | All 655,360 integer outputs exact; cleanup completed |
| NVAPI/CUDA metadata | 7680 cores, 30 raw subpipes, 60 CUDA SMs; unchanged architecture and unsupported GSP query |
| FP32 pedantic GEMM | 7 complete outputs verified; 30.395 nominal TF/s |
| FP16 input / FP32 accumulation GEMM | 7 complete outputs verified; 76.294 nominal TF/s |
| BF16 input / FP32 accumulation GEMM | 7 complete outputs verified; 76.460 nominal TF/s |

The GEMM checks cover 352,321,536 FP32 values using the same uniform fixtures as
the stock baseline. Rates remain workload measurements, not evidence of changed
hardware limits. Full results:
[CUDA](gpu-smoke-after-driver-restart.json),
[NVAPI](gpu-nvapi-after-driver-restart.json), and
[cuBLAS](gpu-cublas-after-driver-restart.json).
Run `python research/ada-4070ti/check_restart_results.py` to verify the saved
output hashes and source hashes offline.

This establishes a vendor-reported restart followed by usable compute on this
host without a PC reboot. It does not prove a particular driver unload sequence,
preservation of an active application's CUDA context, recovery from an injected
GPU fault, selector persistence, or an unlock. No such fault was injected.
