# Follow-up pull request description

English | [한국어](AD104-FOLLOWUP-PR.ko.md)

Title: Add guarded MSI AD104 capture and diagnostic MBR handoff

Base: upstream `main`. Head: `feature/4070ti-capture-preparation`.

Published PR: [#44](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/44).

Related research: [#26](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/26).
This branch follows that research but does not require its stock GPU tools.

The generic diagnostic could not collect deep observations for this MSI AD104
identity, and its GPT-only boot-volume assumption did not fit this UEFI/MBR
Windows installation. This change adds a guarded capture-only AD104 mode and a
diagnostic-only MBR handoff that pins the current system partition and hashes
the exact boot-manager bytes supplied to `LoadImage`. It adds no unlock profile
and leaves supported-board admission unchanged.

The first physical boot found the expected 4070 Ti and preserved a protocol
graph and 32 driver image snapshots. PCI command `0007` failed the required
bus-master-disabled condition before AD104 MMIO/PROM reads. The capture is
explicitly incomplete; neither unlock success nor automatic Windows return is
claimed. See `AD104-FIRST-BOOT.en.md` and `ad104-first-boot-evidence.json`.

Validation recorded for the implementation: 148 RAM-only AD104 checks, 66 MBR
checks, four GPT regressions, 16 rejected compile combinations, 12 profile
Python tests, and native AMD64 EFI PE verification. Physical evidence was
archived privately with matching USB/copy hashes and matching dump sizes.
Raw firmware, partition pins and generated binaries are excluded.

The earlier #26 description concerns its own preboot-free research scope. This
follow-up introduces the later physical diagnostic evidence and should link
back to #26 without implying those earlier controls demonstrated an unlock.
