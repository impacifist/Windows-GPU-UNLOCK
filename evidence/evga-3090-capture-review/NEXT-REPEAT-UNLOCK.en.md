# EVGA cold-boot and repeat unlock test

[English](NEXT-REPEAT-UNLOCK.en.md) | [한국어](NEXT-REPEAT-UNLOCK.ko.md)

EVGA19 records a successful transaction and Windows return, with increased measured tensor throughput. This build repeats the same guarded transaction once using a new durable `EVGA-UNLOCK4.ARMED` marker. It preserves and requires the previous markers and exact EVGA19 capture. It is a second attended attempt, not yet an automatic or repeatedly reusable launcher. Never delete markers or retry after a stop.

First, fully shut down and boot Windows normally without the USB EFI. Run the same benchmark and preserve its results to measure the cold-boot state. Then prepare and stage the new EFI, fully shut down again, boot the UEFI USB, wait for live guards and press fresh Shift+G once. After direct Windows return, run the same benchmark again and send both result folders and the newest EVGA log. A cold-boot baseline and post-transaction result are observations, not proof of restart or sleep behavior.

For each full shutdown, save work first, then run `shutdown /s /t 0` from Windows. This shuts down immediately and avoids a Fast Startup hybrid shutdown.

The transaction uses GPU/PCI writes, DMA, firmware execution and FLR. Display/input can disappear, the machine can hang or fail to boot, and GPU, firmware or data damage may be permanent. Recovery is unproven; post-release guard failure holds without automatic rollback. If Windows does not start, manual power-off, USB removal and selection of normal Windows Boot Manager may be necessary; power-off is not guaranteed to recover the system. Proceed only if you accept these risks.

From the repository root after updating the branch, run separately and stop on any error:

```powershell
./research/profile/build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:/Program Files/LLVM/bin/clang.exe" -LldLink "C:/Program Files/LLVM/bin/lld-link.exe" -AttemptUnlock
./research/profile/stage-evga-text-stop.ps1 -Drive D -AttemptUnlock
```

Benchmark in each Windows session using the existing executable and settings:

```powershell
powershell -NoProfile -File ./benchmark/run-benchmark.ps1 -Card 3090
```

The local EFI and mock tests do not prove live repeatability. Its SHA-256 is `5ff53759152b7d5705c02a865e4315091ed250edf4f8cd86685ef9b8ce0ebb73`; contributor builds may differ and staging verifies its own build hash.
