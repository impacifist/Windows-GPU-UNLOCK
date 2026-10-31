# SULK01: 공유 ESP에서 첫 실제 언락 시도

[English and exact commands](SHARED-ESP-UNLOCK.en.md)

QUAL01 원본 마커/로그를 직접 검토하여 consumer/NVIDIA Stop, 소유권 claim/close, platform/state/PROM, decode 복원 및 capture 완료 2개 성공을 확인했습니다. Windows 복귀와 원본 USB 복원은 사용자 보고입니다. 정확한 해시 3개는 `research/profile/shared-unlock-qualification.json`에 고정되어 있으며 미승인/다른 증거는 staging이 거부합니다. **다음 테스트는 공유 ESP에서 실제 기존 EVGA transaction을 실행합니다.** 정확한 시스템용 vendor/transaction 구현은 변경하지 않았으며 공유 볼륨 admission, 새 로그/게이트, USB와 ESP 간 시도 이력이 통합 변경입니다. 첫 공유 ESP transaction이며 실제 성공이나 자동 부팅 설치를 증명하지 않습니다.

**위험:** 새 Shift+G 후 디스플레이/NVIDIA 드라이버를 해제하고 GPU MMIO/PCI 쓰기, bus mastering, DMA 매핑, 펌웨어 업로드/실행, selector 쓰기와 PCIe FLR 1회를 수행합니다. 화면 손실은 예상됩니다. 실패 시 리소스/DMA/GPU 상태가 남아 수동 전원 종료가 필요할 수 있습니다. 실패하면 무조건 복원하거나 Windows를 시작하지 않고 멈춥니다. containment/복구는 보장되지 않습니다. 활성 ESP와 원본 USB 모두 쓰므로 중단/펌웨어/파일시스템 문제로 부팅/데이터에 영향을 줄 수 있습니다. 둘 다 먼저 백업하세요. 이 EVGA 시스템/원본 capture USB/고정 ESP만 사용하세요. Secure Boot는 이미 비활성화되어 있어야 하며 스크립트는 변경하지 않습니다. 실패/취소 후 마커 삭제/초기화나 재실행은 하지 마세요.

다음 전역 시도 번호는 원본 USB 이력으로 계산합니다. 추측하거나 예시 번호를 고정하지 마세요. staging은 이전 capture 해시와 최근 slot/transaction/정리 성공을 검사하고 USB 마커/최근 로그를 ESP의 `RTX3090-UNLOCK/USB-HISTORY-SULK01`에 보존합니다. 부팅 가능한 ESP 파일을 만들기 전에 원본 USB에 `EVGA-UNLOCK<N>.ARMED` **예약**을 만듭니다. 이 USB 예약은 실행 증거가 아니며, 새 공유 시도 검토 없이 구 USB workflow가 진행되지 않게 합니다. ESP의 번호 마커는 새 Shift+G 후 별도로 생성됩니다. SULK01 entry gate는 Prepare 전에 소비되므로 취소/거부 후에도 재사용할 수 없습니다. 예약/게이트는 자동 초기화하지 않습니다.

업데이트된 저장소 루트에서 관리자 PowerShell로 영어 문서의 명령을 순서대로 실행하세요. `D`는 실제 **원본 capture USB** 문자로 바꾸세요. 기존 capture/마커와 ESP 자산은 유지하세요. `get-shared-esp-unlock-slot.ps1`가 반환한 `$slot`을 빌드에 사용하고 USB 밖에 원본 boot 파일 해시와 함께 기록하세요. ESP GUID는 `c36c3e86-02aa-4f76-af1b-96e327f9d1c1`입니다.

새 ESP 파일은 `EFI/EVGA-SHARED-UNLOCK.EFI`, `RTX3090-UNLOCK/SULK01.ARMED`, 실행 중 `SULK01.TXT` 및 키 입력 후 `EVGA-UNLOCK<N>.ARMED`입니다. USB boot 백업은 별도 `EFI/BOOT/BOOTX64.before-shared-unlock.efi`입니다. 기존 파일/이력은 보존합니다. 이전 ESP transaction 마커, 기존 새 테스트 파일/이력, 소스/자산/boot 해시 변화, 이력 누락 또는 공간 부족(32 MiB + snapshot)이 있으면 거부합니다. 우회하려고 파일 삭제/크기 변경을 하지 마세요. `-WhatIf`는 쓰기와 쓰기 블록 내부 boot 해시 검사를 건너뜁니다. 오류/부분 staging이면 그대로 보존하고 멈추세요. 남은 예약을 지워 재시도하지 마세요. 스크립트는 boot 항목/펌웨어 설정 변경, 포맷 또는 재부팅을 하지 않습니다.

