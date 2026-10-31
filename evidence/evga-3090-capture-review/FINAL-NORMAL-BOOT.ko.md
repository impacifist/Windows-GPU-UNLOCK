# 마지막 확인: 정리 후 정상 재시작 한 번

[English](FINAL-NORMAL-BOOT.en.md)

기여자가 SULK02 정리 완료를 확인했습니다. Boot7E00 없음, BootNext 없음, 펌웨어 삭제 1회, BootOrder 쓰기 없음이며 현재 네 항목은 보존되었습니다. 정리 결과는 확인되었지만 이후 정상 재시작은 아직 확인하지 않았습니다.

브랜치를 업데이트하고 저장소 루트의 관리자 PowerShell에서 실행하세요.

```powershell
./research/profile/confirm-shared-esp-normal-boot.ps1
```

`phase: ready_for_normal_restart`가 나오면 작업을 저장하고 USB를 뺀 상태로 정상 재시작을 한 번 하세요. EFI 시험을 선택하지 마세요. Windows 복귀 후 **같은 명령을 다시 실행**하여 JSON을 보내세요. 예상 값은 `normal_restart_confirmed: true`, `temporary_option_absent: true`, `boot_next_clear: true`, `primary_entries_unchanged: true`, `firmware_written: false`입니다.

스크립트는 재시작 전 로컬 Windows 부팅 시점 기준 파일 하나만 새로 기록하고, 이후 펌웨어 상태를 읽어 더 새로운 Windows 부팅과 일반 Limine/Windows BootCurrent를 확인합니다. 기준 파일, 백업, 증거를 보존하며 GPU 작업, 펌웨어 쓰기, EFI 실행, 자동 재시작은 하지 않습니다. 일반 대체/USB 순서의 뒤쪽 변경은 주 부팅 항목 검사 범위 밖이며 복원하지 않습니다. 잠금 해제 유지 시험이 아닌 정상 부팅 확인이므로 일반 재시작이 현재 잠금 해제 상태를 해제할 수 있습니다.

오류나 정상 부팅 실패 시 멈추고 보고하세요. 잠금 해제를 재시도하거나 마커를 삭제하지 마세요. 필요하면 기존 Limine/Windows 메뉴 항목이나 복구 미디어를 사용하며 복구는 보장되지 않습니다. 정상 부팅을 예상하지만 이번 정리 후 실제 결과는 아직 필요합니다.

PowerShell 7과 Windows PowerShell 5.1의 로컬 모의 검사에서 기준 파일, 같은 부팅 거부, 일반 항목 허용, 시험 항목 거부, 기존 BootNext 거부 및 펌웨어 쓰기 없음을 확인했습니다. 실제 펌웨어와 부팅 시간은 모의 값이며 실제 재시작/하드웨어 시험은 없었습니다. 실제 정리 스크립트도 모의 검사에 포함했습니다. 등록 상태의 빈 BootNext JSON 호환성 문제도 수정했지만 펌웨어 변경 동작은 바꾸지 않았습니다.

**2026-10-30 실제 결과:** 기여자가 정상 Windows 부팅을 확인했습니다. 스크립트는 더 새로운 Boot0000 부팅, 임시 항목 없음, BootNext 없음 및 주 항목 변경 없음을 보고했습니다. 추가 재시작/잠금 해제를 요청하지 않습니다. 위 절차는 완료된 시험의 기록이므로 반복하지 마세요.
