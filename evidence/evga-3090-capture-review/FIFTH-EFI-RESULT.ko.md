# 다섯 번째 EFI 결과: 명시적 콘솔 프로토콜 구성

EVGA04는 Stop 호출 없이 이전과 같은 Windows 자동 복귀를 보였습니다. 새 ProtocolsPerHandle 목록은 시스템 콘솔에 GOP GUID가 0개, SimpleTextOut GUID가 정확히 1개임을 확인합니다. 그런데 GOP HandleProtocol은 성공 및 시스템 텍스트와 다른 포인터를 반환합니다. 원인은 아직 알 수 없으며 GOP 설치의 증거로 취급하지 않습니다. [정리된 증거](fifth-efi-run-result.json)를 참조하십시오.

다음 EFI는 매 그래프 검사에서 ProtocolsPerHandle로 콘솔 구성을 직접 확인합니다. GOP 0개와 텍스트 1개를 요구하며 오류, null 항목, 과도한 목록을 거부하고 FreePool 결과도 확인합니다. 목록에 없는 GOP 조회는 하지 않습니다. 이는 부재 확인 방법의 변경이며 추가 GOP를 허용하지 않습니다. 기존 GOP 형태, 소유권, 정확한 코드 지문, 새 G 입력, 일회성 마커, IMG13 Stop 한 번 및 사후 검사는 유지합니다. 추가 GPU 접근 또는 Stop은 없습니다. 펌웨어 조회 불일치 원인은 미해결입니다.

멤버십 호스트 검사 25개와 캡처 회귀 9개, EFI 컴파일 및 PE 검증이 통과했습니다. 이는 로컬 검사이며 실제 Stop 성공의 증거는 아닙니다.

GitHub Desktop으로 업데이트하고 기존 NEXT-EFI-TEST 지침에 따라 빌드 및 USB 준비 후 테스트하십시오. 기존 화면/입력 손실, 정지, Windows 부팅 실패 위험과 검증되지 않은 복구 경계가 적용됩니다. Live guards passed 이후 새 Shift+G만 기존 Stop 한 번을 승인합니다. EVGA05 또는 다음 로그와 Windows 복귀 결과를 보내십시오. unlock 성공은 주장하지 않습니다.
