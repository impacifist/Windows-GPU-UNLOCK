# EFI 자산 및 독점 소유권 통합 검증

[English](NEXT-RESOURCES-PROBE.en.md) | [한국어](NEXT-RESOURCES-PROBE.ko.md)

EVGA15의 공유 PCI 설정/BAR0 설명자 검사가 통과했습니다. 실패 보고가 없으면 정상 Windows 복귀를 가정합니다. 다음 빌드는 기존 상태/PROM/디코딩 비활성화 과정과 두 통합 검사를 한 번의 부팅으로 수행합니다.

Stop 또는 새 Shift+G 입력 전에 RTX3090-UNLOCK/ASSETS의 정확한 파일5개를 CPU EfiLoaderData 풀 메모리로 읽습니다. GuLoadAssets와 동일한 새 공유 읽기 함수로 메타데이터/크기, 분할 읽기, 닫기 및 SHA256을 검사하고 다음 파일 전에 버퍼를 해제합니다. 최대 자산 버퍼84277400바이트, 전체 파일 데이터84427316바이트입니다. 업로드/실행하지 않습니다. 파일 검사 동안 기다린 뒤 Live guards passed가 나타날 때만 Shift+G를 누르세요. Stop 전 거부 시 기존 Windows 대체 경로를 사용할 수 있습니다.

검증된 해제 콜백8회, 메모리 전용 활성화 및 PCI/BAR 검사 후 기존 GuClaimController 구현을 TPL_NOTIFY에서 실행합니다. 수동 조회가 아닌 프로토콜 열기가 있으면 거부하고 현재 EFI 이미지의 PCI I/O 독점 소유권을 요청합니다. 로그/인터페이스 검사 실패 시에도 정확히 획득한 열기를 닫습니다. 닫기 실패 시 대기합니다. 기존 해제 토폴로지를 재확인한 뒤 검증된 상태/PROM, 메모리 비활성화 및 Windows 전달을 수행합니다. 스냅샷/로그 검사도 성공해야 합니다. 이 하네스는 공유 소유권 진단 문자열을 파일 이벤트로 기록합니다. claim-only 컴파일 옵션을 선택하지 않는 기존 소유권 도우미 동작은 유지됩니다.

DMA 매핑/실행, BME/VGA/I/O 활성화, MMIO 쓰기, GPU 리셋, 선택자 쓰기, 펌웨어 업로드/실행 또는 잠금 해제는 없습니다. 쓰기/리셋/벤더 코드는 링크하지 않습니다. CanUnlock=false. 일시적인 독점 획득/반환 검사이며 GPU 작업 중 장기 소유권, 런타임 DMA 버퍼 또는 잠금 해제 복구는 검증하지 않습니다.

위험: 새 EFI 파일 읽기/풀 할당과 독점 프로토콜 호출은 실패하거나 멈출 수 있습니다. 독점 획득은 펌웨어 드라이버 관리에 영향을 줄 수 있고 화면/입력 손실 또는 Windows/GPU 시작 실패가 발생할 수 있습니다. 복구는 보장되지 않으며 수동 전원 종료/USB 제거가 필요할 수 있습니다. 해제 후 검사 실패 시 무조건 복구/재시도 없이 대기합니다. 실행 여부를 직접 결정하며 별도 승인 답장은 필요하지 않습니다.

GitHub Desktop에서 브랜치를 업데이트하고 저장소 루트에서 각 명령을 따로 실행하세요. 오류 시 중단하세요.

```powershell
./research/profile/build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:/Program Files/LLVM/bin/clang.exe" -LldLink "C:/Program Files/LLVM/bin/lld-link.exe" -ProbeResources
./research/profile/stage-evga-text-stop.ps1 -Drive D
```

스테이징은 로컬 자산5개의 크기/해시를 검사하고 NVIDIA-LICENSE/NOTICE.md와 함께 USB RTX3090-UNLOCK/ASSETS에 복사한 뒤 해시를 확인합니다. 기존 파일이 다르면 덮어쓰지 않고 거부합니다. 자산은 Git에 배포하지 않습니다. 정확한 EVGA15/이전 로그가 필요하며 기존 캡처/마커/백업을 보존합니다. 새 EVGA-RESOURCES-PROBE.ARMED는 일회용입니다. 벤치마크 실행 파일/설정을 보존하세요.

완전히 종료 → UEFI USB → 자산 검사와 Live guards passed까지 대기 → Shift+G 한 번. Shift+V는 필요 없습니다. G 이후3분 안에 Windows가 시작하지 않으면 전원을 끄고 USB를 제거한 뒤 정상 Windows Boot Manager를 선택하세요. 반복하거나 마커를 삭제하지 마세요. EVGA16/다음 로그와 빌드/스테이징 출력을 보내고 Windows 시작 실패 또는 수동 복구가 있으면 보고하세요. 실패 보고가 없는 것은 정상 복귀 가정이지 명시적 확인은 아닙니다.

로컬 검증:기존 모의 검사126개, 새 자산/소유권13개, 통합 순서6개로 실제 SHA256 읽기 사례6개로 총151개 통과. 새 자원 해시/콜백은 모의이며 펌웨어를 실행하지 않았습니다. 공유 app_io.c 경고 오류 처리 컴파일 통과. 저장장치 조회를 모의한 임시 폴더 검사에서 WhatIf 무쓰기, 정확한 EFI/자산 복사, 백업, 기존 마커 보존 및 사용 마커 거부 통과. 실제 USB 접근 없음. PE/빌드 통과:37376바이트 SHA256992c563ea3865be3df1fba97db6704b168426c496936cf2f61dbc142250a8230. 기여자 해시는 다를 수 있으며 해당 매니페스트를 적용합니다. 로컬 하드웨어 실행/스테이징 없이 빌드만 완료했습니다.
