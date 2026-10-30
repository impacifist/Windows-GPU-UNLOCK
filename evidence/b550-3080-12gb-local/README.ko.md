# GA102 RTX 3080 12 GB / ASUS TUF B550-PLUS LOCAL 근거

[English](README.md)

2026-10-29에 [이슈 #73](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/73)을
검토했습니다. 기여자의 실기기 보고이며 이번 통합 소스의 실기기 검증은 아닙니다.
원본 PROM, 드라이버 덤프, 펌웨어 자산 및 개인 식별자는 배포하지 않습니다.

보고 구성은 Gigabyte RTX 3080 12 GB `10de:220a / 1458:40a2`, GA102
`b72000a1`, 메모리 `3000` MiB, ASUS TUF GAMING B550-PLUS BIOS 3611,
Ryzen 9 5900X, Windows Insider 26340 / NVIDIA 621.01입니다.
VBIOS는 `94.02.85.00.d7`, GPU 위치는 `0000:08:00.0`,
BAR0는 `0xfb000000`부터 16 MiB입니다. 정확한 PROM/EFI 해시와 소스
이력은 위 English 표 및 원본 보고에 있습니다.

별도 완전 종료 후 캡처 두 개가 일치했고, LOCAL headless USB 실행 한 번에서
SS0 `88888888` / SS1 `8`, FLR 1회, 정상 정리 및 Windows 직접 진입이
보고되었습니다. HDMI는 즉시 복구됐지만 DisplayPort 모니터는 전원 재연결 또는
케이블 재연결이 필요했습니다. 그 원인은 독립적으로 검증되지 않았습니다.
동일 장비 cuBLAS n=4096 중앙값은 FP16 69.30 → 114.03, BF16 69.11 →
117.32, TF32 31.27 → 61.71, FP32 21.40 → 22.35 TFLOPS입니다.
CUDA/cuBLAS 12.8 도구를 사용했으므로 고정 12.6 측정과 직접 비교하지 않습니다.

이번 변경은 `220a10de / b72000a1 / 3000` 조합 및 기존 검증된
`60010` NVIDIA GOP 코드만 허용합니다. 기존 3090/3090 Ti 조합은
유지하고 10 GB/LHR 3080, 3080 Ti 및 다른 칩 리비전은 계속 거부합니다.
펌웨어 템플릿, 레지스터 순서, 소유권, DMA, 리셋 및 정리 로직은 바꾸지 않습니다.

다른 장비의 헤더 대신 [LOCAL 절차](../../docs/NEW-MACHINE.ko.md)를 사용합니다.
서브시스템, PROM, 경로, 콘솔/EDID 코드 및 BAR는 각 소유자의 캡처에 고정되며
현재 호스트 검사도 통과해야 합니다. 이번 검증은 합성/컴파일/RAM 모의 검사뿐입니다.
반복 부팅, 절전 복귀, 장기 안정성 및 애플리케이션 성능은 확인되지 않았습니다.
