# One-block EVGA 3090 unlock attempt

[English](NEXT-UNLOCK-ATTEMPT.en.md) | [한국어](NEXT-UNLOCK-ATTEMPT.ko.md)

**This is the first actual unlock attempt on this card, not another diagnostic.** After the earlier exact-board checks and a fresh Shift+G, it releases the captured console/GPU driver chain, claims PCI ownership, runs GPU MMIO and PCI writes, enables bus mastering, maps DMA buffers, uploads and executes firmware, writes selectors and issues one PCIe function-level reset. It then attempts containment, cleanup and direct Windows boot; run the benchmark below manually only after success. It may stop at a failed guard before reaching the transaction.

**Risk:** the GPU or machine may hang, lose display/input, fail to boot Windows or fail to initialize the card. Firmware, GPU state or data could be damaged, potentially permanently; recovery is not guaranteed. The test has no automatic rollback after a post-release failure and holds rather than continuing to Windows when cleanup is uncertain. Interrupting an active transaction can also cause harm. Run only if you accept those risks and can physically power down, remove the USB and choose normal Windows Boot Manager. A power-off may itself be required and is not guaranteed to recover the system.

EVGA16 records five verified EFI assets, shared exclusive claim/close, original PROM hash, restored decoding and ready-Windows. This attempt holds all five assets and the exclusive claim through the transaction. It repeats the prior state/PROM check, then reads the same 1MiB PROM twice more using the previously observed DWORD width: up to 786,432 PROM DWORD accesses across the three passes, plus other runtime register accesses. Secure Boot must report disabled; no Secure Boot setting is changed by the script. One fresh G is the only transaction authorization; no post-release input is required.

Update the branch in GitHub Desktop. From the repository root, run these separately and stop if either fails:

```powershell
./research/profile/build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:/Program Files/LLVM/bin/clang.exe" -LldLink "C:/Program Files/LLVM/bin/lld-link.exe" -AttemptUnlock
./research/profile/stage-evga-text-stop.ps1 -Drive D -AttemptUnlock
```

Staging requires the reviewed EVGA16 and prior captures. It copies/verifies the five contributor-generated local assets and their NVIDIA-LICENSE/NOTICE.md to the USB, backs up the previous EFI and preserves old logs/markers. Existing different assets are refused. The new EVGA-UNLOCK.ARMED marker is durable and one-shot; do not delete it or repeat the attempt. No firmware binaries are committed or uploaded.

Fully shut down, select the UEFI USB from the boot menu, and wait for the firmware asset check and **Live guards passed**. Press **fresh Shift+G once**. The screen may stop updating; a blinking cursor is not a progress indicator. Allow time for firmware and Windows startup. If the machine remains unresponsive and Windows does not start, manual power-off and USB removal may be necessary; choose the normal Windows Boot Manager. Do not rerun. Send the newest EVGA log and any failure or recovery details. Under the agreed convention normal Windows arrival is assumed unless a failure is reported.

If Windows returns and the GPU works, run the existing benchmark **only if the log confirms the transaction returned success**, using the same executable and settings as the baseline:

```powershell
$log = Get-ChildItem D:/RTX3090-UNLOCK -Filter 'EVGA*.TXT' | Sort-Object Name | Select-Object -Last 1
if (!$log -or !(Select-String -LiteralPath $log.FullName -SimpleMatch 'evga-unlock-transaction-return status=0x0000000000000000' -Quiet)) { throw 'No successful unlock transaction recorded. Send the newest EVGA log before running the benchmark.' }
powershell -NoProfile -File ./benchmark/run-benchmark.ps1 -Card 3090
```

Send the newest EVGA log and the resulting timestamped benchmark ZIP. Compare against the captured locked-state medians: FP32 26.379272, FP16 64.527754, BF16 64.558792, TF32 32.982266 TFLOPS. A successful firmware return is not itself proof of a performance gain; the comparison and runtime correctness are required.

Local verification: 28 real vendor-core/compatibility/port scenarios using the captured EVGA PROM/config and verified assets, with PCI/MMIO/DMA/firmware execution simulated; 19 adapter scenarios; 25 admission cases; retained-asset and combined wrapper failure/cleanup mocks; USB staging fixture and PE checks. Real hardware transaction and recovery remain unverified. Locally built EFI 85,504 bytes SHA256 77ff9d20f3fb0552933b31a776785f0dd7b80ff75c44dd01aaa84ca814d6986e. A contributor compiler may produce a different hash; staging checks that build's source and manifest pins.
# Second attempt after EVGA17

**Third attempt update:** EVGA18 records admission failure with transaction-local SS0 0x12053015, different again. The next build uses an entry-latched SS0 policy: it rejects inaccessible and unlocked sentinel values and requires the latched selector to remain unchanged before the write. This accepts more states than the previous allowlist, and the meaning of the changing bits is unknown. It may reach GPU writes, DMA, firmware and reset; live safety and recovery are unproven. Keep both prior markers and EVGA17/EVGA18. The new marker is EVGA-UNLOCK3.ARMED; do not retry or delete it.

EVGA17 returned ACCESS_DENIED after exclusive PCI acquisition. The earlier stable selector was 0x30053015, while admission required 0x30053014; the later transaction read was not logged, so the exact refusal remains unconfirmed. This build accepts only those two observed selector values and still requires the entry value to remain unchanged. Their hardware equivalence and successful recovery are unproven. It records the transaction-local cold state and admission outcome.

Staging requires the original EVGA-UNLOCK.ARMED and the exact reviewed EVGA17.TXT. Preserve both. The new one-shot marker is EVGA-UNLOCK2.ARMED; do not delete it or repeat. The risk and boot instructions below still apply. The first attempt did not prove an unlock.
