# MSI RTX 4070 Ti: first physical diagnostic capture

English | [한국어](AD104-FIRST-BOOT.ko.md)

On 2026-10-26 the capture-only EFI built from local commit `33670b1`
executed on the MSI AD104 system. This is a partial observation, not an unlock
or a complete ROM/register capture.

## Preserved evidence

The USB contained `DIAG00.TXT` (394,904 bytes) and `IMG00.BIN` through
`IMG31.BIN` (1,090,496 bytes combined). All 33 files were copied to a private,
ignored host archive and each copy's SHA-256 matched its USB source. All 32
image sizes matched their diagnostic records. Raw firmware images and the
machine's full protocol graph are not included here.

The [sanitized evidence summary](ad104-first-boot-evidence.json) records the
observed values and integrity checks without raw firmware or machine paths.
This follows the earlier [4070 Ti research contribution, PR #26](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/26).

The USB EFI SHA-256 remained
`ce222b6034453da0a53532a39d36afdc6f2741bd0ca482d45ec19aa5ba6880f4`.

## Findings

- The diagnostic found one target at `0000:0b:00.0`, device `10de:2782`,
  subsystem `1462:5132`, class/revision DWORD `030000a1`.
- PCI command/status DWORD was `00100007`: the command register is `0007`,
  including memory decoding and bus mastering enabled. BAR0 was `fb000000`.
- `ProbeEnvelope` requires `(cfg[1] & 6) == 2`: memory decoding enabled and
  bus mastering disabled. The observed value instead gives `6`. This alone
  explains the early refusal; BAR resource descriptors were not validated
  because the command check short-circuits first.
- The log records `incomplete reason=ad104-envelope`, `capture-complete=0`,
  and diagnostic status `8000000000000006` (`EFI_NOT_READY`). No `PROM.BIN`
  exists. The AD104 capture path returns before its GPU MMIO reads.
- Firmware exposed one GOP with a 1024 x 768 framebuffer. The log also
  contains driver bindings and protocol ownership observations for offline
  analysis; their presence alone does not identify a safe release procedure.
- The log enumerates 33 driver bindings, including one with a null image
  handle, and records 32 image dumps. The implementation's dump ceiling is
  100, not 32. These are driver-binding image snapshots, not a dump of every
  loaded image or the complete system firmware; repeated image handles can
  also produce duplicate snapshots. Coverage needs ownership analysis.
- Initial handoff preparation returned success and the loaded-image graph
  includes Windows Boot Manager. The log ends at the diagnostic result before
  the Windows start call; it does not prove which boot path eventually returned
  the user to Windows.

## Next investigation

Completed offline follow-up: [ownership findings](AD104-OWNERSHIP.en.md) and
[image hash evidence](ad104-ownership-evidence.json). The PCI-to-GOP owner is
present in IMG29 and contains `BIOS[INT10] Video Driver`; a native NVIDIA GOP
identity or safe release procedure is not established.

Analyze the captured ownership graph and image bytes offline to identify the
GPU GOP's driver/controller relationship. Preserve the bus-master guard: do
not clear the PCI command bit, detach drivers, or treat this partial capture as
an unlock profile. Any future capture mode accepting active bus mastering
needs a separately reviewed observation-only contract and tests. Another boot
of the identical diagnostic is not expected to overcome this refusal.
