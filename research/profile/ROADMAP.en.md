# RTX4070Ti unlock research roadmap

English | [한국어](ROADMAP.ko.md)

As of physical test30, **15/26 milestones=58% checklist completion** (rounded).
Milestones are equally counted, not equally difficult. This is not a probability,
time estimate or percentage of hardware unlocked. Unlock remains unproven.

| Stage | Completed/total | Completion | Evidence and remaining work |
| --- | --- | --- | --- |
| Baseline | 4/4 | 100% | Inventory, stock compute baseline, ownership map, verified archives |
| Firmware ownership release | 4/4 | 100% | Text/GOP consumers, EDID consumer, owner-child, parent stops demonstrated |
| Register capture/recovery | 3/3 | 100% | Limited decode/read, delayed matching batches, decode-off/direct Windows recovery |
| ROM/FWSEC evidence | 3/4 | 75% | RAM ROM, physical header, full PROM captured. FWSEC located/bounded/hashed; execution-readiness validation open |
| Exact-machine admission | 1/4 | 25% | Registers collected. SS1, VGA/layout explanation and justified machine-specific policy remain |
| Guarded unlock transaction | 0/4 | 0% | Exact-machine design, offline validation, approved execution, independent state readback remain |
| Windows unlock proof | 0/3 | 0% | Post-unlock driver initialization, controlled workloads, repeated cold boots/recovery remain |

The100% rows describe the narrowly listed milestones on this machine; recovery
is not guaranteed and broad platform safety has not been established.

## Milestone checklist

- [x] Exact GPU/platform inventory
- [x] Stock compute baseline
- [x] Firmware ownership map
- [x] Hash-verified evidence archives
- [x] Combined text/GOP consumer stops
- [x] EDID consumer stop
- [x] Direct owner-child stop
- [x] Direct parent stop
- [x] Limited decode and live register capture
- [x] Delayed register comparison
- [x] Verified decode-off/direct Windows recovery
- [x] Firmware RAM ROM and bounded PCIR chain analysis
- [x] Physical PROM header observation
- [x] Full physical PROM transport and file verification
- [ ] FWSEC material validated for a proposed execution policy (structural parsing/hash evidence exists; authentication/readiness not established)
- [x] Post-release prerequisite register sample
- [ ] Explain and justify SS1 state
- [ ] Explain VGA_BASE and memory-layout differences
- [ ] Validate exact-machine admission policy without blind reference transfer
- [ ] Design exact-machine transaction and failure boundary
- [ ] Validate transaction offline with required assets and mocks
- [ ] Explicitly approve and execute bounded hardware transaction
- [ ] Independently verify intended unlock-state transitions
- [ ] Verify Windows driver initialization after unlock
- [ ] Compare controlled stock/post-unlock workloads with state evidence
- [ ] Repeat cold-boot behavior and recovery verification

Latest: [test29 guarded recovery](AD104-TWENTY-NINTH-RESULT.en.md), then [test30 BCR refusal/hold](AD104-THIRTIETH-RESULT.en.md). Operator recollection of30 recovery is uncertain; logs establish deliberate hold before Windows handoff.
Next: [test31 measured-input audit](AD104-THIRTY-FIRST-BOOT.en.md), omitting all three refused registers. Tests22–24 established CPU-owned parser/patch/signature assembly;25–30 separated accessible observations from rejected data and one exact recovery path. None completes firmware-entry qualification. Full layout requires parsed/pinned assets, not file-byte counts. SS1/VGA/reference-ROM differences remain; repeated captures or arithmetic alone do not count as unlock/admission.

[Issue54 validation and process corrections](ISSUE54-VALIDATION.en.md): pinned source/report mismatches, claim boundaries and read-only outcome summaries. The staged test31 remains unchanged.
