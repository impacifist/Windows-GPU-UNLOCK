# Combined EFI asset and exclusive-ownership qualification

[English](NEXT-RESOURCES-PROBE.en.md) | [한국어](NEXT-RESOURCES-PROBE.ko.md)

EVGA15 passes shared runtime PCI/configuration and BAR0 descriptor admission. Windows arrival is assumed unless a failure is reported. This next build combines two remaining integration checks in one boot, retaining the existing state/PROM/decode-off sequence.

Before any Stop and before the fresh Shift+G prompt, load each of the five exact assets from RTX3090-UNLOCK/ASSETS into CPU EfiLoaderData pool memory using the same new shared reader as GuLoadAssets. Check file metadata/size, chunked reads, close and SHA256, then free each buffer before the next. Peak asset buffer is84277400bytes; total file data84427316bytes. Buffers are temporary and are not uploaded or executed. Wait while this file check runs; only press Shift+G when Live guards passed appears. A pre-Stop refusal can take the existing Windows fallback.

After the eight proven release callbacks, memory-only enable and full PCI/BAR checks, use the existing GuClaimController acquisition implementation at TPL_NOTIFY. It rejects non-passive protocol opens and requests exclusive PCI I/O ownership for this EFI image. Close that exact open even if logging or interface validation fails; a failed Close holds. Recheck the existing released topology, then read the proven state pairs/PROM hash, disable memory and attempt Windows. Snapshot/log checks must succeed. The shared ownership diagnostic strings are represented by file-log events in this harness. The production ownership helpers remain unchanged unless the claim-only compilation flag is selected.

No DMA mapping/launch, BME/VGA/I/O enable, MMIO writes, GPU reset, selector write, firmware upload/execution or unlock. The write/reset/vendor code is not linked. CanUnlock=false. This tests transient exclusive acquisition/release; it does not qualify long-lived ownership during a GPU transaction, runtime DMA buffers or unlock recovery.

Risk: new EFI file reads/pool allocation and exclusive protocol calls may fail or hang. Exclusive acquisition can interact with firmware driver management; unexpected behavior could lose display/input or prevent Windows/GPU startup. Recovery is unproven; manual power-off/USB removal may be needed. After release, a failed check holds without blind rollback/retry. Decide whether to run; no separate acknowledgment required.

Update this branch in GitHub Desktop, then run separately from the repository root and stop on errors:

```powershell
./research/profile/build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:/Program Files/LLVM/bin/clang.exe" -LldLink "C:/Program Files/LLVM/bin/lld-link.exe" -ProbeResources
./research/profile/stage-evga-text-stop.ps1 -Drive D
```

Staging verifies all five local asset sizes/hashes, copies them plus NVIDIA-LICENSE/NOTICE.md into USB RTX3090-UNLOCK/ASSETS, verifies copied hashes, and refuses any differing existing file. Assets are not distributed in Git. Exact EVGA15/prior logs are required; old captures/markers/backups stay. New EVGA-RESOURCES-PROBE.ARMED is one-shot. Preserve the benchmark executable/settings.

Full shutdown → UEFI USB → wait for asset verification and Live guards passed → fresh Shift+G once. No Shift+V. If Windows does not start within3minutes after G, power off/remove USB/use normal Windows Boot Manager. Do not repeat/delete markers. Send EVGA16/next log and build/staging output; report any Windows-start failure or manual recovery. Silence about failure is an assumption of normal arrival, not explicit confirmation.

Local validation:126 prior mocked checkpoints,13 new asset/ownership scenarios and6 combined-sequence scenarios passed and6 real-SHA256 reader cases (151 total). New resource hashes/callbacks are mocked, not firmware execution. Shared app_io.c compiles with warnings-as-errors. A temporary-directory staging fixture with mocked storage inventory passed WhatIf no-write, exact EFI/asset copies, backup, old-marker preservation and spent-marker refusal; no real USB was accessed. Warnings-as-errors/PE build passes:37376bytes SHA256992c563ea3865be3df1fba97db6704b168426c496936cf2f61dbc142250a8230. Contributor compiler may differ; staging pins its manifest. Built only, not staged or executed on local hardware.
