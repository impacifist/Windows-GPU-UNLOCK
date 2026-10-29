# 스물일곱 번째 결과: GSP 기능 관측과 복구 통과

[English](AD104-TWENTY-SEVENTH-RESULT.en.md) | [한국어](AD104-TWENTY-SEVENTH-RESULT.ko.md)

2026-10-27 실제 테스트27에서11회씩 두 배치, 총22콜백이 상태0으로 완료됐습니다. IRQSTAT=0x10, HWCFG2=0x47b7, HWCFG=0x80420100이 두 배치에서 같았습니다. 해석 필드는 RISCV 기능1, 메모리 스크러빙0, IMEM 크기 필드0x100입니다. 현재 선택/활성 코어나 검증된 메모리 용량의 증거가 아닙니다.

chip194000a1, FalconCPUCTL10, 메일박스0/0, SS024020000, SS1=5, VGA10e09가 기존 관측과 같고 DMACTL은 제외했습니다. 디코딩 해제/Windows진입/실험0, STATUS루트·로그 닫기0 및 StartImage-next0이며 사용자가 Windows 자동 복귀를 확인했습니다.

36파일 원본/복사본 SHA256을 검증했습니다. DIAG434590바이트 SHA2566862a61b64e4bf8415916a105428dfe4c30fbaae85771d41d48f31d5db5c9dee. 펌웨어 업로드/실행/언락은 없습니다. 기능 관측/복구1회 성공이며 일반적 복구 안전성이나 실행 진입 승인이 아닙니다. 다음 후보28은 새 주소3개로 기능과 코어/부트 상태를 구분하며 스테이징 전 별도 승인이 필요합니다. 체크리스트15/26(58%) 유지입니다. [후보28](AD104-TWENTY-EIGHTH-BOOT.ko.md).
