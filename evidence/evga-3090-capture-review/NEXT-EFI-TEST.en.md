# Next attended test: one text-console membership Stop

[English](NEXT-EFI-TEST.en.md) | [한국어](NEXT-EFI-TEST.ko.md)

Status: implemented and compiled, hardware untested, not staged. This is the first EVGA3090 lifecycle test, not an unlock attempt. Read the risk below and choose whether to stage/run it; no separate confirmation reply is required.

## Concrete scope and risk

After a before-capture and fresh uppercase G, the EFI will call only captured IMG13's `Stop(binding, virtualGop, 1, &systemConsoleOut)` once. It identifies the two GOPs and exact ownership links live, checks the single observed EVGA3090 PCI display identity/subsystem, and pins all three console Stop addresses, image sizes, versions and executable-section hashes. The captured IMG13 child slot at RVA 0x6190 matches the captured ConsoleOut handle, consistent with the proposed argument. It creates and flushes EVGA-TEXT-STOP.ARMED before calling; an existing marker prevents another attempt. It neither removes the marker nor retries automatically.

It then checks that IMG13's two selected text-consumer opens are gone and the physical/virtual GOP handles, remaining GOP owners and code pins are unchanged. It captures the graph again, closes files, verifies the selected graph once more immediately before Windows StartImage, and attempts the already-loaded Windows Boot Manager. A successful Stop return without the expected edge changes fails.

This changes live firmware console membership. The reviewed Stop path may free shared console state or run helper callbacks. Display/input may disappear, firmware may hang, or Windows may fail to start or initialize the GPU. Manual power-off and USB removal may be required. Recovery is unproven; there is no automatic restoration. After a Stop starts, a failed guard holds. A logged refusal before any Stop now attempts normal Windows if file closure succeeds. A marker means an attempt was armed, not proof the Stop call completed.

No Stop of IMG16, IMG17, EDID or NVIDIA; no PCI-attribute write, GPU MMIO, reset, DMA launch, firmware upload/execution, unlock or NVRAM write is added. PCI configuration is read for inventory. The tested consumer callback can itself have side effects, so those exclusions do not imply absence of all indirect hardware effects.

## Compile only

On the updated PR96 branch, run from the repository root. Use the Windows ESP GUID already observed on your own PC:

```powershell
.\research\profile\build-evga-text-stop.ps1 -WindowsEspGuid '<your Windows ESP GUID>' -Clang 'C:\Program Files\LLVM\bin\clang.exe' -LldLink 'C:\Program Files\LLVM\bin\lld-link.exe'
```

This writes only the local build folder. It does not copy to USB, change boot configuration or execute EFI. The generated configuration is isolated under `internal-boot/boot/build/evga-text-stop/`; other local platform configuration is untouched. If the build succeeds and you choose to run the test with the stated risk, stage it with the command below. Stop on errors; do not reset local work, delete markers or force a profile.

## Stage and run when you choose

With the existing capture USB still D:, run:

```powershell
.\research\profile\stage-evga-text-stop.ps1 -Drive D
```

This verifies the build/source hashes and this PC's ESP pin, requires a FAT32 USB data volume with the original capture pair, refuses an existing attempt marker, backs up the current EFI and copies the new one with hash verification. It preserves captures, never formats or changes boot settings, and does not reboot. If the USB letter changed, use its actual letter. Read any refusal rather than bypassing it.

Save work, fully shut down, and select the UEFI USB. Wait for `Live guards passed. NOW press fresh Shift+G once`; then press Shift+G once. No Shift+V is needed: passing checks automatically attempt Windows. After G, the screen may stop updating. If Windows has not started within three minutes, power off, remove the USB and select normal Windows Boot Manager. Do not repeat or delete the marker; preserve `RTX3090-UNLOCK/EVGA00.TXT` (or next free EVGAnn name) and EVGA-TEXT-STOP.ARMED. Report whether Windows arrived directly or required manual recovery, plus any display/input change.

## Validation completed locally

- 18 host checkpoints passed with mock EFI services, mock hashes and a mock Stop. They exercise success, wrong inventory/code/version/method/owners/console, armed marker, pre-call logging failure, callback failure, unchanged post-state, cancellation, timeout, short marker write/flush failure and final graph drift. No captured firmware was executed by these host checks.
- Existing diagnostic handoff still compiles with warnings as errors; all 78 Python tests pass.
- New EFI builds with warnings as errors; PE verification reports AMD64 EFI application, no imports/IAT/delay imports, NX and dynamic base. Local build: 18,944 bytes; SHA256 e25e20da76331b1246db96d6e5141b6c934befb5f12cf383876c53853fb4d15b with the contributor ESP pin and local toolchain. A different toolchain can produce another hash; record the actual build.

Passing this test would establish only this exact membership removal and observed handoff on this machine. It would not qualify later consumers or prove unlock.

Corrected build and first-run evidence: [FIRST-EFI-RESULT](FIRST-EFI-RESULT.en.md). The earlier artifact details above describe the initial build.

Latest pre-call refusal and phase-diagnostic build: [SECOND-EFI-RESULT](SECOND-EFI-RESULT.en.md).

[Related reports and evidence limits](RELATED-ISSUES.en.md)
