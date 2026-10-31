# EVGA14 콜드 상태와 PROM 결과

[English](FIFTEENTH-EFI-RESULT.en.md) | [한국어](FIFTEENTH-EFI-RESULT.ko.md)

두 배치 값은 GFW PLM8b8f, BOOT3ff, CPUCTL10, WPR2_HI0, feature PLMffffff8f, SS0 30053014, SS1 7, 메모리6000으로 일치합니다. 전체1MiB PROM해시는 원본과 같습니다. 디코딩 비활성화/복원, 캡처2회, Windows준비 검사가 통과합니다. 운영자는 01:16에 Shift+G 이후 Windows가 정상적으로 시작되었다고 확인했습니다. 준비 이벤트와 별도로 운영자 보고에 따른 복귀 증거입니다.

비선택자 값은 app.c ColdCheck비교를 충족하지만 고정 stock선택자와 SS0/SS1이 다릅니다. port.c GuLatchEntrySelectors는 관측값 범위를 허용합니다. 이는 진입값 래치 정책의 오프라인 호환성이지 업로드, 선택자 변경 또는 잠금 해제 성공이 아닙니다. 다른 시스템의 고정선택자로 대체하지 마세요. PCI/BAR/설정, 소유권 전달과 전체 런타임통합 검증은 남아 있습니다.

[정리 결과](fifteenth-efi-run-result.json). 원본은 제외합니다. 다음은 CPU전용 자산 생성이며 완료된 상태 캡처를 위한 EFI재실행은 요청하지 않습니다.
