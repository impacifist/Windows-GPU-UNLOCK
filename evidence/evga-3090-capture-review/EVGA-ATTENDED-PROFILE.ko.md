# EVGA 3090 입회 USB 프로필

[English](EVGA-ATTENDED-PROFILE.en.md) | [한국어](EVGA-ATTENDED-PROFILE.ko.md)

이 실험적 전용 시스템 프로필은 EVGA19 및 EVGA20 연구 빌드를 확장합니다. 두 트랜잭션은 성공적으로 반환됐고 측정된 텐서 GEMM 처리량 증가가 반복됐습니다. 새로운 슬롯 기반 빌드와 재준비 절차는 호스트 모의 시험과 PE 검사를 통과했지만 실제 펌웨어 실행은 아직 없습니다. USB와 사용자 입회가 필요하며 각 시도마다 새 슬롯, 재빌드, 스테이징 및 Shift+G가 필요합니다. 자동 내부 부팅 프로필이 아니며 다른 하드웨어나 펌웨어 구성에 적합하다고 검증되지 않았습니다.

각 시도는 새로운 영구 `EVGA-UNLOCK<slot>.ARMED` 마커를 만들고 캡처에 슬롯을 기록합니다. 슬롯 5는 검토된 정확한 EVGA20 캡처와 마커 4를 요구합니다. 슬롯 6–99는 이전 마커 및 최신 EVGA 로그의 이전 슬롯, 완료된 캡처 2개, 성공적인 트랜잭션 반환, 자산 정리 및 Windows 준비 상태도 요구합니다. 이전 시도가 없거나 중단되거나 실패했다면 조사해야 합니다. 마커를 삭제하거나 캡처를 수정하거나 슬롯을 건너뛰어 검사를 우회하지 마세요. 스테이징 검사는 기록된 이벤트를 확인할 뿐 실제 Windows 시작이나 하드웨어 안전을 증명하지 않습니다.

실험적인 준비 래퍼는 다음 연속 슬롯을 선택하고 동일한 빌드 및 스테이징 스크립트를 한 명령으로 호출합니다. 재부팅하거나 EFI 트랜잭션을 승인하지 않습니다. 슬롯 선택과 `-WhatIf`는 합성 마커 및 모의 저장소에서 로컬 시험했으며 전체 래퍼는 USB 또는 실제 하드웨어에서 아직 시험하지 않았습니다. `-WhatIf`는 빌드나 스테이징 없이 선택된 슬롯을 보여줍니다. 래퍼가 거부해도 증거를 삭제하지 마세요.

이후 Movian은 자신의 USB에서 드라이런이 슬롯 7을 선택하고 예상된 WhatIf 작업을 표시했다고 보고했습니다. 이는 보고된 드라이런 선택만 확인하며 실제 빌드, 스테이징 또는 부팅은 검증하지 않습니다.

이후 캡처된 EVGA22와 검증된 자산을 사용하는 로컬 시험 환경에서 전체 래퍼가 슬롯 7을 빌드하고 스테이징했습니다. 저장소 장치 정보는 모의 처리했으며 시험 마커를 보존하고 기존 시험 EFI를 백업했습니다. Movian의 실제 USB 스테이징이나 하드웨어 실행을 증명하지는 않습니다.

이후 Movian이 자신의 USB에 슬롯 7을 빌드하고 스테이징한 출력을 제공했습니다. 빌드와 스테이징 SHA-256 `20ce56394feefefb6bb249a8bfc0100c27d1d58a46d9c7cd903e70fd3cf70cb4`가 일치하며 이전 EFI는 백업됐습니다. 이는 기여자가 제공한 출력이며 슬롯 7 EFI 부팅과 하드웨어 동작은 아직 검증되지 않았습니다.

```powershell
./research/profile/prepare-evga-attended.ps1 -Drive D -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:/Program Files/LLVM/bin/clang.exe" -LldLink "C:/Program Files/LLVM/bin/lld-link.exe" -WhatIf
```

**위험:** GPU/PCI 쓰기, DMA, 펌웨어 실행 및 리셋이 포함됩니다. 화면이나 입력이 사라지거나 시스템이 멈추거나 부팅되지 않을 수 있으며 GPU, 펌웨어 또는 데이터가 영구적으로 손상될 수 있습니다. 복구는 보장되지 않으며 해제 후 검사 실패 시 자동 롤백 없이 중단합니다. 필요하면 전원을 끄고 USB를 제거한 뒤 일반 Windows Boot Manager를 선택하세요. 전원 종료로 복구되지 않을 수도 있습니다. 위험을 수용하고 물리적으로 복구를 시도할 수 있는 경우에만 진행하세요.

첫 시험은 브랜치를 업데이트하고 저장소 루트에서 각 명령을 별도로 실행하세요. 오류 시 중단합니다.

```powershell
./research/profile/build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:/Program Files/LLVM/bin/clang.exe" -LldLink "C:/Program Files/LLVM/bin/lld-link.exe" -AttemptUnlock -AttemptSlot 5
./research/profile/stage-evga-text-stop.ps1 -Drive D -AttemptUnlock
```

작업을 저장하고 `shutdown /s /t 0`으로 완전히 종료합니다. UEFI USB로 부팅하고 **Live guards passed**가 표시되면 새로운 **Shift+G를 한 번** 누릅니다. Windows가 시작되면 최신 `EVGA*.TXT`, 빌드 및 스테이징 출력, 관찰된 문제를 보내 주세요. 기존 벤치마크 실행 파일과 설정으로 성능을 다시 확인할 수 있습니다. 모든 마커와 캡처를 보존하고 중단된 시도는 반복하지 마세요. 검토 후 다음 슬롯을 명시적으로 빌드하고 스테이징할 수 있으며 매 시도마다 이 과정이 필요합니다.

로컬 슬롯 5 빌드: 86,016바이트, SHA-256 `410edb27a179ad11bbcb8c9020de6f654b51f298ae57ad3f466d30f3c51e9a48`. 컴파일러에 따라 해시가 다를 수 있으며 스테이징 스크립트는 기여자의 빌드를 검증합니다. 로컬에서 펌웨어를 실행하지 않았습니다.
