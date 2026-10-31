# 공유 ESP 자산 사전 검사 — 다음 하드웨어 테스트

[English and exact commands](SHARED-ESP-ASSETS.en.md)

이전 visible audit의 ESP 진입과 이후 Windows 부팅, USB 복원 및 일반 Windows/CachyOS Limine 부팅이 보고되었습니다. 다음 단계는 동일 ESP의 `\RTX3090-UNLOCK\ASSETS`에 있는 펌웨어 파일 다섯 개를 UEFI에서 읽고 SHA-256으로 검사하는 것입니다. GPU 접근, 펌웨어 실행, 로그/마커 쓰기, 부팅 항목 변경 또는 언락은 수행하지 않습니다. 각 파일 메모리는 검사 직후 해제합니다. 성공/실패 메시지를 10초 표시한 뒤 기존 Windows 연결 경로를 시도합니다. 파일 검사 실패 시에도 Windows 연결을 시도합니다.

**위험:** 활성 공유 ESP에 약 85 MB를 씁니다. ESP와 USB를 먼저 별도로 백업하세요. 추가 16 MiB 여유 공간이 필요하며 기존 자산 디렉터리, 기존 테스트 파일 또는 변경된 Windows/Limine 해시는 거부합니다. 공간 부족 시 파티션 크기를 변경하거나 파일을 지우지 말고 출력을 보내세요. 중단, 파일시스템 오류 및 펌웨어 정지로 부팅에 영향을 줄 수 있으며 복구는 보장되지 않습니다. 실패 시 파일은 자동 삭제하지 않습니다. 동시 파일 변경 위험은 남아 있습니다.

영어 문서의 PowerShell 명령을 저장소 루트에서 관리자 권한으로 순서대로 실행하세요. ESP GUID는 `c36c3e86-02aa-4f76-af1b-96e327f9d1c1`입니다. USB 드라이브 문자는 실제 값으로 바꾸세요. `$assets`는 기존 로컬 준비 결과이며 파일 다섯 개와 `NVIDIA-LICENSE`, `NOTICE.md`가 필요합니다. 펌웨어는 로컬에 보관하고 전송하지 마세요. 오류가 나면 멈추고 파일과 출력을 보존하세요. `-WhatIf`는 복사를 하지 않으며 쓰기 블록 내부의 기존 부팅 파일 해시 검사는 실행하지 않습니다.

새 ESP 파일은 `EFI/EVGA-SHARED-ASSETS.EFI`, USB 백업은 `EFI/BOOT/BOOTX64.before-shared-assets.efi`입니다. 기존 audit 파일과 백업은 유지됩니다. USB 원본 SHA-256을 USB 밖에 기록한 후 진행하세요. 자동 재부팅이나 펌웨어 등록은 없습니다.

이전과 동일한 일회성 펌웨어 USB 선택으로 부팅하세요. `ASSET PREFLIGHT ENTERED`와 그 다음 `FIVE ESP ASSETS VERIFIED` 또는 `ESP ASSET CHECK FAILED` 화면을 촬영하고 Windows 부팅 여부를 알려주세요. 런처의 `AUDIT LOADED FROM PINNED ESP`만으로는 파일 검사 성공을 알 수 없습니다. 3분 안에 Windows가 나오지 않으면 전원을 끄고 USB를 제거한 뒤 기존 Limine/Windows 항목을 사용하세요. 실패한 테스트를 반복 부팅하지 마세요.

Windows에서 영어 문서의 `restore-shared-esp-assets-launcher.ps1` 명령과 기록한 원본 해시로 USB를 복원하세요. USB 제거 후 Windows와 CachyOS의 일반 Limine 부팅도 확인하세요. 실행/복원 출력과 화면 사진 두 장, 부팅 결과를 보내주세요. ESP 테스트 파일과 자산은 검토를 위해 그대로 두세요.

로컬 PE 검사, 파일 읽기/해시/오류 정리 8개 mock, 런처 선택/연결 10개 mock 및 공간 부족/덮어쓰기 거부/USB 복원 fixture가 통과했습니다. 실제 ESP/USB 또는 GPU는 테스트하지 않았습니다. 정확한 빌드 해시는 영어 문서에 있습니다. 하드웨어 결과는 아직 없으며 내부 언락, 동시 자산 보존, 로그/마커 쓰기 및 자동 실행은 검증하지 않습니다.

## 실제 하드웨어 결과

2026-10-28 기여자는 `ASSET PREFLIGHT ENTERED`와 `FIVE ESP ASSETS VERIFIED`가 보이는 사진을 제출했습니다. 이후 Windows가 정상 부팅되었으며 USB 복원 스크립트가 원래 부팅 파일을 복원하고 검증했다고 보고했습니다. 사진은 이 시스템에서 다섯 자산 검증 성공 경로에 도달했음을 보여 주지만 모든 자산의 동시 보유, 잠금 해제 또는 기록된 Windows 인계는 증명하지 않습니다. 이 시험 후 일반 Limine 부팅은 아직 확인되지 않았습니다.
