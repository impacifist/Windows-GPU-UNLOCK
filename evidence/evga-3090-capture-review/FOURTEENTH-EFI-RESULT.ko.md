# EVGA13 해제 후 접근 결과

[English](FOURTEENTH-EFI-RESULT.en.md) | [한국어](FOURTEENTH-EFI-RESULT.ko.md)

EVGA13은 Stop콜백8회와 사후 검사, MEMORY전용 활성화와 명령2검사 성공을 기록했습니다. BAR0칩ID2회 모두 b72000a1이며 비활성화 및 명령0/저장속성 검사2회도 성공했습니다. 캡처2회와 Windows준비 이벤트가 완료되었습니다. 운영자는 로그/마커를 업로드했으나 Windows자동/수동 복귀는 명시하지 않아 미확인입니다.

이 실행에서 제한된 해제 후 칩 접근과 기록된 정리 경로가 확인되었습니다. 나머지 콜드 레지스터, 해제 후 PROM동일성, DMA매핑 또는 잠금 해제는 입증되지 않았습니다. 최종 무로그 검사 자체는 별도 기록이 아닙니다. 원본은 Git에서 제외합니다. [정리 결과](fourteenth-efi-run-result.json). SHA256 4c381605ffb0a25e07414388758d6bb2cb542b4ab4e04e34610d8fd4a9ec3864.
