# 다음 EFI: NVIDIA PCI 부모 Stop

[English](NEXT-NVIDIA-PARENT.en.md) | [한국어](NEXT-NVIDIA-PARENT.ko.md)

[EVGA11](TWELFTH-EFI-RESULT.ko.md)은 자식 해제 단계에서 매우 좋은 결과입니다. 자식3개 Stop 성공/후속 검사, 핸들3개 제거, 가상 GOP1개/EDID-discovered0개, PCI 소유권 유지 및 긴 지연 후 자동 Windows 복귀가 확인되었습니다. 언락은 미입증입니다. 해제 테스트 후 step4 언락 시도 전 잠긴 상태 벤치마크를 저장하고 이후 동일한 작업/설정으로 비교해야 합니다.

새 모드는 기존 콜백7회와 사전 검사를 반복한 뒤 IMG45 Stop(binding, PCI parent, 0, NULL)을 한 번 호출합니다. 직전 검사에서 물리/EDID 자식과 부모 경로 활성 open이 없고 정확한 코드/PCI 소유자, 저장 자식NULL/flag98=0을 요구합니다. NVIDIA 자식0개 경로0x6e1은 부모 private 객체를 조회하고 flag98이0이 아니면0x731의 간접 메서드를 호출한 뒤 private 프로토콜 제거 및0x135c를 수행합니다. 이 함수는 할당을 해제하고0x113c의 객체0x48/0x40/0x38 간접 소멸자를 호출하며 PCI 소유권을 닫고 부모 저장소를 해제합니다. 전체 간접/서비스 동작은 검증되지 않았습니다. 관측된 flag0은0x731 조건부 호출만 건너뛰며 나머지 정리가 무해하다는 뜻은 아닙니다. 캡처 코드는 로컬 실행하지 않았습니다.

후속 검사는 기존 가상 GOP/시스템 콘솔/자식 부재 조건을 유지하고 PCI 활성 소유자 및 새 ProtocolsPerHandle 목록의 부모 private 프로토콜 부재를 요구합니다. 해제된 부모 객체는 조회/역참조하지 않습니다. NVIDIA binding/코드 핀은 유지되어야 합니다. 기존 PCI 구성 첫16 DWORD만 읽어 GPU/서브시스템/클래스 일치를 확인하고 command word를 기록합니다. decoding 활성 상태를 가정하거나 강제하지 않습니다. 파일 종료 후 로깅 없이 재검사한 뒤 미리 로드한 Windows로 이동합니다. 실패 시 정지하며 재시도/재연결/decoding 활성화/GPU 리셋/자동 롤백은 없습니다.

**새 위험:** NVIDIA의 더 넓은 객체/리소스 소멸자와 PCI 소유권 종료에 진입합니다. 간접 메서드가 GPU 상태/decoding/매핑을 바꿀 수 있고 전체 하드웨어 효과는 미검증입니다. 화면/입력 손실, 펌웨어 정지, 부분 정리 또는 Windows/GPU 초기화 실패로 수동 전원 종료/USB 제거가 필요할 수 있습니다. 복구는 미입증이며 자동 복원은 없습니다. 하네스는 직접 MMIO/PCI 속성 쓰기, DMA 시작, 명시적 리셋, 펌웨어 업로드/실행 또는 언락을 추가하지 않지만 간접 효과를 배제하지 못합니다. 위험을 읽고 실행 여부를 선택하며 별도 동의 답변은 필요 없습니다.

GitHub Desktop으로 같은 브랜치를 업데이트한 뒤 실행합니다.

```powershell
.\research\profile\build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:\Program Files\LLVM\bin\clang.exe" -LldLink "C:\Program Files\LLVM\bin\lld-link.exe" -StopNvidiaParent
.\research\profile\stage-evga-text-stop.ps1 -Drive D
```

새 마커 EVGA-NVIDIA-PARENT.ARMED 와 기존 마커7개/로그/번호별 백업을 보존하세요. EVGA05~11 및 소스/EFI/ESP/USB 핀을 검사합니다. 오류 시 중지하고 마커 삭제/우회를 하지 마세요.

완전 종료 → 일회성 UEFI USB → Live guards passed 이후 새 Shift+G 한 번. Shift+V는 없습니다. G 이후 화면은 진행 표시가 아닙니다. 3분 내 Windows가 없으면 전원 종료/USB 제거/정상 Windows Boot Manager를 사용하세요. 반복하지 말고 EVGA12 또는 다음 로그, 빌드/스테이징 출력 및 자동/수동 Windows 복귀 결과를 보내주세요. 저장 로그 분석은 여기서 수행합니다. 향후 언락 트랜잭션 전에 벤치마크가 반드시 필요합니다.

모의 검사107개 통과(기존96+부모11). 성공/로깅 없는 최종 검사, 부모 콜백 실패, PCI 소유자/private 프로토콜 잔류, 이전 자식 실패 후 중지, 마커/해시 거부 및 최종 PCI 식별 변화 등을 포함합니다. EFI Werror/PE 통과:28672바이트 SHA256aae2237f5463ad11133f154f3d3a07d9029d29f000312d4a530d6f210fe4acad, AMD64/imports없음/NX/dynamic base. 스테이징 문법/새 마커 및 stageddiff 검사 통과. 로컬 빌드만 했고 스테이징/실기 실행하지 않았으며 기여자 도구 체인 해시는 다를 수 있습니다. 새 부모 경로 실기 미검증이며 언락 증거는 없습니다.
