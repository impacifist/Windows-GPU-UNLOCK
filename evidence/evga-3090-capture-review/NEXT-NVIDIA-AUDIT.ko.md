# 다음 EFI: NVIDIA 해제 사전 검사

[English](NEXT-NVIDIA-AUDIT.en.md) | [한국어](NEXT-NVIDIA-AUDIT.ko.md)

[EVGA09](TENTH-EFI-RESULT.ko.md)은 소비자 해제 단계에서 매우 좋은 결과입니다. Stop6회와 EDID 활성 소유자3개 제거, GOP2개/핸들549개 유지, 캡처2회 및 보고된 자동 Windows 복귀를 확인했습니다. 이 머신의 소비자 순서는 입증되었지만 NVIDIA 자식/PCI 해제와 언락은 아직 미입증입니다. 언락 성공률을 숫자로 예측하는 결과가 아닙니다.

## 다음 검사가 필요한 이유

정확한 NVIDIA 이미지는 다른 머신의 IMG29가 아닌 IMG45입니다. 크기0x2b700, binding버전0x60009, StopRVA0x698, 코드RVA0x280/길이0x23d82/SHA256 d045cee2113e461c7c678ec21542b1d0f18356765c0fc6a0bf2dcf824088e011 입니다. 자식 수가 0이 아니면 helper0x1620을 호출합니다. 이 함수는 요청한 자식 목록보다 먼저 부모 private offset0x88의 저장된 자식을 확인하며 GOP 제거/helper0x3f0c 및 EDID-active 정리0x3088을 수행할 수 있습니다. 이후 요청 자식마다0x13f0을 호출하여 private 프로토콜 제거/메모리 해제 및0x2894 호출을 수행합니다. 따라서 EDID 형제 하나만 요청해도 물리 GOP가 유지된다고 볼 수 없습니다. 전체 간접/helper 효과는 미검증이며 캡처 코드를 로컬 실행하지 않았습니다.

새 모드는 기존 Stop6회를 반복합니다. G 전, 6단계 후 및 로깅 없는 최종 검사에서 NVIDIA PCI 소유자/코드 및 일치하는 부모 장치경로 BY_CHILD open3개를 확인합니다. 부모 private 프로토콜 fe47349a-7f0d-4641-822b-34baa28ecdd0 에서 offset0x88의 핸들8바이트와 offset0x98의 플래그1바이트만 CPU로 읽습니다. 주소 대신 분류/플래그를 기록합니다. 분류0=null,1=물리GOP,2=EDID 형제,4=알 수 없음. 알 수 없는 분류나0..1 밖의 플래그는 거부하며 핸들과 플래그는 동일하게 유지되어야 합니다. 소비자 해제 후 어떤 저장 자식 경로가 있고 플래그가 변하는지 확인하지만 향후 Stop의 안전성을 입증하지 않습니다. 기존 캡처에는 private 필드가 없어 오프라인 재생으로 얻을 수 없습니다.

**새 위험:** private 드라이버 인터페이스를 통한 새 CPU 읽기이며 UEFI는 할당 크기를 제공하지 않습니다. 잘못된 포인터로 펌웨어 오류/정지가 가능하며 기존 6개 콜백의 화면/입력/Windows 위험도 유지됩니다. 수동 전원 종료/USB 제거가 필요할 수 있고 복구는 미입증입니다. NVIDIA Stop/private 메서드, 리셋, GPU MMIO/PCI 속성 쓰기, DMA 시작, 펌웨어 업로드/실행 또는 언락을 추가하지 않습니다. 위험을 읽고 실행 여부를 선택하며 별도 동의 답변은 필요 없습니다.

GitHub Desktop으로 같은 브랜치를 업데이트한 뒤 실행합니다.

```powershell
.\research\profile\build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:\Program Files\LLVM\bin\clang.exe" -LldLink "C:\Program Files\LLVM\bin\lld-link.exe" -AuditNvidia
.\research\profile\stage-evga-text-stop.ps1 -Drive D
```

새 마커는 EVGA-NVIDIA-AUDIT.ARMED 입니다. 기존 마커5개/로그/백업을 보존하세요. EVGA05~09 및 소스/EFI/ESP/USB 검사를 요구합니다. 오류 시 중지하세요. 완전 종료 → 일회성 UEFI USB → Live guards passed 이후 새 Shift+G 한 번. Shift+V는 없습니다. G 전에 복귀하면 키를 서두르지 말고 로그를 보내주세요. G 이후3분 내 Windows가 없으면 전원 종료/USB 제거/정상 Windows Boot Manager를 사용합니다. 반복/마커 삭제를 하지 마세요. EVGA10 또는 다음 로그, 빌드/스테이징 출력 및 자동/수동 복귀 결과를 보내주세요. 오프라인 분석은 여기서 수행합니다.

모의 검증 총85개 통과(기존74+NVIDIA audit11). 소유자/코드/자식 검사, private 인터페이스 거부, 알 수 없는 자식/플래그, 최종 플래그 변경, 마커 및 이전 단계 실패를 포함하며 NVIDIA 콜백을 실행하지 않습니다. 경고 오류 처리 EFI 빌드/PE 검사 통과:26112바이트 SHA256 ac600b84cb64c1bda6ced86db87a07cd5ea82d4eb8441444502545593c59e358, AMD64/imports없음/NX/dynamic base. 로컬에서 빌드만 했으며 스테이징/실기 실행하지 않았습니다. 기여자 도구 체인의 해시는 다를 수 있습니다. 새 감사 단계의 실기 결과는 아직 없습니다.
