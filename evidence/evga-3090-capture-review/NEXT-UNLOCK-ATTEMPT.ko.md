# EVGA 3090 통합 잠금 해제 시도

[English](NEXT-UNLOCK-ATTEMPT.en.md) | [한국어](NEXT-UNLOCK-ATTEMPT.ko.md)

**이 카드에서 실제 잠금 해제를 처음 시도하는 실행입니다.** 기존 보드 검증과 새 Shift+G 입력 후 캡처된 콘솔/GPU 드라이버 체인을 해제하고 PCI 소유권을 얻습니다. GPU MMIO/PCI 쓰기, 버스 마스터 활성화, DMA 버퍼 매핑, 펌웨어 업로드/실행, 선택자 쓰기 및 PCIe 기능 수준 리셋1회를 수행합니다. 이후 격리/정리 및 직접 Windows 부팅을 시도합니다. 성공한 경우에만 아래 벤치마크를 수동으로 실행하세요. 검사에 실패하면 트랜잭션 전에 중단할 수 있습니다.

**위험:** GPU나 PC가 멈추거나 화면/입력을 잃을 수 있고 Windows 또는 GPU 초기화가 실패할 수 있습니다. 펌웨어, GPU 상태 또는 데이터가 영구 손상될 가능성도 있으며 복구는 보장되지 않습니다. 해제 후 실패 시 자동 롤백은 없으며 정리가 불확실하면 Windows로 진행하지 않고 대기합니다. 실행 중 강제 종료도 해를 줄 수 있습니다. 위험을 이해하고 물리적으로 전원 종료, USB 제거 및 정상 Windows Boot Manager 선택이 가능한 경우에만 실행하세요. 전원 종료가 필요할 수 있지만 복구가 보장되지는 않습니다.

EVGA16에서 EFI 자산5개 확인, 공유 PCI 독점 획득/닫기, 원본 PROM 해시, 디코딩 복원 및 Windows 준비가 기록되었습니다. 이번 시도는 자산5개와 독점 소유권을 트랜잭션 동안 유지합니다. 기존 상태/PROM 검사 후 같은1MiB PROM을 이전에 사용한 DWORD 방식으로 두 번 더 읽습니다. 세 번의 전체 읽기는 최대 786,432 DWORD 접근이며 런타임의 다른 레지스터 접근도 있습니다. Secure Boot가 비활성 상태로 보고되어야 하며 스크립트는 Secure Boot 설정을 변경하지 않습니다. 새 G 한 번이 트랜잭션 승인이며 해제 후 추가 입력은 없습니다.

GitHub Desktop에서 브랜치를 업데이트하고 저장소 루트에서 각 명령을 따로 실행하세요. 오류가 있으면 중단하세요.

```powershell
./research/profile/build-evga-text-stop.ps1 -WindowsEspGuid "c36c3e86-02aa-4f76-af1b-96e327f9d1c1" -Clang "C:/Program Files/LLVM/bin/clang.exe" -LldLink "C:/Program Files/LLVM/bin/lld-link.exe" -AttemptUnlock
./research/profile/stage-evga-text-stop.ps1 -Drive D -AttemptUnlock
```

스테이징은 검토된 EVGA16과 이전 캡처가 필요합니다. 기여자가 로컬 생성한 자산5개 및 NVIDIA-LICENSE/NOTICE.md를 USB에 복사/검증하고 기존 EFI를 백업하며 이전 로그/마커를 보존합니다. 기존 자산이 다르면 거부합니다. 새 EVGA-UNLOCK.ARMED 마커는 일회용이므로 삭제하거나 시도를 반복하지 마세요. 펌웨어 바이너리는 Git에 포함하거나 업로드하지 않습니다.

완전히 종료하고 부팅 메뉴에서 UEFI USB를 선택하세요. 자산 검사와 **Live guards passed**까지 기다린 뒤 **새 Shift+G를 한 번** 누르세요. 화면이 멈출 수 있으며 깜박이는 커서는 진행 표시가 아닙니다. 펌웨어와 Windows 시작에 시간을 주세요. 응답이 없고 Windows가 시작되지 않으면 수동 전원 종료와 USB 제거 후 정상 Windows Boot Manager를 선택해야 할 수 있습니다. 반복하지 마세요. 최신 EVGA 로그와 실패/복구 내용을 보내세요. 약속한 보고 규칙에 따라 실패 보고가 없으면 정상 Windows 도착을 가정합니다.

