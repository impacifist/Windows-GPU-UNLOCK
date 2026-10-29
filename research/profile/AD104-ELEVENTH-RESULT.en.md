# Combined display-consumer release: tenth and eleventh results

English | [한국어](AD104-ELEVENTH-RESULT.ko.md)

The eleventh physical run successfully removed the text, GOP-consumer and EDID
relationships in that order. The user confirmed that Windows started afterward.
Every callback, repeated postcheck, final keyboard challenge, protocol-absence
check and Windows-handoff eligibility check returned success.

| Stage | Active relationships | IMG08 shared byte | Pointer counts |
| --- | --- | --- | --- |
| Initial | 8 | 1 | 1 / 1 |
| Text removed | 6 | 1 | 1 / 1 |
| GOP consumer removed | 5 | 1 | 1 / 1 |
| EDID removed | 4 | 0 | 1 / 1 |

All eight full 256-byte PCI configuration snapshots are identical. Command stays
`0007`, current attributes `0718`, supported attributes `1b77b`. The retained
GOP metadata and PCD state matched the guarded expectations. Pointer relationship
counts and keyboard input were checked; physical pointer movement was not tested.
The current Windows NVIDIA driver reports RTX 4070 Ti and version `610.88`.

The first tenth run completed text and GOP removal but received an unexpected
key at the final input challenge (`EFI_ABORTED`). It deliberately blocked Windows
handoff. The actual key was not recorded; Ctrl instead of Shift is only a possible
explanation. A second attempt refused the existing PCD output file
(`EFI_ALREADY_STARTED`) before any detach. After archival, the tenth retest
passed both stages, input and Windows eligibility. The eleventh then combined
these with the separately demonstrated EDID transition.

All 35 files from the first tenth attempt/retry, 34 from the tenth retest and 34
from the eleventh were copied to private host archives and SHA-256 verified.
Earlier tenth captures are also archived on USB; eleventh originals remain in
the active capture folder pending preparation of a new test. Raw firmware and
private machine identifiers remain excluded. See [sanitized evidence](ad104-display-release-evidence.json)
for exact log lengths, hashes and statuses.

Implementation `1a16c2c` adds a separate three-consumer mode. Each stage requires
fresh code/graph/state checks and flushed intent before one targeted callback;
errors still trigger postcollection. Any failure blocks later stages and Windows
handoff, with no retry, reconnect or firmware fallback. The latest RAM-only suite
passed, including 139 three-consumer, 112 GOP, 139 text and 84 EDID lifecycle
checks, plus the existing capture/PCD/display/handoff and compile-admission checks.
The 44,544-byte AMD64 EFI passed PE verification (no imports/IAT/delay imports,
NX and dynamic base), SHA-256
`686fbc99760ac77ef848807cb1a6f4fa1362f961aaa83ed02f84d80fb54dd562`.
Host mocks do not establish recovery for a different firmware or further stops.

This proves the combined consumer transition on this machine and run. The four
remaining relationships are PCI ownership, its display-child relationship and
the two retained pointer relationships. The PCI owner is still active and BME
is enabled. No GPU MMIO, physical PROM capture, cold-entry/protected-memory
admission, unlock transaction or post-unlock workload has been demonstrated.

The next investigation is the pinned IMG29 PCI-owner child cleanup and zero-child
Stop path. It must define expected protocol disappearance, attribute changes,
partial-failure handling and Windows handoff before any new EFI is staged.
Consumer success does not authorize treating the owner as native NVIDIA GOP or
transferring a different board's unlock profile. See [proof boundary](AD104-UNLOCK-EVIDENCE.en.md).
