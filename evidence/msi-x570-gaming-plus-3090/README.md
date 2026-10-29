# RTX 3090 / MSI MPG X570 GAMING PLUS: successful experimental USB unlock

One supervised Windows USB unlock and handoff succeeded. This is an experimental report, not a claim of general compatibility or repeated-boot stability.

- MSI MPG X570 GAMING PLUS (MS-7C37), BIOS A.R1; Ryzen 9 5900X without integrated graphics.
- RTX 3090, 24 GiB, GA102, PCI ID `10de:2204`, subsystem `1b4c:1454`, VBIOS `94.02.59.00.6e`.
- One NVIDIA display controller, one observed GOP, no EDID driver bound; NVIDIA GOP version `0x6000a`.
- Windows 11 Pro build 26200, NVIDIA driver 610.88; Secure Boot disabled.

Two corrected captures from separate full shutdowns passed the offline compatibility checker against the experimental MSI X570 branch at `ff93077`. The pre-step from issue #102 corrected the PCI attribute cache from `0x300` to `0x200` while the PCI command remained `0x0002`; the 1/2/4/8-second checks were stable. A local profile was generated from this machine's captures and all 20 profile host jobs passed. The gated pre-step and generated headless USB build were then booted once. The report recorded transaction status 0, clean containment and FLR, and the wrapper completed its checks before starting Windows; the display and NVIDIA driver recovered.

On the same driver and 296 W limit, a short WSL cuBLAS 4096 × 4096 FP16-input GEMM with FP32 accumulation/output (5 warmups, 10 iterations) measured 58.30 TFLOPS before and 89.77 TFLOPS after, approximately +54%. These are single short runs, not a multi-run stability or application benchmark. A later 281.2 W run measured 110.14 TFLOPS; the different power limit and conditions prevent a matched comparison.

Limits: one supervised USB boot only. Repeated cold boots, default automatic boot, suspend/resume, long-term numerical correctness and real workloads remain unverified. The display path changed between captures; runtime checks may reject some boots. No raw captures, firmware images, serials, device UUIDs or partition IDs are included. Based on the approach in https://github.com/impacifist/Windows-GPU-UNLOCK/issues/102.
