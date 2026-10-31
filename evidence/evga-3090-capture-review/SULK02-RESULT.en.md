# SULK02 result and next step

[한국어](SULK02-RESULT.ko.md)

The uploaded SULK02 log records slot 9, successful transaction return, asset release, close, Windows-ready and two complete captures. Movian reports normal Windows boot and the expected confirmation screen, and explicitly confirms USB was absent, Shift+G was used and no reboot occurred before the benchmark. [Sanitized checks and hashes](sulk02-result.json).

The completed benchmark passes the offline completion, protocol, timing and identity checks. Its recorded artifact/runtime/settings context equals the baseline. [Sanitized benchmark review](sulk02-benchmark.json).

| Mode | Baseline TFLOPS | SULK02 TFLOPS | Change |
| --- | ---: | ---: | ---: |
| FP32 pedantic | 26.379272 | 26.489006 | +0.42% |
| FP16 tensor | 64.527754 | 123.248599 | +91.00% |
| BF16 tensor | 64.558792 | 123.361880 | +91.08% |
| TF32 tensor | 32.982266 | 64.159053 | +94.53% |

This supports attended USB-free unlock on the reviewed machine. Session timing and USB absence are operator-reported, not independent attestation. Manual tuning was not freshly reconfirmed; the recorded settings match. These GEMM results do not establish all-workload gains, complete numerical correctness or long-term stability. Cleanup is now complete; reusable packaging and the final ordinary-restart confirmation remain separate work.

Collection initially refused cleanup after generic fallback entries 000E/000F/0010 disappeared. The four primary/current entries remained equal to backup and the cause is unknown. The targeted cleanup subsequently completed on the contributor machine: Boot7E00 absent, BootNext clear, one deletion, no BootOrder write. The original backup and all evidence were retained. Do not rerun cleanup or EFI.

**Next: [one normal restart confirmation](FINAL-NORMAL-BOOT.en.md).** This runs the same read-only firmware check before and after an ordinary restart; it does not launch the unlock. The script records only a local Windows-boot-time baseline and performs no firmware writes. Local simulations passed in PowerShell 7 and Windows PowerShell 5.1; the real post-cleanup normal restart remains pending.

**Final outcome:** the post-cleanup ordinary restart passed through Boot0000 with the temporary entry absent and BootNext clear. The attended evidence sequence is complete. [Current publication scope](PUBLICATION-STATUS.en.md); no more test commands are needed for this sequence.
