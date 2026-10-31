# 다음 테스트: 자산 동시 보존 및 일회성 ESP 기록

[English and exact commands](SHARED-ESP-RETAIN.en.md)

이전 사진에서 `FIVE ESP ASSETS VERIFIED`가 확인되었고 Windows 복귀 및 USB 복원이 보고되었습니다. 이번에는 자산 다섯 개를 **동시에 메모리에 보존**하고 다시 SHA-256 검사한 뒤 모두 해제합니다. 같은 ESP의 512바이트 테스트 기록 파일에 쓰기, Flush 및 다시 읽기 검증도 수행합니다. GPU 접근, 펌웨어 자산 실행 또는 언락은 없습니다. 기존 unlock의 `Prepare`, logger, capture graph 및 transaction marker를 실행하는 테스트는 아니므로 성공해도 내부 언락 검증은 아닙니다.

**위험:** 이제 UEFI에서 활성 Windows/CachyOS 공유 ESP에 파일을 씁니다. 중단, 파일시스템 오류, 메모리 부족 또는 펌웨어 정지로 데이터나 부팅에 영향을 줄 수 있습니다. ESP와 USB를 먼저 별도로 백업하세요. 복구는 보장되지 않습니다. 기존 자산/기록/마커/EFI/USB 백업은 유지합니다. 같은 테스트 파일이 있으면 staging이 거부됩니다. 실행 시 정확한 PENDING 기록만 허용하고 자산 할당 전에 STARTED를 기록하므로 재실행도 거부합니다. 거부를 우회하려고 기록을 지우거나 초기화하지 마세요. 실패한 부분 파일은 자동 삭제하지 않습니다.

업데이트된 저장소 루트에서 관리자 PowerShell로 영어 문서의 명령을 순서대로 실행하세요. USB 문자는 실제 값으로 바꾸고, 필요하면 이전 성공한 컴파일러 인수를 사용하세요. ESP GUID는 `c36c3e86-02aa-4f76-af1b-96e327f9d1c1`입니다. 이전 자산은 ESP에 그대로 있어야 합니다. 다시 복사하지 않고 검증합니다. 16 MiB 여유 공간이 필요합니다. 공간 부족 시 파일 삭제나 파티션 변경을 하지 말고 결과를 알려주세요.

새 파일은 ESP의 `EFI/EVGA-SHARED-RETAIN.EFI`, `RTX3090-UNLOCK/RETAIN01.TXT`와 USB의 별도 백업 `EFI/BOOT/BOOTX64.before-shared-retain.efi`입니다. USB 원본 SHA-256을 USB 밖에 기록하세요. `-WhatIf`는 쓰기를 건너뛰며 쓰기 블록 내부 부팅 파일 해시 검사도 수행하지 않습니다. 오류 시 멈추고 파일과 출력을 보존하세요. 자동 재부팅/펌웨어 항목 변경은 없습니다.

이전과 같은 펌웨어 USB 선택으로 **한 번만** 부팅하세요. `RETAIN01 ENTERED`와 최종 `RETAIN01 COMPLETE` 또는 `RETAIN01 FAILED OR ALREADY USED`를 촬영하세요. COMPLETE는 다섯 메모리 할당의 동시 보존/재검사/해제와 최종 기록 Flush/다시 읽기/닫기 검증이 성공했다는 뜻입니다. 언락은 수행하지 않았습니다. 성공/실패 표시 10초 후 Windows 연결을 시도합니다. 3분 안에 Windows가 나오지 않으면 전원을 끄고 USB를 제거한 뒤 기존 Limine/Windows를 선택하세요. 실패한 실행은 반복하지 마세요.

Windows 복귀 후 `./research/profile/read-shared-esp-retain.ps1 -WindowsEspGuid $esp` 명령으로 `RTX3090-UNLOCK/RETAIN01.TXT`를 읽거나 복사하세요. 파일은 NUL 패딩 포함 512바이트입니다. 첫 줄이 `COMPLETE RETAIN01 - FIVE ASSETS RETAINED REHASHED FREED - NO GPU`여야 합니다. STARTED/FAILED/PENDING, 누락/잘림/오류는 성공이 아닙니다. 기록은 Windows 연결 호출 전에 쓰므로 Windows 부팅 성공 자체를 증명하지 않습니다. ESP 파일과 기록은 그대로 보존하세요.

영어 문서의 `restore-shared-esp-retain-launcher.ps1`과 기록한 원본 해시로 USB를 복원하세요. USB 제거 후 Windows/CachyOS의 일반 Limine 부팅도 확인하세요. 빌드/staging/복원 출력, 사진 두 장, 작은 기록 파일 및 부팅 결과를 보내주세요. 펌웨어 자산은 보내지 마세요. 새 PowerShell 창에서는 기존 `$original` 값이 없으므로 USB 밖에 기록한 실제 해시를 사용하세요.

로컬 mock 9개, 런처 mock 10개, 공간 부족/WhatIf/덮어쓰기 거부/기록 내용 및 USB 복원 fixture와 PE 검사를 통과했습니다. 실제 하드웨어 실행은 아직 없습니다. 빌드 크기 및 SHA-256은 영어 문서에 있습니다.
