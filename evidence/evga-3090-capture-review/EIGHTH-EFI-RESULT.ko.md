# EVGA07: 소비자 Stop 3단계 완료

[English](EIGHTH-EFI-RESULT.en.md) | [한국어](EIGHTH-EFI-RESULT.ko.md)

EVGA07은 IMG13, IMG16, IMG17 자식 Stop이 각각 한 번 성공하고 후속 검사, 완전한 캡처 2회 및 ready-windows가 완료되었음을 기록합니다. Movian은 이전과 같은 자동 Windows 복귀를 보고했습니다. 기여자 EFI는 23040바이트, SHA256 db0044a358f52aef115f466a1195f43631f5e15fcb91f70056612a5f50aa7fbc 입니다. [요약 결과](eighth-efi-run-result.json).

전체 기록 그래프를 로컬 비교한 결과 핸들 549개와 GOP 2개가 유지됩니다. 물리 GOP의 IMG17 활성 open 2개가 제거되었고, 가상 GOP의 IMG16 활성 open 1개 및 기존 텍스트 관련 프로토콜 3개가 제거되었습니다. 제거된 텍스트 프로토콜에는 IMG13 활성 open 2개가 있었습니다. 나머지 기록된 활성 open은 동일하며 추가 프로토콜은 없습니다. 수동 open 횟수는 캡처 조회에 따라 달라질 수 있습니다.

IMG07은 물리 GOP와 GOP가 없는 형제 2개를 포함한 EDID-discovered 핸들 3개를 계속 소유합니다. EDID-discovered GUID는 1c0c34f6-d380-41fa-a049-8ad06c1a66aa 입니다. c7a7030c-c3d8-45ee-bed9-5d9e76762953은 별도 시스템 콘솔 프로토콜이므로 EDID 선택에 사용하면 안 됩니다. NVIDIA 소유권도 유지됩니다. 전체 GPU 해제, 프로필 승인, 복구 신뢰성 또는 언락 증거가 아닙니다.
