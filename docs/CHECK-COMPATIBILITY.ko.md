# 언락 실행 없이 기존 진단 자료 확인하기

[English](CHECK-COMPATIBILITY.en.md) | **한국어** | [README](../README.ko.md)

파일 지문과 관찰한 Windows 결과를 하나로 묶으려면 [증거 보고 도구](EVIDENCE-REPORT.ko.md)를 사용하세요. 같은 오프라인 검사를 재사용하며 실험을 실행하지 않습니다.

이 오프라인 검사기는 **이미 수집한 진단 파일**이 자동 로컬 프로필 생성 조건에
맞는지와 막힌 이유를 설명합니다. 현재 PC를 스캔하거나 자료를 수집하지 않으며,
펌웨어 실행·프로필 생성·언락 빌드·USB 준비·부팅 설정 변경도 하지 않습니다.
이 검사에는 Python 3.10+만 필요하고 컴파일러나 펌웨어 파서 실행 파일은 필요하지 않습니다.

저장소 루트에서 `DIAGnn.TXT`, 마지막 캡처에 대응하는 `IMGnn.BIN`, `PROM.BIN`이
있는 폴더를 지정하세요.

```powershell
python -I research/profile/check-compatibility.py "D:\RTX3090-UNLOCK" --lang ko
```

