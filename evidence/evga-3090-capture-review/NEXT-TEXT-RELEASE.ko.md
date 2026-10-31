# 다음 EFI: IMG13 멤버십 및 IMG16 텍스트 해제 결합

[English](NEXT-TEXT-RELEASE.en.md) | [한국어](NEXT-TEXT-RELEASE.ko.md)

로컬 빌드 완료, USB 준비 및 실기 실행 전입니다. 성공한 EVGA05의 첫 Stop과 새 IMG16 Stop 한 번을 같은 부팅에서 결합합니다. unlock은 아직 입증되지 않았습니다.

## 검토 및 순서

IMG13에서 사라진 GUID d3b36f2c-d551-11d4-9a46-0090273fc14d는 기존 정적 검토의 UninstallMultipleProtocolInterfaces 인자와 일치하며 EVGA05에서 실제 제거를 관측했습니다. 다음 호출 전 해당 GUID 부재를 요구합니다. 다른 간접 상태 변화가 없다고 추론하지 않습니다.

IMG16 Stop RVA0xb2c의 검토 경로에는 중첩 로컬 helper 호출이 없습니다. 텍스트 인터페이스 조회, 참조 닫기, SimpleTextOut 및 e1e4a857-c970-4075-a4da-e9c41b69adfc 제거, 실패 분기, GOP 닫기 및 메모리 3개 해제를 수행합니다. 서비스 테이블 전역0x3820은 이전에 BootServices 초기화로 추적했습니다. 모든 CloseProtocol/FreePool 결과를 개별 확인하지 않으므로 사후 상태 검사가 필요합니다. 해당 경로는 자식 수로 분기하지 않으며 하네스는 0/NULL을 전달합니다.

1. 완전한 사전 캡처, 정확한 EVGA 식별/GOP/소유권/코드 검사 후 새 Shift+G를 받습니다.
2. 새 EVGA-TEXT-RELEASE.ARMED를 저장합니다. 기존 EVGA-TEXT-STOP.ARMED는 삭제하거나 재사용하지 않습니다.
3. 검증된 IMG13 자식 Stop을 반복하고 텍스트 open 제거, 두 GOP 및 나머지 선택 소유자/코드를 확인합니다.
4. 앞서 제거된 GUID 부재 및 IMG16 텍스트 GUID 2개 존재를 확인하고 IMG16 Stop(binding, virtualGop, 0, NULL)을 한 번 호출합니다.
5. IMG16 GOP 활성 open 및 텍스트 GUID 2개 부재, 기존 두 GOP/물리 소유권/시스템 콘솔 텍스트/코드 지문 유지를 요구합니다. 해제된 텍스트 인터페이스를 역참조하지 않습니다.
6. 사후 캡처, 파일 닫기, 최종 재검증 후 미리 로드한 Windows를 시작합니다. Stop 이후 실패는 정지하며 재시도/롤백/리셋하지 않습니다.

IMG17, EDID, NVIDIA Stop이나 새 GPU MMIO/PCI 속성 쓰기, 리셋, 펌웨어 업로드/실행, unlock은 없습니다. 기존 PCI 구성 읽기는 유지하며 드라이버 호출의 간접 효과는 가능합니다.

## 위험 및 실행

새 IMG16 호출은 인터페이스를 제거하고 메모리를 해제합니다. 화면/입력 손실, 펌웨어/Windows 정지 또는 GPU 초기화 실패가 가능하며 수동 전원 종료와 USB 제거가 필요할 수 있습니다. 복구는 검증되지 않았고 자동 복원은 없습니다. 위험을 읽고 실행 여부를 선택하며 별도 확인 메시지는 필요하지 않습니다.

GitHub Desktop으로 같은 브랜치를 업데이트한 뒤 저장소 루트에서 실행합니다.

```powershell
.\research\profile\build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:\Program Files\LLVM\bin\clang.exe" -LldLink "C:\Program Files\LLVM\bin\lld-link.exe" -ReleaseVirtualText
.\research\profile\stage-evga-text-stop.ps1 -Drive D
```

준비 도구는 검토한 EVGA05 해시, 빌드 모드/소스/해시/ESP를 확인하고 백업/캡처를 보존합니다. 새 단계 마커가 있으면 거부합니다. 오류를 우회하지 마십시오. 기존 마커는 보존하며 이번 별도 실험을 막지 않습니다.

작업 저장, 완전 종료, 일회성 UEFI USB 부팅 후 Live guards passed에서만 새 Shift+G 한 번을 누르십시오. Shift+V는 없습니다. 이후 화면은 진행 표시가 아닙니다. 3분 안에 Windows가 시작되지 않으면 전원을 끄고 USB를 제거한 뒤 정상 Windows Boot Manager를 선택하십시오. 새 마커를 보존하고 반복하지 마십시오. 다음 EVGAnn.TXT(예상 EVGA06), 빌드/준비 출력 및 자동/수동 Windows 복귀 결과를 보내십시오. 중복 오프라인 검사는 여기서 처리할 수 있습니다.

로컬 검증은 기존 멤버십 25개, 결합 모드 7개, 캡처 회귀 9개를 통과했습니다. EFI 경고 오류 처리 빌드 및 PE 검증도 통과했습니다. 이는 모의/빌드 검사이며 캡처 펌웨어 실행이나 실기 성공이 아닙니다.
