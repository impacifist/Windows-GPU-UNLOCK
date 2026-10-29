# 스물여덟 번째 후보: GSP 코어 및 부트 상태 관측

[English](AD104-TWENTY-EIGHTH-BOOT.en.md) | [한국어](AD104-TWENTY-EIGHTH-BOOT.ko.md)

검토용 빌드/호스트 검사 완료, 사용자가 새MMIO 위험과 스테이징을 명시적으로 승인했습니다. 실제28은 센티널 보호 검사에서 정지했으며 검증된 스테이징 전에27원본을 보존합니다.27의 RISCV 기능 비트는 현재 활성/선택 코어를 입증하지 않으므로 관련 상태/제어 필드를 변경 없이 읽습니다.

14회씩 두 배치:
`0, 0x110100, 0x110040, 0x110044, 0x110008, 0x1100f4, 0x110108, 0x1103c0, 0x111388, 0x111668, 0x82381c, 0x823820, 0x625f04, 0`.
새 주소는 GSP ENGINE0x1103c0, RISCV CPUCTL0x111388, BCR_CTRL0x111668이며 각각 두 번 읽습니다. [NVIDIA GSP](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/main/src/common/inc/swref/published/ampere/ga102/dev_gsp.h)는 ENGINE/reset비트0, [RISCV 정의](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/main/src/common/inc/swref/published/ampere/ga102/dev_riscv_pri.h)는 기준0x111000과+0x388/+0x668, CPU active7/halted4 및 BCR valid0/configured-core4를 명시합니다. 로컬610.43.03 정의도 확인했으나 AD104 접근 안전성을 보장하지 않습니다.

원본과 필드만 기록합니다. BCR 코어 선택은 구성 값(0Falcon/1RISCV)으로 VALID와 함께 해석하며 VALID1도 현재 실행을 입증하지 않습니다. active/halted/reset은 관측 상태이지 준비/인증/언락 증거가 아닙니다. 새 값의 배치 간 변화는 거부하지 않습니다. 리셋 설정/해제, 시작/중지, 코어 전환, 부트 명령 또는 확인 쓰기는 없습니다.

기존 칩/CPUCTL/SS0/SS1/VGA 일치 및 전송/모두1/BADF/메서드/PCI/입력/소유권/로그 검사를 유지합니다. 읽기 중 파일/콘솔/할당 콜백 없이 로그 전후 전체 상태를 검증합니다. 한 번만 시도하며 실패하면 디코딩이 켜진 채 영구 정지할 수 있고 Shift+V로 재개할 수 없습니다.

성공 시 추가읽기는27의22회에서28회(전체262348회)입니다. DMACTL은 제외하고 새GPU쓰기/리셋/DMA시작/BMEVGA활성화/업로드·실행/재연결/언락/자동롤백은 없습니다. 새읽기는 부작용/정지와 화면/입력/Windows실패를 일으킬 수 있어 수동 전원 차단/USB 제거가 필요할 수 있고 복구는 미검증입니다. 이 스테이징 경계의 명시적 승인을 받았습니다.

호스트는25/26/27/28모드,28개 전송 오류 위치, 새레지스터별 모두1/BADF 거부 및 값 변화 허용과 기존 상태/메서드/로그 검사를 확인합니다. AMD64/가져오기 없음 EFI67,584바이트 SHA256:
`5cb2e6d75b1ff7dda0872b903077bd06349544eec9ae3999f4fdf65fde3a0316`.
코어 상태=true, 추가28, 리셋/DMACTL/펌웨어실행/언락=false입니다. 호스트 증거이며 실제 실행 성공이 아닙니다. 체크리스트15/26(58%) 유지입니다.

검증된스테이징 후 작업저장/완전종료/10초/UEFIUSB/Shift+G한번,10초후Shift+V를2초마다최대3분.10분후Windows가없으면전원차단/USB제거/정상BootManager.한번만실행하고캡처보존,복구미검증입니다.

[Actual result / 실제 결과](AD104-TWENTY-EIGHTH-RESULT.ko.md).
