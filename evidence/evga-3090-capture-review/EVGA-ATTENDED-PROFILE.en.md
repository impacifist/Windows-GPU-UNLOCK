# EVGA 3090 attended USB profile

[English](EVGA-ATTENDED-PROFILE.en.md) | [한국어](EVGA-ATTENDED-PROFILE.ko.md)

This experimental exact-machine profile extends the EVGA19 and EVGA20 research builds. The two transactions returned successfully and the measured tensor GEMM gain repeated. This new slot-based build and its rearm process have passed host mocks and PE checks, not an actual firmware run. It is USB-based and attended: each attempt needs a new slot, a rebuild, staging and Shift+G. It is not an automatic internal boot profile and is not qualified for other hardware or firmware configurations.

Each attempt creates a new permanent `EVGA-UNLOCK<slot>.ARMED` marker and records its slot in the capture. Slot 5 requires the exact reviewed EVGA20 capture and marker 4. For slots 6–99, staging additionally requires the previous marker and the latest EVGA log to record the previous slot, two complete captures, successful transaction return, asset cleanup and ready-Windows. A missing, interrupted or failed previous attempt must be investigated; do not delete markers, modify captures or skip slot numbers to bypass the checks. Staging checks recorded events, not actual Windows startup or proof that hardware is safe.

An experimental preparation wrapper can select the next contiguous slot and invoke the same build and staging scripts with one command. It does not reboot or authorize the EFI transaction. Slot selection and `-WhatIf` were tested locally with synthetic markers and mocked storage; the complete wrapper has not yet been tested on a USB or live hardware. `-WhatIf` shows the selected slot without building or staging. Do not use a wrapper refusal as a reason to remove evidence.

Movian subsequently reported the dry run on his USB selected slot 7 and displayed the expected WhatIf operation. This confirms the reported dry-run selection; it does not verify an actual build, staging or boot.

The full wrapper subsequently built and staged slot 7 in a local fixture using captured EVGA22 and verified assets, with mocked storage inventory. It preserved fixture markers and backed up the prior fixture EFI. This does not prove staging on Movian's physical USB or execution on his hardware.

Movian subsequently supplied output showing slot 7 was built and staged on his USB with matching build/staged SHA-256 `20ce56394feefefb6bb249a8bfc0100c27d1d58a46d9c7cd903e70fd3cf70cb4`, with the previous EFI backed up. This is contributor-reported output; the slot-7 EFI boot and hardware behavior remain unverified.

```powershell
./research/profile/prepare-evga-attended.ps1 -Drive D -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:/Program Files/LLVM/bin/clang.exe" -LldLink "C:/Program Files/LLVM/bin/lld-link.exe" -WhatIf
```

**Risks:** This performs GPU/PCI writes, DMA, firmware execution and reset. Display or input may disappear; the machine may hang, fail to boot, or suffer permanent GPU, firmware or data damage. Recovery is not guaranteed; a failed post-release guard holds without automatic rollback. If needed, power off, remove the USB and select normal Windows Boot Manager; power-off may not recover the machine. Proceed only if you accept these risks and can physically recover the machine.

For the first test, update this branch and run each command from the repository root, stopping on error:

```powershell
./research/profile/build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:/Program Files/LLVM/bin/clang.exe" -LldLink "C:/Program Files/LLVM/bin/lld-link.exe" -AttemptUnlock -AttemptSlot 5
./research/profile/stage-evga-text-stop.ps1 -Drive D -AttemptUnlock
```

Save work and fully shut down with `shutdown /s /t 0`. Boot the UEFI USB; when **Live guards passed** appears, press fresh **Shift+G once**. If Windows returns, send the newest `EVGA*.TXT`, the build/stage output, and any observed issues. The prior benchmark binary and settings can verify performance again. Keep all markers and captures; do not retry a stopped attempt. After review, the next slot can be built and staged explicitly; these steps are required again before another attempt.

Local slot-5 build: 86,016 bytes, SHA-256 `410edb27a179ad11bbcb8c9020de6f654b51f298ae57ad3f466d30f3c51e9a48`. Different compilers may yield a different hash; the staging script validates the contributor's build. No firmware has been executed locally.
