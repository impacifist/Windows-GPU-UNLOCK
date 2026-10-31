# Combined EVGA runtime PCI/BAR probe

[English](NEXT-PLATFORM-PROBE.en.md) | [한국어](NEXT-PLATFORM-PROBE.ko.md)

EVGA14 returned normally to Windows, confirmed by the operator at 01:16. At 01:20 the contributor reported all five firmware assets matching the pinned sizes/hashes. This is manifest output supplied by the contributor, not independent inspection of their binaries. Preparation accessed no GPU. Preserve private assets and the existing benchmark binary/settings.

The next EFI repeats the eight proven release callbacks and memory-only decoding. Before EVGA14's state/PROM reads, it reads all 256 PCI configuration bytes and invokes the shared runtime validator with exact EVGA identity: capability-chain bounds, PM D0, disabled MSI/MSI-X, PCIe FLR support/no active FLR, command/BME and ROM-decode state. The original compile-time wrapper retains its machine pins. Additional checks pin revision 030000a1, BAR0 84000000 and c8=00050011. GetBarAttributes checks BAR0 base 0x84000000/length 0x1000000 and descriptor shape, then frees the descriptor.

It then combines the proven two chip reads, eight-register pairs and one full 1MiB PROM hash, followed by the existing single memory-disable, restored command/attributes, captures and Windows handoff. No additional MMIO addresses/volume: 262162 explicit reads. No DMA mapping/launch, BME/VGA/I/O enable, GPU reset, firmware upload/execution or unlock. CanUnlock=false. Full runtime ownership/buffer integration and entry-latched selector policy remain unqualified.

Risk: expanded PCI reads and the firmware BAR query are new in this released state. Firmware may fail/hang; release/MMIO may lose display/input or prevent Windows/GPU startup. Recovery is unproven; manual power-off/USB removal may be needed. A failed check holds without blind recovery/retry. Decide whether to run; no separate acknowledgment required.

Update this branch in GitHub Desktop. Run each command separately from the repository root; stop on errors:

```powershell
./research/profile/build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:/Program Files/LLVM/bin/clang.exe" -LldLink "C:/Program Files/LLVM/bin/lld-link.exe" -ProbePlatform
./research/profile/stage-evga-text-stop.ps1 -Drive D
```

Staging pins reviewed EVGA14/prior evidence, preserves backups/old markers and uses EVGA-PLATFORM-PROBE.ARMED. Full shutdown → UEFI USB → fresh Shift+G once after Live guards passed. No Shift+V. If Windows has not started within three minutes, power off, remove USB and choose normal Windows Boot Manager. Do not repeat/delete marker. Send EVGA15/next log, build/staging output and automatic/manual Windows outcome. Do not upload firmware binaries.

Local validation:126 mock host checkpoints passed, plus the existing shared validator's synthetic identity/power/capability/path cases in an adapted freestanding host build. Original CRT/capture-fixture suite not run: installed VS launcher lacks vcvarsall.bat. Warnings-as-errors/PE verification passed. EFI33792bytes SHA2560c2731e88d3da60f1acedabb71c8024a93d078f4132c13b805ee50dae5794814. Contributor compiler may differ; staging pins its manifest. Built only, not staged or executed locally.
