# AD104 runs 13–18: PCI release, live reads and Windows recovery

English | [한국어](AD104-THIRTEENTH-EIGHTEENTH-RESULT.ko.md)

## Outcome

On 2026-10-27 the MSI RTX 4070 Ti (`10de:2782 / 1462:5132`, AD104)
completed the full attended sequence: five driver stops, explicit I/O and
memory decode enable with bus mastering/VGA off, two identical register batches,
explicit decode disable, and user-confirmed direct Windows return. NVIDIA driver
610.88 subsequently reported the GPU normally. This demonstrates one successful
capture-and-return sequence. It does not demonstrate unlock or guaranteed recovery.

[Sanitized evidence](ad104-thirteenth-eighteenth-evidence.json) records stage
statuses, register observations, file counts and diagnostic hashes for every run.
Raw loaded firmware, handles, platform identifiers and private build configuration
remain excluded. Each host copy was SHA-256 checked against its USB source before
analysis; source captures are retained and never overwritten during staging.

## Physical chronology

| Run | Observed result | Windows observation |
| --- | --- | --- |
| 13 | Direct IMG29 zero-child parent Stop succeeded after the four previous stops; graph 3→2, attributes 0x718→0, PCI command 7→0; repeated checks passed | User returned to Windows |
| 14 | Enable(0x300) succeeded; attributes 0x300/command 3; first batch attempted 22 reads, last offset 0x840100 returned 0xbadf5620; guard skipped batch two and deliberately held | Manual power-off and normal Windows boot |
| 15 | Excluded 0x840100; replaced it with a second chip-ID read. Both 22-read batches matched, V and final state checks passed | No visible boot; manual recovery |
| 16 | Same capture; root Close independently returned success; last marker preceded main-log Close and StartImage | No visible boot; manual recovery |
| 17 | Independent STATUS.TXT remained open: both root and main-log Close returned success; StartImage-next persisted, no returned status | No visible boot; manual recovery |
| 18 | Same capture, then one Disable(0x300) after V; repeated whole-state comparison matched saved post-parent state, command 0/attributes 0; final verification passed | User confirmed direct Windows return |

The complete relevant graph progression is 8→6→5→4→3→2: text consumer,
GOP consumer, EDID consumer, direct owner-child Stop, direct parent Stop.
Only the two pointer relationships remain; shared flag is zero, pointer counts
remain 1/1. Parent Stop removes I/O/memory/BME/VGA decoding. Limited Enable(0x300)
restores only I/O and memory; command is 3 and BME/VGA remain off.

Run 18's successful return after Disable(0x300), compared with earlier command-3
failures, supports returning decoding to the observed post-parent state before
Windows on this machine. This is a recovery hypothesis supported by one successful
run, not a controlled universal causality claim. It does not undo internal effects
of previous reads. The independent status handle remained open in both runs 17
and 18, so its presence alone does not explain their different outcomes.

## Register evidence and admission limits

Run 18 made 22 reads per batch: 21 unique offsets plus a repeated chip-ID
bookend. Both bookends were 0x194000a1, both batches were byte-identical and all
read callbacks returned success. There was no intentional dwell between batches.
The excluded 0x840100 was not retried; BADF, all-ones, callback error, method drift,
PCI/attribute/ownership drift and identity guards remain active.

| Observation | Run 18 value | Interpretation boundary |
| --- | --- | --- |
| Chip identity, 0x0 | 0x194000a1 | Matches observed AD104 chip identity; not a write profile |
| GFW fields, 0x118128 / 0x118234 / 0x110100 | 0x8b8f / 0x3ff / 0x10 | Matches these reference fields only |
| WPR2 high, 0x1fa828 | 0 | Does not by itself prove full cold-entry eligibility |
| Feature PLM, 0x823804 | 0xffffff8f | Captured protected state, no transition demonstrated |
| SS0, 0x82381c | 0x24020000 | A captured value is not a universally valid immutable baseline |
| SS1, 0x823820 | 5 | Reference profile requires 1; unresolved difference |
| Usable framebuffer MiB, 0x1183a4 | 0x2ffa | 12,282 MiB; register-derived usable size, not marketed capacity |
| VGA_BASE, 0x625f04 | 0x10e09 | Reference profile requires 1; unresolved difference |

Reference gate comparisons use the pinned AD104 research reference's [profile.c](https://github.com/JustAResearcher/gpu-unlock-ada/blob/4e7fefcc4923f9e083d5324dc75a7d2f906af77f/windows/ad104-msi/profile.c)
Registers() policy, not the repository's default RTX 3090 profile. Reference PCI
BAR placements also differ. These differences must not be accepted by copying
profile constants or weakening gates. No FWSEC execution, protected-memory
transaction, firmware upload, reset, unlock or post-unlock workload ran.
The existing 150,016-byte firmware RAM ROM capture is distinct from a physical
1 MiB BAR PROM capture; its hash cannot stand in for missing PROM evidence.

## Failure visibility and implementation lessons

A blinking cursor cannot distinguish waiting for V, a deliberate guard stop,
an ordinary error return, firmware/boot-manager stall, or display-only failure.
Run 14 was a deliberate guard stop before the V wait. Runs 15–18 accepted V.
The original file-close log could not distinguish main-log Close from StartImage.
STATUS.TXT is now created before detach, refuses overwrite and aliased handles,
and records bounded, flushed stage results independently of the main log.
Its ExitBootServices callback sets only a RAM flag. No filesystem access occurs
in that callback, and post-return file access is suppressed once notified.
Thus absence of a StartImage-returned marker is not proof that the call hung,
and ExitBootServices notification is not proof that Windows booted.

Each register batch accumulates values/statuses in RAM and logs only afterward.
Logging per read can trigger implicit file writes when a text buffer fills;
that implementation was corrected before physical execution. Fresh full state
admission follows logging before the next batch. Decode-off similarly requires
fresh admission after intent logging, one mutation only, postchecks even after
callback errors, exact full-state equality twice, and another check after flush.
No freed GOP/child allocations are queried after removal. Failure holds without
retry, reset, reconnect or automatic rollback.

The monitor runs Windows at 5120×1440, 144 Hz, while captured EFI GOP is
1024×768. Full-panel scaling explains roughly 2.67× horizontal stretching.
Shorter text does not correct aspect ratio. No firmware display mode was changed;
changing it would add another hardware-state variable to the experiment.

## Validation and next test

The final run-18 build was 53,760 bytes, SHA-256
`92860ce034a6f12aca9560e06656515eca4d72c3e0988a5aea7078d83b883a76`.
It is an AMD64 EFI PE with no imports. Before physical execution, the preparation
suite included 2,074 decode-off RAM checks and 33 Python tests; build/mock results
are separate from the physical observations above. Review corrected handle-alias
admission and a builder output-directory selection bug before the candidate ran.

The next test adds a nominal one-second inter-batch Stall to the same read set,
with complete fresh admission afterward, then retains the proven decode-off and
status-log handoff. It tests delayed stability and repeatability of recovery,
not unlock eligibility. No new register, PROM read or GPU write is added.
