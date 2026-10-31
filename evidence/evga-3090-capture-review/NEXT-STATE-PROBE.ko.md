# EVGA 콜드 상태와 PROM 통합 증거 검사

[English](NEXT-STATE-PROBE.en.md) | [한국어](NEXT-STATE-PROBE.ko.md)

EVGA13의 해제, MEMORY전용 활성화, 칩ID2회 및 조건부 비활성화/Windows복귀를 반복합니다. 메모리 디코딩 중 BAR0 DWORD8개를2회씩 추가로 읽습니다: GFW PLM0x118128, BOOT0x118234, CPUCTL0x110100, WPR2_HI0x1fa828, feature PLM0x823804, SS0 0x82381c, SS1 0x823820, 메모리 크기0x1183a4. unlock/app.c ColdCheck가 사용하는 값입니다. 기록 후 all-ones/BADF와 불안정한 쌍은 거부하지만 전체 잠금 해제 정책 충족을 주장하지 않습니다.

고정1MiB PROM창0x300000..0x3ffffc를 순방향1회,262144 DWORD호출로 읽어 CPU버퍼 해시를 원본 f1c7c6b205d9a5c0626af4f82e28cc377aa16cde8c25c61c77b4b5160b26e71f와 비교합니다. 두 번째 전체 읽기나 새 ROM파일은 없습니다. 명시적 프로브 MMIO읽기는 총262162회(칩2+상태16+PROM262144)이며 기존 캡처도 수행됩니다. GPU MMIO쓰기, BME/VGA/I/O활성화, 리셋, DMA실행, 펌웨어 업로드/실행 또는 잠금 해제는 없습니다. CanUnlock=false입니다.

새 위험: 새 MMIO주소와 확대된 PROM읽기가 부작용/멈춤을 유발할 수 있습니다. 화면/입력, 펌웨어 또는 Windows/GPU시작 실패 시 전원 종료/USB제거가 필요할 수 있으며 복구는 미입증입니다. 오류, 센티널, 불안정값 또는 해시 불일치 시 무조건 디코딩복구/재시도 없이 정지합니다. 성공 경로만 이전과 같이 메모리를 비활성화하고 명령/속성 복원을 검사합니다. 실행 여부는 읽은 후 결정하며 별도 동의 답변은 필요 없습니다.

GitHub Desktop에서 현재 브랜치 업데이트 후 각각 실행합니다.

```powershell
./research/profile/build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:/Program Files/LLVM/bin/clang.exe" -LldLink "C:/Program Files/LLVM/bin/lld-link.exe" -ProbeState
./research/profile/stage-evga-text-stop.ps1 -Drive D
```

정확한 EVGA13과 이전 증거를 요구하고 새 EVGA-STATE-PROBE.ARMED를 사용하며 백업/기존마커는 보존합니다. 오류 시 중단합니다. 완전종료→UEFI USB→Live guards passed후 새 Shift+G1회입니다. Shift+V는 없습니다. 화면 소실은 진행 증거가 아닙니다.3분 내 Windows가 없으면 전원 종료/USB제거/일반 Windows Boot Manager선택입니다. 반복실행/마커삭제는 하지 마세요. EVGA14/다음 로그, 빌드/배치 출력, 자동/수동 Windows복귀를 보내고 EVGA13복귀 여부도 확인해 주세요.

예상 증거는 상태값 쌍, PROM해시 성공, 메모리 비활성화/복원 검사, 캡처2회 완료와 Windows준비입니다. 분석은 여기서 합니다. 벤치마크 실행 파일/설정은 보존하되 이 비잠금해제 검사 후 재측정은 필요 없습니다. 펌웨어 자산검증과 집계 잠금해제 런타임통합은 별도 필수작업입니다.

로컬 모의 검사121개(기존115+새6), 경고오류 처리 및 PE빌드 통과입니다. EFI31744바이트 SHA25659fde4e3e0d09c0115e12140d7b64c5ba05e838cb03cba920f17bb000e2e3ed2. 실제GPU/캡처펌웨어는 실행하지 않았으며 로컬 배치도 하지 않았습니다. 기여자 컴파일러의 해시는 달라도 되지만 해당 빌드 매니페스트와 배치는 일치해야 합니다.
