# 검토 후 새 슬롯 EVP11 재시험

[English](EVP11-RETRY.en.md)

기여자는 EVP10 정리 성공, USB 항목/실패 증거 보존, BootOrder 쓰기 및 GPU 접근 없음을 보고했습니다. EVP10 결과는 여전히 실패입니다. SULK02 진입 SS1은 7, EVP10은 0이었으며 원인은 알 수 없습니다.

명시적 `-ReviewedEvp10Retry`는 이 특정 실패와 정리 다음의 슬롯 11만 허용합니다. 실패 증거 세 해시, 실패/취소 상태, 정리 영수증, 볼륨 식별과 이전 임시 항목 부재를 확인합니다. 일반 실패나 슬롯 11 실패는 우회할 수 없습니다. SS1=7과 새로운 Shift+G를 포함한 모든 장비 검사는 유지합니다. 자동 복구/재부팅/재시도를 추가하지 않습니다.

브랜치를 업데이트하고 원래 USB를 D:로 연결한 후 저장소 루트 관리자 PowerShell에서 실행하세요.

```powershell
./research/profile/manage-evga-profile.ps1 -Action Rearm -ReviewedEvp10Retry
```

`prepared: true`, `slot: 11`, `run: EVP11`을 확인합니다. 오류 시 중지하고 보내세요. 상태/영수증을 초기화하지 마세요. 준비 후 USB를 제거하고 작업을 저장한 다음 Shift를 누른 채 Windows 종료를 선택하여 빠른 시작을 우회한 완전 종료 후 전원을 켭니다. 재시작/완전 종료 차이에 대한 가설 시험이며 검증된 해결책이 아닙니다. BootNext가 새 시험을 선택합니다. 일반 부팅 메뉴만 나타나면 설정을 바꾸지 말고 보고하세요.

실시간 검사 통과 후 새로운 Shift+G를 한 번 누르고 최대 3분 기다립니다. 계속 멈추면 전원을 끄고 일반 Limine/Windows로 부팅하세요. 복구는 보장되지 않습니다. 허용되면 기존 드라이버 해제, decode, MMIO, DMA/펌웨어 및 리셋 트랜잭션을 수행하며 멈춤이나 Windows/GPU 초기화 실패가 가능합니다. SS1이 계속 0이면 다시 거부하고 멈춥니다. Shift+V 입력이나 EFI 반복을 하지 마세요.

Windows 복귀 후 `./research/profile/manage-evga-profile.ps1 -Action Collect`를 실행하고 JSON/ZIP, 직접/수동 복귀, 완전 종료 및 USB 부재를 보내세요. 참석형 시험이며 영구/무인/Secure Boot 지원을 주장하지 않습니다.

모의 펌웨어/저장소/해시를 사용한 PowerShell 7과 5.1 검사에서 명시적 슬롯11 경로, 실패 결과10 보존 및 일반/변조/반복 거부가 통과했습니다. EFI 빌드/PE 검증은 완료했으며 실제 슬롯11 성공은 아직 없습니다.
