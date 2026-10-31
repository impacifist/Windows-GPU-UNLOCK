# EVGA06 결과 및 다음 그래픽 소비자 해제 결합 테스트

[English](NEXT-GRAPHICS-RELEASE.en.md) | [한국어](NEXT-GRAPHICS-RELEASE.ko.md)

EVGA06는 IMG13/IMG16 Stop과 사후 검사 성공, 완전한 캡처 2개 및 ready-windows를 기록합니다. 참가자는 EVGA05와 같은 Windows 자동 복귀를 보고했습니다. 전체 기록 그래프 비교에서 핸들 추가/제거는 없고 가상 GOP의 IMG16 활성 open 및 텍스트 프로토콜 387477c2/e1e4a857이 제거되었습니다. 기존 IMG13 관련 d3b36f2c GUID도 제거됩니다. 제거된 텍스트 프로토콜에는 기존 IMG13 open이 있었습니다. 다른 활성 open과 두 GOP는 유지됩니다. [정리된 결과](seventh-efi-run-result.json). unlock은 없습니다.

## 다음 테스트: 빌드 완료, 실기 실행 전

기존 두 Stop 및 검사를 반복한 뒤 정확히 고정한 IMG17 Stop(binding, physicalGop, 1, &virtualGop)을 한 번 호출합니다. 물리 GOP의 combo open 2개 제거, 두 GOP 핸들/형태 유지, IMG16 텍스트 인터페이스/open 부재, 시스템 콘솔 텍스트 및 코드 지문 유지를 요구합니다. Windows 직전 최종 재검증을 수행합니다. Stop 이후 오류는 정지하며 재시도/부모 Stop/리셋을 추가하지 않습니다.

IMG17 자식 경로는 저장된 자식/컨트롤러 목록을 확인하고 GOP 참조 2개를 닫고 노드를 제거/해제한 뒤 helper0x594로 모드 목록을 재구성합니다. 마지막 멤버 helper0x156c는 화면 크기 버퍼 할당 및 인터페이스 오프셋0x10 간접 호출 후 집계 모드 필드를 변경합니다. 간접 대상의 전체 동작은 검증되지 않았습니다. GPU 비활동이나 무해함의 증거가 아닙니다. 성공 반환뿐 아니라 새 관계를 확인합니다. IMG17 자식 0개 경로는 호출하지 않습니다. EDID 및 NVIDIA 소유권은 유지하며 이후 과제입니다.

**새 위험:** OEM 마지막 멤버/helper 경로의 할당 및 간접 그래픽 호출을 실행합니다. 그래픽 상태 변경, 화면/입력 손실, 펌웨어/Windows 정지 또는 GPU 초기화 실패가 가능하며 수동 전원 종료와 USB 제거가 필요할 수 있습니다. 복구는 검증되지 않았고 자동 복원은 없습니다. 하네스의 직접 GPU MMIO/PCI 속성 쓰기, GPU 리셋, 펌웨어 업로드/실행, unlock은 추가하지 않지만 콜백의 간접 하드웨어 효과는 가능합니다. 위험을 읽고 실행 여부를 선택하며 별도 확인 메시지는 필요하지 않습니다.

GitHub Desktop으로 업데이트 후 저장소 루트에서 실행하십시오.

```powershell
.\research\profile\build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:\Program Files\LLVM\bin\clang.exe" -LldLink "C:\Program Files\LLVM\bin\lld-link.exe" -ReleaseGraphics
.\research\profile\stage-evga-text-stop.ps1 -Drive D
```

빌드는 이전 두 단계를 포함합니다. 새 EVGA-GFX-RELEASE.ARMED가 재시도를 막으며 기존 마커 2개는 보존합니다. 준비 도구는 정확한 EVGA06 해시 및 모드/소스/EFI/ESP를 확인하고 캡처/백업을 보존합니다. 오류나 마커를 우회하지 마십시오.

완전 종료 → 일회성 UEFI USB 부팅 → Live guards passed 이후 새 Shift+G 한 번. Shift+V는 없습니다. 이후 화면은 진행 표시가 아닙니다. 3분 내 Windows가 시작되지 않으면 전원 종료/USB 제거 후 정상 Windows Boot Manager를 사용하십시오. 마커를 보존하고 반복하지 마십시오. EVGA07 또는 다음 로그, 빌드/준비 출력과 자동/수동 Windows 복귀 결과를 보내십시오. 오프라인 검증은 여기서 처리합니다.

기존 25개, 두 Stop 7개, 세 Stop 8개, 캡처 회귀 9개 호스트 검사가 통과했습니다. EFI 경고 오류 처리 빌드 및 PE 검증도 통과했습니다. 이는 모의/빌드 검사이며 실기 검증은 아닙니다.
