# SULK02: USB 없이 한 번만 실행하는 입회 잠금 해제 부팅

[English](SHARED-ESP-SECOND-RUN.en.md)

2026-10-29 기여자는 두 번째 런처를 `Boot7E00`으로 등록했습니다. 실제 결과는 BootNext 없음, 기존 부팅 순서 유지, Limine/Windows 검사 통과, 시도 미준비 상태였습니다. 이는 등록만 증명합니다. SULK01은 USB 런처를 통해 ESP의 잠금 해제와 벤치마크 향상을 이미 보였습니다. SULK02는 부팅 시 USB를 제거한 전체 펌웨어→ESP 잠금 해제 경로를 시험합니다. 아직 재사용 가능한 완성 프로필이 아닌 정확한 해당 장비의 개발 시험입니다.

## 범위와 위험

새 스크립트는 등록 백업/현재 펌웨어, 정확한 ESP 위치, EFI/정상 부팅 파일 해시, 자산 5개와 검토된 두 볼륨 이력을 확인합니다. 로컬 준비 기록을 저장하고 원본 USB에 슬롯 9를 예약한 뒤 ESP에 고정 512바이트 영수증을 생성/검증하며, **마지막으로** 이미 등록된 항목을 BootNext로 선택합니다. BootOrder, EFI 파일, Secure Boot는 변경하지 않으며 자동 재부팅이나 마커 초기화도 없습니다. USB는 예약할 때만 필요하며 실제 부팅 전에 제거합니다.

새 Shift+G 입력 후 EFI는 디스플레이/NVIDIA 드라이버를 중지하고 GPU MMIO/PCI 쓰기, 버스 마스터 활성화, DMA 매핑, 펌웨어 업로드/실행, 선택자 쓰기 및 FLR 1회를 수행합니다. 화면 소실이 예상됩니다. 검사 실패 시 무작정 진행하지 않고 멈춥니다. 실패하면 리소스/GPU 상태가 남거나 펌웨어/Windows가 멈추고, 활성 ESP 쓰기 중단으로 부팅/데이터가 손상되거나 수동 전원 차단이 필요할 수 있습니다. 복구/격리는 보장되지 않습니다. NVRAM 선택은 원자적 작업이 아니므로 다른 부팅 관리 도구를 닫으세요. 검토된 EVGA/Z790 장비만 사용하고 백업/복구 미디어를 준비하세요. 준비 후 실행 전까지 하드웨어, 설정, 자산, 파일을 바꾸지 마세요.

## 한 번만 준비하고 실행

브랜치를 업데이트하되 고정된 EFI를 다시 빌드/복사하지 마세요. 원본 USB를 D:로 연결하고 작업을 저장한 뒤 저장소 루트의 관리자 PowerShell에서 실행합니다.

```powershell
./research/profile/run-shared-esp-second.ps1
```

상태 디렉터리는 `E:\UNLOCK\ESP-SECOND02`입니다. 예상 출력은 `armed: true`, `attempt_slot: 9`, `boot_next_selected: true`, `boot_order_written: false`, `gpu_executed: false`, `automatic_reboot: false`입니다. 오류가 나면 **재부팅/재시도하지 말고** 상태를 보존하여 오류를 보내세요. 부분 실패 시 파일 또는 BootNext가 남을 수 있습니다. 거부를 우회하려고 기록/예약/영수증을 삭제하지 마세요.

성공 출력 후에만 저장소 USB를 안전하게 제거하고 정상 재시작을 **한 번** 하세요. USB 선택이나 부팅 순서 변경은 하지 않습니다. `UNLOCK CHILD LOADED FROM PINNED ESP`와 검사 통과 화면을 촬영하세요. `Live guards passed. NOW press fresh Shift+G once`를 기다린 뒤 **Shift+G를 한 번** 누르세요. Esc 취소도 진입 게이트를 소비합니다. 화면 소실 후 Windows를 최대 3분 기다리며 Shift+V를 반복하지 마세요. 커서 깜박임은 진행 증거가 아닙니다. Windows가 나오지 않으면 전원을 끄고 USB를 뺀 상태로 기존 Limine/Windows 항목을 사용하세요. 재시도하지 말고 수동 복구 후 수집/보고하세요. 펌웨어가 BootNext를 소비할 것으로 예상되지만 실제 확인이 필요하며 복구는 보장되지 않습니다.

## 수집과 정리

Windows 복귀 후 다른 재부팅 전에 같은 스크립트를 실행합니다.

```powershell
./research/profile/run-shared-esp-second.ps1 -Action Collect
```

새 로컬 디렉터리에 SULK02 영수증/로그와 선택적 런타임 마커를 복사/검증하여 ZIP을 만들고 소유한 펌웨어 항목만 정리합니다. ZIP과 콘솔 JSON, 자동/수동 Windows 복귀 여부, 부팅 시 USB가 없었는지 보내세요. 비공개 펌웨어 백업은 ZIP에 포함하지 않으며 보존합니다. 정리 거부 시에도 ZIP 경로와 `cleanup_error`를 반환하므로 상태와 오류를 보내세요. 변경된 BootOrder를 덮어쓰지 않습니다. Windows 복귀만으로 잠금 해제 성공을 주장하지 않습니다. 후속 벤치마크를 위해 같은 Windows 세션을 유지하고 불필요한 재부팅/잠금 해제 재실행을 하지 마세요.

부팅 전 취소는 `./research/profile/run-shared-esp-second.ps1 -Action Cancel`, 읽기 전용 펌웨어 상태는 `-Action Status`입니다. 취소해도 영수증, 예약, 준비 기록을 보존하며 시도를 재사용할 수 없습니다. 취소가 거부되면 재부팅하거나 임의 펌웨어 삭제를 하지 말고 오류를 보내세요.

## 검증 한계

로컬 순서/실패/영수증 검사 26개 및 RAM 전용 Native와 모의 저장소/해시/이력을 사용한 실제 스크립트 검사가 통과했습니다. 등록, WhatIf, 영수증/예약, 마지막 BootNext 선택, 재실행 거부, ZIP 수집, 취소, 정상 정리 및 정리 거부 후 ZIP 보존을 확인했습니다. 기존 등록 검사 32개도 통과했습니다. EFI 실행, 실제 펌웨어 API 호출 또는 GPU 접근은 없었습니다. 실제 USB 없는 잠금 해제, 이후 정리 및 같은 세션의 벤치마크는 아직 필요합니다.
