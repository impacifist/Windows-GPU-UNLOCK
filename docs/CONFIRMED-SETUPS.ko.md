# 보고된 동작 구성과 프로필 적용 범위

[English](CONFIRMED-SETUPS.en.md) | [Compatibility](COMPATIBILITY.ko.md)

2026-10-29 스냅샷: 공개/종료 이슈 **105개와 댓글 73개**를 검토한 구성 기록입니다. PR은 이슈 수에 포함하지 않았습니다. 기여자의 과거 빌드 성공 보고를 분류한 것으로, 최신 main의 실기기 검증이나 GPU 모델 전체 지원 목록이 아닙니다.

**고정 프로필과 LOCAL 프로필은 다릅니다.** 기존 고정 프로필 6개는 호환성 문서에 유지됩니다. LOCAL은 소유자 자신의 캡처에서 헤더를 생성하고 해당 소스/자산/캡처로 호스트 검사를 통과해야 합니다. 아래 기록이나 다른 PC의 헤더를 입력으로 복사하지 마세요.

각 구성의 소스 이력·측정·공개 해시와 전체 검토 이슈 목록은 [기계 판독 기록](../evidence/compatibility-audit/setups.json) 및 [이슈별 평가](ISSUE-AUDIT.ko.md)에 있습니다. 기록은 실행 권한이 없고 어떤 빌드 스크립트에도 등록하지 않습니다.

## 기존 프로필 경로