기존 펌웨어 선택으로 USB를 **한 번만** 부팅하세요. `Live guards passed. NOW press fresh Shift+G once`를 기다려 사진을 찍고 **Shift+G를 한 번** 눌러 위 실제 transaction을 시작하세요. Esc는 해제 전에 취소하지만 entry gate/예약은 소비됩니다. 첫 Stop 후 화면이 검게 될 수 있으며 최종 EFI 성공 화면은 기대하지 마세요. Windows를 최대 3분 기다리고 나오지 않으면 전원을 끄고 USB를 제거한 뒤 기존 Limine/Windows를 사용하세요. 복구는 검증되지 않았습니다. 자동 재시도/마커 초기화는 하지 마세요.

Windows에서 영어 문서의 `read-shared-esp-unlock.ps1`로 새 로컬 폴더에 증거를 복사하고 `restore-shared-esp-unlock-launcher.ps1`로 USB boot 파일만 복원하세요. `G:\Movian\SULK01-results`는 사용자 지정 desktop 예시이며 ESP 밖의 다른 새 로컬 경로도 가능합니다. USB 예약은 유지하세요. JSON, `SULK01.ARMED`, `SULK01.TXT`, 생성되었다면 **ESP의 번호 runtime 마커**, prompt 사진, Windows 결과 및 복원 출력을 비공개로 보내주세요. 경로/주소가 로그에 있으므로 공개하지 말고 펌웨어 자산은 보내지 마세요. ESP 이력 snapshot/USB 예약은 보존하세요. USB 제거 후 Windows/CachyOS의 일반 Limine 부팅도 확인하세요. 새 PowerShell에서는 기록한 slot/원본 해시를 사용하세요.

성공 판단에는 올바른 slot, transaction-return 성공, containment/정리, 자산 해제, decode 복원, capture 완료 2개, ready-Windows 성공과 정상 Windows 부팅 보고가 모두 필요합니다. STARTED, USB 예약, 키 마커, 단일 callback 또는 Windows만으로는 언락 증거가 아닙니다. 로그 검토 성공 이후에만 기존 수동 benchmark 절차를 사용하세요. 반복 가능성/자동 부팅/복구/성능 향상은 이 테스트만으로 증명하지 않습니다.

로컬 PE, gate 6개, Prepare/logger/Windows 8개, 런처 10개, 교차 볼륨 승인/해시/WhatIf/예약/snapshot/재시도 거부/수집/USB 복원 fixture를 통과했습니다. 기존 transaction adapter 및 release/보존 자산 정리 mock도 통과했습니다. 실제 하드웨어/펌웨어 transaction은 실행하지 않았습니다. slot 7 예시 해시는 영어 문서에 있으며 실제 slot/toolchain에 따라 달라질 수 있습니다.

## 한 명령으로 준비

관리자 PowerShell에서 `prepare-shared-esp-unlock.ps1`은 슬롯 확인, 빌드, 호스트 테스트 세 가지를 순서대로 실행합니다. `-Stage`를 지정할 때만 기존 보호 스테이징 스크립트 두 개를 실행합니다. 슬롯과 원래 USB 부팅 해시를 출력하므로 USB 외부에 기록하세요. 재부팅이나 실제 트랜잭션은 수행하지 않습니다. `-Stage`가 없으면 빌드와 테스트만 합니다. 기존 위험, 백업, 중단 및 재시도 금지 지침도 동일하게 적용됩니다.

```powershell
./research/profile/prepare-shared-esp-unlock.ps1 -WindowsEspGuid 'c36c3e86-02aa-4f76-af1b-96e327f9d1c1' -Drive D -Clang 'C:/Program Files/LLVM/bin/clang.exe' -LldLink 'C:/Program Files/LLVM/bin/lld-link.exe' -Stage
```

부팅 후 `finish-shared-esp-unlock.ps1`에 기록한 슬롯, 원래 해시와 ESP 외부의 새 출력 폴더를 전달하면 증거를 수집하고 USB를 복원합니다. 증거 수집이 실패하면 복원 전에 중단하므로 파일을 보존하고 오류를 보고하세요.

```powershell
./research/profile/finish-shared-esp-unlock.ps1 -WindowsEspGuid 'c36c3e86-02aa-4f76-af1b-96e327f9d1c1' -Drive D -AttemptSlot <slot> -OriginalSha256 '<hash>' -OutputDirectory 'G:/Movian/SULK01-results'
```

## SULK01 실제 하드웨어 결과

2026-10-28 기여자는 슬롯 8 실행 마커와 SULK01 로그(SHA-256 `c0cb1b121fbe24610e84dfdaee52da512856af39a65768a8441012e4236e5f23`)를 제공했습니다. 로그는 트랜잭션 결과 0, FLR=1, quiesced=1, fault=0, retained=0, maps=0, 성공적인 트랜잭션 반환과 자산 해제 및 디코드 복원, 완료된 캡처 두 개와 Windows 준비 성공을 기록합니다. 기여자는 Windows에서 수집기를 실행하고 종료 명령이 성공했다고 보고했습니다. 이는 공유 ESP에서 처음 기록된 성공 트랜잭션이나 성능 향상이나 지속적인 자동 부팅을 증명하지 않습니다. 추가 종료 또는 재부팅 전에 동일 바이너리의 벤치마크를 요청했습니다.
