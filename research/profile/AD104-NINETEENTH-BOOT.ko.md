# 19차 참석 시험: 시간차 레지스터 안정성

실행 결과: [19차](AD104-NINETEENTH-RESULT.ko.md). 아래는 후보 계약입니다.


[English](AD104-NINETEENTH-BOOT.en.md) | 한국어

빌드·호스트 검사를 마친 후보이며 실제 실행 결과가 아닙니다. 성공한
[18차](AD104-THIRTEENTH-EIGHTEENTH-RESULT.ko.md)를 확장하지만 새 GPU 주소나 쓰기는 없습니다.

기존 다섯 중지와 Enable(0x300) 후21개 고유 레지스터 및 마지막 chip-ID 확인을 포함한
22회 읽기를 수행합니다. 첫 배치·로그 성공 후 대기 intent를 flush하고 Stall(1000000)을
한 번 호출하며 반환 상태를 flush합니다. 이후 PCI/속성/소유권/이전 자식 부재 전체 상태를
새로 수집한 뒤 두 번째 배치를 읽습니다. 메서드 식별자와 오류/sentinel 보호를 유지하고
배치 안에서는 로그를 쓰지 않습니다. Stall 부재·실패나 상태 변화 시 다음 MMIO를 막습니다.
1초는 펌웨어에 요청한 시간이며 실제 경과 시간을 측정한 값은 아닙니다.

두 배치가 정확히 일치해야 새 V 입력과 최종 검사를 수행합니다. Disable(0x300) 한 번으로
저장된 부모 중지 직후 전체 상태를 복원하며 두 번 및 flush 뒤 다시 검사한 후 Windows를
시작합니다. Disable 뒤 MMIO는 없습니다. 독립 STATUS.TXT는 main-log Close/StartImage 동안
열려 있고 ExitBootServices 이후 파일 접근 금지 보호를 유지합니다. 리셋, 재연결,
BME/VGA 활성화, PROM 읽기, 펌웨어 업로드, unlock 또는 rollback은 없습니다.

시간차 안정성/상태 정착과 복귀 재현성을 검사합니다. 참조 SS1=1, VGA_BASE=1은 관찰값
5/0x10e09와 여전히 다릅니다. 안정된 차이가 cold-entry/unlock을 허용하지 않습니다.
값이 변해도 보수적으로 복귀를 막으며 수동 전원 종료가 필요할 수 있습니다.
새 실행에서의 복구는 미입증입니다.

후보: import 없는53,760바이트 AMD64 PE, SHA-256
`f63b358ef0c42e314103ab07655d40f9c5d81a385c6685520a0e228a48734600`.
시간차 읽기 RAM 검사2,701개, 디코딩 비활성화2,074개와 기존 캡처/수명주기/PCD/입력/
복귀/컴파일 보호 검사를 포함합니다. mock은 실기기 증거가 아닙니다. 빌더는
시간차 읽기→decode-off→decode-read를 요구하고 manifest 대기는1,000,000us입니다.

작업 저장 후 완전히 종료하고10초 기다린 뒤 ASUS UEFI USB를 선택합니다. Shift+G를
한 번 누르고5초 기다린 뒤 Shift+V를2초마다 최대1분 누릅니다. 해제 후 추가 화면 안내는
없습니다. V는 decode-off 및 Windows 복귀를 허용합니다.5분 안에 Windows가 없으면
전원을 끄고 USB를 빼고 정상 Windows Boot Manager를 선택합니다. 한 번만 실행하고
재시도 전 캡처를 보관합니다. EFI 1024×768/Windows 5120×1440 종횡비 차이는 그대로이며
깜박이는 커서는 진행 표시가 아닙니다.
