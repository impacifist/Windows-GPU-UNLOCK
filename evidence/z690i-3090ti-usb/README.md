# ASUS Z690-I / RTX 3090 Ti: USB and internal EFI evidence

Status on 2026-10-26: **successful USB and internal EFI unlock boots**. The USB run used
source `4ecda4a45f3028620a7dac4c4c1407b2b594b34a` and the EFI hashes below.
Matched locked/unlocked BF16 measurements show 1.68× throughput at 8192 square
and 1.83× at 4096 square, with the same 450 W limit.

The owner requested this port after the generic local-profile generator refused
the machine's aggregate console. The existing B660M aggregate release code is
reused without changing its runtime checks. The generic generator still refuses
aggregate consoles; this profile does not broaden that admission policy.

## Captured machine

- ASUS ROG STRIX Z690-I GAMING WIFI, BIOS 4601; i9-12900KS, Intel UHD 770 enabled.
- RTX 3090 Ti, PCI `10de:2203`, subsystem `10de:1618`, VBIOS `94.02.a0.00.01`.
- GPU at `0000:01:00.0`, root port `00:01.0`, BAR0 `76000000–76ffffff`.
- Chip `b72000a1`, device memory `5ff4`, display child ADR `80010340`.
- Windows driver 610.88; Secure Boot disabled at the preceding live check.

The confirmed BIOS 4601 capture is `DIAG01.TXT`, SHA-256
`3eb513670d446b5b33c01c7f2eede9a3c048ab27f54880659fd2927e88e06658`.
Its diagnostic result is zero and capture-complete is one. PROM SHA-256 is
`586baad14e09c28019499c7270aa608b878f8a87c457de13490fa7a855cfb30a`.
The firmware images and raw captures remain outside Git.

AMI's combo driver owns the physical NVIDIA GOP and creates the aggregate GOP.
AMI graphics and text console drivers own the aggregate. The captured binding
versions, image sizes and Stop offsets match the B660M profile. The AMI EDID and
NVIDIA GOP code hashes also match. Hardware identity, BAR0 and the display path
are specific to this new profile and are not borrowed from the B660M machine.

The ordinary reboot used for this capture happened to satisfy the cold checks:
SS0 `00452002`, SS1 `6`, WPR2 zero. That observation does not establish that all
warm restarts will work. The transaction latches and verifies the actual cold
entry; it does not force that recorded SS0 value.

## Verification

`research/profile/test_z690i.py` compiles production code into RAM-only host
fixtures. It authenticates the PROM and all five firmware assets, extracts the
captured ownership graph and driver images, and now runs 12 jobs:

- FWSEC location and the actual PCI configuration: 244 preflight checks.
- Rejection of this capture by the B660M profile.
- 65 captured graph/driver checks, including changed driver code or versions,
  missing and duplicate ownership edges, extra owners, and incomplete release.
- 197 cold precheck cases and 39 full handoff flow cases.
- Port operations, preflight write locks, 29 bus-master containment scenarios,
  transaction fault injection, and automatic headless USB/internal children.

The captured PCI case failed before the new profile was added and passes with
the profile. The existing 12 Python capture/PE parsing tests also pass. These
checks establish the exercised software behavior. The subsequent physical boot
is recorded below; repeated-boot and long-term stability remain untested.

The only shared test changes add this capture's cold line and FWSEC offsets,
and cast `UINTN` counters to `size_t` for portable `%zu` diagnostics.

## Build

On Linux with Clang and LLD capable of targeting Windows x64:

```sh
python3 research/profile/build_z690i.py \
  --capture /path/to/confirmed-capture/DIAG01.TXT \
  --assets /path/to/verified-610.43.03-assets \
  --windows-esp "$WINDOWS_ESP_GUID" \
  --clang clang --lld-link lld-link
```

Set WINDOWS_ESP_GUID to the current Windows system ESP GUID, verified on the
target. Never reuse another machine's partition configuration. The builder runs the host
suite before compiling and writes the same USB manifest consumed by
`research/headless/stage-usb.ps1`. Build commands and test logs are under
`research/profile/build/z690i/`. It performs no staging or firmware changes.

