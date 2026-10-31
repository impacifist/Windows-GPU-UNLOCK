# EVGA post-shutdown benchmark

[English](COLD-BOOT-RESULT.en.md) | [한국어](COLD-BOOT-RESULT.ko.md)

Movian reported shutting down and booting Windows normally without the unlock EFI. His posted three-trial medians were FP32 26.411228, FP16 tensor 64.762241, BF16 tensor 64.683243 and TF32 tensor 32.965178 TFLOPS. These are near the original pre-attempt baseline and far below the post-EVGA19 tensor results, strongly suggesting the performance effect did not persist through that shutdown on this machine.

The uploaded result folder passed the offline comparison with the original baseline, including its completion, recorded artifact, runtime, settings and trial checks. Median changes versus baseline were FP32 +0.12%, FP16 +0.36%, BF16 +0.19% and TF32 -0.05%. The same-machine flag was supplied based on the contributor's reported sequence; the shutdown method was not independently verified. Warm restart, sleep and hibernation remain untested. The next attended EFI attempt and matching benchmark are described in [the repeat test](NEXT-REPEAT-UNLOCK.en.md).

Raw ZIP SHA-256: `ae8135268979dc2ccbd4b7ceef3413d6c75634e16104daae28c5afc6bb5e77d4`.
