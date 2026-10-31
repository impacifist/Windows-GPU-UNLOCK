# EVGA12 NVIDIA 부모 해제

[English](THIRTEENTH-EFI-RESULT.en.md) | [한국어](THIRTEENTH-EFI-RESULT.ko.md)

EVGA12는 소비자 Stop 6회와 NVIDIA 자식 3개 Stop 이후 NVIDIA 부모 Stop 성공을 기록합니다. 부모 해제 후 PCI 명령 워드는 `0x0000`이며 사후 검사를 통과하고 캡처 2회가 완료되었습니다. Windows 준비 이벤트도 성공했습니다. 운영자는 이전과 동일하게 지연 후 Windows로 자동 복귀했다고 보고했습니다.

기록된 그래프를 독립적으로 비교하면 핸들이 549개에서 546개로 줄었으며 자식 핸들 3개만 제거되고 추가는 없습니다. 부모 비공개 프로토콜과 NVIDIA PCI BY_DRIVER 열기, 부모 장치 경로의 자식 열기가 제거되었습니다. 가상 GOP만 남고 EDID-discovered 핸들은 없습니다. 이 정확한 시스템에서 중요한 소유권 해제 결과이지만 잠금 해제나 모든 잠금 해제 전제 조건의 충족을 입증하지는 않습니다. 간접적인 하드웨어 효과 전체가 규명된 것도 아닙니다.

잠금 해제 트랜잭션, 펌웨어 업로드 또는 명시적 GPU 리셋은 포함하지 않았습니다. Windows 직전 최종 검사는 의도적으로 로그를 쓰지 않으므로 준비 이벤트 및 운영자 보고와 최종 검사 자체의 기록을 구별해야 합니다.

[결과](thirteenth-efi-run-result.json). 원본 EVGA12는 Git에서 제외됩니다. 다음은 step4 잠금 해제 시도 전 [기준 벤치마크](NEXT-BENCHMARK.ko.md)입니다. 이후 비교를 위해 동일한 실행 파일과 설정을 보존합니다.
