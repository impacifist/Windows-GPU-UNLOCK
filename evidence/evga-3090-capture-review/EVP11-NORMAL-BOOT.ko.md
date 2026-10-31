# EVP11 정리 확인; 일반 부팅 검사

[English](EVP11-NORMAL-BOOT.en.md)

기여자는 Boot7E00 별도 정리 성공을 보고했습니다. 영수증은 검토한 로그/영수증/런타임 마커 해시에 연결되며 원래 결과를 보존하고 BootOrder 쓰기/GPU 접근 없음을 기록합니다. 트랜잭션, 같은 세션 벤치마크 및 임시 항목 정리는 입증되었으며 이후 일반 재부팅 검증은 남았습니다.

업데이트한 저장소 루트의 관리자 PowerShell에서 실행하세요.

```powershell
./research/profile/confirm-evga-profile-normal-boot.ps1
```

`phase: ready_for_normal_restart`를 기다립니다. 성공 증거와 일치하는 정리 영수증, 임시 항목 부재, 빈 BootNext 및 정상 부팅 항목을 확인하고 Windows 부팅 시각을 로컬에만 저장합니다. 오류 시 중지하고 보내세요.

작업을 저장하고 USB를 제거한 상태에서 일반 Windows 다시 시작을 한 번 수행합니다. USB/시험 EFI 선택이나 부팅 순서 변경을 하지 마세요. 이 과정은 잠금 해제를 다시 적용하지 않으며 성능 유지도 입증하지 않습니다. Windows가 돌아오지 않으면 기존 Limine/Windows 항목을 선택하고 수동 복구를 보고하세요. EFI를 반복하지 마세요.

Windows 복귀 후 같은 명령을 다시 실행하고 JSON을 보내세요. `normal_restart_confirmed: true`, 정상 BootCurrent, 임시 항목 부재, 빈 BootNext 및 기존 항목 동일이 예상됩니다. 스크립트는 펌웨어 쓰기, GPU 접근이나 자동 재부팅을 하지 않으며 로컬 결과 영수증만 저장합니다. PS7/5.1 모의 검사에서 기준/새 부팅 허용, 동일 세션 및 임시 BootCurrent 거부가 통과했습니다. 실제 일반 부팅과 기본 재준비 흐름은 별도 증거입니다.
