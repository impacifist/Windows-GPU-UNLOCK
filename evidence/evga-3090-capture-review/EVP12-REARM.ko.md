# 기본 재준비 검증 — EVP12

[English](EVP12-REARM.en.md)

EVP11 정리 후 기여자는 Boot0000 일반 재부팅, 임시 항목 부재, 빈 BootNext 및 기존 항목 동일을 보고했습니다. EVP11의 참석형 트랜잭션, 벤치마크, 정리 및 일반 부팅 순서를 완료했습니다. 재부팅 후 잠금 해제 유지가 입증된 것은 아닙니다.

남은 재사용 흐름 검사는 실패 예외 없는 기본 재준비입니다. 원래 USB를 D:로 연결하고 브랜치를 업데이트한 다음 저장소 루트 관리자 PowerShell에서 실행하세요.

```powershell
./research/profile/manage-evga-profile.ps1 -Action Rearm
```

`prepared: true`, `slot: 12`, `run: EVP12`, `full_shutdown_required: true`를 확인합니다. EVP11 성공 증거와 별도 정리 영수증을 검증하고 원래 결과를 보존하며 새 슬롯을 할당합니다. `-ReviewedEvp10Retry`는 이전 실패에만 적용되므로 사용하지 마세요. 오류 시 중지하고 보내세요. 상태 삭제나 자동 반복을 하지 않습니다.

준비 후 USB를 제거하고 작업을 저장한 다음 Shift를 누른 채 Windows 종료를 선택하고 전원을 켭니다. 실시간 검사 후 새로운 Shift+G를 한 번 누르세요. SS1=7 조건은 유지합니다. 완전 종료는 시험한 절차이며 모든 경우의 해결책으로 입증하지 않았습니다. 허용 시 기존 GPU/PCI 쓰기, 드라이버 해제, DMA/펌웨어 실행 및 리셋을 수행하며 멈춤이나 Windows/GPU 초기화 실패가 가능합니다. 복구는 보장되지 않습니다. 3분 후에도 멈추면 전원을 끄고 일반 Limine/Windows로 복구한 뒤 수동 복구를 보고하세요. EFI를 반복하지 마세요.

Windows에서 `./research/profile/manage-evga-profile.ps1 -Action Collect`를 실행하고 JSON/ZIP, 직접/수동 복귀, 완전 종료와 USB 부재를 보내세요. 같은 세션 검사가 필요할 수 있으므로 검토 전 재부팅하지 마세요. 자동 재부팅/복구, Secure Boot 변경이나 무인 잠금 해제는 없습니다.

로컬 EVP12는 88064바이트 AMD64 EFI, import/IAT/delay import 없음, NX/dynamic base이며 SHA256은 `2d0db39cd798516adccb696b084708d68baf4221485c4fdd3ac2c7de3c214a98`입니다. 호스트 미디어/영수증 검사가 통과했습니다. PS7/5.1 모의 컨트롤러 검사에서 슬롯12 기본 진행, 이전 결과 보존, 종료 안내 및 대기 슬롯 재준비 거부가 통과했습니다. 실제 EVP12 검증은 대기 중입니다.
