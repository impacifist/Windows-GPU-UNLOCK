# 임시 SULK02 항목 정리 완료하기

[English](SULK02-CLEANUP.en.md)

2026-10-30 비교 결과 현재 BootOrder는 정확히 `0000,0008,000C,000D`이며 Limine/Windows/USB 항목의 바이트와 속성이 등록 백업과 같습니다. 일반 대체 항목 `000E`(CD/DVD), `000F`(이동식), `0010`(네트워크)만 없습니다. BootNext는 없고 Boot7E00은 소유한 SULK02 항목과 정확히 일치합니다. 대체 항목이 사라진 원인은 알 수 없으며 다시 만들지 않습니다.

브랜치를 업데이트한 뒤 저장소 루트의 관리자 PowerShell에서 명령 하나만 실행하세요.

```powershell
./research/profile/cleanup-shared-esp-second.ps1
```

예상 출력은 `cleanup_completed: true`, `temporary_option_present: false`, `boot_next_clear: true`, `boot_order_written: false`, `firmware_writes: 1`입니다. JSON을 보내세요. 이 단계는 재부팅/벤치마크/EFI 실행을 요구하지 않습니다.

위에서 검토한 정확한 축소 상태만 허용합니다. 남은 네 항목, 사라진 대체 항목, BootNext 없음, 정확한 소유 항목을 반복 검사하고 ESP 외부에 새 `cleanup-sulk02.json`을 덮어쓰기 없이 저장한 뒤 저장된 속성 7로 Boot7E00만 삭제하고 확인합니다. BootOrder, 다른 항목, ESP 파일, GPU 상태 또는 기존 백업/영수증/예약은 변경하지 않습니다. 예상하지 못한 상태는 거부하며 새 스냅샷은 원래 등록 백업을 대체하지 않습니다.

펌웨어 NVRAM 삭제 1회이므로 펌웨어가 거부하거나 잘못 처리하여 부팅에 영향을 줄 수 있으며 복구는 보장되지 않습니다. 다른 부팅 관리 도구를 닫으세요. 오류 시 모든 파일/출력을 보존하고 재시도나 임의 삭제 없이 멈추세요. 삭제 후 오류라면 임시 항목이 이미 없을 수 있습니다. 자동 롤백/재부팅은 없습니다. 이후 정상 부팅에 실패하면 기존 Limine/Windows 항목 또는 복구 미디어를 사용하고 보고하세요.

로컬 RAM 검사 21개로 순서/항목 변경, 대체 항목 재등장, 기존 BootNext, 임시 항목 변경, 백업 실패, 경합, 삭제 실패/읽기 검증 실패 및 삭제 후 변경을 검사했습니다. 실제 진입 스크립트의 RAM 전용 Native 모의 검사로 WhatIf, 새 스냅샷, 삭제 속성 7, 원래 백업 보존 및 재실행 거부도 확인했습니다. 실제 펌웨어 API/GPU 작업은 로컬에서 실행하지 않았으며 실제 정리는 아직 필요합니다.

**2026-10-30 실제 결과:** 정리 완료, Boot7E00 없음, BootNext 없음, BootOrder 쓰기 없음, 삭제 1회를 기여자가 확인했습니다. 정리를 반복하지 마세요. [마지막 정상 재시작 확인](FINAL-NORMAL-BOOT.ko.md).
