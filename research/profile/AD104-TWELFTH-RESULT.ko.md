# 열두 번째 AD104 결과: 디스플레이 자식 제거 및 Windows 복귀

[English](AD104-TWELFTH-RESULT.en.md) | 한국어

2026-10-27 MSI RTX 4070 Ti(AD104, `10de:2782 / 1462:5132`)에서 IMG29의 직접
자식 하나 Stop이 성공했습니다. 기존 텍스트/GOP 소비자/EDID 해제 뒤 정확히
고정한 바인딩의 `Stop(binding, gpu, 1, &child)`를 호출했습니다. 자식 없는 부모
Stop은 호출하지 않았습니다.

두 번의 사후 수집, 새 전체 핸들 탐색에서 이전 자식 부재 확인, 최종 키보드,
최종 상태 및 Windows 인계 조건이 모두 성공했습니다. 사용자는 Windows 부팅을
확인했습니다. 관련 관계는 `8 → 6 → 5 → 4 → 3`으로 바뀌었습니다. 남은 세 관계는
PCI BY_DRIVER 소유권과 포인터 두 관계입니다. 공유 바이트는0, 포인터 개수는
`1/1`입니다. 직접 호출 후 이전 자식 프로토콜을 질의하거나 저장한 GOP/mode
할당을 역참조하지 않습니다. Stop 성공 코드만 믿지 않고 새 전체 핸들 탐색에서
이전 자식이 없는지 확인했습니다.

전체256바이트 PCI 구성 스냅샷 열 개는 동일합니다. command `0007`(BME 활성),
속성 `0718`, 지원 `1b77b`가 유지되었습니다. 현재 Windows에서 RTX 4070 Ti와
NVIDIA 드라이버 `610.88`을 확인했습니다. 사용자는 텍스트 삭제, 깜박이는 밑줄과
잠깐의 오래된 형태 Windows 부팅 화면을 관측했습니다. 레거시 비디오 영향과
일치할 수 있지만 Windows 부팅 모드 변경이나 unlock을 입증하지는 않습니다.

34개 캡처와 매니페스트를 비공개 호스트 저장소에 복사하고 USB/복사본 SHA-256을
모두 확인했습니다.411,742바이트 DIAG 로그 SHA-256:
`a93f83a9338b974fbf6d27eb184f0048119f78a9d08038b7b4e25ee2e24c9945`.
46,592바이트 EFI SHA-256:
`bb82bd260c9b8df0142318692a090b8f28ecf8ecb8e3858593d2f145c8b20975`.
이 EFI는 이후 main 병합 전에 구현 `4eb0c15`에서 빌드했습니다. 원시 펌웨어,
비공개 디스크 식별자 또는 절대 펌웨어 주소는 공개하지 않습니다.
[정제된 증거](ad104-twelfth-boot-evidence.json)를 참조하세요.

이는 이번 실행의 자식 제거와 Windows 복귀를 입증합니다. 전체 PCI 소유자 해제,
BME 비활성/cold-entry 조건, FWSEC/WPR2 트랜잭션 또는4070 Ti unlock은 입증하지
않습니다. [#54](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/54)의 AD103
FWSEC 실패를 해결한 결과가 아니며 [#2](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/2)의
전체 RTX4000 호환성을 입증하지도 않습니다.

다음 단계는 별도 속성/구성/입력/복구 조건을 갖춘 부모 Stop 실험 설계입니다.
부모 Stop 시험은 준비하지 않았습니다. [열두 번째 계약](AD104-TWELFTH-BOOT.ko.md)과
[입증 단계](AD104-UNLOCK-EVIDENCE.ko.md)는 완료 단계와 남은 조건/트랜잭션을 구분합니다.
이 결과 보고서를 작성하면서 추가 하드웨어 동작은 수행하지 않았습니다.
