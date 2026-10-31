# 검토된 EVP10 정리 시험

[English](EVP10-CLEANUP.en.md)

업데이트한 저장소 루트의 관리자 PowerShell에서 `./research/profile/cleanup-reviewed-evp10.ps1`를 실행합니다. 재부팅은 필요하지 않습니다. 선택적 `-WhatIf`는 검사만 수행합니다.

이 시험은 펌웨어 항목 정리 쓰기를 수행하며 새 잠금 해제 시도가 아닙니다. 정확한 실패 로그 해시와 패키지/기록, 빈 BootNext, 변경 없는 기존 순서 항목 및 검토한 속성/해시의 추가 Boot000D 하나를 요구합니다. Boot000D, BootOrder 전체, 파일, 영수증 및 실패 결과를 보존합니다. 현재 펌웨어 상태를 백업한 다음 기존 경합 검사 정리 함수를 통해 정확한 소유 Boot7Exx 항목만 삭제합니다. BootNext 선택, GPU 레지스터 접근, EVP10 재준비나 재부팅을 하지 않습니다. 예상과 다른 상태는 거부합니다. 펌웨어 API 실패 시 항목이 남을 수 있으며 수동 삭제로 거부를 우회하지 마세요.

최종 JSON을 보내세요. `cleanup_completed`, `failed_attempt_preserved`, `usb_entry_preserved`는 true이고 `rearm_authorized`, `boot_order_written`, `gpu_accessed`는 false여야 합니다. 기존 실패 결과를 성공으로 바꾸지 않습니다. 이 시험은 새 EFI를 설치하지 않습니다.

PowerShell 7과 Windows PowerShell 5.1에서 실제 SHA256, 모의 검토 항목 허용, 변경 거부 네 사례 및 구문을 검증했습니다. 기존 모의 정리 경합/소유 검사도 통과했습니다. 실제 삭제 성공은 아직 검증하지 않았습니다. 진입 SS1은 SULK02의 7과 EVP10의 0으로 달랐으며 새 잠금 해제 전에 원인 검토가 필요합니다.
