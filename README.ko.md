# Windows GPU UNLOCK

[English](README.md) | **한국어**

기여자 도구: [진입점 목록 및 정리 제안](docs/ENTRYPOINT-PLAN.ko.md).

커뮤니티 기여로 확장하는 실험용 UEFI GPU 언락과 Windows 직접 인계 구현입니다.

처음이라면 [캡처 설정 안내](docs/CAPTURE-SETUP.ko.md)부터 시작하세요.
진단만 준비하며 지원 여부를 입증하거나 언락을 실행하지 않습니다.

## 에이전트를 통한 캡처

로컬 Windows 도구를 사용할 수 있는 에이전트라면 [단일 설정 프롬프트와 실행
안내](docs/AGENT-CAPTURE.ko.md)를 복사하세요. 도구 설치, USB 준비, 사람이 지켜보는
부팅 이후 재개와 GitHub 보고서 검토를 다룹니다. 기본 범위는 캡처이며 언락은
별도 작업입니다.

[![Discord 커뮤니티 참여](https://img.shields.io/badge/Join%20Discord-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/WjKsjEqxF)
[![지원 구성과 호환성 확인](https://img.shields.io/badge/Compatibility-455A64?style=for-the-badge)](docs/COMPATIBILITY.ko.md)
[![연구와 개선에 기여하기](https://img.shields.io/badge/Contribute-37474F?style=for-the-badge&logo=github&logoColor=white)](docs/CONTRIBUTING.ko.md)

## 여기서 시작하세요

**PC 정보를 확인하거나 호환성을 제보하려면 Windows 내 검사부터 시작하세요.** 언락을 시도할 필요가 없습니다.

| 목표 | 진행 방법 |
| :--- | :--- |
| **처음인데 무엇을 보고해야 하나요?** | [초보자 안내: PC 확인 및 결과 공유](docs/START-HERE.ko.md) |
| **내 PC의 캡처 파일이 이미 있어요** | [기존 파일 오프라인 검사](docs/START-HERE.ko.md#2--기존-파일-검사) |
| **동작한 구성들을 알고 싶어요** | [보고된 구성과 제한](docs/CONFIRMED-SETUPS.ko.md) |
| **언락을 조사할 준비가 됐어요** | [정확한 구성과 부팅 경로부터 확인](docs/COMPATIBILITY.ko.md) |

### 빠른 확인 · 재부팅 없음

**Code → Download ZIP**으로 다운로드하고 압축을 푼 다음 프로젝트 폴더에서 PowerShell을 엽니다. Python 3.10 이상이 설치되어 있으면 실행합니다.

```powershell
python -I .\tools\collect_compatibility.py
```

하드웨어/드라이버 정보를 출력하며 언락, 설정 변경, 업로드는 하지 않습니다. **`not-assessed`와 `can_unlock: false`는 정상입니다.** 개인 정보를 제거한 텍스트를 검토한 뒤 [호환성 보고](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/new?template=compatibility.yml)에 붙여 넣습니다. 펌웨어나 원시 덤프는 첨부하지 마세요.

> [!TIP]
> Python이나 터미널이 익숙하지 않나요? [단계별 안내](docs/START-HERE.ko.md)에서 시작 방법, 결과 의미, 공유 항목을 설명합니다. 하드웨어 정보를 직접 입력해도 됩니다. 제보에 EFI 테스트는 필요하지 않습니다.

## 커뮤니티

[Discord 커뮤니티](https://discord.gg/WjKsjEqxF)에서 편하게 대화하고,
진행 중인 작은 발견을 공유하며 개발·연구를 함께해요.
중요한 연구 결과, 재현 가능한 테스트 보고서, 코드 검토와 프로젝트 결정은
누구나 찾아보고 검토할 수 있도록 [GitHub 이슈](https://github.com/impacifist/Windows-GPU-UNLOCK/issues)와
PR에 남겨주세요. [커뮤니티 룰 — English](docs/DISCORD-RULES.en.md)도 확인해 주세요.

## GPU별 지원 현황

**성공 보고는 특정 카드와 장비 구성에 해당하며, 같은 GPU 모델 전체의 지원을 뜻하지 않습니다.**
빌드 전에 상세 조건에서 메인보드·BIOS·카드 변형을 확인하세요.

| GPU | 현재 상태 | 상세 조건 |
|---|---|---|
| **RTX 3060 12GB (GA104 / GA106)** | 실기기 성공 보고 · headless USB 전용 | [GA104](docs/COMPATIBILITY.ko.md#프로필-4-asus-prime-b660-plus-d4--rtx-3060-12gbheadless-usb) · [GA106](docs/COMPATIBILITY.ko.md#프로필-5-gigabyte-h610m-h--rtx-3060-12gb-ga106headless-usb) |
| **RTX 3060 Ti 8GB (24c9 / GA104)** | 기여자 로컬 USB 성공 보고 · 실험적 자동 프로필 | [정확한 조건](docs/COMPATIBILITY.ko.md#로컬-프로필-성공-보고) |
| **RTX 3070 8GB (2484 / GA104)** | 기여자 로컬 USB 성공 보고 · 실험적 자동 프로필 | [정확한 조건](docs/COMPATIBILITY.ko.md#로컬-프로필-성공-보고) |
| **RTX 3070 Ti 8GB** | 실기기 성공 보고 · 내부 headless | [지원 구성](docs/COMPATIBILITY.ko.md#프로필-2-asus-tuf-b660m-e-d4--rtx-3070-tiaggregate-console) |
| **RTX 3080 12GB (220a / GA102)** | 기여자 USB 성공; 좁은 실험적 LOCAL 허용, DP 복구 제한 포함 | [정확한 구성과 근거](evidence/b550-3080-12gb-local/README.ko.md) |
| **기타 RTX 3080 / RTX 3080 Ti** | 비공개/연구 성공 보고; 일반적인 upstream 지원 아님 | [구성 감사](docs/CONFIRMED-SETUPS.ko.md) |
| **RTX 3090 FE 24GB** | 실기기 성공 기록 · 내부 headless · 기본 프로필 | [지원 구성](docs/COMPATIBILITY.ko.md#프로필-1-asus-tuf-b650m--rtx-3090-fe최초-검증) |
| **RTX 3090 Ti 24GB** | 실기기 성공 보고 · 정확한 장비별 경로 확인 | [X870 USB](docs/COMPATIBILITY.ko.md#프로필-3-gigabyte-x870-aorus-elite-wifi7--rtx-3090-tiheadless-usb) · [B660M 내부](docs/COMPATIBILITY.ko.md#프로필-6-asus-tuf-b660m-e-d4--colorful-rtx-3090-ti내부) |

정확한 빌드 프로필과 유지관리자·기여자의 시험 근거는 [호환성 문서](docs/COMPATIBILITY.ko.md)에
정리했습니다. 실기기 결과는 보고 당시의 빌드에 해당합니다. 이후 main의 보강은
host·빌드 검증을 거쳤으며, **새 실기기 부팅·복구 시험을 한 것은 아닙니다.**

최신 태그 **v0.3.1은 소스 전용 사전 릴리스**입니다. 현재 소스는 그 태그 이후의
USB 경로와 X870 프로필 등을 포함하며, 이를 새 릴리스가 발행됐다는 뜻으로 읽지 마세요.
자신의 파티션 GUID로 로컬 빌드해야 합니다. 펌웨어와 개인 파티션 설정이 들어간
EFI 바이너리는 포함하지 않습니다.

[보고된 동작 구성 및 프로필 범위](docs/CONFIRMED-SETUPS.ko.md) · [전체 이슈 감사](docs/ISSUE-AUDIT.ko.md).

## 부팅 경로 선택

| 경로 | 화면·입력 조건 | 안내 |
|---|---|---|
| **내부 headless** | B650M/3090 FE 또는 B660M/3070 Ti·3090 Ti 프로필; 모니터는 RTX, 도중 화면 제거, T/U/Enter 불필요 | [내부 headless 안내](docs/HEADLESS.ko.md) |
| **Headless USB** | 성공 보고가 있는 RTX 3060 / 3090 Ti 구성; FAT32 USB에 프로필별 wrapper·child 배치 | [Headless USB 안내 — 영어](docs/HEADLESS-USB.en.md) |
| 기존 내부 AMD 화면 manual / Auto | 최초 B650M 프로필, 메인보드/iGPU 화면 유지, manual은 U 필요 | [기존 내부 부팅 안내](docs/INTERNAL-BOOT.ko.md) |
| 기존 USB manual | 최초 AMD/iGPU 화면 프로필, U 한 번, 리셋 없이 Boot Override로 Windows | [기존 USB 안내](docs/GUIDE.ko.md) |

X870·B660-PLUS·H610M은 `Configure.ps1`·`Headless.ps1`에 연결하지 않았으므로 USB 안내를 사용하세요.
첫 두 프로필도 USB 패키지 빌드는 가능하지만, 기록된 내부 부팅이 USB 경로까지 검증하지는
않습니다. 기존 USB EFI에는 AMD 출력 조건이 있습니다. BIOS를 `USB → Windows`로
설정하는 것만으로 직접 인계가 구현되지는 않습니다.

## 내부 headless 빠른 시작

<details>
<summary><strong>고급 · 정확한 프로필의 내부 헤드리스 설정</strong></summary>


**아래 예시는 기본 B650M / RTX 3090 FE 프로필용입니다.** B660M은
[호환성 문서](docs/COMPATIBILITY.ko.md)에 따라 설정과 빌드 양쪽에
`-HardwareProfile b660m-3070ti`를 지정하세요. X870은 별도 USB 경로를 사용합니다.

부팅 설정을 바꾸기 전에 [상세 절차와 복구 방법](docs/HEADLESS.ko.md)을 읽으세요.
정확한 지원 하드웨어, Secure Boot 꺼짐, 완전히 복호화되고 BitLocker 보호가 꺼진
Windows C:, 연결된 키보드가 필요합니다. 도구는 암호화 설정을 해제하지 않습니다.

**기존 Windows ESP와 복구 파티션은 보존**하세요. U: 같은 문자를 가진 별도 GPT FAT32
**EFI 시스템 파티션 1 GiB 이상**을 준비합니다. 도구는 파티션을 생성·축소·삭제·포맷하지
않습니다. 일반 FAT32 데이터 파티션만으로는 ESP 조건을 충족하지 않습니다.

Python 3.9+, clang/lld-link **20.1.8**, 저장소의 고정 의존성과 직접 준비한
[펌웨어 파일 5개](docs/ASSETS.ko.md)가 필요합니다. Host 통합 시험에는 추가로
MSVC/Windows SDK와 개인 fixture가 필요합니다.

저장소 루트의 관리자 PowerShell에서 실행합니다.

```powershell
# GUID와 도구 경로를 자신의 값으로 바꾸세요.
.\internal-boot\Configure.ps1 -UnlockDriveLetter U `
  -WindowsEspGuid 'YOUR-WINDOWS-ESP-GUID'
.\research\headless\build-unlock.ps1 `
  -Clang 'C:\TOOLS\LLVM20\bin\clang.exe' `
  -LldLink 'C:\TOOLS\LLVM20\bin\lld-link.exe'

# 파일 배치와 별도 F8 시험 항목 추가. 기존 기본 우선순위는 유지합니다.
.\internal-boot\Headless.ps1 -Assets '.\assets' -Register
```

동작 스위치 없이 `Headless.ps1 -Assets '.\assets'`만 실행하면 파일만 배치합니다.
설정·빌드는 부팅 항목을 등록하지 않습니다.

1. 작업을 저장하고 완전히 전원을 끕니다. 시험한 ASUS에서는
   **F8 → GPU Headless UNLOCK - EXPERIMENTAL**을 선택합니다.
2. **T·U·Enter를 누르지 말고 기다립니다.** 최초 5초 안의 Esc는 해제·언락을 생략하고
   Windows로 직접 들어갑니다.
3. RTX GOP를 제거하면 화면이 검게 변합니다. Child가 한 번 시도하고 wrapper가
   Windows를 직접 시작합니다. 언락과 Windows 사이에 리셋하지 마세요.
   화면 복귀를 확인하고 [성능을 측정](docs/BENCHMARK.ko.md)합니다.
4. 그 F8 시험에 성공한 뒤에만 기본 자동 부팅을 활성화합니다.

   ```powershell
   .\internal-boot\Headless.ps1 -EnableAutomatic -ConfirmedSuccess
   ```

이 명령은 headless를 첫 번째, Windows Boot Manager를 두 번째로 배치하고 Windows
빠른 시작을 끕니다. 시험 항목 등록만으로는 바뀌지 않습니다. 릴리스 공개 작업 자체는
연구 PC의 현재 기본 부팅 선택을 변경하지 않습니다.

**Esc 구간은 두 개이며 효과가 다릅니다.** 최초 wrapper Esc는 해제 없이 Windows로
갑니다. 나중에 나오는 child의 3초 Esc 구간은 이미 해제한 뒤이므로 취소하고 정지합니다.
화면을 복구하거나 Windows로 계속 진행하지 않습니다. 실패하거나 Windows가 돌아오지
않으면 완전히 전원을 끈 다음 **F8 → Windows Boot Manager**를 선택하세요.

```powershell
.\internal-boot\Headless.ps1 -RestoreWindowsFirst
```

Windows 우선순위 복구는 실험 EFI·빌드 산출물·저장된 상태 없이도 진행합니다.
이전 빠른 시작 값 복원에는 로컬 상태가 필요하므로 상태와 백업을 보존하세요.
[자세한 복구 설명](docs/HEADLESS.ko.md#recovery)을 참고하세요.


</details>

## 최초 B650M 결과와 한계

최초 B650M 실기기 시험에서 성공한 child는 SS0 `0x88888888`, SS1 `0x8`, BME 꺼짐, 남은 mapping 없음,
정리 완료와 FLR 1회를 기록했습니다. Wrapper가 Windows 시작 전 해제 토폴로지를
검증했고, 운영자가 Windows 화면 복귀를 확인했습니다.

| CUDA 12.6 / cuBLAS 12.6, 4096 × 4096 행렬 | 3회 중앙값 |
|---|---:|
| FP32 pedantic | 22.85 TFLOPS |
| FP16 Tensor / FP32 누산 | 124.97 TFLOPS |
| BF16 Tensor / FP32 누산 | 128.93 TFLOPS |

앞으로 [벤치마크 도구](docs/BENCHMARK.ko.md)를 실행하면 TF32도 별도로 측정합니다.
위 표는 TF32 추가 전의 과거 측정값이며, TF32 결과를 소급해 추가하지 않았습니다.

이번 headless 부팅에는 **동시대의 통제된 잠금 상태 기준 측정이 없습니다.** 언락 증가율을
계산하지 않습니다. 게임·ComfyUI 향상, 장기 안정성, 절전·최대 절전 복귀, 반복 자동 콜드
부팅의 신뢰성을 입증하지도 않습니다. [공개 측정 요약](evidence/headless-handoff/summary.json)과
[이전 결과](docs/RESULTS.ko.md)는 서로 다른 실험입니다.

다른 장비의 측정값은 [호환성 문서의 프로필 2](docs/COMPATIBILITY.ko.md),
[X870 USB 근거](evidence/x870-3090ti-usb/README.md),
[B660-PLUS / 3060 USB 근거](evidence/b660plus-3060-usb/README.md)(같은 장비의 잠금 상태 기준선 대비 두 차례 부팅에서
FP16 1.88–1.92배, BF16 1.92–1.95배, TF32 1.93–1.94배)를 참고하세요. 각 결과는 보고한
빌드에 해당하며, 이후 빌드·host 시험 통과가 실기기 시험 기록을 확장하지는 않습니다.

## 기존 진단 자료 확인

[오프라인 호환성 검사기](docs/CHECK-COMPATIBILITY.ko.md)로 기존 캡처가 자동 로컬
프로필 조건에 맞는지와 누락 자료를 확인할 수 있습니다. Python 3.10+만 필요하며
진단 도구나 언락을 실행하지 않습니다.

```powershell
python -I research/profile/check-compatibility.py "D:\RTX3090-UNLOCK" --lang ko
```

후보 판정 후에도 프로필 생성·호스트 시험·실기기 검증이 필요합니다.
자동 처리 범위 밖이라는 결과가 기존 고정 프로필까지 미지원이라는 뜻은 아닙니다.
`--json`으로 제한된 모델·구성 정보가 담긴 보고서를 만들고 검토 후
[호환성 이슈](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/new?template=compatibility.yml)의
선택 항목에 붙여 넣을 수 있습니다. 안내에는 보고서 두 개의 차이를 비교하는 방법도 있습니다.

## 구현과 경계

[GPU 계열 공통화](docs/GPU-FAMILIES.ko.md)로 GA102·GA104·GA106 특성과 동일한
펌웨어 템플릿을 공유하고, 정확한 카드·플랫폼 고정값은 분리합니다.
기존 프로필 이름과 지원 범위는 유지합니다.

Headless는 별도 컴파일 프로필입니다. 정확한 GPU·PROM·PCI 상태·펌웨어 드라이버 코드·
소유권·cold 상태·불변 selector와 정리 검사를 유지합니다. Child는 파일에만 기록하고
물리 GOP 재등장이나 콘솔·입력 인터페이스 변경을 거부합니다. 일반 빌드의 AMD 화면
검사는 그대로입니다. 다른 PC에서 실행하려고 guard를 삭제하거나 PROM 해시만 바꾸지 마세요.

| 경로 | 내용 |
|---|---|
| `internal-boot/`, `research/headless/` | 내부·headless 소스, 로컬 빌드·설치·host 시험 |
| `unlock/`, `release/` | 보존한 기존 USB 수동 구현과 과거 산출물 |
| `benchmark/`, `evidence/` | 측정 도구와 선택한 공개 근거 |
| `docs/` | 호환성·assets·부팅 안내와 한계 |

VBIOS 플래싱이나 Windows 드라이버 교체는 절차에 포함하지 않습니다. 기록된 실행에서
617.14 드라이버는 그대로였습니다. Host mock 시험이 지원 하드웨어를 추가하지는 않습니다.
새 시스템은 [호환성 보고](docs/CONTRIBUTING.ko.md)와
[포팅 기준](docs/PORTABILITY.en.md)을 참고하세요.
검증된 하드웨어 범위 안의 새 PC는 [로컬 프로필 생성 절차](docs/NEW-MACHINE.en.md)를
이용할 수 있습니다. 이 실험적 절차는 NVIDIA 디스플레이 장치 정보를 수집하되,
상세 GPU 읽기는 검토된 PCI·BAR 상태의 단일 RTX 3090 / 3090 Ti, RTX 3060 12GB, RTX 3060 Ti `24c9` 또는 RTX 3070 `2484` 식별자로 제한합니다.
범용 진단 도구는 관측 조건이 맞으면 상위 PCI 장치 정보도 자동 기록해 메인보드 차이 분석에
활용합니다. 이 추가 관측이 언락을 허용하거나 PCI 상태를 보정하지는 않습니다.
로컬 프로필 생성만으로 실기기 검증 프로필이 추가되지는 않습니다. USB 쓰기 빌드에는
PCI 신원 재확인·입력 정책·콘솔 구성별 전체 드라이버 해제 재현을 포함한 host 작업 22개 통과와
프로필·시험 자료·빌드 입력 일치가 필요하며, 실기기 검증은 별도입니다.
이 범위 안에서 진단기가 PCI 버스 위치를 자동 탐지하고 생성한 프로필에 고정합니다.
선택 옵션 `-TargetBus`를 지정하면 탐지와 기존 캡처로 재개할 때 모두 해당 버스만 허용합니다.
로컬 탐색은 PNP0A03/PNP0A08 루트와 최대 16개 PCI 경로 노드를 처리하며, 전체 경로와
PCI 부모 위치를 고정·재검사합니다. 이 실험적 브리지 경로 지원도 장비별 검증이 필요합니다.
로컬 탐색과 프로필 생성은 16비트 PCI 세그먼트 어디에 있는 단일 대상을 고정할 수 있습니다.
준비·해제·PCI 접근에서도 캡처한 세그먼트와 버스가 일치해야 합니다.
세그먼트가 0이 아닌 구성은 실험적 지원이며 실기기 검증이 필요합니다.
내장 그래픽이나 별도 EDID 드라이버가
없는 단일 RTX 구성을 처리합니다(EDID 생략은 비 aggregate 경로만 가능).
별도 EDID 드라이버가 없는 비 aggregate 로컬 프로필은 고유 NVIDIA 디스플레이 자식
1~4개를 생성·재현합니다. 3~4개 구성은 실험적이며 4개 구성은 호스트 시험만 거쳤습니다.
EDID 드라이버가 있거나 aggregate인 구성의 생성 제한은 2개를 유지합니다.
인식 가능한 AMI aggregate 콘솔도 디스플레이 자식 핸들 1~2개 구성에서 실험적 로컬
프로필을 생성하며 별도 준비·해제 재현 시험을 요구합니다. 이는 새 실기기 성공 보고가 아닙니다.
여러 NVIDIA GPU 구성은 아직 생성하지 않습니다.
언락 패키지를 USB에 복사하려면 `-StageUnlock`을 명시해야 합니다.

## 라이선스와 출처

포트와 재사용 코어는 **GPL-2.0-only**이며 EDK2 등 별도 구성 요소의 라이선스는 유지합니다.
[rough2515-hub/rtx3080-unlock-public](https://github.com/rough2515-hub/rtx3080-unlock-public/tree/a08254d269af30ace9e57237abd3d8e6c261e8f5)와
[Zagitta/gpu-unlock](https://github.com/Zagitta/gpu-unlock/tree/0ebdb1c6308b6a2072be5bf04513472942541956)에 기반합니다.
[LICENSE](LICENSE), [외부 코드 고지](THIRD_PARTY_NOTICES.md), [변경 이력](CHANGES.md)을
보존하세요. NVIDIA 펌웨어·PROM/BIOS 이미지·드라이버·CUDA DLL은 이번 소스 릴리스에
재배포하지 않습니다.
