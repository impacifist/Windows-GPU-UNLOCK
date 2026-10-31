# 다음 EFI: NVIDIA 자식3개 Stop 한 번

[English](NEXT-NVIDIA-CHILDREN.en.md) | [한국어](NEXT-NVIDIA-CHILDREN.ko.md)

[EVGA10](ELEVENTH-EFI-RESULT.ko.md)은 정확한 NVIDIA 코드/소유권 및 소비자 Stop6회 전후 저장된 물리 GOP 자식/부모 flag98=0 유지 상태를 확인했습니다. 다음 모드는 같은 순서/사전 검사 후 IMG45 Stop(binding, PCI parent, 3, children)을 정확히 한 번 호출합니다. 물리 GOP와 검증된 EDID 형제2개 순서입니다. NVIDIA 자식0개/부모 Stop은 호출하지 않고 PCI 소유권은 유지합니다. 언락은 시도하지 않습니다.

## 검토 및 예상 결과

자식이 있으면0x6b7에서 helper0x1620 호출 후 반환하며 자식0개 부모 정리0x6e1로 진입하지 않습니다. helper는 요청 목록과 관계없이 저장된 GOP를 제거할 수 있습니다. GOP 정리0x3f0c는 버퍼/객체를 해제하고 EDID-active 정리0x3088/0x303c는 프로토콜 제거/해제를 수행합니다. 각 자식0x13f0은 EDID-discovered/버퍼를 제거한 뒤0x2894에서 부모 장치경로 자식 open을 닫고 자식 경로 제거 및0x259c 저장소 해제를 수행합니다. 제거 실패 시 open 재설정이나 부분 정리가 가능하며 전체 드라이버/펌웨어 서비스 동작은 검증되지 않았습니다. 캡처된 코드를 로컬 실행하지 않았습니다.

사전 검사에서 EVGA10의 분류1/flag0, 정확한 코드 핀, 자식3개의 동일한 경로/소유자 및 기존 소비자 조건을 요구합니다. 호출 후에는 원래 가상 GOP만 남고 프레임버퍼가0이어야 하며 EDID-discovered 핸들과 원래 자식 핸들3개는 없어야 합니다. 부모 장치경로 활성 open은 없어야 하지만 NVIDIA PCI 소유자/코드는 유지되고 저장 자식은NULL/flag0이어야 합니다. 기존 시스템 콘솔 텍스트와 가상 텍스트 인터페이스 부재도 유지합니다. 파일 종료 후 로깅 없이 다시 검사한 뒤 미리 로드한 Windows로 이동합니다. 제거된 물리 GOP/자식 인터페이스는 조회/역참조하지 않습니다. 실패 시 정지하며 재시도/부모 Stop/리셋/자동 롤백은 없습니다.

**새 위험:** 첫 NVIDIA Stop이 유일한 물리 GOP, EDID, 장치경로 인터페이스를 제거하고 드라이버 객체를 해제합니다. 화면/입력 손실, 펌웨어 정지, 부분 서비스 실패 또는 Windows/GPU 시작 실패로 수동 전원 종료/USB 제거가 필요할 수 있습니다. 복구는 미입증이며 자동 복원은 없습니다. 하네스는 직접 GPU MMIO/PCI 속성 쓰기, DMA 시작, 리셋, 펌웨어 업로드/실행 또는 언락을 추가하지 않지만 간접 콜백/서비스 효과는 배제하지 못합니다. 위험을 읽고 실행 여부를 선택하며 별도 동의 답변은 필요 없습니다.

GitHub Desktop으로 같은 브랜치를 업데이트한 뒤 실행합니다.

```powershell
.\research\profile\build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:\Program Files\LLVM\bin\clang.exe" -LldLink "C:\Program Files\LLVM\bin\lld-link.exe" -StopNvidiaChildren
.\research\profile\stage-evga-text-stop.ps1 -Drive D
```

EVGA05~10 해시 및 현재 소스/EFI/ESP/USB 핀을 검사하고 이전 증거/백업을 보존합니다. 새 마커 EVGA-NVIDIA-CHILDREN.ARMED 와 기존 마커6개를 보존하세요. 오류 시 중지하고 재시도를 위해 마커를 삭제하지 마세요.

완전 종료 → 일회성 UEFI USB → Live guards passed 이후 새 Shift+G 한 번. Shift+V는 없습니다. G 이후 화면 손실은 예상되지만 진행 증거는 아닙니다. 3분 내 Windows가 없으면 전원 종료/USB 제거/정상 Windows Boot Manager를 사용합니다. 반복하지 말고 EVGA11 또는 다음 로그, 빌드/스테이징 출력과 자동/수동 Windows 복귀 결과를 보내주세요. 저장 로그 분석은 여기서 합니다.

로컬 모의 검사96개 통과(기존85+NVIDIA 자식11). 정확한 자식3개 호출 한 번, 성공/로깅 없는 최종 검사, 콜백 실패, 물리 GOP 잔류 거부, 이전 실패 후 중지, flag/소유자/해시/마커 거부 및 최종 상태 변화를 확인합니다. 최초 호스트 컴파일 오타는 수정 후 통과했습니다. EFI Werror/PE 검사 통과:28160바이트 SHA256615b224e567684fa98765f0b2543ef200b3999a269023f892364753cef1bb29d, AMD64/imports없음/NX/dynamic base. 스테이징 문법/새 마커 선택 및 diff 검사 통과. 로컬 빌드만 했고 스테이징/실기 실행하지 않았으며 기여자 도구 체인 해시는 다를 수 있습니다. 새 NVIDIA 경로는 실기 미검증입니다.
