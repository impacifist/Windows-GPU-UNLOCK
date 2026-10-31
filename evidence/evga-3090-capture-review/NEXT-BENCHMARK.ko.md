# EVGA 잠금 상태 성능 기준 측정

[English](NEXT-BENCHMARK.en.md) | [한국어](NEXT-BENCHMARK.ko.md)

EVGA12 이후, 잠금 해제 시도 전에 실행합니다. GitHub Desktop에서 현재 브랜치를 업데이트합니다. EFI USB로 부팅하지 말고 일반 Windows로 부팅합니다. 이후 비교를 위해 BIOS, 드라이버, 전력 제한, 클록/오버클록 및 냉각 설정을 유지합니다. GPU 작업을 종료하고 식힌 뒤 다른 작업을 저장합니다. 부하로 전력과 온도가 올라가고 드라이버/앱 충돌이 발생할 수 있습니다. GPU 설정 변경, EFI 배치 및 잠금 해제는 하지 않습니다.

각 블록을 따로 실행하고 이전 단계가 성공했을 때만 계속합니다. setup은 해시를 검증한 NVIDIA CUDA 12.6 구성 요소를 저장소 안에 다운로드하며 설치 프로그램을 사용하지 않습니다. benchmark/의 고정된 업스트림 참조 파일 2개도 갱신하므로 로컬 수정은 먼저 보존합니다. 패키지 디렉터리가 처음 생성될 때만 setup을 실행합니다. 기존/부분 디렉터리 오류가 나면 삭제하지 말고 오류를 보내 주세요. 시스템 CUDA 설치는 필요 없습니다.

두 번째 블록은 Community를 포함한 VS2019 C++ x64 도구를 검색하여 함께 설치된 최신 버전 선택을 피합니다. 기여자는 VS2019와 VS2026이 설치되어 있다고 보고했으며 이전 latest 검색은 지원되지 않는 컴파일러를 선택했습니다. setup이 이미 완료되었다면 첫 블록은 건너뛰고 빌드부터 다시 실행합니다. CUDA 12.6이 최신 컴파일러를 거부할 수 있습니다. 이 경우 전체 오류를 보내고 검사 우회나 무작정 재설치는 하지 마세요. Preflight는 GPU 부하 없이 메타데이터를 확인합니다. 마지막 블록만 실제 성능을 측정합니다.

```powershell
cd E:\UNLOCK\Windows-GPU-UNLOCK
powershell -NoProfile -File .\benchmark\setup.ps1
```

```powershell
powershell -NoProfile -File .\benchmark\build.ps1 -Card 3090
```

```powershell
powershell -NoProfile -File .\benchmark\run-benchmark.ps1 -Card 3090 -PreflightOnly
```

```powershell
powershell -NoProfile -File .\benchmark\run-benchmark.ps1 -Card 3090
```

최종 명령은 정확성 검사를 포함한 FP32, FP16 텐서/FP32 누산, BF16 텐서/FP32 누산, TF32 텐서/FP32 누산 4종을 각각 3회 측정합니다. GPU 사용/고온 상태, 다른 GPU 식별 정보, 비교 설정 변경 시 거부합니다.

새 `benchmark/results/` 타임스탬프 폴더를 압축하고 `benchmark/build-manifest.json`, `benchmark/runtime-verification.json`과 함께 보내 주세요. 완료된 실행에는 `comparison-complete.json`이 있어야 합니다. 부분 결과와 오류도 유용하지만 기준 측정으로 인정하지 않습니다. 이후 측정을 위해 `benchmark/bin/`과 빌드 매니페스트를 그대로 보존합니다. 결과 분석은 여기서 수행하므로 같은 검사를 중복 실행할 필요는 없습니다.

이는 시도 전 성능 기준입니다. 이후 동일한 실행 및 출처 비교로 성능 변화를 측정할 수 있으나 부모 해제나 높은 점수 하나만으로 잠금 해제가 입증되지는 않습니다. 기준 결과 검토 및 다음 범위 안내 전에는 새 EFI를 실행하지 마세요.
