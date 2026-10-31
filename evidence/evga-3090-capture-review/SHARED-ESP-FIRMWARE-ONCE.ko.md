# 다음 테스트: USB 없이 펌웨어에서 visible audit 직접 부팅

[English](SHARED-ESP-FIRMWARE-ONCE.en.md)

**GPU transaction은 수행하지 않습니다.** 이미 성공한 ESP의 visible audit를 펌웨어에서 일회성으로 직접 실행한 뒤 기존 Windows 연결을 시도합니다. 기존 파일의 SHA-256은 `f4cf33771fcc1e9c463a94561898457d0ce6ee7e4e17da5546c8a9752d373c92`로 고정하며 재빌드/교체하지 않습니다. ESP/Limine/Windows EFI/BCD/언락 로그/transaction 마커는 쓰지 않습니다. 준비는 임시 `Boot####`, 일회성 `BootNext` 및 ESP 밖의 새 로컬 복구 백업만 만듭니다. **BootOrder는 쓰지 않습니다.**

업데이트된 저장소 루트의 **관리자 PowerShell**에서 준비 명령 한 개를 실행하세요. 사용자 지정 desktop 드라이브 예시이며 필요하면 ESP 밖의 다른 새 로컬 폴더를 지정하세요.

```powershell
./research/profile/shared-esp-firmware-once.ps1 -StateDirectory 'G:\Movian\ESP-ONCE01'
```

오류가 나면 멈추고 **재부팅하지 마세요**. JSON에 `registered: true`, `boot_next_one_time: true`, `boot_order_written: false`, `limine_first_verified: true`, `windows_entry_verified: true`가 나와야 합니다. `firmware-backup.json`을 보존하고 복구 미디어는 준비하되 테스트 중에는 분리하세요. 스크립트는 재부팅하거나 앱을 종료하지 않습니다. 저장 장치 USB를 제거하고 작업을 저장한 뒤 **일반 재시작을 한 번** 하세요. 언락 EFI를 선택하거나 일반 부팅 순서를 바꾸지 마세요.

`AUDIT ENTERED ON PINNED ESP. Loading Windows in 10 seconds.` 화면을 촬영하고 Windows 로그인 여부를 알려주세요. USB 런처를 사용하지 않으므로 USB launcher 문구는 나오지 않습니다. 화면이 없으면 결론을 내리지 말고 결과를 보내세요. USB 없이 audit 진입이 보여야 새 펌웨어 경로 증거이며 Windows만으로는 부족합니다. 언락/영구 자동 실행 증거가 아닙니다.

Windows 복귀 후 정리 명령 한 개를 실행하세요.

```powershell
./research/profile/shared-esp-firmware-once.ps1 -Action Cleanup -StateDirectory 'G:\Movian\ESP-ONCE01'
```

출력은 `boot_order_unchanged: true`, `temporary_option_present: false`, `boot_next: null`이어야 합니다. 단, 다른 도구가 새 일회성 요청을 만든 경우에는 보존됩니다. 백업은 유지합니다. 한 번 더 재시작하여 일반 Limine이 먼저 나오고 Windows/CachyOS 항목이 동작하는지 확인하세요. 준비/정리 출력, 사진/영상 및 부팅 결과를 비공개로 보내주세요. 백업에는 장치 경로가 있으므로 공개하지 마세요. 같은 폴더로 Prepare를 반복하거나 백업을 삭제하지 마세요.

재시작 전에 취소하려면 같은 Cleanup 명령을 사용하세요. 읽기 전용 확인은 같은 폴더와 `-Action Status`입니다. 쓰기 뒤 준비 실패 시 자기 임시 변수만 정확히 확인하여 제거를 시도하며 rollback 실패는 명시합니다. 3분 안에 Windows가 없으면 전원을 끄고 기존 펌웨어 메뉴의 Limine/Windows를 선택하세요. UEFI는 audit 진입 전에 BootNext를 소비하도록 규정되어 있고 BootOrder는 변경하지 않았습니다. 복구는 보장되지 않습니다. 필요하면 복구 미디어로 Windows에 돌아온 뒤 Cleanup 하세요. 상태가 달라 정리가 거부되면 출력/백업을 보존하고 검토를 요청하세요. 번호 변수를 추측해서 삭제하거나 BootOrder 백업을 무조건 복원하지 마세요.

**위험:** 펌웨어 NVRAM을 변경하므로 펌웨어가 거부/변경/오처리하여 부팅에 영향을 줄 수 있습니다. unsigned audit를 거부하거나 실행 중 멈출 수 있습니다. Secure Boot는 이미 비활성화되어 있어야 하며 설정은 바꾸지 않습니다. API에 원자적 compare-and-swap이 없어 다른 도구/업데이트/펌웨어와의 경쟁을 제거할 수 없습니다. 다른 boot/firmware 관리 도구는 닫으세요. 기존 BootNext가 있으면 거부하고 첫 active Limine/Windows 항목과 원본 BootOrder/모든 참조 항목 바이트를 반복 확인합니다. 예상 밖 변화는 덮어쓰지 않고 멈춥니다. 삭제 직전 자기 BootNext/option의 정확한 바이트/속성을 재확인하며 다른 요청은 보존합니다.

이는 Windows BCD `/bootsequence`가 아니라 Microsoft firmware API를 통한 실제 UEFI NVRAM입니다. 프로세스의 `SeSystemEnvironmentPrivilege`는 잠시 활성화한 뒤 원래 상태로 복원합니다. 새 항목은 GPT HD short-form 경로와 정확한 audit 경로를 포함합니다. 변수 속성은 NV|BS|RT=7, 별도 load option active 속성은 1입니다. BootNext는 해당 번호의 2바이트 little-endian UINT16이며 실행 전에 소비된 뒤 일반 BootOrder로 돌아갑니다. 공식 문서 링크는 영어 문서에 있습니다.

로컬 RAM mock 18개와 독립 Python packed-layout 검사를 통과했고 native bridge 컴파일/스크립트 구문과 별도 RAM-native 환경에서 실제 entry script의 경로/WhatIf/준비/백업/상태/취소/정리 및 해시/BootNext/Secure Boot/Limine 순서 거부를 확인했습니다. **실제 firmware API/NVRAM/ESP/하드웨어는 테스트하지 않았습니다.** 새 펌웨어 선택 경로는 아직 하드웨어 미검증입니다. 펌웨어 정책/권한/부팅 항목 형식/해시 문제로 거부되면 우회하지 말고 정확한 오류를 보내주세요.
