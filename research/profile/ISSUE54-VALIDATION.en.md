# Issue54 validation and AD104 process review

[English](ISSUE54-VALIDATION.en.md) | [한국어](ISSUE54-VALIDATION.ko.md)

Reviewed2026-10-27: [issue54 and discussion](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/54), the [shared source revision09ab194a9d9446c2bd56318815e459984a46d5ad](https://gist.github.com/yakold/31c18ec34eea889dce4222c59b7a2cdf/09ab194a9d9446c2bd56318815e459984a46d5ad), our source and four private physical capture archives. No third-party code or hardware experiment was executed. Source-file hashes and sanitized capture summaries are in [validation evidence](issue54-validation-evidence.json). This review cannot certify which omitted files or earlier/later builds produced the author's hardware results.

## Source-confirmed corrections

| Claim | Evidence and correction |
| --- | --- |
| The shared allocator uses below4GiB pages and PCI DMA mapping | In the pinned gist, `linux_env.c:199–220` uses AllocateAnyPages/EfiBootServicesData and returns the physical allocation address as the DMA address. It ignores the device and has no Map call. A pointer cast compared with its source address does not validate device translation. The reported mapped version may be another revision; this source does not establish it. |
| The shared failure branch halts Falcon then performs FLR | `unlock_uefi.c:591–597` calls the transaction, dumps the engine, releases shared resources and returns. The described failure-specific halt/FLR branch is absent there. Upstream transaction code is omitted, so this is a source/report mismatch, not proof no cleanup ran anywhere. The reported five successful recoveries remain author-reported. |
| EFI_FILE_INFO.FileSize is at offset16 | Standard FileSize offset is8; PhysicalSize is16. The gist comment at `linux_env.c:708` is wrong, but lines712/721 use the typed structure/field. No functional offset bug is established without the actual included header. Our handoff already uses typed fields. Host ABI assertions now check both offsets. |
| One million500ns reads explain nine minutes | 1,048,576×500ns=0.524288seconds. Nine minutes versus0.567seconds is about952×; changing1-byte to8-byte width alone reduces transaction count8×. Reported timings need measurement/stall/platform context; the stated arithmetic is not their explanation. |
| This artifact is a DXE driver | `build-c.sh:24` selects efi_application. It is an EFI application executing before the OS; that execution environment does not change its PE subsystem into an EFI boot-service driver. |
| The DMA mode verifies allocation and readback | `unlock_uefi.c:386–410` allocates, touches and frees each buffer sequentially. No explicit byte readback comparison is present. A further limitation for our planning is that this does not establish simultaneous transaction allocation feasibility or device DMA correctness. |
| The c1 mode does not touch the card | `unlock_uefi.c:426–434` changes and restores PCI command/BME. It is a PCI configuration write, not read-only enumeration. |
| Checkpoint logging reliability | `uefi_lib.c:242–277` selects an SFS via LocateProtocol, deletes the prior log, writes a replacement, ignores short-write/Close results, and discards the result in ul_mark. The loaded-image volume is not selected explicitly. Persistence is intended, but not verified by that path. Close can flush; the concern is selection/error handling, not a claim Close never flushes. |

The [UEFI media specification](https://uefi.org/specs/UEFI/2.10/13_Protocols_Media_Access.html#efi-file-info) defines the typed file structure. [PCI I/O Map](https://uefi.org/specs/UEFI/2.10/14_Protocols_PCI_Bus_Support.html) provides the device-visible address; CPU physical and device addresses cannot be assumed interchangeable in a portable DMA shim. Mapping presence alone would still not prove correct operation, mapped byte count, lifetime or synchronization.

## Claims requiring narrower interpretation

- The register list contains260 entries (64+16+8+128+32+12), as stated. Equal samples do not prove all internal GPU state equal or establish that driver binding caused equality. Comparisons need phase, binding, reset/power history, register-list hash, widths and timestamps. Cross-boot comparisons can be useful if labelled; they do not establish same-cycle equivalence.
- BADF5620 is unusable observed data here. It alone does not prove firmware started/crashed. Our25/28/30 captures returned the pattern without uploaded firmware execution, at different addresses from the reported transaction. Correlation is not a shared-cause diagnosis.
- The [UEFI boot-manager specification](https://uefi.org/specs/UEFI/2.10/03_Boot_Manager.html#load-option-processing) requires consuming BootNext before control transfers to that boot option. Nonvolatile storage can survive a power loss before consumption; repeating after an in-application hang is not universal BootNext behavior. Record variable state/firmware behavior rather than treating the issue's report as a platform-independent rule. Our attended USB path does not write BootNext/BootOrder.
- Neither a1GiB ESP nor a particular boot-order position is established as a GPU-unlock prerequisite by the supplied evidence. Boot selection and actual GPU state must be distinguished.
- A halt/FLR finalizer may itself change device state or fail. It is unqualified on this AD104 setup and is not added by this review. Successful capture/decode-off recovery is not recovery from an executed or failed FWSEC transaction.

## Our evidence and corrections

The original issue replies correctly distinguished child removal from parent release at their respective dates. Later [runs13–18](AD104-THIRTEENTH-EIGHTEENTH-RESULT.en.md) demonstrated the five-stop graph8→6→5→4→3→2, GPU PCI BY_DRIVER-owner relationship removal and command7→0/attributes718→0. This is a fresh observed target relationship change, not unloading the reusable global DriverBinding image or proving absence of all firmware activity. No FWSEC transaction followed, so whether release fixes the reported AD103 failure remains untested.

[Run29](AD104-TWENTY-NINTH-RESULT.en.md) preserves the failed observation and independently records guarded recovery; the operator confirmed automatic Windows arrival. [Run30](AD104-THIRTIETH-RESULT.en.md) records a returned BCR refusal and explicit hold before Windows, with uncertain operator recovery recollection. All36files in each of25/28/29/30 were rehashed against their saved ledgers during review. The58% figure is mathematically correct15/26 rounded, with equally counted milestones; it is not likelihood, time or hardware unlocked. No numerical increase is justified.

[Test31](AD104-THIRTY-FIRST-BOOT.en.md) is still unexecuted. Its arithmetic is already established offline; a boot adds integration/recovery evidence only. It excludes the three refused registers, does not change the hardware permission boundary and does not qualify firmware entry. Its staged EFI/source pins remain unchanged by this review. The older English13–18 report's double-encoded arrows/Korean link text were repaired with an exact reversible encoding check; facts are unchanged.

## Process adopted

1. Pin source revision, exact EFI hash, included ABI headers, payload hashes and build arguments before transferring another report's exclusions. Request the version actually associated with the result when source and description disagree.
2. Keep phase-specific evidence: return status, returned data validity, ownership/configuration, decode state, recovery branch and operator outcome are separate. Do not infer Windows boot from StartImage-next or infer a hang from missing logs.
3. Use our existing loaded-image-volume selection, no-overwrite captures, pinned file methods, checked counts and flush/readback/hash checks. These improve evidence reliability, not device execution safety. No broad260-register scan, wide MMIO optimization, reset or mode-file-controlled execution is adopted without separate review of the concrete scope.
4. Use the new read-only summary locally after preservation:

```powershell
python -I research/profile/attended-result.py "<saved capture>/DIAG00.TXT" --status "<saved capture>/STATUS.TXT" --operator-return unknown
```

Choose automatic/manual only from an operator confirmation; unknown is the default. The summary reports contradictory evidence as inconsistent, preserves refused observations after successful recovery, grants no unlock authority and does not replace provenance/full-log review. It reads bounded logs and prints sanitized JSON without touching the USB or hardware.

Before an execution experiment, obtain the complete tested mapping/cleanup implementation, paired raw phase logs and exact assets; qualify simultaneous allocation, mapping length/lifetime/synchronization and failure recovery offline first. The current staged test31 is unchanged, and no new reboot is required for these validation findings.
