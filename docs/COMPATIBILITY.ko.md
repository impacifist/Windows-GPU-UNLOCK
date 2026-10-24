# 검증한 하드웨어와 경로별 조건

[English](COMPATIBILITY.en.md) | **한국어**

모델명만 확인하는 구현이 아닙니다. 두 PC에서 내부 headless 부팅 성공이 기록되어 있습니다.
빌드는 [internal-boot/unlock/machine-profile.h](../internal-boot/unlock/machine-profile.h)에서
빌드 시점에 선택된 하나의 검토된 프로필 전용이며, 다른 GPU·플랫폼은 지원하지 않습니다.
두 번째 프로필(B660M-E D4 BIOS 4003 / RTX 3070 Ti / iGPU 없는 Intel 호스트, AMI aggregate
console)의 지표·조건은 [English 문서](COMPATIBILITY.en.md)의 Profile 2 표를 참조하세요.

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
| Headless USB | 미시험. 일반 USB EFI의 AMD 조건 유지 |

Headless 시험에서는 모든 모니터를 RTX에 연결했고 활성 AMD GOP가 없었습니다.
7800X3D 자체에는 iGPU가 있으며, GOP가 없다는 사실만으로 BIOS에서 iGPU를 비활성화했다고
단정하지 않습니다. **iGPU 없는 CPU는 시험하지 않았습니다.** Intel 호스트나 모든
firmware의 iGPU 구현과 독립적으로 동작한다는 주장도 하지 않습니다.

| 다른 구성·검증 범위 | 상태 |
|---|---|
| 정확히 같은 PC, F8에서 선택한 headless 자동 child | 2026-10-24 언락·Windows 화면 복귀·Tensor 측정 1회 검증 |
| 기본 자동 항목으로 반복 콜드 부팅 | F8 시험 1회로 입증되지 않음 |
| 다른 RTX 3090, 같은 FE를 다른 PC에 설치한 경우 포함 | 미검증. 고정 검사에서 거부될 수 있음 |
| 다른 ASUS BIOS·AMD 보드/CPU·Intel 호스트 | 미검증. 별도 프로필·검증 필요 |
| iGPU 없는 CPU·복수 외장 GPU·노트북/hybrid | 미검증 |
| 다른 NVIDIA 모델·아키텍처 | 별도 GPU 포팅과 검증 필요 |
| AMD Radeon·Intel 외장 GPU 언락 | 구현 없음 |
| 게임·ComfyUI·절전/최대 절전·장기 안정성 | GEMM·부팅 근거로 입증하지 않음 |

[Headless 안내](HEADLESS.ko.md), [기존 USB 안내](GUIDE.ko.md),
[공개 근거](../evidence/headless-handoff/README.md)를 참고하세요.
[기존 root unlock](../unlock/app.c)의 guard는 유지합니다. 별도 headless 구현은
[internal unlock](../internal-boot/unlock/app.c),
[headless 정책](../internal-boot/unlock/headless-policy.h),
[wrapper 해제 정책](../internal-boot/boot/full-release.c)에 있습니다.

상수·PROM 해시를 바꾸거나 화면 검사를 삭제하는 것만으로 검증된 포팅이 되지 않습니다.
새 지원에는 메모리/WPR 배치·펌웨어 호환성·소유권·DMA 정리·실패 경로의 근거가 필요합니다.
[포팅 기준](PORTABILITY.en.md) / [호환성 보고](CONTRIBUTING.ko.md).
