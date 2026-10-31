# EVGA 통합 디코딩 진입 검사

[English](NEXT-DECODE-PROBE.en.md) | [한국어](NEXT-DECODE-PROBE.ko.md)

EVGA12와 기준 벤치마크 이후 검증된 Stop 콜백8회를 반복합니다. 부모 소유권 해제와 PCI명령0을 확인한 뒤 MEMORY만 활성화(속성0x200, 명령2)하고 BAR0 오프셋0 칩ID를2회 읽습니다. 두 값 모두 GA102 b72000a1이어야 합니다. MEMORY를1회 비활성화하고 명령0 및 저장된 정확한 속성을2회 검사합니다. Windows 직전에도 명령0과 소유권을 검사합니다. I/O/BME/VGA 활성화, PROM 접근, GPU리셋, DMA실행, 펌웨어 업로드 또는 잠금 해제는 포함하지 않습니다.

새 위험: 디코딩 활성화와 최초 해제 후 MMIO읽기는 부작용이나 멈춤을 일으킬 수 있습니다. 화면/입력, 펌웨어, Windows/GPU 시작이 실패하여 전원 종료/USB제거가 필요할 수 있습니다. 복구는 입증되지 않았습니다. 호출/검사 실패 시 정지하며 무조건 복원하거나 재시도하지 않습니다. 성공 경로의 비활성화는 실패 자동 복구가 아닙니다. 읽은 후 실행 여부를 결정하며 별도 동의 답변은 필요 없습니다.

GitHub Desktop에서 현재 브랜치를 업데이트하고 저장소 루트에서 각각 실행합니다. 오류가 나면 중단합니다.

```powershell
.\research\profile\build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:\Program Files\LLVM\bin\clang.exe" -LldLink "C:\Program Files\LLVM\bin\lld-link.exe" -ProbeDecode
.\research\profile\stage-evga-text-stop.ps1 -Drive D
```

배치는 검토된 EVGA12와 이전 캡처를 요구하며 백업/기존 마커를 보존하고 새 EVGA-DECODE-PROBE.ARMED를 사용합니다. 마커 삭제나 EFI반복 실행은 하지 마세요. 완전 종료→UEFI USB선택→Live guards passed 이후 새 Shift+G1회입니다. Shift+V는 필요 없습니다. 화면 소실은 진행 증거가 아닙니다.3분 내 Windows가 시작되지 않으면 전원 종료/USB제거 후 일반 Windows Boot Manager를 선택합니다.

EVGA13.TXT 또는 다음 번호 로그, 빌드/배치 출력, 자동/수동 Windows복귀 여부를 보내 주세요. 활성화 전 명령, 활성화 성공/명령2, 칩ID2회, 비활성화 성공/명령0, 완료 캡처와 Windows준비 이벤트를 여기서 확인합니다. 벤치마크 실행 파일/설정은 보존합니다. 이 검사는 잠금 해제가 아니므로 아직 성능 재측정은 필요 없습니다.

실제 잠금 해제 전 펌웨어 자산 검증, 집계 토폴로지와 잠금 해제 검사/런타임 통합, 해제 후 진입 조건과 복귀 동작의 증거가 남아 있습니다. 칩ID 성공만으로 모든 레지스터/PROM/DMA전제 조건이 충족되지는 않습니다. 실제 트랜잭션 제공 전 해결해야 하며 CanUnlock=false를 유지합니다.

로컬 검증: 기존107개와 새 디코딩8개 호스트 검사 통과, EFI경고 오류 처리 및 PE검사 통과입니다. 모의 인터페이스만 실행했으며 실제 GPU나 캡처된 펌웨어는 실행하지 않았습니다. 새 하드웨어 동작은 아직 미검증입니다.
