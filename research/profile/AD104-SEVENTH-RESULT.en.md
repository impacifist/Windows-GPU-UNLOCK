# Seventh AD104 boot: EDID release observed

English | [한국어](AD104-SEVENTH-RESULT.ko.md)

The 2026-10-26 physical test completed its single targeted EDID disconnect.
All experiment stages returned success, including both actual keyboard
challenges, two postcollections and the final postcheck.

| Observation | Before | After |
| --- | --- | --- |
| Relevant active relationships | 8 | 7; EDID relationship absent |
| IMG08 shared byte | 1 | 0 |
| Two pointer counts | 1 / 1 | 1 / 1 |
| Device attributes | `718` | `718` |
| Supported attributes | `1b77b` | `1b77b` |
| PCI command | `0007` | `0007` |

All four recorded 256-byte PCI configurations (initial, post, post-repeat and
final-post) are identical. The exact post-state checker also accepted the pinned
code/bindings, remaining relationships, BAR descriptor and console/input
identities. A successful uppercase `V` challenge demonstrates keyboard input
after the disconnect. Pointer movement was not tested; matching pointer edges
and counts alone cannot establish it.

The user explicitly reported that Windows started booting when the EFI test
exited. A subsequent Windows `nvidia-smi` query found RTX 4070 Ti at
`00000000:0B:00.0`, driver `610.88`, VBIOS `95.04.31.00.f0`. Together these are
evidence of Windows return and driver availability on this run. The log records
Windows eligibility, not a post-StartImage trace. This does not establish general
recovery reliability or behavior after other driver removals.

All 33 files were preserved in a new private host archive, with each USB/copy
SHA-256 equal. The log is 400,491 bytes, SHA-256
`3746cbe874eb563a6e1fe824b2afc1283f7fd6307216fc75745b2cee9ae764ab`.
The USB EFI still matches the prepared SHA-256
`5503b5fc3cda5cac2d9bc852d669ec509890f94800a205510da54b099b9d69f7`.
See [sanitized evidence](ad104-seventh-boot-evidence.json). Raw firmware remains
private; USB originals and the installed EFI were not changed during analysis.

This validates the predicted EDID lifecycle transition on this firmware. It is
not PCI-owner release, cold-entry evidence or unlock: BME remains enabled, and
no GPU MMIO, attribute write or unlock transaction ran. Earlier
`capture-complete=0`/`EFI_NOT_READY` describes the metadata baseline; the later
experiment success is a separate result.

The next candidate is the IMG14 text binding, before the IMG17 GOP consumer.
It requires additional offline analysis and a separate experiment contract.
Initial Stop inspection finds child-sensitive cleanup, CloseProtocol calls and
pool frees; zero-child success alone does not establish release. Removing text
relationships may invalidate visible prompts, so the current post-test prompt
path cannot simply be reused. No next EFI test is staged and no repeat of this
successful EDID test is required.
