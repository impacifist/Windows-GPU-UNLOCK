# 내부 headless 언락 → Windows

[English](HEADLESS.en.md) | **한국어** | [README](../README.ko.md)

v0.3.1 소스 사전 릴리스는 2026-10-24 ASUS BIOS 3057 / RTX 3090 Founders Edition
한 대의 성공을 기록합니다. 모든 모니터를 RTX에 연결했고, 해제 전 RTX 물리 GOP 1개,
해제 후 0개를 확인했습니다. 자동 child가 한 번 시도해 정리했고 Windows 화면 복귀를
관찰했습니다. [공개 근거](../evidence/headless-handoff/README.md).

CPU는 iGPU가 있는 Ryzen 7 7800X3D입니다. 활성 AMD GOP는 사용하지 않았지만 iGPU 없는
CPU나 다른 시스템을 검증한 것은 아닙니다. [정확한 호환성](COMPATIBILITY.ko.md)을
확인하세요. Headless USB는 미검증이며, [기존 USB 안내](GUIDE.ko.md)는 AMD 화면을
유지하는 수동 경로입니다.

## 준비 조건과 저장 공간

main에는 기여자가 내부 부팅을 보고한 B660M/3070 Ti·3090 Ti 프로필도 있습니다.
이후 추가된 프로필은 아래 v0.3.1 대신 main 소스를 사용하고 Configure와 build-unlock
양쪽에 정확한 `-HardwareProfile`을 전달해야 합니다.
[구성·근거의 한계](COMPATIBILITY.ko.md)를 확인하세요. 기본값은 B650M입니다.

- 기록된 정확한 GPU/PROM·메인보드·BIOS·PCI 프로필. Windows 11 x64 UEFI,
  Secure Boot 꺼짐, Windows C: 완전 복호화·BitLocker 보호 꺼짐.
  도구는 암호화 설정을 해제하지 않습니다.
- RTX에 연결한 모니터와 Esc/F8 복구용 키보드. 시험 전에 firmware Boot Override와
  Windows Boot Manager에 접근할 수 있는지 확인하세요.
- U: 같은 문자가 있는 별도 GPT FAT32 **EFI 시스템 파티션(ESP) 1 GiB 이상**.
  원래 Windows ESP와 복구 파티션은 보존합니다. 공간을 만들려고 기존 Windows
  100 MiB ESP를 삭제하지 마세요.
- Python 3.9+, clang/lld-link 20.1.8, 저장소 고정 의존성,
  직접 준비한 [펌웨어 파일 5개](ASSETS.ko.md). 펌웨어·PROM/BIOS 이미지·드라이버와
  개인 파티션 전용 EFI는 릴리스에 포함하지 않습니다.
- Host 통합 시험을 실행하려면 MSVC/Windows SDK와 개인 fixture도 필요합니다.

스크립트는 파티션 생성·축소·삭제·포맷을 하지 않습니다. 백업 후 별도로 저장 공간을
준비하고 대상을 정확히 확인하세요. FAT32 basic-data 파티션은 ESP 형식 조건을
충족하지 않습니다. Windows C:·시스템 ESP·복구 파티션의 형식을 바꾸지 마세요.

다음 읽기 전용 명령으로 별도 ESP와 기존 시스템 ESP를 구분합니다.

```powershell
Get-Partition | Format-Table DiskNumber,PartitionNumber,DriveLetter,Size,GptType,Guid,IsSystem
Get-Volume -DriveLetter U | Format-List FileSystem,Size,Path
```

Windows ESP GUID는 현재 시스템 EFI 파티션 중
`EFI\Microsoft\Boot\bootmgfw.efi`가 있는 파티션의 GUID입니다.
새로 준비한 별도 ESP의 GUID와 혼동하지 마세요.

## 설정과 빌드

