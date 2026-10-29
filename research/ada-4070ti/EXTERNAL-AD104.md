# Existing AD104 Windows reference: offline follow-up

Investigated 2026-10-25. This materially advances the initial inventory report:
there is already a public Windows reference for the same GPU/subsystem IDs.
It is external to Windows-GPU-UNLOCK and does not validate this B550 machine.
No hardware transaction, pre-boot diagnostic or CUDA workload was run here.

## Pinned source, not a model-name assumption

Reviewed `JustAResearcher/gpu-unlock-ada` commit
`4e7fefcc4923f9e083d5324dc75a7d2f906af77f` (abbreviated links below use `4e7fefcc`).
Its [Windows reference source](https://github.com/JustAResearcher/gpu-unlock-ada/tree/4e7fefcc/windows/ad104-msi)
is separate from Linux module admission. Do not confuse the non-SUPER 4070 Ti
with the AD103 4070 Ti SUPER experiment. The fork's Windows report concerns
AD104 `10de:2782`, subsystem `1462:5132`, matching this inventory.

| Field | Published Windows reference | This machine |
|---|---|---|
| GPU / subsystem | `10de:2782` / `1462:5132` | Match from two Windows metadata queries |
| Board / CPU | MSI Z490 MS-7C75 / i9-10850K | ASUS B550-F / Ryzen 5600X |
| BIOS | A.E0 | 3205 |
| VBIOS version | `95.04.3c.40.fd` | `95.04.31.00.f0` |
| NVIDIA Windows driver | 616.56 | 610.88 |
| Reported memory | Reference reports 12,282 MiB preboot | 12,282 MiB runtime-visible only |
| PROM, cold registers, GOP graph | Captured for the reference | Not collected here |
| Hardware result | One published cold/manual launch and short GEMM tests | No unlock attempted |

The version mismatch warrants separate firmware research. It is not an observed
PROM-hash rejection: our PROM hash is unknown. Similarly, runtime-visible memory
matching the reference number does not validate its cold/preboot memory geometry.

## What can transfer, and what cannot be copied blindly

The following locations are in that pinned external tree:

| Source | Useful reusable design | Evidence still needed for this host |
|---|---|---|
| `windows/ad104-msi/profile.c`, `port.h`, `checks.c` | Separate immutable device, firmware and cold-state admission | Exact ROM/FWSEC identity, config/capabilities, BAR and cold-state snapshots; neither CIM nor NVML supplies these |
| `windows/ad104-msi/vendor/arch_610.h`, `vendor/unlock_610.c` | Explicit Ada transaction path rather than relabelled Ampere assets | Verify applicable firmware/layout assumptions for our actual ROM; do not treat shared payload naming as universal support |
| `windows/ad104-msi/msi_release.c` | Callback-based ownership release with bounded topology checks | Our B550 GOP, text/graphics console and USB ownership graph/code fingerprints differ or remain unknown |
| `windows/ad104-msi/boot_io.c`, `boot_profile.h` | Pinned read-only Windows boot-image loading and explicit handoff | Local ESP geometry/GUID and boot image identity; the reference's partition values are not ours |
| `windows/ad104-msi/diagnostic/` | Separate capture path and host tests | Independent review of access bounds/side effects and our topology before any future preboot execution |
| `windows/ad104-msi/benchmark.py`, `compare.py` | Explicit FP32 accumulation, matched libraries, recorded output hashes | Ordinary stock measurements on this host would be a separate workload experiment; no local baseline exists yet |

Concrete constraints: `profile.c` pins a 1 MiB PROM hash and exact FWSEC image/
signature geometry. `Config` and `Registers` constrain PCI resources and cold
values. `msi_release.c` embeds device paths and loaded-driver code fingerprints.
`boot_io.c` requires a private ESP GUID plus exact partition geometry and a
Windows boot-manager length/hash. Replacing PCI IDs or a ROM digest would leave
these independent requirements unresolved.

The existing diagnostic is described by its author as reading bounded PCI/PROM/
register data and loaded code, and writing a capture to its USB. That is more
invasive than ordinary Windows metadata queries: MMIO reads can have device-
specific semantics, and a firmware application runs in a different environment.
Its read-only label alone does not establish that booting it on this host is safe.
We inspected source but did not build, stage or boot it. Raw captures may include
proprietary code and local identifiers and should stay private.

## Reproduced arithmetic and evidence consistency

An independent standard-library checker, [check_public_evidence.py](check_public_evidence.py),
reads the pinned fork as data. It imports no third-party module, executes no
downloaded script, and makes no GPU/driver/network call. It checks:

- All 16 reviewed source inputs against our pinned provenance, including the
  evidence manifest and benchmark source. All 15 evidence files against that
  manifest, plus the benchmark source hash against the six measurement records.
- Three stock and three post-transaction runs with matching software, GPU,
  driver/VBIOS/power metadata, modes, output hashes and recorded numerical flags.
- Exactly the three expected modes in all six runs (18 mode records): sample
  counts, finite positive times, minimum/maximum,
  per-run median time and `2*n^3 / (median_ms*1e9)` TFLOP/s. Times are already
  divided by batch upstream and must not be divided a second time.
- Medians across processes and percentage changes against `validation.json`.

| Mode | Stock TFLOP/s | After TFLOP/s | Recomputed change |
|---|---:|---:|---:|
| FP32 pedantic | 30.226964 | 30.527229 | +0.9934% |
| FP16 / FP32 accumulation | 84.077557 | 151.429601 | +80.1070% |
| BF16 / FP32 accumulation | 84.026565 | 148.247498 | +76.4293% |

[public-evidence-audit.json](public-evidence-audit.json) records the local result;
[external-provenance.json](external-provenance.json) records the external revision
and SHA-256 of the inspected source inputs. No third-party firmware is copied.

Reproduce against a separately obtained checkout of the exact commit:

```powershell
python research/ada-4070ti/check_public_evidence.py ../gpu-unlock-ada-research
python -m unittest discover -s research/ada-4070ti -p test_public_evidence.py
```

The checker requires Python 3.9+ (`Path.is_relative_to`). Checkout line endings
must preserve the published bytes; the fork contains appropriate attributes.
Nine tests cover known arithmetic and rejection of changed driver/output hash,
incorrect throughput, nonfinite/missing samples, failed recorded validation,
replaced source and source paths outside the selected checkout.

**Limits:** matching an author's manifest establishes internal consistency, not
source authenticity, independently measured performance or proof of unlock.
Original output matrices are not included; we compare their recorded hashes and
validation flags, not recompute matrix outputs. The source hash is checked but
the publisher's historical running process is not independently authenticated.
The published measurements are one machine, three processes per phase, with
different temperatures/background use and sampled clocks. Cold A/B/A repetition,
long-duration stability and application gains remain unestablished. No reported
percentage applies to this B550 machine.

## Highest-value next work within research scope

1. Use this existing reference as the starting point for an Ada-to-Windows
   design comparison; starting another Ampere-to-Ada port from scratch would
   duplicate already public work. Keep attribution and license provenance.
2. Review the external diagnostic's selected reads and host test coverage offline,
   including unknown board/GOP graphs, malformed PE data and partial captures.
   Compilation and mock execution can be done on local files without touching
   the GPU. A successful mock is not approval to boot the resulting EFI.
3. Inspect a legally obtained offline ROM image for this exact VBIOS if one
   becomes available with provenance. Existing parser code can be studied using
   synthetic fixtures now; no live-ROM dump tool is needed merely for analysis.
4. If pursuing actual hardware characterization later, first define the smallest
   independently reviewed capture and recovery procedure. Firmware boot, display
   release and register writes are outside the present research execution.


Primary reference: [Latence.app Windows report at the pinned revision](https://github.com/JustAResearcher/gpu-unlock-ada/blob/4e7fefcc/docs/ad104-windows-latence-app.md)
and its linked source/evidence. These are externally reported experiments, not
upstream Windows-GPU-UNLOCK support or a reproduction on this machine.
