# 다음 통합 EFI: 물리 GOP의 EDID 소비자 해제

[English](NEXT-EDID-RELEASE.en.md) | [한국어](NEXT-EDID-RELEASE.ko.md)

[EVGA07](EIGHTH-EFI-RESULT.ko.md)은 IMG13/16/17 Stop 및 자동 Windows 복귀를 완료했습니다. 다음 빌드는 세 단계를 검사와 함께 반복한 뒤 IMG07 Stop(binding, physicalGop, 0, NULL)을 한 번 추가합니다. EDID 전용 형제 2개와 NVIDIA 드라이버는 중지하지 않습니다. 언락은 입증되지 않았습니다.

## 범위와 코드 검토

첫 Stop 전, 각 단계 후 및 Windows 직전에 동일한 EDID-discovered 핸들 3개를 요구합니다. 각 핸들의 BY_DRIVER open/count1과 소유자가 일치해야 합니다. 새 Stop 후에는 물리 GOP의 활성 EDID open만 없어야 하며 형제 2개의 소유권은 유지되어야 합니다. 기존 GOP/인터페이스/시스템 콘솔 검사도 유지합니다. 파일 GUID 대신 실제 활성 소유자를 선택합니다. binding 버전1, 이미지0x9000, Stop RVA0x14d4, .text RVA0x1000/길이0x37da/SHA256 a015d52f55fa6441414458e2b699ba2edcb53f03b001ac6367e45bcd908c418e 를 고정합니다. 기존 TE 이미지와 다른 PE이므로 해시 함수에 코드 RVA를 명시했으며 기존 핀은 동일합니다.

캡처된 IMG07 정적 분석에서 0x1514의 EDID-discovered GET_PROTOCOL 성공 경로는 0x153e CloseProtocol 및 저장된 플래그 해제 후 반환합니다. 최초 OpenProtocol 실패 시 포인터 입력 정리와 helper0x3080의 간접 그래픽 인터페이스 호출/추가 서비스 호출에 도달할 수 있습니다. 사전 조회 성공이 내부 OpenProtocol 성공을 보장하지 않으며 간접 동작 전체는 검증되지 않았습니다. 캡처된 코드를 로컬 실행하지 않았습니다. EDID GUID는 저장소 EdidDiscovered.h 기준이며 c7a7030c 시스템 콘솔 프로토콜과 구별합니다.

**새 위험:** 새 OEM 콜백이 EDID 소유권과 전역 상태를 변경합니다. 다른 정리 경로는 입력/디스플레이 및 그래픽 서비스를 건드릴 수 있습니다. 화면/입력 손실, 펌웨어/Windows 정지 또는 GPU 초기화 실패가 가능하며 수동 전원 종료/USB 제거가 필요할 수 있습니다. 복구는 미입증이며 자동 복원은 없습니다. 하네스는 GPU 리셋, 직접 MMIO, PCI 속성 변경, DMA 시작, 펌웨어 업로드/실행 또는 언락을 추가하지 않지만 콜백의 간접 하드웨어 효과는 가능합니다. 위험을 읽고 실행 여부를 선택하며 별도 동의 답변은 필요 없습니다.

## 빌드와 실행

GitHub Desktop으로 같은 브랜치를 업데이트한 뒤 저장소 루트에서 실행합니다.

```powershell
.\research\profile\build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:\Program Files\LLVM\bin\clang.exe" -LldLink "C:\Program Files\LLVM\bin\lld-link.exe" -ReleaseEdid
.\research\profile\stage-evga-text-stop.ps1 -Drive D
```

기존 세 단계는 자동 포함됩니다. 기존 마커 3개와 캡처/번호별 백업을 모두 보존합니다. 새 마커는 EVGA-EDID-RELEASE.ARMED 입니다. 스테이징은 검토된 EVGA05/06/07 해시와 소스/EFI/ESP 핀을 확인합니다. 모드 검사는 배열과의 스칼라 비교 대신 명시적 배열 멤버십을 사용하도록 수정했습니다. 오류 시 중지하고 검사 우회나 마커 삭제를 하지 마세요.

완전 종료 → 일회성 UEFI USB 부팅 → Live guards passed 이후 새 Shift+G 한 번. Shift+V는 없습니다. G 이후 화면은 진행 표시가 아닙니다. 3분 내 Windows가 시작되지 않으면 전원을 끄고 USB를 제거한 뒤 정상 Windows Boot Manager로 부팅합니다. 증거를 보존하고 반복하지 마세요. EVGA08 또는 다음 EVGAnn, 빌드/스테이징 출력, 자동/수동 Windows 복귀 결과를 보내주세요. 저장 로그 분석은 여기서 수행합니다.

로컬 검증: 프로토콜/해시/Stop 모의 검사 및 캡처 회귀 총61개 통과. 4번째 Stop 실패, 물리 EDID 해제 실패, 잘못된 핸들/소유자/버전, 이전 단계 실패 후 중지, 로깅 없는 최종 검사를 포함합니다. 경고 오류 처리 EFI 빌드 및 PE 검사 통과: 23552바이트, SHA256 c0cfbe2a4911551e85a05e8740c2da49057833ae1f908c85393b41fa154aeba0, AMD64, imports/IAT/delay imports 없음, NX/dynamic base. 로컬에서는 빌드만 했고 스테이징/실기 실행하지 않았습니다. 기여자 도구 체인의 해시는 다를 수 있습니다. 언락 또는 일반 복구 입증이 아닌 새 실기 테스트입니다.
