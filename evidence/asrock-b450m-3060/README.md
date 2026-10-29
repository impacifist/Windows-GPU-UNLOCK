# ASRock B450M Pro4-F / RTX 3060: selected public evidence

Observed 2026-10-27; publication summary prepared 2026-10-28.

This is an operator-observed exact-machine experiment, not upstream certification.
The source base is `20c44fecac13cdf601b6a9357a46d8fea29f2678` plus the local
ASRock modifications described in [the port report](../../docs/ASROCK-B450M-RTX3060.en.md).
Raw captures, PROM, driver dumps, private host evidence and partition-specific
EFI executables are not published.

## Hardware transaction

One successful supervised USB unlock was recorded. Selected report fields:

```text
pre-FLR selectors: ss0=0x88888888 expected=0x88888888 ss1=0x8 expected=0x8
transaction result=0 containment=0 started=1 quiesced=1 fault=0 retained=0 maps=0 FLR=1
FINAL status=0 stage=transaction attempted=1 driver_release=0 clean=1
```

The wrapper recorded successful original-unlock return, verification and Windows
handoff. The operator returned to Windows, the NVIDIA driver responded and the
CUDA benchmark completed. The child report's `driver_release=0` field is quoted
unchanged: wrapper-level driver release is a separate stage, covered by its own log.
These excerpts do not establish repeated-boot reliability or long-term stability.

## Matched PyTorch GEMM

Protocol: PyTorch 2.11.0+cu128, CUDA 12.8, Windows driver 617.42, RTX 3060 12 GB,
170 W power limit, dense 8192 × 8192 matrices, 20 warm-up operations per mode,
five timed samples of ten operations each, CUDA-event timing and medians.
FP16 full accumulation and reduced-precision FP16/BF16 reductions were disabled.
The result tensor retains the input dtype. A 16 × 16 output sample was checked
against a double-precision calculation using dtype-specific tolerances.

Primary comparison uses the first saved unlocked run and first successful saved
locked run. The labels are operator-supplied, not runtime register measurements;
the unlocked label is additionally supported by the boot reports above.
Both result files have the same benchmark script SHA-256 and recorded settings.

| Mode | Locked TFLOPS | Unlocked TFLOPS | Throughput increase | Time reduction |
|---|---:|---:|---:|---:|
| FP32 | 10.21 | 10.23 | 0.1% | 0.1% |
| FP16 | 28.76 | 49.95 | 73.7% | 42.4% |
| BF16 | 28.87 | 51.78 | 79.4% | 44.3% |
| TF32 | 14.43 | 25.38 | 75.9% | 43.1% |

The unlocked run recorded approximately 63–64 °C after each mode, compared with
54–56 °C in the primary locked run. FP16/BF16/TF32 reached roughly 169–170 W
unlocked versus approximately 149–160 W locked; unlocked clocks were lower.
Temperatures and starting conditions were not identical, and no formal statistical
significance or long-duration thermal comparison was performed. The five samples
are repeats inside a run, not five independent boots.

`benchmark-locked-2.json` preserves the additional saved locked run rather than
silently discarding it. It is not substituted into the primary published comparison.
`summary.json` contains the exact primary ratios and selected host/build provenance.

## Limits

The measurements support a large improvement in this dense GEMM workload on this
machine. They do not demonstrate equivalent gains in games, image generation,
quantized single-user LLM decoding or arbitrary applications. Short Ollama greeting
measurements were not controlled enough to publish as performance evidence.
Sleep, hibernation, Windows restart, recovery after arbitrary failures and other
cards/BIOS versions remain unverified.