Windows가 돌아오고 GPU가 동작하면 **로그에서 트랜잭션 성공이 확인될 때만** 기존 실행 파일과 기준 측정 설정으로 벤치마크를 실행하세요.

```powershell
$log = Get-ChildItem D:/RTX3090-UNLOCK -Filter 'EVGA*.TXT' | Sort-Object Name | Select-Object -Last 1
if (!$log -or !(Select-String -LiteralPath $log.FullName -SimpleMatch 'evga-unlock-transaction-return status=0x0000000000000000' -Quiet)) { throw 'No successful unlock transaction recorded. Send the newest EVGA log before running the benchmark.' }
powershell -NoProfile -File ./benchmark/run-benchmark.ps1 -Card 3090
```

최신 EVGA 로그와 타임스탬프가 있는 벤치마크 ZIP을 보내세요. 잠금 상태 기준 중앙값은 FP32 26.379272, FP16 64.527754, BF16 64.558792, TF32 32.982266 TFLOPS입니다. 펌웨어 성공만으로 성능 향상이 입증되지는 않으며 비교와 런타임 정확성 확인이 필요합니다.

로컬 검증: 캡처된 EVGA PROM/설정 및 검증된 자산을 사용하는 실제 벤더 코어/호환성/포트 시나리오28개(PCI/MMIO/DMA/펌웨어 실행은 모의), 어댑터19개, 검사25개, 자산 유지 및 통합 래퍼 실패/정리 모의 검사, USB 스테이징 및 PE 검사 통과. 실제 하드웨어 트랜잭션/복구는 미검증입니다. 로컬 EFI 85,504바이트 SHA256 77ff9d20f3fb0552933b31a776785f0dd7b80ff75c44dd01aaa84ca814d6986e. 기여자의 컴파일러에서 해시가 다를 수 있으며 해당 빌드의 소스/매니페스트를 검사합니다.
# EVGA17 이후 두 번째 시도

**세 번째 시도 업데이트:** EVGA18에서 트랜잭션 내부 SS0가 다시 다른 0x12053015였고 허용 검사가 실패했습니다. 다음 빌드는 진입 시 SS0를 고정합니다. 접근 불가 및 이미 잠금 해제된 센티널 값을 거부하고 쓰기 전까지 선택자 값이 변하지 않아야 합니다. 이전 허용 목록보다 더 많은 상태를 허용하며 변경된 비트의 의미는 알 수 없습니다. GPU 쓰기, DMA, 펌웨어 실행 및 리셋에 도달할 수 있고 실제 안전성과 복구는 검증되지 않았습니다. 이전 두 표식과 EVGA17/EVGA18을 보존하세요. 새 표식은 EVGA-UNLOCK3.ARMED이며 삭제하거나 반복하지 마세요.

EVGA17은 PCI 독점 획득 후 ACCESS_DENIED를 반환했습니다. 앞서 안정적으로 읽힌 선택자는 0x30053015였지만 기존 허용값은 0x30053014였습니다. 트랜잭션 내부의 이후 읽기 값은 기록되지 않아 정확한 거부 원인은 확인되지 않았습니다. 이 빌드는 관찰된 두 값만 허용하고 시도 중 최초 선택자 값이 바뀌면 거부합니다. 두 값의 하드웨어 동등성과 복구 성공은 검증되지 않았습니다. 트랜잭션 내부 초기 상태와 허용 결과를 기록합니다.

스테이징에는 원래 EVGA-UNLOCK.ARMED와 검토한 EVGA17.TXT가 필요합니다. 둘 다 보존하세요. 새 일회용 표식은 EVGA-UNLOCK2.ARMED입니다. 삭제하거나 반복하지 마세요. 아래 위험과 부팅 지침은 계속 적용됩니다. 첫 시도는 잠금 해제 성공을 입증하지 못했습니다.