The Windows builders also accept `-HardwareProfile z690i-3090ti`. The write
profile supports headless USB and dedicated internal ESP modes. Building alone
does not install a package or change firmware entries. For internal configuration,
installation and recovery use [the headless guide](../../docs/HEADLESS.en.md).

Clang/LLD 20.1.2 produced these PE-verified USB artifacts with the privately
retained partition configuration. Your EFI hashes will differ with your GUID:

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| Wrapper `HEADLESS.EFI` | 39424 | `e976d28cef44dca9841c66f1ea18504f4955a9d7248083d911fe878b6c52e7dd` |
| Child `UNLOCK-HEADLESS.EFI` | 67584 | `29729569c613e7fed16303a6b7faa3ccd997473a8edbccc59e704855263e0a64` |

Both are x64 EFI applications with no imports, with relocation and NX enabled.
The wrapper pins the child hash and length. Neither executable nor any NVIDIA
firmware asset is committed here.

## First hardware test

The first test completed on 2026-10-26. The wrapper recorded successful driver
release, unlock return and released-state verification before Windows handoff.
The child reported SS0 `88888888`, SS1 `00000008`, one successful FLR,
`quiesced=1 fault=0 retained=0 maps=0`, and final status zero with `clean=1`.
Windows showed both display devices healthy and NVIDIA driver 610.88 working.

Log SHA-256:

- `HLS000000.TXT`: `2650ee1b57321a13063bd61dcde5f95fc59f94cf0e4281d534c346167dd5a062`.
- `AUTO000000/REPORT.TXT`: `2a3ab8558969fc19c60e6c7594f76bbd026ddccdef27ee732375f8089a3a2f95`.

PyTorch 2.12.1+cu130 / CUDA 13.0 dense BF16 inputs with FP32 accumulation/output
measured 142.870 TFLOPS at 8192 square (five-sample range 142.785–144.107), and
132.952 TFLOPS at 4096 square. Each sample timed 500 GEMMs after 200 warmups.
Sixty-four elements per size matched independent CPU FP64 references exactly.
At the unchanged 450 W limit, the 8192 run averaged 448.39 W and reached 67 °C.
The desktop and resident services remained present. This is not a reproduction
of the quoted RTX 3090 / 300 W configuration.

Sanitized samples from all three runs are in [bf16-results.json](bf16-results.json).
The standalone [benchmark](benchmark_bf16.py) preserves the measured tensor
generation, warmup, CUDA-event timing and CPU reference checks; only local fleet
lease enforcement/metadata and the device UUID field were removed. The original
measured script SHA-256 is recorded in the results. Run it with the tested
CUDA-enabled PyTorch environment: `python benchmark_bf16.py new-result.json`.
It refuses to overwrite an existing result or run while GPU utilization exceeds
5%. It changes no driver, power, clocks, firmware or resident services. Coordinate
exclusive GPU use before running. Raw captures and firmware remain private.

## Locked baseline comparison

The owner then cold-booted Windows Boot Manager directly, without the USB unlock.
Windows boot time was `2026-10-26T21:09:22.5000000Z`; no new USB unlock log appeared.
The identical benchmark source, installed PyTorch/CUDA, driver, GPU UUID, power
limit and numerical settings were verified before comparison. Five samples of
500 GEMMs per size and the CPU reference checks completed in both states.

| Square size | Locked TFLOPS | Unlocked TFLOPS | Ratio |
| --- | ---: | ---: | ---: |
| 8192 | 85.183 | 142.870 | 1.677× |
| 4096 | 72.767 | 132.952 | 1.827× |

Available 8192 power samples average 447.49 W locked and 448.39 W unlocked.
The locked telemetry subprocess left a partial last row and no samples for the
4096 interval; do not infer a power or energy figure for that interval. GEMM
timing and correctness results completed. These are one locked boot and one
unlocked boot with the desktop and resident services present, not an application
benchmark or a long-term stability result. See the sanitized samples linked above.

