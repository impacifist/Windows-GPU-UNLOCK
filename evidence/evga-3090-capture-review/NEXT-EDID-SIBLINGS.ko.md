# 다음 통합 EFI: EDID 형제 2개 해제

[English](NEXT-EDID-SIBLINGS.en.md) | [한국어](NEXT-EDID-SIBLINGS.ko.md)

[EVGA08](NINTH-EFI-RESULT.ko.md)은 Stop 4회 성공과 자동 Windows 복귀를 확인했습니다. 다음 빌드는 기존 4단계를 반복하고 동일하게 고정된 IMG07 Stop(binding, sibling, 0, NULL)을 EDID 전용 형제 2개에 각각 한 번 호출합니다. 한 번의 참석 부팅에 총 Stop 6회이며 각 단계 후 그래프/코드 검사를 다시 수행합니다. 실패나 예상 밖 상태에서는 후속 실행을 중단하며 재시도/롤백은 없습니다.

EDID 핸들 3개는 캡처된 정확한 장치 경로와 일치해야 합니다. 공통 ACPI 루트/PCI 부모 및 서로 다른 ADR 0x80010320 (물리 GOP), 0x80010340, 0x80010370 (형제)를 확인합니다. 노드 헤더를 페이로드보다 먼저 검사하며 범위는 36바이트로 고정됩니다. 동일한 핸들/소유자를 유지하고 완료된 핸들의 활성 EDID open은 없어야 하며 미완료 핸들은 정확한 소유자/count1을 유지해야 합니다. 최종적으로 EDID 활성 open 3개는 모두 없어야 하고 GOP 2개와 기존 콘솔/인터페이스 조건은 유지되어야 합니다. IMG07 버전/이미지/Stop/.text 핀은 기존과 동일합니다. 새 실행 이미지는 호출하지 않습니다.

**새 위험:** IMG07을 새로운 컨트롤러 2개에서 호출하며 마지막 EDID 소유권도 해제합니다. 정상 EDID 경로는 open을 닫고 전역 플래그를 해제하지만 내부 조회 실패 시 입력 정리/간접 그래픽 서비스에 도달할 수 있습니다. 물리 컨트롤러 성공이 형제/마지막 소유자 동작을 보장하지 않습니다. 화면/입력 손실, 펌웨어/Windows 정지 또는 GPU 초기화 실패로 수동 전원 종료/USB 제거가 필요할 수 있습니다. 복구는 미입증이며 자동 복원은 없습니다. NVIDIA 자식/부모 Stop, GPU 리셋, 직접 MMIO/PCI 속성 변경, DMA 시작, 펌웨어 업로드/실행 또는 언락은 추가하지 않습니다. 위험을 읽고 실행 여부를 선택하며 별도 동의 답변은 필요 없습니다.

GitHub Desktop으로 같은 브랜치를 업데이트한 뒤 실행합니다.

```powershell
.\research\profile\build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:\Program Files\LLVM\bin\clang.exe" -LldLink "C:\Program Files\LLVM\bin\lld-link.exe" -ReleaseEdidSiblings
.\research\profile\stage-evga-text-stop.ps1 -Drive D
```

기존 4단계는 자동 포함됩니다. EVGA05/06/07/08 해시와 현재 소스/EFI/ESP/USB 검사를 요구합니다. 새 일회성 마커는 EVGA-EDID-SIBLINGS.ARMED 입니다. 기존 마커 4개와 로그/번호별 백업을 모두 보존하세요. 오류 시 중지하고 재시도를 위해 마커를 삭제하지 마세요.

완전 종료 → 일회성 UEFI USB 부팅 → Live guards passed 이후 새 Shift+G 한 번. Shift+V는 없습니다. G 이후 화면은 진행 표시가 아닙니다. 3분 내 Windows가 시작되지 않으면 전원 종료/USB 제거 후 정상 Windows Boot Manager를 선택하세요. 반복하지 마세요. EVGA09 또는 다음 로그, 자동/수동 Windows 복귀 결과 및 빌드/스테이징 출력을 보내주세요. 저장 로그 분석은 여기서 수행합니다.

로컬 모의 검사 총74개 통과: membership25, text7, graphics8, physical EDID12, sibling13, capture9. 각 형제 콜백 실패/해제 실패, 이전 단계 실패 시 중지, 잘못된 경로/인벤토리, 마커/해시 거부, 로깅 없는 최종 검사를 포함합니다. 스테이징 문법과 모드별 마커 5개 검사 통과. 경고 오류 처리 빌드/PE 검사 통과: 25088바이트 SHA256 476c6564ad62e0e31c2bd6181f19d2a55d4418ef7e40219afb7ffa05e8e42a0e, AMD64/imports 없음/NX/dynamic base. 로컬에서 빌드만 했고 스테이징/실기 실행하지 않았습니다. 기여자 도구 체인의 해시는 다를 수 있습니다. 새 모드 실기 성공이나 언락을 주장하지 않습니다.
