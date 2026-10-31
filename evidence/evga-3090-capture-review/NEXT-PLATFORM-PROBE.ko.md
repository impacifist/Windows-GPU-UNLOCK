# EVGA 런타임 PCI/BAR 통합 검사

[English](NEXT-PLATFORM-PROBE.en.md) | [한국어](NEXT-PLATFORM-PROBE.ko.md)

운영자는 01:16에 EVGA14 이후 정상 Windows 복귀를 확인했습니다. 01:20에는 펌웨어 파일 5개의 크기/해시가 고정 명세와 일치한다고 보고했습니다. 이는 운영자 출력이며 바이너리를 독립 검사한 결과는 아닙니다. 생성 과정은 GPU에 접근하지 않았습니다. 비공개 자산과 기존 벤치마크 실행 파일/설정을 보존하세요.

기존 해제 콜백 8회와 메모리 전용 디코딩을 반복합니다. 상태/PROM 읽기 전에 전체 256바이트 PCI 설정을 읽고 정확한 EVGA 식별자로 공유 런타임 검증기를 호출합니다. 기능 목록 경계, PM D0, MSI/MSI-X 비활성화, PCIe FLR 지원/진행 상태, 명령/BME 및 ROM 디코딩을 검사합니다. 기존 컴파일 시 시스템 고정값은 유지합니다. 리비전 030000a1, BAR0 84000000, c8=00050011을 추가 확인합니다. GetBarAttributes로 BAR0 주소 0x84000000, 길이 0x1000000과 설명자 형식을 확인하고 설명자 메모리를 해제합니다.

이어서 기존 칩 읽기 2회, 레지스터 8개의 두 배치, 전체 1MiB PROM 해시를 수행합니다. 성공 시 기존 메모리 비활성화 1회, 명령/속성 복원 확인, 캡처와 Windows 전달을 수행합니다. 새 MMIO 주소나 읽기 증가 없이 명시적 읽기 262162회입니다. DMA 매핑/실행, BME/VGA/I/O 활성화, GPU 리셋, 펌웨어 업로드/실행 및 잠금 해제는 없습니다. CanUnlock=false. 전체 런타임 소유권/버퍼 통합과 진입값 선택자 정책은 아직 검증되지 않았습니다.

위험: 해제 상태의 확장 PCI 읽기와 BAR 펌웨어 조회는 새로운 작업입니다. 펌웨어가 실패/정지하거나 화면/입력 손실 및 Windows/GPU 시작 실패가 발생할 수 있습니다. 복구는 보장되지 않으며 수동 전원 종료/USB 제거가 필요할 수 있습니다. 검사 실패 시 무조건적인 복구/재시도 없이 대기합니다. 실행 여부는 직접 결정하며 별도 승인 답장은 필요하지 않습니다.

GitHub Desktop에서 브랜치를 업데이트하고 저장소 루트에서 각 명령을 따로 실행하세요. 오류 시 중단하세요.

```powershell
./research/profile/build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:/Program Files/LLVM/bin/clang.exe" -LldLink "C:/Program Files/LLVM/bin/lld-link.exe" -ProbePlatform
./research/profile/stage-evga-text-stop.ps1 -Drive D
```

스테이징은 검토된 EVGA14/이전 증거를 요구하고 백업/기존 마커를 보존하며 EVGA-PLATFORM-PROBE.ARMED를 사용합니다. 완전히 종료 → UEFI USB 부팅 → Live guards passed 이후 Shift+G를 한 번 새로 누르세요. Shift+V는 필요 없습니다. 3분 안에 Windows가 시작되지 않으면 전원을 끄고 USB를 제거한 뒤 정상 Windows Boot Manager를 선택하세요. 반복하거나 마커를 삭제하지 마세요. EVGA15/다음 로그, 빌드/스테이징 출력과 자동/수동 Windows 복귀 여부를 보내세요. 펌웨어 바이너리는 업로드하지 마세요.

로컬 모의 검사126개와 기존 공유 검증기의 합성 식별/전원/기능/경로 검사가 프리스탠딩 호스트에서 통과했습니다. 설치된 VS 실행기에 vcvarsall.bat가 없어 기존 CRT/캡처 자료 검사는 실행하지 않았습니다. 경고 오류 처리 빌드/PE 검증 통과. EFI33792바이트 SHA2560c2731e88d3da60f1acedabb71c8024a93d078f4132c13b805ee50dae5794814. 기여자 컴파일러의 해시는 다를 수 있으며 해당 매니페스트로 검증합니다. 로컬 빌드만 완료하고 하드웨어 실행/스테이징은 하지 않았습니다.
