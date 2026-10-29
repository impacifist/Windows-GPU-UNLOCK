# Twelfth AD104 result: display child removed, Windows returned

English | [한국어](AD104-TWELFTH-RESULT.ko.md)

On 2026-10-27 the direct IMG29 one-child Stop succeeded on the MSI RTX 4070 Ti
(AD104, `10de:2782 / 1462:5132`). After the previously demonstrated text/GOP-consumer/
EDID stops, the exact pinned driver binding was invoked as
`Stop(binding, gpu, 1, &child)`. No zero-child parent Stop was invoked.

Both postcollections, fresh old-child handle-absence checks, the final keyboard
challenge, final state checks and Windows-handoff eligibility returned success.
The user confirmed Windows booted. Relevant relationships progressed
`8 → 6 → 5 → 4 → 3`; the remaining three are PCI BY_DRIVER ownership and both
pointer relationships. Shared flag is zero and pointer counts remain `1/1`.
The collector does not query old child protocols or dereference saved GOP/mode
allocations after the direct call. A fresh full handle enumeration found the
old child absent, rather than relying solely on the Stop return code.

All ten full 256-byte PCI configuration snapshots are identical. Command remains
`0007` (BME enabled), attributes `0718`, supported `1b77b`. Current Windows readback
reports RTX 4070 Ti and NVIDIA driver `610.88`. The user observed text clearing,
a blinking underscore and a briefly older-looking Windows boot display. These
are visual observations, consistent with possible legacy video effects; they do
not establish a changed Windows boot mode or any unlock state.

All 34 capture files and the manifest were copied into a private host archive,
with each USB/copy SHA-256 verified. The 411,742-byte DIAG log SHA-256 is
`a93f83a9338b974fbf6d27eb184f0048119f78a9d08038b7b4e25ee2e24c9945`.
The staged 46,592-byte EFI SHA-256 is
`bb82bd260c9b8df0142318692a090b8f28ecf8ecb8e3858593d2f145c8b20975`,
built from implementation `4eb0c15` before the later main merge. No raw firmware,
private disk identifiers or absolute firmware addresses are published.
See [sanitized evidence](ad104-twelfth-boot-evidence.json).

This demonstrates child removal and Windows return on this run. It does **not**
demonstrate complete PCI-owner release, BME-off/cold-entry admission, FWSEC/WPR2
transaction success or a 4070 Ti unlock. It does not resolve the AD103 FWSEC
failure reported in [issue #54](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/54),
and establishes no general RTX 4000 compatibility ([issue #2](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/2)).

Next is a separately designed parent-stop experiment with its own attribute,
configuration, input and recovery conditions. No parent-stop test is staged.
The [twelfth contract](AD104-TWELFTH-BOOT.en.md) and
[proof gates](AD104-UNLOCK-EVIDENCE.en.md) distinguish this completed step from
remaining admission and transaction work. No further hardware action was taken
while preparing this result report.
