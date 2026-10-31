# 재사용 입회 EVGA 설치/재준비 프로필

**현재 검증:** 특정 장비에서 EVP11/EVP12 SharedEsp 흐름을 입증했습니다. [최종 상태와 전달](EVP12-RESULT.ko.md). 아래 초기 설치 명령을 기존 설치에 반복하거나 소비 슬롯을 재사용하지 마세요. 참석형 잠금 해제 전에는 다시 시작 대신 Shift+종료 완전 종료를 사용합니다. 관리자 승인과 Usb/SeparateEsp 재사용 패키지 실제 검증은 대기 중입니다.

[English](REUSABLE-PROFILE.en.md)

대상은 **정확히 캡처된 EVGA RTX 3090/Z790 장비**이며 PCI `10de:2204`, 하위 시스템 `3842:3387`, PROM SHA-256 `f1c7c6b205d9a5c0626af4f82e28cc377aa16cde8c25c61c77b4b5160b26e71f`입니다. GPU/BAR/PROM, 콘솔 그래프와 드라이버 코드 검사는 기존 트랜잭션 어댑터에 컴파일된 상태로 유지합니다. JSON은 부팅 위치를 선택할 뿐 다른 GPU를 승인하지 않습니다. 일반 자동/헤드리스 설치기에 이 프로필을 등록하지 마세요.

| `research/profile/profiles/` 파일 | 경로 | 상태 |
| --- | --- | --- |
| `evga-z790-3090-shared-esp.json` | 기존 Windows ESP에 잠금 해제/로그/자산 저장 | 이전 SULK02의 실제 증거 있음. 새 재사용 포장은 별도 실제 확인 필요 |
| `evga-z790-3090-usb.json` | USB에 저장, Windows ESP는 읽기 전용 | 이전 USB 실제 증거 있음. 새 포장은 빌드/모의 검사 통과 |
| `evga-z790-3090-separate-esp.json` | 기존 별도 내부 FAT32 ESP에 저장 | 빌드/호스트 검사만 확인. 전용 ESP 실제 결과는 없음 |

영구/무인 잠금 해제나 일반 호환성을 주장하지 않습니다. EFI는 항상 실제 검사 후 새 **Shift+G**를 요구합니다. 컨트롤러는 포맷/파티션 변경, BootOrder 쓰기, Secure Boot 변경, 자동 재부팅, 마커 초기화 또는 Windows 앱 중지를 하지 않습니다.

## Movian: 첫 공유 ESP 패키지 시험

브랜치를 업데이트하고 원본 캡처 USB를 D:로 연결하세요. 기존 ESP 자산과 모든 SULK02 증거를 보존합니다. 저장소 루트의 관리자 PowerShell에서 명령 하나만 실행합니다.

```powershell
./research/profile/manage-evga-profile.ps1
```

기본값은 Install/SharedEsp, 검토된 Windows ESP GUID `c36c3e86-02aa-4f76-af1b-96e327f9d1c1`, 새 상태 디렉터리 `E:\UNLOCK\EVGA-PROFILE`, `C:/Program Files/LLVM/bin`의 LLVM입니다. 슬롯 9 로그/USB 이력을 확인하여 **슬롯 10 / EVP10**을 선택하고 새 EFI/패키지를 빌드/검증한 뒤 불변 기록, 예약, 자산/EFI/512바이트 영수증을 저장하며 마지막으로 BootNext를 설정합니다. 기존 Windows/Limine 파일과 증거는 보존합니다.

최종 예상 값은 `prepared: true`, `slot: 10`, `run: EVP10`, `requires_fresh_shift_g: true`, `boot_next_selected: true`, `boot_order_written: false`, `automatic_reboot: false`입니다. 오류 시 **재부팅하거나 Install을 반복하지 말고** 상태와 오류를 보내세요. 자동 롤백은 보장하지 않으며 상태 디렉터리 삭제로 거부를 우회하면 안 됩니다.

준비 성공 후에만 USB를 안전하게 제거하고 작업을 저장한 뒤 정상 재시작을 한 번 합니다. 새 패키지는 직접 실행되므로 이전 SULK02 런처 배너/카운트다운은 예상하지 않습니다. `Live guards passed. NOW press fresh Shift+G once` 이후 Shift+G를 한 번 누릅니다. 화면 소실 후 최대 3분 기다리고 Shift+V는 반복하지 마세요. Windows가 복귀하지 않으면 전원을 끄고 USB 없이 기존 Limine/Windows 항목을 선택하세요. 복구는 보장되지 않으며 시도를 반복하면 안 됩니다.

Windows 복귀 후:

```powershell
./research/profile/manage-evga-profile.ps1 -Action Collect
```

JSON/ZIP, 자동 또는 수동 Windows 복귀 및 USB 미연결 여부를 보내세요. 영수증/로그/런타임 마커/패키지 정보를 복사하고 결과를 확인한 뒤 정확한 임시 항목만 정리합니다. 비공개 펌웨어 백업은 ZIP에 포함하지 않습니다. 실패를 성공으로 바꾸거나 자동 재준비하지 않습니다.

## 이후 성공 실행의 재준비

성공한 실행을 수집/정리한 뒤 다음 시도는 명령 하나입니다.

