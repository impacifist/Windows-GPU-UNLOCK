# GPU families and machine configuration

**English** | [한국어](GPU-FAMILIES.ko.md) | [Compatibility](COMPATIBILITY.en.md)

The existing transaction and memory-layout implementation is shared by the
recorded GA102, GA104 and GA106 configurations. Machine profiles compose that
implementation with exact card identity, memory size, firmware and console data;
they do not contain a separate unlock implementation for each motherboard.

| Layer | Source | Responsibility |
|---|---|---|
| GPU family | `internal-boot/unlock/gpu-family.h` | Exact chip revision and reviewed PCI-ID/VRAM combinations |
| Shared transaction/layout | `internal-boot/unlock/vendor/unlock_610.c`, `layout_610.c` | Existing guarded transaction and memory-layout calculation |
| Card and platform pins | `internal-boot/unlock/machine-profile.h` | PCI/subsystem, PROM, exact usable VRAM, BAR, root port, display paths and topology |
| Fixed firmware templates | `internal-boot/unlock/firmware-profile.h` | Exact console/EDID/nvgop fingerprint tuples, selected independently of GPU family |
| Generated local data | ignored `profile-local.h` | This machine's captured values, bound to its host-test evidence |

## Current family catalog

| Family | Exact chip revision | Currently admitted card/layout combinations |
|---|---|---|
| GA102 | `0xb72000a1` | RTX 3090 / 3090 Ti; `0x6000` or `0x5ff4` MiB, still pinned exactly per machine |
| GA104 | `0xb74000a1` | `10de:2482` / `10de:2484` / `10de:24c9` with `0x2000` MiB; `10de:2487` with `0x3000` MiB |
| GA106 | `0xb76000a1` | `10de:2504` with `0x3000` MiB |

These describe current source admission, not physical verification of every
combination or blanket support for a family. Individual hardware results remain
in the compatibility records. Marketing names do not identify the chip: the recorded
RTX 3060 configurations include both GA104 and GA106. Revision bits are not masked.
Unsupported families, mismatched chip IDs and unreviewed PCI-ID/VRAM combinations
are compile errors in this catalog.

## Firmware is independent

GA102 machines already use different console firmware and nvgop builds. A family
selection therefore cannot supply a firmware fingerprint. The fixed templates
share only values captured identically on the recorded machines. For example,
B660M/3070 Ti, B660M/3090 Ti and B660-PLUS/3060 share console fingerprints while
retaining their own GPU/PROM/display-path pins and child-count policies. Five
fixed profiles share the same nvgop `0x60010` tuple.

LOCAL profiles always keep their captured driver hashes, including nvgop
`0x60009`; they inherit no fixed template. Existing generated headers retain their
literal chip/memory values. Family metadata is derived from the exact captured
chip. The [local generator](NEW-MACHINE.en.md) also admits the exact 12 GiB
GA104 `248710de` and GA106 `250410de` tuples. These require the reviewed nvgop
`60010` code. RTX 3070 `248410de` / 8 GiB uses `60009`, based on
[issue #113](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/113).
RTX 3060 Ti `24c910de` / 8 GiB requires its own reviewed `60015` code hash,
from [issue #105](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/105).
Its contributor reports one restart-based transaction; current-main hardware qualification remains pending.
GA104 requires PCI-specific driver pins; a chip-level match cannot substitute for them.
The existing GA102 combinations remain available. Recognized aggregate and
non-aggregate LOCAL topologies require all 22 host jobs, including fresh PCI identity, input policy
and the topology-specific complete driver-release replay. Generated profiles are experimental candidates, not
new physical compatibility reports.

The identity-only H610M probe still has no chip, VRAM or PROM pins and cannot
compile into a write/release image. External overrides of the selected family
or firmware template are refused. All existing runtime ownership, cold-state,
PROM, driver-code, DMA/FLR and cleanup checks remain.

## What this changes for existing users

Existing profile names, build commands and hardware scope stay the same. The six
machine configurations remain exact validation records and compatibility data.
Shared traits and fingerprint tuples are now defined once, reducing repeated code
when investigating another card/platform combination. New hardware is not proven
by selecting a matching family.

New headers participate in source manifests and the LOCAL input digest. Previously
generated LOCAL test evidence becomes stale when the source tree changes; rerun
the required host tests and rebuild before any subsequent staging. This is the
existing provenance gate, not a new authorization to run the unlock.