자료가 없다면 먼저 [캡처 수집 절차](NEW-MACHINE.en.md#steps)를 확인하세요.
자료 수집은 별도의 펌웨어 부팅 작업입니다. 위 명령으로 실행되지 않습니다.
다른 사람의 캡처를 검사한 결과는 자신의 PC에 대한 진단이 아닙니다.

| 결과 | 의미 | 다음 단계 |
|---|---|---|
| `candidate` | 캡처 관측값이 일치하며 공통 구조 검사를 통과했고, 마지막 캡처의 드라이버 코드와 PROM 크기·해시도 일치 | [프로필 생성과 호스트 시험](NEW-MACHINE.en.md#steps) 진행 |
| `needs_data` | 캡처·관측값·필수 펌웨어 파일이 부족 | 안내에 따라 누락 자료 확보 |
| `outside_scope` | GPU·콘솔 구조·펌웨어 드라이버가 현재 자동 생성 범위 밖 | [정확한 지원 구성](COMPATIBILITY.ko.md) 확인. 프로젝트 전체의 미지원 판정은 아님 |
| `invalid_data` | 자료 형식·일치 여부·캡처 상태에 문제가 있거나 입력 제한 초과 | 표시된 사유와 원본 파일을 로컬에서 검토하고 필요시 다시 수집 |
| `error` | 설치된 검사기나 허용 범위 파일을 읽지 못함 | 소스 설치 상태 확인 |

종료 코드는 후보일 때 `0`, 그 외 진단 결과는 `2`, 설치 오류는 `3`입니다.
**종료 코드 0은 언락 실행 승인이 아닙니다.** 검사기는 쓰기 빌드 자격 증거를
읽거나 발급하지 않으며 기존 빌드 검증은 그대로 유지됩니다.
고정 프로필의 지원 범위와 자동 로컬 프로필의 생성 범위는 서로 다릅니다.

## 보고서와 언어

```powershell
# 영어 설명
python -I research/profile/check-compatibility.py "D:\RTX3090-UNLOCK"

# JSON 보고서를 표준 출력으로만 표시
python -I research/profile/check-compatibility.py "D:\RTX3090-UNLOCK" --json --lang ko
```

검사기는 Python 바이트코드 캐시를 포함해 파일을 쓰지 않습니다. 출력은 정해진 사유
코드·안내문과 표준 `DIAGnn.TXT` 파일명, 형식이 제한된 관측값으로 구성합니다.
Format 2에는 PCI 모델·subsystem·class ID, PCI segment/bus/device/function,
디스플레이·GOP 수, EDID open 기록 수, 관측된 칩·메모리 정보와 알려진
콘솔·EDID·nvgop binding의 버전·이미지 크기가 포함됩니다. 이는 차이 분석용 자료이며
지원 증거가 아닙니다. 미관측 값은 `0` 대신 `null`, 불완전한 그래프는 빈 장치 목록
대신 `available: false`로 표시합니다.

EDID `by_driver`는 정확히 `EFI_OPEN_PROTOCOL_BY_DRIVER`인 기록 수, `active`는
수동 조회 속성 `1`, `2`, `4` 이외의 기록 수입니다. 고유 드라이버 수와는 다릅니다.
선언된 binding 자료가 없거나 불완전하면 버전 정보도 미확인으로 남깁니다.
캡처당 장치·알려진 binding은 각각 최대 16개까지 표시하고 생략 여부를 표시합니다.
허용 범위 해시는 공개 검사 설정을 식별하며 장비 고유 해시가 아닙니다. 입력 경로, 원본 기록,
일련번호, 메모리 주소, PROM 해시와 펌웨어 내용은 포함하지 않습니다.
자료나 보고서를 업로드하지 않습니다. 공유할 내용은 직접 확인하고, 원본 펌웨어
덤프는 이슈에 첨부하지 마세요.

검토한 JSON을 [호환성 이슈 양식](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/new?template=compatibility.yml)의
선택 항목 **Offline compatibility report**에 붙여 넣으세요. 보드·BIOS, 실제 화면 연결,
소스·빌드 정보, VBIOS 변경 이력은 다른 항목에 적습니다. 이 보고서로는 자동 판별할 수 없습니다.
영문·한글 어느 쪽으로도 제보할 수 있습니다. 캡처가 없어도 기존
[Windows 환경 수집기](CONTRIBUTING.ko.md)나 수동 모델 정보로 제보할 수 있습니다.

## 보고서 두 개 비교

파일로 보관하려면 새 이름을 선택해 직접 저장할 수 있습니다.

```powershell
python -I research/profile/check-compatibility.py "D:\RTX3090-UNLOCK" --json | Out-File -LiteralPath .\compatibility-report-probe.json -Encoding utf8 -NoClobber
python -I research/profile/compare-compatibility.py .\compatibility-report-reference.json .\compatibility-report-probe.json --lang ko
```

저장은 셸이 수행하며 도구 자체는 파일을 쓰지 않습니다. `compatibility-report*.json`은
Git에서 제외합니다. 공유 전 내용을 확인하세요. 비교기는 format-2 검사 보고서 두 개만
읽고 형식·타입을 검증합니다. UTF-8 또는 BOM이 있는 UTF-16, 파일당 최대 1 MiB를 지원합니다.
다르거나 미확인인 항목에 `same`(같음)·`different`(다름)·`unavailable`(미확인)을 구분하고,
`--json`을 붙이면 같은 항목도 포함합니다. 중복 키·비정상 숫자·잘못된 필드는 거부하며,
검사기 버전이나 허용 범위 해시가 다르면 비교를 보류합니다. 이전 format-1 보고서는
같은 캡처로 다시 생성하면 됩니다.

캡처 번호나 반복 관측값은 장비 식별 정보가 아닙니다. 비교기는 각 보고서의 서로 다른
관측값을 비교할 뿐 부팅 이력을 검증하지 않습니다. 서명·출처를 검증하지 않으며 보고서의
자유 입력 문구도 재출력하지 않습니다. 차이는 공통 개선의 조사 단서이고, 유사하다는
이유만으로 다른 프로필을 적용해도 된다는 뜻은 아닙니다.
종료 코드 `0`은 차이 여부와 무관하게 비교 완료, `2`는 자료 오류나 비교 보류입니다.
Windows 환경 수집기 보고서는 별도 형식이므로 이 비교기에 입력할 수 없습니다.

## 아직 검증하지 않는 것

- 파일 내용만으로 출처·서로 다른 완전 종료 부팅·현재 하드웨어 상태를 증명할 수
  없습니다. 같은 파일 여러 개가 별도 부팅의 증거가 되지는 않습니다.
- FWSEC 파싱은 실행하지 않습니다. 프로필 생성 단계에는 신뢰할 수 있는 호스트 파서가 필요합니다.
- 필수 호스트 재현 시험, 실제 드라이버 해제, Windows 복구, 실기기 안정성은
  시험하지 않습니다. 후보 판정으로 실기기 검증 장비가 추가되지는 않습니다.
- 생성기와 동일하게 마지막 캡처의 드라이버 덤프를 검사합니다. 이전 캡처는 프로필
  상수·PCI 구성·디스플레이 장치 정보가 일치해야 합니다. 실행 중 읽은 파일은 크기가
  제한된 메모리 사본으로 고정해 재사용합니다.
- 입력 제한은 `DIAGnn.TXT` 32개, 진단 파일당 8 MiB, 필요한 드라이버 덤프당 4 MiB,
  일치하는 PROM은 정확히 1 MiB, 전체 읽기 64 MiB입니다. 일반 파일만 읽습니다.
  정해진 캡처 파일명만 읽으며 캡처 폴더의 실행 파일은 실행하지 않습니다.

이후 단계는 [새 장비 가이드](NEW-MACHINE.en.md)를 따르세요. 통과시키려고 프로필이나
펌웨어 해시를 임의로 바꾸지 마세요.
