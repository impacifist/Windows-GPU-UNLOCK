# QUAL01: 공유 ESP GPU 해제 및 리소스 검증

[English and exact commands](SHARED-ESP-QUALIFICATION.en.md)

수신한 INTG01 마커와 원본 로그에서 Prepare, 자산 해제, capture 완료/결과 및 ready-Windows 성공을 확인했습니다. Windows 부팅과 USB 복원도 보고되었습니다. 다음 테스트는 실제 공유 ESP handoff 진입점을 통해 기존 USB의 release-only/resources 단계를 통합합니다. consumer Stop 6개, NVIDIA child/parent Stop, 일시적인 GPU 메모리 decode 활성화, 제한된 state/PROM/platform 읽기, 배타적 PCI 소유권 획득/해제 및 decode 복원을 수행합니다. 해제 전에 기존 자산 다섯 개를 실제 resources loader로 검증하지만 실행하지 않습니다. **언락 transaction, vendor 펌웨어 실행, DMA 또는 FLR은 수행하지 않습니다.** 관찰 전용 테스트의 반복이 아니라 공유 ESP에서 GPU 해제/소유권 경계를 검사합니다.

**위험:** 실행 중인 드라이버 소유권과 GPU PCI decode를 변경합니다. 첫 Stop 이후 화면이 검게 될 수 있으며 성공 여부는 최종 EFI 화면이 아닌 로그와 Windows 부팅으로 확인합니다. 실패 시 드라이버가 분리되거나 decode가 활성화된 상태로 남아 수동 전원 종료가 필요할 수 있습니다. 자동 rollback/재시도는 없습니다. 활성 공유 ESP에 파일을 계속 쓰므로 중단/펌웨어/파일시스템 오류로 부팅과 데이터에 영향을 줄 수 있습니다. ESP와 USB를 먼저 별도 백업하세요. 복구는 보장되지 않습니다. 다른 시스템/카드/EFI에 적용하지 마세요. 마커 삭제/초기화로 재실행하지 마세요.

업데이트된 저장소 루트에서 관리자 PowerShell로 영어 문서 명령을 순서대로 실행하세요. 실제 USB 문자와 기존 성공한 컴파일러 인수를 사용하세요. ESP GUID는 `c36c3e86-02aa-4f76-af1b-96e327f9d1c1`입니다. 기존 자산/기록/로그를 유지하세요. staging은 검토한 INTG01 마커/로그의 정확한 해시, 기존 Windows/Limine 해시와 16 MiB 여유 공간을 요구합니다. 원본 USB 해시를 USB 밖에 기록하세요.

새 ESP 파일은 `EFI/EVGA-SHARED-QUALIFICATION.EFI`, `RTX3090-UNLOCK/QUAL01.ARMED`, 실행 중 `QUAL01.TXT` 및 키 입력 후 `QUAL01-GPU.ARMED`입니다. 기존 파일은 유지합니다. 새 USB 백업은 `EFI/BOOT/BOOTX64.before-shared-qualification.efi`입니다. 새 테스트 파일이 이미 있으면 거부하므로 그대로 보존하세요. entry gate는 Prepare 전에 PENDING에서 STARTED로 바뀌며 성공/취소 후에도 STARTED입니다. 완료 기록이 아닙니다. `-WhatIf`는 쓰기와 쓰기 블록 내부 부팅 해시 검사를 건너뜁니다. 스크립트는 부팅 항목 변경/포맷/재부팅을 하지 않습니다.

기존 펌웨어 선택 방법으로 USB를 **한 번만** 부팅하세요. `Live guards passed. NOW press fresh Shift+G once`가 나올 때까지 기다리고 사진을 찍으세요. 그 후 **Shift+G를 한 번** 눌러 위 드라이버/decode 테스트를 시작하세요. Esc는 해제 전에 취소합니다. guard 거부/오류 시 그대로 보존하고 반복하지 마세요. 첫 Stop 이후 화면 손실은 예상됩니다. Windows가 나올 때까지 최대 3분 기다리세요. 나오지 않으면 전원을 끄고 USB를 제거한 뒤 기존 Limine/Windows를 사용하세요. 이 복구는 보장되지 않습니다. 취소/실패 후에도 마커를 초기화하거나 재실행하지 마세요.

Windows 복귀 후 영어 문서의 `read-shared-esp-qualification.ps1` 명령으로 ESP 밖의 **새 로컬 폴더**에 증거를 복사하고 `restore-shared-esp-qualification-launcher.ps1`로 USB를 복원하세요. 예시 `G:\Movian\QUAL01-results`는 사용자 지정 desktop 위치에 맞춘 것이며 다른 새 로컬 경로를 지정해도 됩니다. JSON, `QUAL01.ARMED`, `QUAL01.TXT`, 생성되었다면 `QUAL01-GPU.ARMED`, prompt 사진, Windows 부팅 결과 및 복원 출력을 비공개로 보내주세요. 로그에는 경로/주소가 있습니다. 펌웨어 자산은 보내지 마세요. ESP 파일은 보존하고 USB 제거 후 Windows/CachyOS의 일반 Limine 부팅도 확인하세요. 새 PowerShell에서는 저장한 원본 해시를 사용하세요.

성공 판단에는 모든 Stop/claim/close/state/PROM/platform 검사, 메모리 decode 비활성화/복원 검사, capture 완료 2개, `evga-text-stop-ready-windows` 성공과 정상 Windows 부팅이 필요합니다. STARTED, 키 마커, 부분 capture 또는 단일 callback 성공만으로는 검증이 아닙니다. 드라이버 호출 전 실패는 정상 Windows fallback을 시도할 수 있지만 Stop 이후 실패는 자동 복원 없이 멈춥니다. 전부 성공해도 언락/vendor transaction/자동 부팅/복구 검증은 아닙니다.

로컬 QUAL01 resources wrapper, gate 6개, 런처 10개, staging/이전 해시/공간/덮어쓰기 거부/evidence 복사 및 USB 복원 fixture를 통과했습니다. 기존 EVGA Stop/decode/state/platform/resources/ownership/정리 mock 회귀 검사도 통과했습니다. 하드웨어/해시/Stop은 모의 환경이며 실제 실행은 아직 없습니다. 빌드 해시는 영어 문서에 있습니다.
