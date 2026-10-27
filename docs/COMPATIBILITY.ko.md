# 검증한 하드웨어와 경로별 조건

[English](COMPATIBILITY.en.md) | **한국어**

현재 소스에는 실기기 결과가 기록된 정확한 장비 프로필 여섯 개가 있습니다.
내부 headless 세 구성과 headless USB 세 구성입니다. 최초 B650M은 유지관리자가
시험했고, B660M·X870·B660-PLUS의 근거는 기여자의 보고입니다
([PR #5](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/5),
[PR #10](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/10),
[PR #24](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/24),
[PR #30](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/30)).
B760M / RTX 3080 LHR 결과는
[issue #28](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/28)에 기록되어 있습니다.
빌드는 [internal-boot/unlock/machine-profile.h](../internal-boot/unlock/machine-profile.h)에서
빌드 시점에 선택된 하나의 검토된 프로필 전용이며, 다른 GPU·플랫폼은 지원하지 않습니다.

| 빌드 프로필 | 정확한 플랫폼 / CPU | 기록된 headless 경로 |
|---|---|---|
| `b650m-3090fe` | ASUS TUF GAMING B650M-PLUS WIFI BIOS 3057 / Ryzen 7 7800X3D / RTX 3090 FE | 내부; 최초 유지관리자 시험 |
| `b660m-3070ti` | ASUS TUF GAMING B660M-E D4 BIOS 4003 / iGPU 없는 i7-12700F / MSI RTX 3070 Ti | 내부; 기여자 보고 |
| `x870-3090ti` | Gigabyte X870 AORUS ELITE WIFI7 BIOS F13d / Ryzen 7 9800X3D / ASUS RTX 3090 Ti | USB 전용; 기여자 보고 |
| `b660plus-3060` | ASUS PRIME B660-PLUS D4 BIOS 3010 / iGPU 없는 i5-12600KF / GA104 RTX 3060 12GB | USB 전용, 디스플레이 child 두 개, CSM 끔; 기여자 보고 |
| `h610m-3060` | Gigabyte H610M H BIOS FC / i5-14400(iGPU 켜짐, GOP 없음) / Gigabyte GA106 RTX 3060 12GB | USB 전용, 디스플레이 child 두 개; 기여자 보고 |
| `b760m-3080lhr` | Maxsun MS-Terminator B760M D4 BIOS H7.8G / iGPU 없는 i5-12490F / Maxsun RTX 3080 10GB LHR | 내부; 기여자 보고(issue #28) |

최신 태그 v0.3.1은 소스 전용 사전 릴리스이며, 현재 소스에는 그 이후의 USB 경로와
X870 프로필 등이 포함됩니다. 실기기 보고는 각 기록의 빌드에 해당합니다. 현재 cold
사전검사 보강은 host·빌드 검증을 거쳤지만 새 실기기 부팅·복구 결과는 없습니다.
프로필이 컴파일된다는 사실만으로 실기기 검증이 추가되지는 않습니다.

## 프로필 1: ASUS TUF B650M / RTX 3090 FE(최초 검증)

| 공통 GPU·플랫폼 항목 | 검증한 값 |
|---|---|
| 카드 | NVIDIA RTX 3090 Founders Edition, 24 GiB |
| PCI 신원 | `10de:2204`, subsystem `10de:1454` |
| GPU BDF | `0000:01:00.0` |
| Chip / VRAM 레지스터 | `0xb72000a1` / `0x6000` MiB |
| BAR0 | `0xf5000000`, 16 MiB |
| PROM SHA256 | `8ea297373e9e5d3600ff41ad9fae88f2d14c98c017d100669f7333981da673b4` |
| 메인보드 / BIOS | ASUS TUF GAMING B650M-PLUS WIFI / 3057 |
| CPU | AMD Ryzen 7 7800X3D |
| 기록한 Windows / 드라이버 | Windows 11 Home 26200 / NVIDIA 617.14 |
| UEFI / Secure Boot | x64 / 꺼짐 |
| Selector 프로필 | `entryLatchedSs1Full`; SS0 목표 `0x88888888`, SS1 목표 `0x8` |

이 표는 성공한 값의 기록이며 실행 허용 목록이 아닙니다. 코드에는 configuration·
capability·PROM·cold 상태·드라이버 코드·소유권·DMA 정리·실패 차단 검사가 더 있습니다.
내부 EFI 빌드에는 자신의 Windows ESP·별도 ESP GUID 설정도 들어갑니다.

| 경로 | 화면 정책과 검증 |
|---|---|
| 기존 USB / 일반 내부 빌드 | AMD 물리 GOP 보존, PCI `1002:164e`, subsystem `1043:8877`, BDF `0000:0c:00.0`와 검증한 출력 ancestry 필요 |
| 내부 headless 프로필 | 해제 전 RTX 물리 GOP 1개, 이후 물리 GOP 0개. 콘솔·입력 인터페이스와 보호 ownership graph 유지, child 파일 전용 로깅 |
| Headless USB (`GU_USB_MEDIA`) | 이 프로필로 빌드할 수 있지만 기록된 B650M headless 시험은 내부 경로. 기여자의 USB 실기기 근거는 아래 프로필 3에 해당 |

Headless 시험에서는 모든 모니터를 RTX에 연결했고 활성 AMD GOP가 없었습니다.
7800X3D 자체에는 iGPU가 있으며, GOP가 없다는 사실만으로 BIOS에서 iGPU를 비활성화했다고
단정하지 않습니다. 이 B650M 시험으로 모든 펌웨어의 iGPU 구현과 독립적으로
동작한다고 주장하지 않습니다. iGPU 없는 Intel 호스트의 별도 사례는 프로필 2·6입니다.

## 프로필 2: ASUS TUF B660M-E D4 / RTX 3070 Ti(aggregate console)

[PR #5](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/5)에서 기여자가
내부 headless 성공을 보고한 구성입니다. iGPU 없는 Intel 호스트에서 NVIDIA가
콘솔 GOP를 소유하고 AMI GOP combo splitter가 그 위의 시스템 콘솔을 제공합니다.
Wrapper가 nvgop·EDID·splitter 소유권을 해제하고 한 번 언락한 뒤 리셋 없이
Windows Boot Manager를 시작하는 경로입니다.

| 공통 GPU·플랫폼 항목 | 기록한 값 |
|---|---|
| 카드 | MSI RTX 3070 Ti, 8 GiB(GA104) |
| PCI 신원 | `10de:2482`, subsystem `1462:5052` |
| GPU BDF | `0000:01:00.0` |
| Chip / VRAM 레지스터 | `0xb74000a1` / `0x2000` MiB |
| BAR0 | `0x82000000`, 16 MiB |
| PROM SHA256 | `f429aa1d5f0f18d11ed9a19503683a674fe3541ae77e959df8e732652c2b4df0` |
| 메인보드 / BIOS | ASUS TUF GAMING B660M-E D4 / 4003 |
| CPU | Intel Core i7-12700F(iGPU 없음) |
| 기록한 Windows / 드라이버 | Windows 11 Pro 22000 / NVIDIA 616.92 |
| UEFI / Secure Boot | x64 / 꺼짐 |
| Selector 프로필 | `entryLatchedSs1Full`; SS0 목표 `0x88888888`, SS1 목표 `0x8` |

cuBLAS GEMM n=4096 중앙값은 FP16 88.7 / BF16 88.6 TFLOPS이며 잠금 기준값은
약 43.7 TFLOPS입니다. S3 절전은 이 프로필에서도 언락을 무효화하며 resume 경로는 없습니다.
`research/headless/build-unlock.ps1`과 `internal-boot/Configure.ps1` 양쪽에
`-HardwareProfile b660m-3070ti`를 지정합니다. 기본값은 `b650m-3090fe`입니다.

## 프로필 3: Gigabyte X870 AORUS ELITE WIFI7 / RTX 3090 Ti(headless USB)

[PR #10](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/10)에서 기여자가
headless USB 성공을 보고한 구성입니다. 모든 모니터는 RTX에 연결하고, Ryzen iGPU는
PCI에서 확인되지만 활성 GOP는 없습니다. B650M처럼 non-aggregate single-RTX
콘솔 구조(`GU_CONSOLE_AGGREGATE 0`)입니다.

| 공통 GPU·플랫폼 항목 | 기록한 값 |
|---|---|
| 카드 | ASUS RTX 3090 Ti, 24 GiB(GA102), VBIOS `94.02.a0.00.2c` |
| PCI 신원 | `10de:2203`, subsystem `1043:8870` |
| GPU BDF | `0000:01:00.0` |
| Chip / VRAM 레지스터 | `0xb72000a1` / `0x5ff4` MiB |
| BAR0 | `0xdd000000`, 16 MiB |
| PROM SHA256 | `943565975885ee71a7e861265984c785f564f35272c32395ee7410b353d9bbc4` |
| 메인보드 / BIOS | Gigabyte X870 AORUS ELITE WIFI7 / F13d |
| CPU | AMD Ryzen 7 9800X3D(iGPU `1002:13c0`, 활성 GOP 없음) |
| 기록한 Windows / 드라이버 | Windows 11 Pro 26300 / NVIDIA 621.01 |
| UEFI / Secure Boot | x64 / 꺼짐 |
| Selector 프로필 | `entryLatchedSs1Full`; SS0 목표 `0x88888888`, SS1 목표 `0x8` |

기여자는 완전 종료 뒤 두 번과 Windows 재시작 뒤 한 번, 총 세 번의 정상 transaction을
보고했습니다. 이 보드는 재시작 시 GPU를 리셋합니다. n=4096 중앙값은 BF16
81.0 → 150.2 / FP16 80.4 → 149.3 TFLOPS입니다. 시작 selector는 부팅마다
`0x40123070/3`, `0x44123070/7`, `0/0`으로 달랐습니다.
[공개 근거](../evidence/x870-3090ti-usb/README.md)를 참고하세요.

`research/headless/build-usb.ps1 -HardwareProfile x870-3090ti`로 빌드합니다.
이 프로필은 `Configure.ps1`·`Headless.ps1` 내부 ESP 설치 경로에 연결하지 않았습니다.
[Headless USB 절차는 영어 문서](HEADLESS-USB.en.md)를 사용하세요.

X870 실기기 근거와 byte-identical 빌드 설명은 현재 main의 cold 사전검사 보강 전인
원래 PR #10 revision `7b578247a28ff66f2707bea5af0e3943aede7974`에 해당합니다.
이전 wrapper·child 해시는 과거 provenance이며 새 빌드의 예상 해시가 아닙니다.
보강된 빌드의 새 X870 실기기 부팅 보고는 없습니다.

## 프로필 4: ASUS PRIME B660-PLUS D4 / RTX 3060 12GB(headless USB)

[PR #24](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/24)가 기여한 정확한
GA104 변형입니다. PCI `10de:2487`, subsystem `10de:1530`, VBIOS 94.04.71.00.87,
12 GiB(`0x3000`), BIOS 3010, iGPU 없는 i5-12600KF 구성입니다. **CSM을 꺼야** 하며,
모니터를 제보 당시처럼 연결한 GOP child 하나와 EDID 전용 child 하나의 aggregate
콘솔을 요구합니다. 다른 RTX 3060 변형을 지원한다는 뜻은 아닙니다.

`research/headless/build-usb.ps1 -HardwareProfile b660plus-3060`와
[USB 안내](HEADLESS-USB.en.md)를 사용하세요. 내부 ESP 설치 도구에는 연결하지 않았고
쓰기 빌드는 USB 경로만 허용합니다. 범용 상세 진단이나 GA102 전용 로컬 프로필
생성 범위가 넓어진 것은 아닙니다.

기여자는 완전 종료 후 USB 부팅 두 번에서 화면 복구와 Windows 직접 시작을 보고했습니다.
원 기여 `60ae7e4`의 같은 장비 잠금 → 언락 PyTorch/cuBLAS n=4096 중앙값은 FP16
26.12 → 49.07, BF16 25.77 → 49.41, TF32 12.98 → 25.06, FP32 7.99 → 7.98 TFLOPS입니다.
저장소의 native 3090 측정 도구와 절차가 다르므로 카드 간 비교 수치로 사용하지 마세요.

현재 통합본은 소유권·런타임·진단 검사를 보강했고 host·빌드 검증을 했지만 새 실기기
부팅 시험은 하지 않았습니다. 재시작·절전 복귀·장기 안정성은 미검증입니다.
[원 빌드 근거와 한계](../evidence/b660plus-3060-usb/README.md)를 참고하세요.

## 프로필 5: Gigabyte H610M H / RTX 3060 12GB GA106(headless USB)

[PR #30](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/30)의 PCI `10de:2504`,
subsystem `1458:4096`, VBIOS 94.06.2f.40.73, 12 GiB(`0x3000`) 구성입니다.
Gigabyte H610M H BIOS FC와 i5-14400을 사용하고 UHD 730은 켜져 있지만 GOP는 없습니다.
모니터 두 대 모두 RTX에 연결하며, 프로필 4의 GA104 aggregate 콘솔과 달리
non-aggregate 디스플레이 child 두 개를 사용합니다.

`research/headless/build-usb.ps1 -HardwareProfile h610m-3060`와
[headless USB 절차](HEADLESS-USB.en.md)만 지원하며 내부 쓰기 빌드는 거부합니다.
범용 진단·GA102 로컬 프로필 생성기의 허용 범위가 넓어진 것은 아닙니다.

기여자는 깨끗한 트랜잭션 한 번과 Windows 직접 시작·두 화면 복구를 보고했습니다.
그 전 RTX GOP가 없던 부팅은 해제 전에 거부됐습니다. 같은 장비의 torch.mm n=8192
중앙값은 FP16 27.06 → 46.61, BF16 27.18 → 46.28, TF32 13.50 → 24.02,
FP32 9.58 → 9.34 TFLOPS입니다. 저장소의 cuBLAS n=4096과는 다른 측정 절차입니다.
실기기 보고와 해시는 원 기여 `4b748f5`에 해당하며 이후 보강한 통합본의 실기기 시험이
아닙니다. [근거와 한계](../evidence/h610m-3060-usb/README.md)를 참고하세요.

## 프로필 6: Maxsun MS-Terminator B760M D4 / RTX 3080 LHR(aggregate console)

[issue #28](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/28)의 내부
headless 실기기 결과 한 건입니다. Maxsun MS-Terminator B760M D4 BIOS H7.8G,
iGPU 없는 i5-12490F, RTX 3080 10GB LHR(GA102) 구성입니다. 펌웨어가 nvgop
디스플레이 child를 비표준 open attribute로 노출하고 AMI DevicePath consumer가
조건부로 붙으며, `GU_TOPO_B760M` 허용 범위가 그 차이를 처리합니다. 이 프로필은
exact-machine 전용입니다.

| 공통 GPU·플랫폼 항목 | 검증한 값 |
|---|---|
| 카드 | Maxsun RTX 3080 10 GiB LHR(GA102) |
| PCI 신원 | `10de:2216`, subsystem `7377:2000` |
| GPU BDF | `0000:01:00.0` |
| Chip / VRAM 레지스터 | `0xb72000a1` / `0x2800` MiB |
| BAR0 | `0x80000000`, 16 MiB |
| PROM SHA256 | `88a6bce1bd790d3e4c542cccafbbd88a7ef73e2e0bef34310d7e8ed780e56d25` |
| 메인보드 / BIOS | Maxsun MS-Terminator B760M D4 / H7.8G |
| CPU | Intel Core i5-12490F (no iGPU) |
| 기록한 Windows / 드라이버 | Windows 11 IoT Enterprise LTSC 26100 / NVIDIA 617.14 |
| UEFI / Secure Boot | x64 / 꺼짐 |
| Selector 프로필 | `entryLatchedSs1Full`; SS0 목표 `0x88888888`, SS1 목표 `0x8` |

최초 검증 실행은 언락 트랜잭션 1회, Windows 직접 시작, 화면 복귀를 기록했고
wrapper `64D887E4…`와 child `1265F152…`를 사용했습니다. 현재 main으로 rebase한
뒤 child hash는 그대로이고, wrapper는 upstream의 `handoff.c` 변경(cold pre-check,
USB hand-back) 때문에 `942EDCF3…`로 다시 빌드됩니다. B760M의 디스플레이/해제
로직은 바뀌지 않았지만, rebase된 wrapper는 자체 hash를 검증됐다고 표시하기 전에
새 실기기 시험이 필요합니다. 빌드는 고정된 20.1.8 대신 clang/lld-link 23.1.4를
사용했습니다.
cuBLAS GEMM n=4096, FP32 accumulation, 3회 중앙값은 FP16 109.8, BF16 113.2
TFLOPS(locked 56.4/59.9)이고 FP32는 변화가 없었습니다. 한 번의 부팅과 한 쌍의
before/after이며 S3/resume, 반복 자동 콜드 부팅, 게임/ComfyUI, 장기 안정성과
다른 RTX 3080은 미검증입니다.

`research/headless/build-unlock.ps1 -HardwareProfile b760m-3080lhr`과
`internal-boot/Configure.ps1 -HardwareProfile b760m-3080lhr`로 선택합니다.

## 기록된 구성 밖의 범위

| 다른 구성·검증 범위 | 상태 |
|---|---|
| 최초 B650M, F8에서 선택한 내부 headless 자동 child | 2026-10-24 언락·Windows 화면 복귀·Tensor 측정 1회 검증 |
| 기본 자동 항목으로 반복 콜드 부팅 | 제한된 감독하 실행으로 입증되지 않음 |
| 다른 RTX 3090, 같은 FE를 다른 PC에 설치한 경우 포함 | 미검증. 고정 검사에서 거부될 수 있음 |
| 다른 RTX 3080, 같은 `10de:2216`이라도 subsystem/VBIOS/PROM/보드가 다르면 | 미검증. 별도 프로필·검증 필요 |
| 다른 ASUS BIOS·AMD 보드/CPU·Intel 호스트 | 미검증. 별도 프로필·검증 필요 |
| iGPU 없는 CPU | 위 프로필 2·6의 기여자 보고 두 건. 일반화하지 않음 |
| 복수 외장 GPU·노트북/hybrid | 미검증 |
| 다른 NVIDIA 모델·아키텍처 | 별도 GPU 포팅과 검증 필요 |
| AMD Radeon·Intel 외장 GPU 언락 | 구현 없음 |
| 게임·ComfyUI·절전/최대 절전·장기 안정성 | GEMM·부팅 근거로 입증하지 않음 |

[내부 headless 안내](HEADLESS.ko.md), [headless USB 안내 — 영어](HEADLESS-USB.en.md), [기존 USB 안내](GUIDE.ko.md),
[공개 근거](../evidence/headless-handoff/README.md)를 참고하세요.
[기존 root unlock](../unlock/app.c)의 guard는 유지합니다. 별도 headless 구현은
[internal unlock](../internal-boot/unlock/app.c),
[headless 정책](../internal-boot/unlock/headless-policy.h),
[wrapper 해제 정책](../internal-boot/boot/full-release.c)에 있습니다.

상수·PROM 해시를 바꾸거나 화면 검사를 삭제하는 것만으로 검증된 포팅이 되지 않습니다.
새 지원에는 메모리/WPR 배치·펌웨어 호환성·소유권·DMA 정리·실패 경로의 근거가 필요합니다.
[포팅 기준](PORTABILITY.en.md) / [호환성 보고](CONTRIBUTING.ko.md).
