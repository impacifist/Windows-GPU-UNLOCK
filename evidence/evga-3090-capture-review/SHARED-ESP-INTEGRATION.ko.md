# 다음 하드웨어 테스트: 실제 공유 ESP handoff 및 capture 통합

[English and exact commands](SHARED-ESP-INTEGRATION.en.md)

RETAIN01 COMPLETE 사진, Windows 정상 복귀, COMPLETE 기록(SHA-256 `f8b09577c3919b8eba2627e0df6be22d6a1814674b6a69f78894afc86954d2c6`)과 원본 USB 복원이 보고되었습니다. INTG01은 실제 `handoff.c`의 공유 ESP `Prepare`, logger/Flush, 파일 닫기 및 `Windows` StartImage 경로와 실제 `GuCaptureGraphOnVolume`을 통합합니다. 자산 다섯 개의 동시 보존/재검사/해제도 포함합니다. 그래프 수집을 위해 프로토콜 정보와 PCI 설정을 읽지만 driver Stop/disconnect, GPU/PCI 쓰기, DMA, FLR, 펌웨어 자산 실행, NVRAM 쓰기 또는 언락은 없습니다. EVGA Stop/transaction/vendor 코드는 링크하지 않습니다.

**위험:** UEFI에서 활성 공유 ESP에 새 로그(그래프 최대 8 MiB 및 상태 줄)와 별도 512바이트 마커를 씁니다. 펌웨어/GPU 인터페이스 읽기와 약 85 MB 동시 메모리 보존도 수행합니다. 펌웨어 오류, 중단, 파일시스템 문제, 메모리 부족 또는 정지로 데이터/부팅에 영향을 줄 수 있으며 복구는 보장되지 않습니다. ESP와 USB를 별도로 백업하세요. 기존 자산/기록/마커/EFI/백업은 보존합니다. 실패 후 마커를 지우거나 초기화해 재시도하지 마세요. staging은 정확한 RETAIN01 COMPLETE 기록을 요구하며 기존 INTG01 파일이 있으면 거부합니다. 런타임은 Prepare 전에 일회성 마커를 소비하고 기존 로그가 있으면 다른 슬롯을 선택하지 않고 거부합니다. UEFI CREATE는 원자적 create-new가 아니므로 동시 외부 쓰기 위험은 남습니다.

업데이트된 저장소 루트에서 관리자 PowerShell로 영어 문서의 명령을 순서대로 실행하세요. 실제 USB 드라이브 문자로 바꾸고 이전 성공한 컴파일러 인수를 유지하세요. ESP GUID는 `c36c3e86-02aa-4f76-af1b-96e327f9d1c1`이며 자산은 다시 복사하지 않습니다. 이전 USB 복원을 먼저 확인하세요. 원본 USB 해시를 USB 밖에 기록하세요.

새 ESP 파일은 `EFI/EVGA-SHARED-INTEGRATION.EFI`, `RTX3090-UNLOCK/INTG01.ARMED`, 실행 후 `RTX3090-UNLOCK/INTG01.TXT`입니다. 새 USB 백업은 `EFI/BOOT/BOOTX64.before-shared-integration.efi`입니다. 기존 Windows/Limine 해시 및 16 MiB 여유 공간을 검사합니다. `-WhatIf`는 쓰기와 쓰기 블록 내부 부팅 파일 해시 검사를 건너뜁니다. 오류 시 멈추고 파일/출력을 보존하세요. 자동 재부팅/부팅 항목 변경은 없습니다.

기존 펌웨어 USB 선택으로 **한 번만** 부팅하세요. `INTG01 ENTERED`와 최종 `INTG01 COMPLETE` 또는 `INTG01 FAILED`를 촬영하고 Windows 부팅 여부를 알려주세요. COMPLETE는 Prepare, 자산 보존/해제, capture 및 handoff 전 상태 쓰기 성공을 뜻합니다. 10초 후 실제 Windows 경로에서 로그/root를 닫고 StartImage를 시도합니다. COMPLETE 마커 뒤에도 로그/닫기/handoff가 실패할 수 있으므로 사진, 로그, 부팅 결과를 함께 봐야 합니다. 부분 capture는 언락 허가가 아닙니다. Prepare 성공 후 실패 시에는 마커 쓰기/닫기가 성공했을 때 Windows를 시도하며, 더 이른 실패는 펌웨어로 돌아갈 수 있습니다. 3분 안에 Windows가 나오지 않으면 전원을 끄고 USB를 제거한 뒤 일반 Limine/Windows를 사용하세요. 실패한 테스트를 반복하지 마세요.

Windows에서 영어 문서의 `read-shared-esp-integration.ps1` 명령으로 새 Desktop 출력 폴더에 기록 두 개를 복사하고 `restore-shared-esp-integration-launcher.ps1`로 USB를 복원하세요. 읽기 스크립트는 ESP에 쓰지 않고 기존 출력 폴더를 거부합니다. JSON 출력, `INTG01.ARMED`, `INTG01.TXT`, 사진 두 장, 복원 출력 및 부팅 결과를 비공개로 보내주세요. 로그에는 장치 경로/주소가 있습니다. 펌웨어 자산은 보내지 마세요. ESP 파일은 보존하고 USB 제거 후 Windows/CachyOS 일반 Limine 부팅도 확인하세요. 새 PowerShell 창에서는 기록해 둔 원본 해시를 사용하세요.

로컬 Prepare/logger/Windows 8개, 자산/마커 9개, 런처 10개 mock과 staging/복원/evidence fixture, 실제 diagnostic 56개 host 검사를 통과했습니다. 기존 probe/profile 회귀 검사도 통과했습니다. 기본 VS2022 경로가 없어 설치된 VS2019 BuildTools 경로로 검사했습니다. PE 결과/해시는 영어 문서에 있습니다. 실제 하드웨어 실행은 아직 없으며 내부 언락/실제 transaction marker 연속성/자동 실행/복구 증거가 아닙니다.