| 근거 | GPU / PCI / 서브시스템 | 보드 / BIOS / CPU | 보고 경로 및 결과 |
|---|---|---|---|
| [#4](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/4) | MSI RTX 3070 Ti 8GB · `10de:2482` / `1462:5052` | ASUS TUF GAMING B660M-E D4 / 4003 / i7-12700F | b660m-3070ti internal. 트랜잭션 및 Windows/화면 복구. 반복 부팅도 보고됨. |
| [#12](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/12) | MSI RTX 3090 24GB · `10de:2204` / `1462:3881` | MSI B550M PRO-VDH WIFI / 2.O1 / Ryzen 7 5700X | LOCAL; no EDID driver; bus 2b. 트랜잭션 및 Windows/화면 복구. no-EDID / 비기본 버스. |
| [#81](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/81) | ZOTAC RTX 3090 24GB · `10de:2204` / `19da:1613` | ASUS TUF GAMING B650-PLUS / 3287 / Ryzen 9 7950X3D | LOCAL-220410de-161319da-a95a2b89 USB. 완전 종료 USB 1회 및 Windows/화면 복구. |
| [#84](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/84) | ASUS Turbo RTX 3090 24GB · `10de:2204` / `1043:87d5` | ASUS PRIME B550M-A WIFI II / 3621 / Ryzen 5 5600X | LOCAL-220410de-87d51043-f4c45666 USB. 콜드 USB 1회 및 화면 복구. ZIP의 정확한 커밋은 모름. |
| [#87](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/87) | ZOTAC RTX 3060 GA104 12GB · `10de:2487` / `19da:2630` | ASUS PRIME B760M-A WIFI / 1820 / i7-14700F | LOCAL USB; two-child aggregate. 최신 댓글에서 3회 성공. 초기 캡처 거부 기록을 갱신함. |
| [#91](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/91) | EVGA RTX 3070 XC3 8GB · `10de:2484` / `3842:3755` | Gigabyte B550M DS3H / F21c / Ryzen 5 5600X | LOCAL-248410de-37553842-e177a27c USB. 콜드 USB 1회 및 Windows 복구. 2484 조합은 이미 반영됨. |
| [#105](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/105) | Gigabyte RTX 3060 Ti LHR 8GB · `10de:24c9` / `1458:40cd` | Gigabyte X870 EAGLE WIFI7 / F11 / Ryzen 9 9950X | LOCAL USB; exact 24c9/2000/60015 tuple. 재시작 기반 1회 및 화면 복구. 전체 완전 종료 근거 아님. |
| [#113](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/113) | Zotac RTX 3070 8GB · `10de:2484` / `19da:2617` | Gigabyte B460M D2VX SI / FE / i5-10400F | LOCAL-248410de-261719da-b0dc28e6 USB. 콜드 USB 1회 및 Windows/화면 복구. |
| [#124](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/124) | Gainward RTX 3090 24GB · `10de:2204` / `10b0:2204` | Gigabyte Z790 GAMING X AX / F15 / i7-13700K | LOCAL-220410de-220410b0-e4482647 USB. USB 1회 및 Windows 복구. 완전 종료 캡처는 1개뿐임. |
| [#133](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/133) | EVGA RTX 3090 Ti FTW3 Ultra 24GB · `10de:2203` / `3842:4985` | MSI PRO Z690-P DDR4 / A.20 / i7-12700K | LOCAL-220310de-49853842-79d1fad6 USB. 수동/USB 우선 2회 및 Windows 복구. 사용자 ctypes 벤치마크. |
| [#136](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/136) | MSI RTX 3090 SUPRIM 24GB · `10de:2204` / `1462:3882` | MSI MAG B650 TOMAHAWK WIFI / 1.A0 / Ryzen 7 7800X3D | LOCAL-220410de-38821462-a1201d9a USB. 완전 종료 1회, 두 화면 복구. 소스 변경 없음. |

## 새 LOCAL 범위 및 과거 포크 후보

| 근거 | GPU / PCI / 서브시스템 | 보드 / BIOS / CPU | 보고 경로 및 결과 |
|---|---|---|---|
| [#50](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/50) | RTX 3060 GA106 12GB · `10de:2504` / `10de:1536` | Gigabyte Z690 UD DDR4 / F27 / i5-12400F | historical z690ud-3060 fork; current LOCAL candidate. 기여자 고정 포트로 완전 종료 USB 2회. 현재 LOCAL 재생성 후보. |
| [#73](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/73) | Gigabyte RTX 3080 12GB · `10de:220a` / `1458:40a2` | ASUS TUF GAMING B550-PLUS / 3611 / Ryzen 9 5900X | LOCAL tuple added by this PR. USB 1회, Windows/HDMI 복구. DP 모니터 재연결 필요. |

## 성공한 비공개/연구 빌드: 통합은 별도

| 근거 | GPU / PCI / 서브시스템 | 보드 / BIOS / CPU | 보고 경로 및 결과 |
|---|---|---|---|
| [#7](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/7) | ASUS RTX 3080 Ti 12GB · `10de:2208` / `1043:8803` | MSI MPG X570 GAMING PLUS / A.N0 / Ryzen 9 3900X | private e1adcc5; PR139. 비공개 USB 실행 성공. 디코드 사전 단계와 PR139 통합은 별도. |
| [#28](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/28) | Maxsun RTX 3080 LHR 10GB · `10de:2216` / `7377:2000` | Maxsun MS-Terminator B760M D4 / not stated / i5-12490F | private b760m-3080lhr internal. 추가 독점 소유자/EDID 처리의 비공개 포트 성공. |
| [#32](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/32) | EVGA RTX 3090 24GB · `10de:2204` / `3842:3982` | ASUS ROG MAXIMUS Z790 DARK HERO / 2202 / not stated; UHD770 display | experimental separate-display UEFI. iGPU 화면의 실험 포트 1회 및 측정. 반복 부팅 미검증. |
| [#43](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/43) | RTX 3060 GA106 12GB · `10de:2504` / `10de:1536` | MSI MAG B550M MORTAR MS-7C91 / not stated / Ryzen 5 5600X | private GA106 manual/automatic SD route. GA106 비공개 SD 포트. 트랜잭션/Windows 보고, diff 없음. |
| [#86](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/86) | RTX 3060 Ti 8GB GA104-200 · `10de:2414` / `10de:2414` | Gigabyte Z590 VISION G / F8 / i7-11700KF | LOCAL-241410de-241410de-2036183f internal. b73000a1 / 2414 포트. Windows 복구 보고, 현재 GA104 조합과 다름. |
| [#90](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/90) | Palit RTX 3090 24GB · `10de:2204` / `1569:f278` | Gigabyte B550M AORUS PRO-P / F15a / Ryzen 9 5900X | private LOCAL; SS1 preserved. Windows 복구 및 LLM 측정. 비공개 게이트 변경 검토 필요. |
| [#93](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/93) | Dell OEM RTX 3090 24GB · `10de:2204` / `1028:3880` | MSI MAG B760M MORTAR WIFI II / 1.90 / i5-14600K | private separate Intel-display USB. Intel iGPU 화면 경로의 비공개 포트. 일반 headless와 다름. |
| [#102](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/102) | Gigabyte RTX 3090 24GB · `10de:2204` / `1458:403a` | MSI MPG X570 GAMING PRO CARBON WIFI / 1.Q2 / Ryzen 9 3950X | private decode pre-step + LOCAL USB. 사전 디코드 교정 후 트랜잭션/Windows. 현재 기본 경로와 구분. |
| [#106](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/106) | Dell RTX 3090 24GB · `10de:2204` / `1028:3880` | Gigabyte X570 AORUS ULTRA / F30 / Ryzen 5 5600X | private parent-bridge BME wrapper. 부모 브리지 BME 포트로 Windows 복구. Drama3 유의미한 향상 없음. |
| [#121](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/121) | Reported MSI RTX 3090 24GB · `10de:2204` / `1458:4043` | MSI MAG X570S TORPEDO MAX / A.B3 / Ryzen 9 5950X | contributor decode pre-step + LOCAL USB. 사전 교정 후 1회 성공. 보고한 제조사/서브시스템 불일치 보존. |
| [#129](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/129) | Dell OEM RTX 3090 24GB · `10de:2204` / `1028:3880` | ASRock X570 Taichi / not stated / Ryzen 7 5800X3D | external decode pre-step + LOCAL USB. 디코드 사전 교정 후 성공. VBIOS 재플래시 공개, GEMM 정확성 미검사. |
| [#131](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/131) | ZOTAC RTX 3090 24GB · `10de:2204` / `19da:1613` | ASUS ROG CROSSHAIR VIII HERO WI-FI / 5601 / Ryzen 9 5950X | private x570-3090 one-shot USB. Windows/드라이버/화면 복구 보고. 전체 로그와 소스는 비공개. |
| [#102](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/102#issuecomment-6504092640) | RTX 3090 24GB (reported subsystem 1b4c:1454) · `10de:2204` / `1b4c:1454` | MSI MPG X570 GAMING PLUS / not stated / Ryzen 9 5900X | private decode pre-step + generated LOCAL USB; PR138. 별도 X570 GAMING PLUS / RTX3090 댓글 구성. 사전 교정 후 1회. |

## 외부 구현 또는 불완전 성공 보고

| 근거 | GPU / PCI / 서브시스템 | 보드 / BIOS / CPU | 보고 경로 및 결과 |
|---|---|---|---|
| [#29](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/29) | MSI RTX 3090 24GB · `10de:2204` / `1462:3884` | Gigabyte model not stated / not stated / not stated | entryLatchedSs1Full internal. 트랜잭션/Windows 보고. 보드 모델·CPU·소스 불완전. |
| [#36](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/36) | RTX 3050 Laptop 4GB GA107M · `10de:25a2` / `1043:13fc` | ASUS TUF F15 FX506HCB / 316 / i5-11400H | independent yuksew-derived manual USB. 별도 프로젝트의 노트북 수동 실행. 이 저장소 지원 아님. |
| [#99](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/99) | Palit RTX 3090 24GB · `10de:2204` / `1569:2204` | HP Z640 / M60 / not stated | modified legacy UEFI. 트랜잭션/측정 주장. 부분 레거시 diff, 인계 근거 불충분. |
| [#140](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/140) | ASUS TUF RTX 3080 Ti 12GB · `10de:2208` / `1043:8803` | withheld / withheld / withheld | private manual USB; Secure Boot policy modified. 소유자 성공 보고. 보드 정보 비공개, Secure Boot 정책 수정됨. |
| [#2](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/2) | Inno3D RTX 4070 Ti · `10de:2782` / `10de:16e1` | ASRock Z690M-ITX/ax / 20.01 / not stated | Zihao-Wu06 Ada port. 별도 Ada 포트의 성공. 이 저장소의 지원 아님. |
| [#13](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/13) | PNY RTX 3080 LHR (comment, not original Gigabyte reporter) · `10de:2216` / `not stated` | not stated / not stated / not stated | unidentified private Windows port. 다른 댓글 작성자의 PNY 카드 주장. 원래 X570 구성과 구분. |

## 중요한 범위와 다음 단계

- #73은 `220a / b72000a1 / 3000 MiB`와 기존 `60010` 코드만 추가합니다. 10 GB/LHR 3080, 3080 Ti, 다른 칩/드라이버를 동시에 허용하지 않습니다. [통합 근거](../evidence/b550-3080-12gb-local/README.ko.md).
- #125는 트랜잭션 성공 후 PC 전원이 꺼져 Windows 복구에 실패했습니다. 동작 확인 목록에 포함하지 않습니다. #35의 종료 상태도 attempted=0인 읽기 전용 검사였으므로 성공 목록이 아닙니다.
- #13의 PNY 댓글은 원래 Gigabyte/X570 작성자의 성공이 아닙니다. #102의 댓글 구성도 원래 보고와 다른 GPU/CPU/보드이므로 따로 기록했습니다.
- #86의 실제 보고 ID는 `2414`, 칩은 `b73000a1`입니다. 마케팅 이름으로 이를 `24c9 / b74000a1`로 바꾸지 않습니다. #121의 MSI 이름과 Gigabyte 서브시스템 `1458:4043`도 보고 그대로 보존했습니다.
- #73의 DP 수동 복구, #90의 작은 prefill 향상/변화 없는 decode, #106의 Drama3 향상 미입증을 보존합니다. GEMM 성능은 게임/LLM 또는 전체 애플리케이션 속도 보장이 아닙니다.
- 사전 디코드/부모 브리지 변경, 별도 Ada/노트북 코드 및 Secure Boot 정책 변경은 기본 경로에 이식하지 않았습니다.
- 최신 소스에서 새 LOCAL 헤더를 생성하면 기존 테스트 증거는 재사용할 수 없습니다. [현재 절차 — 영어](NEW-MACHINE.en.md)를 따르며 이 문서는 새 부팅 요청이 아닙니다.
- READ 권한으로 사용자 정의 `resolved` 라벨은 만들 수 없습니다. 대신 [PR #142](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/142)에 #12, #73, #81, #84, #87, #91, #105, #113, #124, #133, #136의 `Resolves` 키워드를 넣었습니다. 기본 `main`으로 병합될 때만 자동 종료되며 아직 이슈를 닫지 않았습니다. #4는 이미 종료 상태입니다. 비공개 정책/불완전 근거/인계 실패 보고는 계속 보류합니다.
