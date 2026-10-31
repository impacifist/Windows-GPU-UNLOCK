# EVGA 완전 종료 후 재부팅 및 잠금 해제 반복 시험

[English](NEXT-REPEAT-UNLOCK.en.md) | [한국어](NEXT-REPEAT-UNLOCK.ko.md)

EVGA19는 트랜잭션 성공과 Windows 복귀를 기록했으며 텐서 처리량 증가도 측정됐습니다. 이 빌드는 새 영구 마커 `EVGA-UNLOCK4.ARMED`를 사용해 동일한 보호 트랜잭션을 한 번 반복합니다. 이전 마커와 정확한 EVGA19 캡처를 보존하고 요구합니다. 이는 두 번째 입회 시험이며 자동 또는 반복 사용 가능한 런처가 아닙니다. 중단 후 마커를 삭제하거나 재시도하지 마세요.

먼저 완전히 종료한 뒤 USB EFI 없이 Windows로 부팅하고 동일한 벤치마크를 실행해 완전 종료 후 상태를 측정합니다. 결과를 보존한 뒤 새 EFI를 준비하고 다시 완전히 종료합니다. UEFI USB로 부팅하여 실시간 검사가 통과되면 새로운 Shift+G를 한 번 누릅니다. Windows가 직접 시작되면 동일한 벤치마크를 다시 실행하고 두 결과 폴더와 최신 EVGA 로그를 보내 주세요. 이 결과는 재시작이나 절전 동작을 증명하지 않습니다.

완전히 종료할 때마다 먼저 작업을 저장한 후 Windows에서 `shutdown /s /t 0`을 실행하세요. 즉시 종료되며 빠른 시작의 하이브리드 종료를 방지합니다.

트랜잭션에는 GPU/PCI 쓰기, DMA, 펌웨어 실행 및 FLR이 포함됩니다. 화면이나 입력이 사라지거나 시스템이 멈추거나 Windows가 시작되지 않을 수 있으며 GPU, 펌웨어 또는 데이터 손상이 영구적일 수 있습니다. 복구는 입증되지 않았고 해제 후 검사 실패 시 자동 롤백 없이 중단합니다. Windows가 시작되지 않으면 수동 전원 종료, USB 제거 및 일반 Windows Boot Manager 선택이 필요할 수 있지만 복구가 보장되지는 않습니다. 위험을 수용하는 경우에만 진행하세요.

브랜치를 업데이트한 후 저장소 루트에서 각 명령을 별도로 실행하고 오류가 나면 중단합니다.

```powershell
./research/profile/build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:/Program Files/LLVM/bin/clang.exe" -LldLink "C:/Program Files/LLVM/bin/lld-link.exe" -AttemptUnlock
./research/profile/stage-evga-text-stop.ps1 -Drive D -AttemptUnlock
```

각 Windows 세션에서 기존 실행 파일과 설정을 사용합니다.

```powershell
powershell -NoProfile -File ./benchmark/run-benchmark.ps1 -Card 3090
```

로컬 EFI 빌드와 모의 테스트는 실제 반복 성공을 증명하지 않습니다. 로컬 EFI SHA-256은 `5ff53759152b7d5705c02a865e4315091ed250edf4f8cd86685ef9b8ce0ebb73`이며 기여자 빌드는 다를 수 있습니다. 스테이징은 해당 빌드의 해시를 검증합니다.
