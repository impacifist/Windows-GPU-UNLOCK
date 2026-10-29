# 여섯 번째 AD104 부팅: 실시간 사전 검사 일치

[English](AD104-SIXTH-RESULT.en.md) | 한국어

2026-10-26 실제 부팅에서 이 캡처에 맞춘 실시간 토폴로지 검사를 통과했다.

```text
topology-status=0x0000000000000000
roles=0xf null-binding-present=1 matched-edges=0x08
first-rejection-step=0x0 second-rejection-step=0x0
shared-byte=0x01 shared-count1=1 shared-count2=1
```

두 번의 새 수집에서 고정한 네 드라이버의 로드 코드 식별 정보, 바인딩
콜백, null-image 바인딩 및 관련 활성 관계 여덟 개가 일치했다. 입력/콘솔
포인터와 PCI 구성의 일관성 검사도 통과했다. 이번 부팅에서 탐색 구현이
작동함을 확인한 것이며, 원자적 스냅샷이나 실제 키보드 동작의 증거는 아니다.

BAR0은 다시 32비트 메모리 범위 `fb000000..fbffffff`, 길이 `01000000`,
변환 값 0, BAR별 지원 플래그 0을 보고했다. 장치 전체 속성 조회도 성공했다.
현재 속성은 `718`, 지원 속성은 `1b77b`, PCI command는 `0007`이다.
BME는 여전히 켜져 있다. 전체 MMIO 허용 검사는 시도하지 않았다.

로그의 `release-authority=0`, `cold-state-known=0`,
`input-execution-tested=0`, `resource-admission-separate=1`은 그대로다.
Stop, DisconnectController, 속성 쓰기, GPU MMIO, ROM 복사 또는 unlock은
실행하지 않았다. 이 모드의 `capture-complete=0` 및 최종 `EFI_NOT_READY`는
예상된 결과다. 사용자의 Windows 복귀만으로 무인 복구 경로를 입증하지는 못한다.

33개 파일을 새 비공개 호스트 보관 폴더에 복사했고 각 USB/복사본의 SHA-256이
일치했다. `DIAG00.TXT`는 396,633바이트이며 SHA-256은
`8499c5df90fa10a9f79dee2d4ade30f8c10e03d2c6194989894f3cac414e2d73`이다.
USB EFI의 SHA-256은 준비한 28,672바이트 파일과 일치한다.
`df8245799c9c4ca010e56e71d7a21c486ac6933862eed6fd271b68bba0d7a169`.
원본 자료는 비공개로 유지하며 USB 원본도 보존했다.

같은 사전 검사로 다시 부팅해도 다음 질문에는 답할 수 없다. 다음 후보는
별도 보호 장치를 둔 일회성 EDID 소비자 수명 주기 실험이다.
[실험 계약](AD104-EDID-EXPERIMENT.ko.md)에 설명한다. 예상 전이는 실제
관측 결과나 unlock 자격이 아니다.
