# 스물네 번째 결과: 관측 샘플 기반 서명 조립 통과

[English](AD104-TWENTY-FOURTH-RESULT.en.md) | [한국어](AD104-TWENTY-FOURTH-RESULT.ko.md)

2026-10-27 실제 테스트24에서 raw1/버전1, 서명 오프셋384, supported1, image-match1, 변경393바이트를 확인했습니다. 선택 서명 SHA2567806dc110f1bbf58f3b41f4414c4e0426000b0d985a66013d035a6ea4d0e6dd5와 조립 RAM SHA2565004460ab7fea25b83cc95b6468456d830b98700926ed0c8696e49505eb19d07은 호스트 값과 같습니다. 버전 샘플은 PROM 전에 수집했으며 새 퓨즈 읽기는 없습니다.

36개 파일의 원본/복사본 SHA256을 검증했습니다. DIAG00.TXT432202바이트, SHA2569377ab508b8204f92e82d0ec42a136305e5f053fba129926888f5fddd03b162d. PROM은 accf2a054de0ec394078f933f74356ed7c4057ee0bd1ce6734cacda54ca9e1e1로 동일합니다. Windows 진입/실험 상태0, 루트/로그 닫기0 및 StartImage-next0이며 사용자는 Windows 자동 복귀를 확인했습니다.

계획된 EFI RAM 조립 통합을 완료했지만 펌웨어 인증/실행/준비를 입증하지 않습니다. 권한/업로드/실행은0, 체크리스트는15/26(58%)입니다. 다음 후보는 새 주소를 읽어 GSP 엔진 기준 상태를 수집하므로 USB 스테이징 전에 별도 승인이 필요합니다. [테스트25](AD104-TWENTY-FIFTH-BOOT.ko.md).