## Repeating the hardware test

Stage with the existing USB script after verifying the current machine and
stick. It preserves the previous `BOOTX64.EFI` under `RTX3090-UNLOCK/backup/`.
Use the firmware's one-time USB boot selection. This package automatically
attempts the unlock after the five-second Esc cancellation window. The display
can stop updating when its firmware drivers are released.

On release or unlock failure, the existing code stops without retrying or
starting Windows. Recover with a full power-off and Windows Boot Manager, then
retrieve the USB logs. A successful host test is not a reason to bypass a
runtime identity, graph, cold-state or cleanup failure. After a successful
hardware boot, verify NVIDIA driver health and measure BF16 against a locked
baseline under the same workload and power limit.

## Internal ESP build extension — 2026-10-26

The named profile now permits the existing dedicated-ESP headless mode.
Runtime release/unlock algorithms and guards are unchanged. The Linux builder
accepts the target's Configure.ps1 output and publishes the same input/child
provenance contract as the PowerShell builder. Manifest entries are compared
by path/hash after ordering normalization, preserving rejection of omitted,
duplicate or modified inputs across the two platforms.

The suite adds an internal-media app replay: the configured GPT partition is
accepted; a different GUID and a USB-only path are refused before GPU effects.
All 12 host jobs pass. PowerShell 7 package tests pass 48 checks, actual installer
integration with mocked disk/BCD/registry passes 18 checks, and automatic-policy
failure/recovery tests pass. The installer fixture now supplies the required
hardware profile and uses the actual Z690-I board/BIOS identity. These tests do
not establish a physical internal boot. The subsequent hardware result follows.

## First internal boot and automatic promotion — 2026-10-26

Source `63e934f` internal wrapper SHA-256
`d1acd8440aa10690a9a1b607d43e423145c049d9cb843644ff84ad19a7105ef4`
and child
`57697b04acf705b96f3cea1b57b638b35186aa55cdf20a6d6004261b646e72cf`
completed a physical boot from the dedicated system-SSD ESP. Windows started
at 21:58:38.5 UTC; fresh wrapper and transaction logs on that ESP report success,
clean resource release and Windows handoff. Both Windows display devices are OK.

HLS000000.TXT SHA-256:
`0fa511e8fb10029ca74000b229d37036af8892424fba293acaa7f21a6430b940`.
AUTO000000/REPORT.TXT SHA-256:
`d3c1850e605dbc45a283c3466da90e70657a90c5ed8087fd26e2db12728c46cb`.
Transaction: status 0, clean 1, quiesced 1, fault 0, retained 0, maps 0, FLR 1;
SS0 `88888888`, SS1 `00000008`.

Same benchmark, Torch 2.12.1+cu130, driver 610.88 and 450 W:
143.228 TFLOPS at 8192² (1.681× locked), 133.639 at 4096² (1.837× locked).
Each size's 64 CPU FP64 reference dots matched with zero error.

The existing installer then enabled automatic order: internal unlock first,
Windows Boot Manager second, other entries retained and Fast Startup off.
Native readback passed. The new default order has not yet been exercised by a
second boot. Lexar still enumerated during Windows inspection; the fresh internal
logs and partition-bound build establish internal execution, not USB removal.
The original build/installer HardwareTested metadata remains false; this dated
report records the later physical result without rewriting build provenance.

## Published scope

The source revisions above identify the original local builds, before the public
snapshot was prepared. No firmware/PROM/image dumps, device UUIDs, partition
GUIDs, installed EFI binaries, local fleet credentials or raw system reports are
included. Physical card branding and cross-flash history were not independently
verified; the PCI/subsystem/VBIOS identities above are observed software identity.
The iGPU was enabled, without a physical preboot GOP; the RTX display path is
pinned by the capture. This is evidence for this exact board/BIOS/card profile,
not general support for all RTX 3090 Ti systems.
