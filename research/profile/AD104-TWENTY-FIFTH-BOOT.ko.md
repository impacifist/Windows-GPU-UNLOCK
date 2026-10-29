# 스물다섯 번째 후보: 제한된 GSP Falcon 기준 상태

[English](AD104-TWENTY-FIFTH-BOOT.en.md) | [한국어](AD104-TWENTY-FIFTH-BOOT.ko.md)

사용자가 명시된 새 MMIO 위험과 이 후보의 스테이징을 승인했습니다. 빌드/호스트 검사를 완료했으며 USB 스테이징 준비 상태입니다. 실제 테스트25는 아직 실행하지 않았습니다. 테스트24 원본은 스테이징 전에 보존합니다. 새 경계는 기존에 시험하지 않은 주소3개를 포함한 추가 BAR0 DWORD18회 읽기입니다. RAM 통합 반복이 아니라 실제 엔진 기준 상태 관측입니다.

테스트24 후 PROM 캡처/저장/메모리 해제를 마치고 Shift+V 전에9회씩 두 배치로 읽습니다:
`0, 0x110100, 0x110040, 0x110044, 0x11010c, 0x82381c, 0x823820, 0x625f04, 0`.
새 주소는 GSP Falcon MAILBOX0/MAILBOX1/DMACTL입니다. [NVIDIA GSP](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/main/src/common/inc/swref/published/ampere/ga102/dev_gsp.h)와 [Falcon 정의](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/main/src/common/inc/swref/published/ampere/ga102/dev_falcon_v4.h)는 주소를 뒷받침하지만 AD104 읽기 안전성을 보장하지 않습니다.

칩 시작/종료 값은194000a1, CPUCTL/SS0/SS1/VGA는 이전 두 번째 배치 값과 같아야 합니다. 새 배치 간 메일박스/DMA 변화는 관측 정보입니다. 원본 값과 안정성 플래그를 기록하며 DMACTL/메일박스만으로 유휴/완료/언락/실행 준비를 판단하지 않습니다. 향후 GSP 실행 설계의 기준 자료지만 SS1/VGA 진입 문제를 해결하지 않습니다.

배치 전/사이/후 및 로그 후 Mem.Read와 PCI/소유권/입력 상태를 검증합니다. 읽기 중 로그/파일 호출/할당은 없습니다. 한 번만 시도하며 읽기 오류, 모두1/BADF 값, 칩/기존 상태 불일치, 메서드/로그/하드웨어 오류는 재시도 없이 복귀를 차단합니다. 센티널 값 거부는 접근 불가 또는 허용되지 않은 관측이며 그 값이 메일박스에 불가능하다는 뜻은 아닙니다. 부분 결과도 기록합니다. 성공 시에만 기존 디코딩 해제/Windows 절차로 가며 실패 시 디코딩이 켜진 채 무기한 정지할 수 있습니다.

성공 시 Mem.Read18회 추가, 합계262338회(PROM262274회 포함)입니다. 새 GPU 쓰기, 리셋, 메일박스 지우기, 인터럽트 확인, DMA 시작, BME/VGA 활성화, 펌웨어 업로드/실행, 재연결, 언락 또는 자동 롤백은 없습니다. 새 읽기도 부작용/정지를 일으킬 수 있으며 화면/입력/Windows가 실패할 수 있습니다. 수동 전원 차단/USB 제거가 필요할 수 있고 복구는 미검증입니다. 사용자가 이 스테이징 경계를 명시적으로 승인했습니다.

네이티브 EFI66,560바이트, AMD64/가져오기 없음, SHA256:
`abf77fd008606705ab8e73e0bdc0bfe33b67cbacf35598b14615d4d6d2b11425`.
호스트는18개 읽기 실패 위치, 센티널/칩/선택자/메서드/상태, 시작/배치 사이/요약 로그 오류와 메일박스 변화를 확인합니다. RAM 모의 콜백이며 하드웨어 증거가 아닙니다.

검증된 스테이징 후 작업 저장/완전 종료/10초/UEFIUSB/Shift+G 한 번,10초 후 Shift+V를2초마다 최대3분 누릅니다.10분 후 Windows가 없으면 전원 차단/USB 제거/정상 Boot Manager를 선택합니다. 한 번만 실행하고 재스테이징 전에 모든 캡처를 보존합니다.

[Actual result: guarded stop/manual recovery / 실제 결과: 보호 정지 및 수동 복구](AD104-TWENTY-FIFTH-RESULT.ko.md).
