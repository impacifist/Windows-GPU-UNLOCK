# Thirteenth AD104 test: direct PCI-owner Stop

Executed result: [runs 13–18 and recovery findings](AD104-THIRTEENTH-EIGHTEENTH-RESULT.en.md). The contract below describes the original thirteenth candidate.


English | [한국어](AD104-THIRTEENTH-BOOT.ko.md)

Status: built and host-tested; not staged or physically executed. This adds a
fifth stage after the four steps demonstrated in the [twelfth result](AD104-TWELFTH-RESULT.en.md).
The exact pinned IMG29 binding is called as `Stop(binding, gpu, 0, NULL)` once.
The driver may disable PCI I/O/memory decoding, bus mastering and VGA attributes.
No explicit application attribute write, decode re-enable, reconnect, reset,
GPU MMIO, FWSEC transaction, firmware/NVRAM write or unlock is included.

Before the call, two new observations must agree with the successful child-stop
state: three edges, no old child handle, command7, attributes718/support1b77b,
shared flag0/pointer counts1/1, unchanged full PCI/BAR/PCD/input state and exact
code/binding pins. The alternate branch GUID at IMG29 RVA39d0 must still equal
`c7735a2f-88f5-4882-ae63-faac8c8b86b3`; this protocol must be absent on the GPU.
Presence, an unexpected error or a non-NULL result refuses admission. These
checks, including binding instance and branch absence, repeat after intent flush.

Static captured code supports the proposed branch: query Supported, request
Disable `(Supported & 0x700) | 0x18` (expected718), close PCI BY_DRIVER ownership.
Intermediate return values are not all checked by the driver, so callback success
alone cannot qualify this test. These are predictions to verify on hardware.

Require two fresh postcollections with exactly two relevant edges (retained
pointer relationships), old child absent, command0, attributes0, support1b77b,
and every other captured field unchanged, including the PCI status high16 bits.
No saved GOP/mode or old child protocol is accessed after child removal, including
on errors. Any failed callback or unexpected poststate holds without retry or
restoration. Only matching postchecks, a fresh uppercase V, final matching state
and successful log close permit the preloaded Windows boot manager to start.
Command0/BME-off would still not establish protected-memory/cold-entry admission.

New risk boundary: this actually stops the PCI owner and asks the driver to
remove decoding and DMA permission. Display/input can disappear, firmware can
hang, Windows can fail to initialize the GPU or boot, and recovery may require
manual power-off/USB removal. No automatic restoration is attempted. The prior
successful child-only return does not prove recovery from this new state.
Acceptance of this boundary is required before USB staging; do not boot yet.

Validation:336 direct-parent RAM checks cover callback failure/partial mutation,
remaining ownership, no-op/residual attribute changes, wrong command/status/BAR,
pointer/input drift, old child reappearance, intent/final log failures, binding
or branch drift and final-key changes. The actual branch helper is separately
tested for its fixed GUID bytes, bounds, presence and defined absence statuses;
full-engine mocks model that helper's outcomes. The preparation suite and33Python
tests pass. Native AMD64 EFI48,128bytes passed no-import/IAT/delay-import and
NX/dynamic-base checks. SHA-256:
`c148f2c09282c4afeaf77e9d8ade33f0551aab2bb064991c2f2dbf11966bfc0c`.
Host tests are not physical recovery evidence.

After acceptance and confirmed staging: full shutdown, wait10seconds, ASUS
Delete/F8 → UEFI USB DISK2.0. Shift+G once authorizes all FIVE stages; wait5seconds,
then tap Shift+V every2seconds for up to1minute. There is no second prompt. Boot
once. If stopped/hung or Windows absent5minutes, power off, remove USB, select
normal Windows Boot Manager; recovery is unproven. Preserve captures before retry.
