# EVGA15 런타임 PCI/BAR 결과

[English](SIXTEENTH-EFI-RESULT.en.md) | [한국어](SIXTEENTH-EFI-RESULT.ko.md)

정확한 식별자를 사용하는 공유 PCI 검증기가 해제 후 메모리 디코딩 활성/BME 비활성 상태의 전체256바이트 설정을 허용했습니다. BAR0 주소는0x84000000..0x84ffffff, 길이는0x1000000입니다. PM/MSI/PCIe 위치는0x60/0x68/0x78이며0xc8의 MSI-X 헤더는 목록에 연결되어 있지 않습니다. BAR 설명자, 안정된 상태 값, 원본 PROM 해시, 메모리 비활성화/복원, 캡처2회 및 Windows 준비 검사가 통과했습니다. 실패 보고가 없으면 정상 Windows 복귀를 가정하는 보고 규칙을 적용합니다. 이번 실행에 대한 별도의 명시적 복귀 확인은 없습니다.

[정리 결과](sixteenth-efi-run-result.json)는 설정 DWORD64개와 원본 해시를 보존합니다. 원본 토폴로지 로그는 제외합니다. 이는 검사 당시의 PCI/BAR 조회를 검증하며 독점 소유권, DMA, 펌웨어 실행 또는 잠금 해제 증거는 아닙니다. 다음은 GPU 업로드/DMA 실행 없이 EFI 자산 로드/해시와 공유 독점 PCI 소유권 획득/반환을 결합합니다.