[v0.3.1 소스 ZIP](https://github.com/impacifist/Windows-GPU-UNLOCK/releases/tag/v0.3.1)을
풀거나 다음 명령으로 고정 릴리스 소스를 받습니다.

```powershell
git clone --branch v0.3.1 --depth 1 https://github.com/impacifist/Windows-GPU-UNLOCK.git
cd Windows-GPU-UNLOCK
```

새 ESP가 아직 없다면 [별도 파티션 준비 절차](INTERNAL-BOOT.ko.md#2-별도-파티션-준비)의
디스크 관리·FAT32·GPT 형식 확인 부분만 따릅니다. 그 문서의 AMD용 빌드/설치 명령 대신
아래 headless 명령으로 돌아오세요. 저장 공간 준비는 사용자가 직접 수행합니다.

저장소 루트의 관리자 PowerShell에서 실행합니다.

```powershell
.\internal-boot\Configure.ps1 -UnlockDriveLetter U `
  -WindowsEspGuid 'YOUR-WINDOWS-ESP-GUID'
.\research\headless\build-unlock.ps1 `
  -Clang 'C:\TOOLS\LLVM20\bin\clang.exe' `
  -LldLink 'C:\TOOLS\LLVM20\bin\lld-link.exe'
```

자리표시자를 자신의 값으로 바꾸세요. 설정은 ignored 로컬 파일만 쓰고, 빌드는 별도
headless wrapper·child와 해시 계약을 생성합니다. 둘 다 부팅 설정을 바꾸지 않습니다.
빌드는 자신의 파티션에 종속되며, 성공한 로컬 hash가 범용 EFI를 의미하지 않습니다.
빌드 시작/종료 및 설치 시 로컬 설정·생성 헤더·런타임 소스·EDK2 헤더·asset 계약과
child manifest를 대조합니다. 소스나 설정을 바꿨다면 다시 빌드하세요.

## 파일 배치·등록·시험

```powershell
# 파일 배치만: 부팅 항목·기본 순서·빠른 시작 변경 없음.
.\internal-boot\Headless.ps1 -Assets '.\assets'

# 파일 배치 + 별도 F8 항목 추가. 기존 기본 우선순위 유지.
.\internal-boot\Headless.ps1 -Assets '.\assets' -Register
```

이 경로에 기존 `Install.ps1`이나 AMD 화면용 Auto EFI를 사용하지 마세요.
Headless 설치는 별도 ESP에 `HEADLESS.EFI`와 `UNLOCK-HEADLESS.EFI`를 배치하며,
기존 Windows EFI와 원래 manual EFI를 보존합니다. 로컬 상태·백업은 복구 입력이므로
보존하고 공개 릴리스에 올리지 마세요.

작업을 저장하고 완전히 종료한 뒤 전원을 켭니다. 시험한 ASUS에서는
**F8 → GPU Headless UNLOCK - EXPERIMENTAL**을 선택합니다.
**T·U·Enter 없이 기다리세요.** 언락과 Windows 사이에 리셋하지 않습니다.

| 구간 | 예상 동작·Esc |
|---|---|
| 최초 wrapper 5초 | Esc는 드라이버 해제·언락을 생략하고 Windows로 이동 |
| 드라이버 해제 | RTX 화면이 꺼지므로 검은 화면 예상 |
| 이후 자동 child 3초 | Esc는 이미 해제한 뒤의 취소·정지. 화면 복구·Windows 진입 없음 |
| Child 성공·정리 완료 | 리셋 없이 Windows Boot Manager 시작, NVIDIA 화면 복귀 예상 |

Wrapper는 해제 전에 Windows와 검증한 child를 미리 로드합니다. 실패하면 재시도하거나
Windows로 강행하지 않고 정지합니다. 검은 화면만으로 성공을 판단하지 마세요.
Windows 화면 복귀·로컬 로그·[benchmark](BENCHMARK.ko.md)를 확인한 뒤 승격합니다.

## 기본 자동 부팅 활성화

자신이 F8 headless 시험에 성공한 것을 확인한 뒤에만 실행합니다.

```powershell
.\internal-boot\Headless.ps1 -EnableAutomatic -ConfirmedSuccess
```

이 명령은 headless 첫 번째·Windows Boot Manager 두 번째로 firmware 우선순위를
설정하고 빠른 시작을 끕니다. 시험 항목 등록만으로는 바뀌지 않으며, 명령이 재부팅하지도
않습니다. Host 시험 통과를 자신의 F8 성공으로 대신하지 마세요.
v0.3.1 공개 작업은 연구 PC의 현재 기본 부팅 설정을 승격하지 않습니다.

<a id="recovery"></a>
## 복구

시도가 정지하거나 child에서 취소했거나 Windows가 돌아오지 않으면 완전히 전원을 끕니다.
다음 부팅에서 **F8 → Windows Boot Manager**로 언락을 우회하세요.
같은 firmware 세션에서 다시 언락하지 마세요.

Windows 관리자 PowerShell에서 실행합니다.

```powershell
.\internal-boot\Headless.ps1 -RestoreWindowsFirst
```

Windows 우선순위 복구는 실험 EFI·빌드 파일·저장된 상태 없이도 진행합니다.
이전 빠른 시작 값은 저장된 값이 있을 때만 복구합니다.
`internal-boot/install/headless-state.json`과 백업을 보존하세요.
오류를 보고하면 firmware 항목을 확인하고 Windows Boot Manager를 직접 선택하세요.
부분 복구만으로 모든 설정이 복원됐다고 가정하지 마세요. 파티션 삭제·포맷은 필요 없습니다.

## 근거와 한계

이번 Windows 부팅의 3회 중앙값은 **FP16 124.969948 / BF16 128.931534 TFLOPS**,
FP32 22.845572였습니다. CUDA/cuBLAS 12.6·드라이버 617.14를 사용했습니다.
Child는 FLR 1회, BME 꺼짐, 남은 mapping 없음과 정리 완료를 기록했습니다.
[공개 측정 요약](../evidence/headless-handoff/summary.json).

동시대의 통제된 잠금 상태 기준 측정이 없으므로 증가율을 계산하지 않습니다.
반복 기본 자동 콜드 부팅, 장기 안정성, 게임·ComfyUI, 절전·최대 절전 복귀,
다른 CPU·카드·BIOS와 headless USB는 미검증입니다. F8에서 선택한 자동 child 성공은
firmware 기본값으로 반복 실행되는 신뢰성 검증과 구분합니다.
소스 guard는 유지하며 host mock을 실기 근거로 사용하지 않습니다.
