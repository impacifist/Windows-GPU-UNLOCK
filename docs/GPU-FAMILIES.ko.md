# GPU 계열과 장비 구성의 분리

[English](GPU-FAMILIES.en.md) | **한국어** | [호환성](COMPATIBILITY.ko.md)

기록된 GA102·GA104·GA106 구성은 기존 트랜잭션과 메모리 배치 구현을 공유합니다.
장비 프로필은 여기에 정확한 카드 ID·메모리 크기·펌웨어·콘솔 정보를 조합합니다.
메인보드마다 별도의 언락 구현을 갖는 구조는 아닙니다.

| 구분 | 소스 | 역할 |
|---|---|---|
| GPU 계열 | `internal-boot/unlock/gpu-family.h` | 정확한 칩 revision과 검증된 PCI ID·VRAM 조합 |
| 공통 트랜잭션·배치 | `internal-boot/unlock/vendor/unlock_610.c`, `layout_610.c` | 기존 보호된 트랜잭션과 메모리 배치 계산 |
| 카드·플랫폼 고정값 | `internal-boot/unlock/machine-profile.h` | PCI/subsystem·PROM·정확한 가용 VRAM·BAR·root port·화면 경로·토폴로지 |
| 고정 펌웨어 템플릿 | `internal-boot/unlock/firmware-profile.h` | GPU 계열과 독립적으로 선택하는 정확한 콘솔·EDID·nvgop fingerprint 묶음 |
| 로컬 생성 자료 | Git에서 제외한 `profile-local.h` | 해당 장비의 캡처 값과 이에 묶인 호스트 검증 증거 |

## 현재 계열 정의

| 계열 | 정확한 칩 revision | 현재 허용하는 카드·메모리 조합 |
|---|---|---|
| GA102 | `0xb72000a1` | RTX 3090·3090 Ti, `0x6000` 또는 `0x5ff4` MiB. 장비별 값은 계속 정확히 고정 |
| GA104 | `0xb74000a1` | `10de:2482` / `10de:2484` / `10de:24c9` + `0x2000` MiB, `10de:2487` + `0x3000` MiB |
| GA106 | `0xb76000a1` | `10de:2504` + `0x3000` MiB |

현재 소스의 허용 범위이며 모든 조합의 실기기 검증이나 계열 전체 지원을 뜻하지 않습니다.
개별 실기기 근거는 호환성 기록에서 확인해야 합니다.
제품명만으로 칩을 판별하지 않습니다. 기록된 RTX 3060에도 GA104와 GA106이 모두
있습니다. revision 비트를 무시하지 않으며 미지원 계열·칩 불일치·검증되지 않은
PCI ID/VRAM 조합은 공통 정의에서 컴파일 오류로 거부합니다.

## 펌웨어는 별도로 선택

같은 GA102에서도 콘솔 펌웨어와 nvgop 빌드가 다릅니다. 따라서 계열 선택만으로
펌웨어 fingerprint를 정하지 않습니다. 고정 템플릿은 기록된 장비 사이에서 실제
동일한 값만 공유합니다. B660M/3070 Ti·B660M/3090 Ti·B660-PLUS/3060은 콘솔
fingerprint를 공유하지만 각 GPU·PROM·화면 경로와 child 수 정책은 별도로 유지합니다.
고정 프로필 다섯 개가 같은 nvgop `0x60010` 묶음을 공유합니다.

LOCAL은 nvgop `0x60009`를 포함해 직접 캡처한 드라이버 해시를 유지하며 고정
템플릿을 상속하지 않습니다. 기존 생성 헤더의 칩·메모리 literal도 유지하고,
정확한 칩 값에서 계열 정보를 유도합니다. [로컬 생성기](NEW-MACHINE.en.md)는 기존
GA102 조합에 더해 GA104 `248710de`·GA106 `250410de`의 정확한 12GiB 조합을 처리합니다.
이 12GiB 조합은 검증된 nvgop `60010` 코드가 필요합니다. RTX 3070 `248410de` / 8GiB는
[이슈 #113](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/113)을 근거로 `60009`를 사용합니다.
RTX 3060 Ti `24c910de` / 8GiB는 [이슈 #105](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/105)의
검토한 패치를 근거로 전용 `60015` 코드 해시를 요구합니다. 기여자는 재시작 후 거래 1회를
보고했으며, 현재 main의 실기기 검증은 남아 있습니다.
GA104는 PCI ID별 드라이버 고정값이 필수이며 칩이 같다는 이유로 다른 카드의 코드를 허용하지 않습니다.
인식 가능한 aggregate·non-aggregate 구성 모두 입력 정책과 구성별 전체 드라이버 해제 재현을
포함한 host 작업 22개 통과가 필수입니다. PCI 신원 재확인 음성 시험도 포함합니다. 생성된 프로필은
실험적 후보이며 새로운 실기기 성공 보고를 뜻하지 않습니다.

Identity-only H610M 진단 프로필은 계속 칩·VRAM·PROM 고정값이 없고 쓰기·해제
이미지로 컴파일할 수 없습니다. 외부 옵션으로 계열이나 펌웨어 템플릿을 덮어쓸 수
없습니다. 기존 소유권·cold 상태·PROM·드라이버 코드·DMA/FLR·정리 검사는 유지됩니다.

## 기존 사용자에게 달라지는 점

기존 프로필 이름·빌드 명령·지원 범위는 같습니다. 장비 구성 여섯 개는 정확한
검증 기록과 호환성 자료로 남습니다. 공통 특성과 fingerprint 묶음을 한 곳에 정의해,
다른 카드·플랫폼 조합을 조사할 때 반복되는 코드를 줄였습니다. 계열이 일치하는
것만으로 새 장비가 검증되지는 않습니다.

새 헤더도 source manifest와 LOCAL 입력 해시에 포함됩니다. 소스가 달라지면 기존
LOCAL 시험 증거는 오래된 입력으로 판정되므로, 이후 staging 전에 필수 호스트 시험과
빌드를 다시 해야 합니다. 기존 provenance gate가 적용되는 것이며 실행 승인과는 별개입니다.