```powershell
./research/profile/manage-evga-profile.ps1 -Action Rearm
```

저장된 경로/GUID/볼륨 식별자를 사용하고 이전 증거 세 해시를 확인하여 다음 연속 슬롯과 새 EFI를 만듭니다. 영수증은 재사용하지 않습니다. 대기/실패/취소/변경/미수집 시도는 검토 전까지 거부합니다. 기존 런타임 제한인 슬롯 8–99를 넘거나 이력을 초기화하지 않습니다. 같은 이력에 경로 변경이나 병렬 설치를 하지 마세요.

Status는 읽기 전용입니다. Cancel은 부팅 전 취소이며 파일과 예약을 보존하고 슬롯을 재사용하지 않습니다. 부팅 후 필수 명령이 아니며 실행 후에는 Collect를 사용합니다. USB 경로는 수집/취소 시 기존 USB 로더를 복원하고 내부 경로는 정확한 소유 Boot7Exx만 삭제합니다. 검증된 일반 대체 항목의 누락/추가만 허용하며 실제 항목/순서 변경은 여전히 거부합니다. 정리 오류 시 JSON/ZIP과 상태를 보존하고 임의 삭제를 하지 마세요.

## Windows ESP를 공유하지 않는 프로필

검토된 기존 슬롯 7 USB 이력 또는 슬롯 9 SULK02 이력만 초기 이전 대상으로 허용합니다. USB/전용 ESP 변형은 공유 ESP 실행 이력을 필수로 하지 않으며 슬롯 7에서 EVP08로 직접 이전할 수 있습니다. 여전히 정확한 해당 장비 전용이며 다른 장비는 자체 검증이 필요합니다.

첫 설치 시 USB 프로필 선택:

```powershell
./research/profile/manage-evga-profile.ps1 -Layout Usb -StateDirectory 'E:\UNLOCK\EVGA-USB-PROFILE'
```

USB 로더만 백업/교체하며 **펌웨어 쓰기는 없습니다**. 메뉴에서 USB를 한 번 부팅하고 자산/로그/영수증은 USB에 둡니다. Windows ESP에는 배포하지 않습니다. 파일 시스템이 원자적 로더 교체를 거부하면 부팅/재시도 없이 오류와 상태를 보고하세요.

기존 전용 내부 ESP 선택:

```powershell
./research/profile/manage-evga-profile.ps1 -Layout SeparateEsp -TargetDrive U -StateDirectory 'E:\UNLOCK\EVGA-SEPARATE-PROFILE'
```

U:는 Windows 시스템/부팅 파티션과 다른 기존 내부 GPT/FAT32 ESP여야 합니다. 고유 GUID를 EFI에 컴파일하고 실행 시 재확인합니다. 파티션 생성/크기 변경은 하지 않습니다. 이 경로는 호스트/빌드 검사만 있으며 실제 시연된 구성은 아닙니다. Windows ESP는 읽기 전용입니다. 선택적 Assets/EvidenceDirectory로 기존 검증 파일을 지정할 수 있습니다. 설치 후 같은 상태 디렉터리의 Rearm/Collect/Status를 사용합니다.

## 배포 패키지와 위험

build-evga-profile.ps1은 배포/펌웨어 접근 없이 EFI, 프로필, 자산 계약, PE 보고서, 생성 헤더, 소스 목록, GPL 라이선스 및 소스 링크가 포함된 ZIP을 만듭니다. 펌웨어 바이너리는 포함하지 않습니다. 설치/재준비에는 일치하는 저장소와 도구 체인이 필요하며 독립 Windows 앱은 아닙니다. 해시는 무결성/출처 기록일 뿐 서명이나 독립 증명이 아닙니다. 로컬 변경은 SourceTreeDirty로 표시합니다.

정확한 펌웨어 자산 5개와 NVIDIA-LICENSE/NOTICE.md가 필요하며 자동 다운로드/추출은 없습니다. 설치된 파일은 해시/크기가 같을 때만 재사용합니다. 기존 캡처/예약/영수증을 보존합니다. 영구 예약 EVGA-UNLOCK-RESERVED&lt;slot&gt;.ARMED가 이전 비조정 설치기를 의도적으로 막으므로 이전 후 새 컨트롤러를 사용합니다.

준비는 로컬/대상 파일과 내부 경로의 임시 NVRAM/BootNext를 씁니다. Shift+G 후 EFI는 드라이버 중지, MMIO/PCI 쓰기, 버스 마스터, DMA, 펌웨어 업로드/실행, 선택자 쓰기와 FLR을 수행합니다. 화면/입력 소실, 펌웨어/Windows 정지, 리소스 잔류, 부팅/데이터/GPU 손상이 가능하며 복구/격리는 보장되지 않습니다. 백업/복구 미디어를 준비하고 다른 부팅 관리 도구를 닫으세요. 실패 시 무작정 진행/롤백하지 않고 멈춥니다. 일반 재부팅이 잠금 해제 상태를 해제할 수 있으며 지속성을 주장하지 않습니다.

[기술 검토와 검증](REUSABLE-PROFILE-REVIEW.ko.md). 이전 실제 실행 증거가 새 설치 패키지의 첫 실제 검증을 대신하지 않습니다.
