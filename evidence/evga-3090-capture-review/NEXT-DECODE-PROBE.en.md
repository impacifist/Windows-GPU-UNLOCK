# EVGA combined decode admission test

[English](NEXT-DECODE-PROBE.en.md) | [한국어](NEXT-DECODE-PROBE.ko.md)

After the reviewed EVGA12 release and saved benchmark, this repeats all eight proven Stop callbacks, verifies parent ownership release and PCI command0, enables MEMORY only (attribute0x200; command2), reads BAR0 chip-ID offset0 twice, requires GA102 b72000a1 both times, disables MEMORY once and checks command0 plus the exact saved attributes twice. Final verification checks command0 and ownership again before Windows. No I/O/BME/VGA enable, PROM access, GPU reset, DMA launch, firmware upload or unlock transaction is included.

New risk: enabling decoding and the first post-release MMIO reads can change hardware state, have side effects or hang. Display/input, firmware or Windows/GPU startup may fail; manual power-off/USB removal may be needed. Recovery is unproven. A failed call/check holds; it does not blindly restore or retry. Successful-path decode disable is conditional cleanup, not automatic failure recovery. Choose whether to run after reading this; no separate acknowledgement required.

Update the existing branch through GitHub Desktop. From the repository root, run separately and stop on any error:

```powershell
.\research\profile\build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:\Program Files\LLVM\bin\clang.exe" -LldLink "C:\Program Files\LLVM\bin\lld-link.exe" -ProbeDecode
.\research\profile\stage-evga-text-stop.ps1 -Drive D
```

Staging requires exact reviewed EVGA12 and earlier captures, preserves backups/old markers, and uses the new `EVGA-DECODE-PROBE.ARMED`. Do not remove any marker or repeat this EFI. Full shutdown, select UEFI USB, then fresh Shift+G once after Live guards passed. No Shift+V. Screen loss is not proof of progress. If Windows has not arrived within3 minutes, power off, remove USB and select normal Windows Boot Manager.

Send EVGA13.TXT (or next numbered log), build/staging output and automatic/manual Windows result. It should record command-before, enable return/command2, two b72000a1 values, disable return and restored command0, complete captures and ready-Windows. We check the log here. Keep the benchmark binary/settings; this admission test itself should not unlock, so it does not need a new performance comparison yet.

Remaining unlock evidence includes validated firmware assets, aggregate-topology integration with the unlock checks/runtime, exact post-release admission and recovery behavior. A passing chip read does not establish all register/PROM/DMA prerequisites. Those must be resolved before supplying an actual unlock transaction. This build deliberately remains CanUnlock=false.

Local validation: existing107 host cases plus8 decode-probe checkpoints pass using mocks; EFI warnings-as-errors and PE checks pass. No captured firmware or real GPU operations were executed locally. Contributor hardware behavior remains untested.
