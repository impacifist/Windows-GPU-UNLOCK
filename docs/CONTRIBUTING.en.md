# Help investigate compatibility

**English** | [한국어](CONTRIBUTING.ko.md)

Maintainers: use the [compatibility triage guide](COMPATIBILITY-TRIAGE.en.md) to track missing evidence, related reports and actual Windows outcomes separately from support qualification.

Reports from other motherboard, CPU and GPU configurations help prioritize ports. **Submitting a report does not require booting the EFI, changing BIOS settings or running the unlock.** EFI support is restricted to the exact [published machine profiles](COMPATIBILITY.en.md); a matching GPU model alone is insufficient.

## Collect an inventory on Windows

With Python 3.9+ installed, run from the repository root in a normal Windows terminal:

```powershell
python .\tools\collect_compatibility.py
```

The tool uses Windows PowerShell and local CIM queries to print motherboard/CPU model, Windows version, BIOS version, display-adapter models, PCI model IDs and driver versions. It does not request elevation, download anything, change settings, access GPU MMIO, read PROM/firmware or run the unlock. It does not create files or upload data.

To save a local report, choose a new filename and redirect stdout yourself:

```powershell
python .\tools\collect_compatibility.py > .\compatibility-report.json
```

Shell redirection can overwrite an existing file. Reports matching `compatibility-report*.json` are ignored by Git. **Review the contents before sharing.** The collector omits hostname, username, serial-number/UUID fields and PnP device-instance suffixes, but model/version strings still describe your configuration and should be inspected.

Exit code `0` means the selected Windows inventory sections were collected, **not** that unlock is supported. Exit `2` means some sections are unavailable; the partial report identifies them. Exit `1` means collection failed. There is no need to elevate or disable security features to submit a report; fill in missing information manually if possible.

`assessment.status` is always `not-assessed`, and `can_unlock` is always `false`, including on a machine whose visible model names match the successful case. Secure Boot state, the physical monitor route, PCI BDF/BARs, PROM, cold state and pre-boot cleanup readiness are not collected. A virtual/non-PCI display adapter has `pci: null` and an explicit `pci_status`; that is not a support verdict. Missing or malformed PCI IDs make collection partial. WMI driver-version strings may differ from NVIDIA's marketing version; do not compare them as if they used the same numbering.

## Share a report manually

Already have probe captures? The [offline checker](CHECK-COMPATIBILITY.en.md) produces
a reviewed-shareable JSON report with refusal reasons and limited model/topology
observations. Paste it into the form's optional offline report field. Its comparison
tool helps identify differences between two checker reports. Neither tool runs an
unlock; no new capture or experiment is required to submit an issue.

Open a [compatibility report issue](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/new?template=compatibility.yml), or a plain issue if the form is unavailable. Include:

- The reviewed inventory, or the motherboard, CPU, GPU and Windows/driver models manually.
- Physical card manufacturer/model and any VBIOS cross-flash history, separately from reported PCI/subsystem IDs and the VBIOS version if already known. Software identity does not establish the physical PCB. Unknown is valid; do not flash or dump firmware for a report.
- Where the monitor is physically connected; whether an iGPU or a separate display GPU is available. An inventory cannot prove this route.
- Whether you have **not attempted unlock**, only collected diagnostics, or have existing test evidence. Untested reports are useful.
- For existing authorized experiments only: exact EFI SHA-256/profile, sanitized final-stage logs, boot handoff and matching benchmark conditions. Do not run a new experiment merely to complete the form.
- Tie each existing capture to its source commit, local diff, compiler version and display/BIOS settings. Separate diagnostic collection, transaction/cleanup, Windows recovery and performance evidence. For existing measurements include precision and accumulation, matrix size, library/driver versions and run counts; distinguish cold boot, restart, sleep and hibernation.

Do not attach firmware assets, VBIOS/PROM dumps, BitLocker recovery keys, serials, device UUIDs or raw system reports. Check existing logs for personal paths before sharing. Nothing is uploaded automatically.

## What counts as support

Use the [validation record](VALIDATION.en.md) to separate existing observation,
transaction, Windows recovery, compute and lifecycle evidence. No new run is required.

Inventory reports are investigation inputs. Host mock tests validate software behavior. A supported write profile additionally requires reviewed GPU identity/firmware/layout constraints, pre-boot ownership and failure cleanup, a measured Windows handoff and reproducible hardware evidence. See the [portability design](PORTABILITY.en.md).

Collector reference APIs: Microsoft's [Get-CimInstance](https://learn.microsoft.com/en-us/powershell/module/cimcmdlets/get-ciminstance), [Win32_VideoController](https://learn.microsoft.com/en-us/windows/win32/cimwin32prov/win32-videocontroller) and [PCI identifiers](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/identifiers-for-pci-devices). Windows inventory alone cannot establish pre-boot compatibility.
