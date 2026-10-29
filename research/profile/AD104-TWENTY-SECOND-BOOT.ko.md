# 22차 참석 후보: 관찰 전용 FWSEC RAM 검사

[English](AD104-TWENTY-SECOND-BOOT.en.md) | 한국어

빌드·호스트 검사 완료, 실제 실행 전입니다. [21차](AD104-TWENTY-FIRST-BOOT.ko.md)와
GPU 작업이 같습니다: PROM262,274회, chip2회, 레지스터44회, 기존 다섯 중지와
Enable/Disable(0x300). 새 GPU 주소·쓰기·펌웨어 업로드/실행·리셋·selector latch·unlock은 없습니다.

전체 캡처 후 파일 저장 전에 정확한21차 PROM 해시를 비교합니다. 알려진 ROM만 CPU가
소유한 RAM에서 생산 파서로 처리합니다. Descriptor16개 필드 및 이미지/서명/DMEM의
독립 범위 검사, 이미지 및768바이트 서명 영역 해시 일치를 요구합니다. 바이트 해시
일치이며 암호학적 서명 인증이 아닙니다.

Gu.profile_verified/prom_verified나 트랜잭션 문맥을 사용하지 않고 별도 관찰 결과만
기록합니다. 모든 결과는 firmware-authenticated=0, cold-entry-admitted=0,
execution-authority=0을 명시합니다. 검사 불일치만으로는 기존 복귀를 막지 않고
PROM.BIN을 보관한 뒤 캡처/상태/decode-off 복귀 계약을 유지합니다. 로그·전송·파일·정리·
하드웨어 상태 실패는 여전히 정지합니다. 실패한 검사가 트랜잭션을 허용할 수 없습니다.
파일 재읽기는 콜백 전 원본 해시에 묶이고 검사 로그 및 I/O 뒤 새 상태 검사를 수행합니다.

이 wrapper translation unit에서만 파서 진단 매크로를 비활성화해 파싱 중 콘솔/파일
콜백이 없습니다. 최소 freestanding errno 상수와 RAM memcpy/memset 선언은 기존
현지 efi_runtime에 연결하며 OS 서비스/트랜잭션 드라이버를 연결하지 않습니다. 사용하지
않는 파서 patch helper는 function-section 링크에서 제거합니다. Native map에서
patch_frts, sig_offset, GuWrite, GuAd104Prepare, unlock_transaction 심벌 부재를 확인했습니다.

호스트 검사는16개 필드 변경, 범위/null/미지 ROM 거부, 실제 비공개21차 PROM 양성
검사, 원본 불변, 검사 불일치 시 복귀를 포함합니다. 검사 로그 오류/BME/소유권 변경은
정지합니다. 실제 캡처는 양성32개 검사를 통과했습니다. 호스트 파싱은 이미 입증됐고
다음 부팅 목적은 EFI 통합과 같은 부팅 재현성이며 발견·인증·cold-entry 준비성 입증이 아닙니다.

관찰 ROM 해시는accf2a05…a9e1e1, 이미지0x45480/0x10300, 서명0x45180/768입니다.
참조 PROMc81eb48f…3ad3fb4 및0x45488/0x45188과 다르고 독립 내용 비교를 위한 참조
payload 바이트는 로컬에 없었습니다. SS1/VGA_BASE/BAR도 미해결이며 조건을 낮추지 않습니다.

Native61,952바이트 AMD64 PE/import 없음, SHA-256은
`7e10d34969bd4ae6a2f791599bb0601f62dcb5506a65c868008d643a93194a86`입니다.
Manifest는 관찰 전용=true/펌웨어 실행=false/unlock=false이며 빌더는
FWSEC-audit→full-PROM→header→delayed→decode-off→read를 요구합니다.

21차와 같은 참석 안내: 완전 종료,10초 대기, UEFI USB, Shift+G 한 번.10초 후
Shift+V를2초마다 최대3분 누릅니다. 내부 새 V 대기는 검사 후60초이며 사용자3분은
내부 타이머가 아닙니다.10분 안에 Windows가 없으면 전원을 끄고 USB를 제거한 뒤 정상
Windows Boot Manager를 고릅니다. 콜백이 멈출 수 있고 새 실행의 복구는 미입증입니다.
한 번만 실행하고 재시도 전에 모든 캡처를 보관하십시오.

[Physical result / 실제 결과](AD104-TWENTY-SECOND-RESULT.ko.md).
