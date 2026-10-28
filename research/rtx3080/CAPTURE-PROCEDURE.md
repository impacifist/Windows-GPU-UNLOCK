# Reviewed capture-only procedure for the RTX 3080 research target

Status: prepared but **not executed or staged**. This procedure is the physical
boundary described by [CAPTURE-DESIGN.md](CAPTURE-DESIGN.md). It authorizes no
unlock transaction, GPU write, driver release, DMA operation, reset or firmware
variable change.

## Reviewed diagnostic identity

The reviewed read-only diagnostic was built from capture source commit
`7eff7c67dc8c6191ef60655149e6b63ab0ade0f9` with Visual Studio LLVM 19.1.5.

| Property | Reviewed value |
|---|---|
| `BOOTX64.EFI` size | 22,016 bytes |
| SHA-256 | `a40b76bcfa95d2803bc061533ad4ae99eebed07bfdfd1e9e70a1fd2b51c039dd` |
| Probe mode | generic read-only probe then Windows |
| Target selection | automatic firmware-bus discovery |
| Exact deep-read PCI pair | `221610de / 403f1458` |
| `CanUnlock` | false |
| `CanDetach` | false |
| `WritesFirmwareVariables` | false |
| `HardwareTested` | false |

If any of those values changes, stop and review the replacement artifact before
physical execution.

## Prerequisites before staging

1. The owner is physically present at the PC for both boots and can use the
   motherboard's one-time UEFI boot menu.
2. A separate USB data stick is connected. The workflow must identify it as
   USB, FAT32, non-system and non-boot. Do not repartition or format an existing
   disk merely to satisfy the workflow without a separate explicit decision.
3. The active monitor remains on the existing RTX 3080 connection. Record any
   physical adapter/switch in the local capture notes. A spare display GPU, if
   available, is recovery equipment only and is not part of the capture.
4. The repository branch and artifact hash are rechecked immediately before
   copying. Generated local partition identifiers remain private.
5. No permanent firmware boot-order entry is created. The stick is selected
   only from the one-time boot menu.

At the time this procedure was prepared, no USB storage device was connected,
so no media was staged.

## Staging operation after explicit approval

Use `research/profile/new-machine.ps1` on the reviewed branch with the chosen
USB drive letter, the current Windows system-ESP GUID obtained locally, the
reviewed clang/LLD paths and a local assets path. For the first capture stage,
the workflow builds and copies only the generic diagnostic because fewer than
the required captures exist.

The script itself verifies that the selected destination is a FAT32 USB data
stick, not the system partition or boot disk. It then:

- rebuilds the generic diagnostic;
- verifies the manifest and copied EFI hash;
- preserves any previous removable `EFI\\BOOT\\BOOTX64.EFI` under the stick's
  `RTX3090-UNLOCK\\backup\\` directory;
- archives earlier diagnostic files from the stick instead of deleting them;
- copies the probe to `EFI\\BOOT\\BOOTX64.EFI`;
- writes a removable-media README identifying it as a read-only probe;
- stops without creating a firmware boot entry or rebooting the machine.

Do not use `-StageUnlock` at this stage.

## Capture 1

1. Verify the staged `BOOTX64.EFI` SHA-256 again against the reviewed hash.
2. Perform a **full Windows shutdown**, not Restart.
3. Power on and use the motherboard's one-time boot menu to select the USB
   stick. Do not change permanent boot priority.
4. Allow the diagnostic to run and hand off to Windows normally.
5. Once Windows starts, do not edit the capture files. Verify that a complete
   `DIAGnn.TXT` exists together with `PROM.BIN` and the required `IMGnn.BIN`
   files. Preserve partial logs rather than treating them as successful.
6. Record the copied EFI hash and capture-file hashes locally.

## Capture 2

1. Perform another **full shutdown**. The second capture must be a separate
   cold boot; a Restart does not count.
2. Select the same USB stick again from the one-time boot menu.
3. Let the same reviewed diagnostic hand off to Windows.
4. Verify a second complete diagnostic log and preserve all raw files locally.

No third boot should be used to compensate for disagreeing captures. If the
two complete captures disagree on fields required to be stable, stop and review
the difference instead of generating a profile.

## Recovery / stop conditions

The diagnostic stage does not modify the internal system ESP or register a
firmware boot entry. If a diagnostic boot does not reach Windows normally:

1. Power the PC off completely.
2. Remove the USB stick.
3. Power on and choose Windows Boot Manager directly if the firmware does not
   choose it automatically.
4. Do **not** retry the diagnostic in the same power session.
5. Preserve any partial removable-media log and report where the run stopped.

If the artifact hash, branch, target identity, selected USB, expected file
layout or recovery assumptions differ from this reviewed procedure, stop before
booting it.

## After both captures

Copy the complete captures into the ignored local research area and run the
offline compatibility/profile analysis. Raw `PROM.BIN`, loaded-image dumps,
device paths and private firmware identifiers remain local and are not committed
or attached to a public PR.

On the current branch, automatic local-profile generation is expected to
remain outside scope for `2216 / 0x2800`; that refusal is a guard, not a failed
capture. Review the two captures first. Any later widening of the hardware-
proven envelope or exact-machine profile is a separate source change with its
own host tests and review.

Only after an exact profile is derived from this machine, its refusal/release/
cleanup simulations pass, and a later state-changing procedure is separately
reviewed should an unlock transaction be considered.
