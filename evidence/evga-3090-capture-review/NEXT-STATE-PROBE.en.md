# Combined EVGA cold-state and PROM evidence

[English](NEXT-STATE-PROBE.en.md) | [한국어](NEXT-STATE-PROBE.ko.md)

Repeats EVGA13's release, memory-only enable, two chip reads and conditional disable/Windows handoff. While memory decoding is enabled, adds two batches of eight BAR0 DWORD reads: GFW PLM0x118128, BOOT0x118234, CPUCTL0x110100, WPR2_HI0x1fa828, feature PLM0x823804, SS0 0x82381c, SS1 0x823820, memory size0x1183a4. These are the values used by unlock/app.c ColdCheck. This mode only records them, rejects all-ones/BADF and unstable pairs; it does not claim they satisfy the full unlock policy.

Then reads the fixed1MiB PROM window0x300000..0x3ffffc in one forward pass, 262144 DWORD calls, and hashes the CPU buffer against original PROM SHA256 f1c7c6b205d9a5c0626af4f82e28cc377aa16cde8c25c61c77b4b5160b26e71f. No second full read or new ROM file. Total explicit probe MMIO reads262162:2chip+16state+262144PROM. Existing captures remain part of the sequence. No GPU MMIO writes, BME/VGA/I/O enable, reset, DMA launch, firmware upload/execution or unlock. CanUnlock=false.

New risk: these new MMIO addresses and expanded PROM exposure may have side effects or hang. Display/input, firmware or Windows/GPU startup may fail; power-off/USB removal may be needed, recovery unproven. Any error, sentinel, unstable pair or hash mismatch holds without blind decode-off recovery/retry. The success path disables memory and verifies restored command/attributes as before. Choose whether to run; no separate acknowledgement required.

Update current branch in GitHub Desktop, then run each command separately:

```powershell
./research/profile/build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:/Program Files/LLVM/bin/clang.exe" -LldLink "C:/Program Files/LLVM/bin/lld-link.exe" -ProbeState
./research/profile/stage-evga-text-stop.ps1 -Drive D
```

Stage pins exact EVGA13 and earlier evidence, uses new EVGA-STATE-PROBE.ARMED and preserves backups/old markers. Stop on errors. Full shutdown→UEFI USB→fresh Shift+G once after Live guards passed. No Shift+V. Screen loss is not a progress signal. If no Windows within3minutes, power off/removeUSB/select normal Windows Boot Manager. Do not repeat/delete marker. Send EVGA14/next log, build/staging output and whether Windows returned automatically or manually. Also confirm EVGA13's Windows outcome, not stated in its attachment message.

Expected evidence: each pair recorded/stable, PROM-hash success, memory-disable success, restored-command checks, two complete captures and ready-Windows. We analyse uploads locally. Preserve benchmark executable/settings; no new performance run needed for this non-unlock test. Firmware asset verification and aggregate unlock-runtime integration remain separate required work.

Local validation:121 mocked host checkpoints (115prior+6new), warnings-as-errors/PE build pass. EFI31744bytes SHA25659fde4e3e0d09c0115e12140d7b64c5ba05e838cb03cba920f17bb000e2e3ed2. No real GPU operation or captured firmware executed locally; built only, not staged. The contributor compiler may produce a different hash; staging must match that build's manifest.
