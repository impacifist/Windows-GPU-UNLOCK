# Twentieth attended candidate: bounded BAR PROM prefix

Executed result: [test20](AD104-TWENTIETH-RESULT.en.md). Original candidate contract follows.


English | [한국어](AD104-TWENTIETH-BOOT.ko.md)

Built and host-tested, not physically executed. New MMIO risk requires explicit
approval before staging. [Test19](AD104-NINETEENTH-RESULT.en.md) repeated the delayed
register capture and direct Windows recovery but left the reference SS1/VGA_BASE
differences unresolved. Physical PROM evidence remains missing.

After the existing five stops, Enable(0x300), delayed register batches and fresh
state admission, this candidate reads only BAR0 offsets0x300000–0x3001fc:
128 DWORDs/512bytes. First DWORD must have AA55 signature; errors or method drift
stop immediately. Arbitrary payload words may legitimately equalFFFFFFFF/BADF;
only the first signature and transport statuses are interpreted as such guards.
No allocation, file or console callback runs inside the read sequence.

One signature reread at0x300000 and one existing chip-ID read follow:129 PROM
DWORD accesses plus one chip read in total. Exact full PCI/attributes/ownership/
old-child state is checked before/after reads and again after logging. This
signature check does not establish temporal stability of every header byte.

Parsing requires a PCIR signature, vendor10de/device2782, minimum24-byte structure
fully inside the captured512bytes, nonzero image length bounded by the known1MiB
window, and an image extent covering its header. A pointer/structure outside the
prefix is incomplete/rejected; it does not trigger more reads. No full image,
checksum, chain, whole-ROM identity, FWSEC metadata or unlock eligibility is claimed.
Only complete admitted prefix DWORDs are logged privately; no PROM.BIN is produced.
Later offline comparison with the existing firmware RAM ROM can establish prefix
equality only. A matching prefix cannot verify a complete physical PROM hash.

Fresh V then permits the same approved Disable(0x300), repeated exact post-parent
state checks, STATUS.TXT witness and Windows handoff. A header/log/state failure
holds before V/Disable/Windows, so manual recovery may be needed. No retry,
reconnect, reset, BME/VGA enable, firmware upload, unlock or automatic rollback.
New MMIO reads may have side effects or hang; Windows/display/input may fail.
Recovery remains unproven for this candidate.

Host suite includes62,991 bounded-header checks: every130 transport callbacks,
signature/PCIR/identity/length faults, legitimate sentinel-like payload, maximum
legal PCIR boundary, intent-flush admission drift and output-write failure/drift.
Earlier delayed-read/decode-off/witness/capture tests remain. Mocks are not physical
proof. Native AMD64 PE55,808bytes, no imports, SHA-256:
`627d156e1a0ccb1fe4832f17f968667e577ae4206f90bee1b177739e675d283d`.
Builder requires PROM-header→delayed-read→decode-off→decode-read and records512bytes
in the manifest. Candidate is staged only after separate risk acceptance.

When staged: save work, fully shut down, wait10seconds, select UEFI USB. Shift+G
once accepts all stages. Wait five seconds, tap Shift+V every two seconds for up
to one minute. No new screen after detach. If no Windows within five minutes,
power off, remove USB, choose normal Windows Boot Manager. Run once and preserve
all captures before another attempt.
