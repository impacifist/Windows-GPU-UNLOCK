# 부팅을 선택하지 않는 SULK02 등록

[English](SHARED-ESP-SECOND-REGISTER.en.md)

기여자는 2026-10-29에 RAM 기반 상태 머신 검사 32개를 통과했습니다. 새 실제 실행 스크립트는 등록 과정에 두 번째 런처/자식 EFI의 고정 해시, 내부 GPT/FAT32 ESP 위치, 이미 비활성화된 Secure Boot, Limine 우선 순서 및 활성 Windows 항목 검사를 연결합니다. 새 복구 백업을 저장하고 확인한 뒤 사용하지 않는 Boot7Exx 항목 하나만 생성합니다. BootOrder, BootNext, ESP 파일을 쓰거나 영수증/시도 예약을 준비하거나 GPU에 접근하거나 재부팅하지 않습니다. 아직 부팅 가능한 완성 프로필이 아닙니다.

브랜치를 업데이트한 뒤 저장소 루트의 관리자 PowerShell에서 다음 명령 하나를 실행합니다. USB는 필요하지 않으며 다시 빌드하거나 복사하지 마세요.

```powershell
./research/profile/register-shared-esp-second.ps1 -StateDirectory 'E:\UNLOCK\ESP-SECOND02'
```

예상 출력은 `registered: true`, `boot_next: null`, `boot_order_written: false`, `limine_first_verified: true`, `windows_entry_verified: true`, `attempt_armed: false`, `boot_authorized: false`입니다. 출력 또는 오류를 보내세요. 이 검사를 위해 재부팅하거나 새 항목을 선택하지 마세요. `E:\UNLOCK\ESP-SECOND02\firmware-backup.json`은 비공개로 보존합니다. 새 디렉터리만 허용하며 재시도를 위해 삭제하거나 재사용하면 안 됩니다.

같은 명령에 `-Action Status`를 추가하면 읽기 전용 상태를 확인합니다. 등록 취소:

```powershell
./research/profile/register-shared-esp-second.ps1 -Action Cleanup -StateDirectory 'E:\UNLOCK\ESP-SECOND02'
```

정리는 저장된 부팅 순서와 기존 항목, 소유한 임시 항목의 정확한 일치를 확인한 뒤 이전에 성공한 정리와 같은 속성 7로 삭제합니다. BootOrder를 복원하거나 다른 도구의 부팅 선택을 삭제하지 않습니다. 등록 또는 정리 실패 시 백업과 출력 및 부분 상태를 보존하고 재시도, 임시 항목 선택, 임의 펌웨어 삭제를 하지 마세요. 자동 롤백은 없습니다. BootNext를 설정하지 않아도 펌웨어 쓰기가 거부되거나 다르게 저장되거나 부팅에 영향을 줄 수 있으며 복구는 보장되지 않습니다. 다른 부팅 관리 도구를 닫으세요. 이후 정상 부팅에 실패하면 기존 Limine/Windows 항목이나 복구 미디어를 사용하고 결과를 보고하세요.

로컬 검증은 실제 스크립트를 RAM 전용 Native 구현과 모의 저장소 목록/해시로 실행하여 등록, 상태, 정리, 점유 슬롯 보존, WhatIf, 기존 백업 거부, 이미 준비된 시도, 런처/자식 해시 불일치, 기존 BootNext, Secure Boot, Limine 순서 및 비로컬 경로 거부를 확인했습니다. 기존 상태 머신 검사 32개도 통과했습니다. 실제 펌웨어 API나 GPU에는 접근하지 않았으며 실제 등록 결과는 아직 필요합니다.

영수증/슬롯 연속성 확인과 마지막 단계의 BootNext 설정은 다음 통합 작업입니다. 등록 성공만으로 USB 없는 잠금 해제 성공을 주장하지 않습니다.
