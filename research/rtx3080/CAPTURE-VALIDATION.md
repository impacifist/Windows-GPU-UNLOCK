# RTX 3080 capture-only validation

This validates the source and an **unexecuted** EFI diagnostic for the exact
`10de:2216 / 1458:403f` research target. It does not validate an unlock.

## Source revisions

- Upstream base inspected: `38b36297fc1fcf4ecca40c5c7e818f0cb7c94b44`.
- Test-harness fix: `85642fa8c45bcaf209b05c85dd86995c3eee9301`.
- Capture-only source: `7eff7c67dc8c6191ef60655149e6b63ab0ade0f9`.

The capture change adds the exact `221610de / 403f1458` pair only to the
generic diagnostic probe's bounded deep-read admission. A different `2216`
subsystem remains inventory-only. `research/profile/proven.json`,
`research/profile/make-profile.py`, `internal-boot/unlock/gpu-family.h` and the
unlock transaction sources are unchanged.

## Host and parser tests

Using Visual Studio LLVM clang/LLD 19.1.5:

- `internal-boot/boot/test-diagnostic.ps1`: all diagnostic, checked-profile,
  generic-probe, automatic-bus and compile-refusal checks passed. These use
  RAM-only/simulated EFI services and provide no real GPU mutation services.
- `internal-boot/unlock/tests/test_gpu_families.py`: 6/6 passed. The suite
  explicitly requires GA102 + device `221610de` + `0x2800` MiB to remain
  refused by the write-enabled family catalog.
- `research/profile` Python tests: 58/58 passed.
- `research/profile/test-probe-build.ps1`: 28/28 synthetic probe
  build/CLI/resume cases passed using mocked partitions and USB media.

The test-harness commit resets the process exit status only after all expected
negative compile guards have been checked; before that fix the script printed
all PASS lines but leaked the final expected compiler rejection as exit code 1.

## Unexecuted diagnostic artifact

Built from clean source commit `7eff7c67dc8c6191ef60655149e6b63ab0ade0f9`
with the installed Visual Studio LLVM 19.1.5 toolchain. The current system EFI
partition identifier was supplied locally to the build script but is not
recorded here or in Git.

| Property | Value |
|---|---|
| File | `internal-boot/boot/build/probe-generic/BOOTX64.EFI` (ignored local build output) |
| Size | 22,016 bytes |
| SHA-256 | `a40b76bcfa95d2803bc061533ad4ae99eebed07bfdfd1e9e70a1fd2b51c039dd` |
| PE machine | AMD64 |
| PE subsystem | 10 (EFI application) |
| Imports | 0 |
| NX compatible | true |
| Dynamic base | true |
| Probe bus mode | automatic firmware discovery |
| `CanUnlock` | false |
| `CanDetach` | false |
| `WritesFirmwareVariables` | false |
| `HardwareTested` | false |

Compiler provenance for this build:

- clang 19.1.5 SHA-256
  `c5d3d309c036d1666d50500e87be514dc9916adc427e7aec267c95178bec14ff`
- LLD 19.1.5 SHA-256
  `b24601b51e04b2ef371c2809ad33e3b35dff5cb7e166e26a1d2ddb74148c212c`

An independent local build with the already-installed LLVM 21.1.6 toolchain
also passed PE verification; compiler-version differences changed the binary
hash, so the artifact above is the single reviewed build identity.

## Deliberate remaining refusal

The automatic compatibility checker and local profile generator still treat
`2216 / 0x2800` as outside the hardware-proven write-profile envelope. That is
intentional. The current envelope schema ties its deep-analysis device IDs to
the exact hardware-proven PCI/chip/memory tuples, so widening that list before
this machine's captures would weaken the write-profile boundary.

The raw diagnostic capture, once separately approved and collected, remains
the evidence source for reviewing the machine-specific PCI/BAR, cold-state,
PROM/FWSEC and firmware ownership data. Only after that review should the
write-profile envelope be considered for a separate change.

## Physical-execution status

The EFI file has **not** been copied to removable boot media, added to a boot
entry, executed, or used to read this GPU's live pre-boot MMIO/PROM. No reboot,
partition change, boot-variable change, driver release, GPU reset, firmware
write or unlock transaction was performed by this validation.

See [CAPTURE-DESIGN.md](CAPTURE-DESIGN.md) for the evidence and review gates
required before any physical capture.
