# 스물두 번째 결과: EFI FWSEC 감사 통과

[English](AD104-TWENTY-SECOND-RESULT.en.md) | [한국어](AD104-TWENTY-SECOND-RESULT.ko.md)

2026-10-27 테스트22는 캡처한 CPU 소유 메모리에서 운영 FWSEC 파서와 해시 감사를 완료했습니다. PROM, 기술자 구조, 이미지 및 서명 영역 일치 값은 모두 1입니다. 이미지 오프셋/크기는 0x45480/0x10300, 서명 영역은 0x45180/768입니다. 암호학적 인증이나 펌웨어 실행을 입증하지 않습니다.

36개 파일을 비공개로 보존하고 SHA-256을 검증했습니다. PROM.BIN은 테스트21과 동일합니다(1,048,576바이트, SHA-256 accf2a054de0ec394078f933f74356ed7c4057ee0bd1ce6734cacda54ca9e1e1). 전송, 정리, 최종 상태, 디코딩 해제 및 Windows 진입 검사가 모두 성공했습니다. STATUS는 루트/로그 닫기 성공과 StartImage-next를 기록하며, 사용자는 Windows 자동 복귀를 별도로 확인했습니다.

firmware-authenticated=0, cold-entry-admitted=0, execution-authority=0입니다. SS1/VGA/BAR 및 기준 ROM 차이는 미해결입니다. 체크리스트는 15/26(58%)로 유지됩니다. 반복 캡처와 EFI 파서 통합은 실행 준비 완료에 해당하지 않습니다.

다음은 CPU 메모리 복사본 패치와 가상 서명 인덱스 계산의 통합 리허설입니다. 호스트 테스트는 바이트 변환을 검증하고 EFI는 캡처/복구와의 통합을 확인합니다. GPU 접근이나 펌웨어 실행은 추가하지 않습니다. [테스트23](AD104-TWENTY-THIRD-BOOT.ko.md).
