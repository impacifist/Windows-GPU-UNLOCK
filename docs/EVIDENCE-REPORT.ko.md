# 기존 캡처를 하나의 보고서로 정리

[English](EVIDENCE-REPORT.en.md) · **한국어** · [오프라인 검사기](CHECK-COMPATIBILITY.ko.md)

기존 오프라인 검사, 파일 지문과 관찰한 부팅 결과를 하나의 JSON으로 묶습니다. **새 부팅이나 언락은 필요하지 않습니다.** 파일을 읽고 터미널에 출력할 뿐 펌웨어 실행, 네이티브 파서 실행, 프로필 생성, 스테이징이나 업로드는 하지 않습니다.

## 명령 하나 실행

압축을 푼 프로젝트 폴더에서 Python 3.10 이상으로 실행합니다. 예제 경로를 기존 캡처 폴더로 바꾸세요.

```powershell
python -I .\research\profile\report-evidence.py "D:\RTX3090-UNLOCK" --windows-outcome direct-usable --boot-kind full-shutdown
```

기대 결과가 아니라 **이미 실행한 결과**에 맞는 값을 선택합니다. 캡처별 결과가 다르면 보고서를 분리하고 이슈에서 각 캡처의 이력을 설명합니다. 선택한 결과는 입력 집합 전체에 대한 운영자 진술이며 도구는 개별 부팅에 결과를 배정할 수 없습니다.

| `--windows-outcome` | 의미 |
| :--- | :--- |
| `unknown` (기본값) | 결과가 기록되지 않았거나 기억나지 않음 |
| `not-run` | EFI를 실행하지 않음 |
| `direct-usable` | Windows가 직접 시작되고 화면을 사용할 수 있었음 |
| `display-intervention` | Windows가 직접 시작됐으나 화면 복구 조작이 필요했음 |
| `manual-recovery` | 수동 전원 종료/재시작 또는 USB 제거가 필요했음 |
| `failed` | Windows가 시작되지 않았거나 펌웨어가 멈춤 |

`--boot-kind`는 `unknown`(기본값), `full-shutdown`, `restart`, `sleep`, `hibernate`를 허용합니다. `not-run`은 `unknown`을 사용해야 합니다. 불확실하면 옵션을 생략해도 됩니다. 로그나 검사 성공으로 이 항목을 자동 입력하지 않습니다.

## 결과 읽기

| 항목 | 의미 |
| :--- | :--- |
| `compatibility` | 하드웨어 관찰 및 거부 이유가 포함된 기존 형식 2 오프라인 결과 |
| `files_read` | 이번 검사에서 실제 사용한 불변 바이트의 표준 파일명, 크기와 SHA-256 |
| `installed_tool_sources` | 보고/검사/파서 코드와 설치된 허용 범위 파일 지문; 소스 트리 증명 아님 |
| `source_revision` | 운영자가 선택적으로 제공한 전체 리비전; 항상 미검증으로 표시 |
| `operator` | 선택한 결과와 부팅 종류; 항상 운영자 보고 및 미검증으로 표시 |
| `build` | 선택적으로 제공한 일반 프로브 매니페스트와 로컬 EFI 바이트 비교 |
| `evidence_gaps` | 누락된 진술 또는 잘못된 빌드 메타데이터; 후보여도 누락이 있을 수 있음 |

`status`는 검사기 상태를 유지하지만 선택적 빌드 메타데이터가 잘못되면 `invalid_data`가 됩니다. 종료 `0`은 오프라인 후보, `2`는 누락/거부/잘못된 데이터, `3`은 잘못된 옵션 또는 설치 오류입니다. **어떤 결과도 완전한 증거 패키지, 하드웨어 지원이나 복구를 인증하지 않습니다.** 정상 Windows 결과를 보고해도 `hardware_tested`, `write_eligible`, `unlock_proven`은 false입니다.

파일 목록은 전체 폴더 매니페스트가 아닙니다. 이전 드라이버 덤프나 무관한 파일은 읽지 않을 수 있습니다. 검사기가 사용한 제한 내 읽기 성공분만 기록하며 거부된 경우의 부분 증거도 포함합니다. 누락/초과/읽기 실패 파일은 목록에 없을 수 있습니다. 해시는 바이트를 식별할 뿐 원래 머신이나 서로 다른 완전 종료 부팅을 입증하지 않습니다. 검사기는 알려진 불일치를 거부하지만 모든 혼합 자료를 식별하지는 못합니다.

## 선택 사항 · 기존 빌드 파일 비교

일반 진단 빌드의 `manifest.json`과 해당 `BOOTX64.EFI`를 이미 가지고 있다면 둘 다 지정합니다. 이 항목을 채우기 위해 새로 빌드하거나 부팅하지 마세요.

```powershell
python -I .\research\profile\report-evidence.py "D:\RTX3090-UNLOCK" --build-manifest ".\internal-boot\boot\build\probe-generic\manifest.json" --efi ".\internal-boot\boot\build\probe-generic\BOOTX64.EFI" --source-revision YOUR_FULL_COMMIT_HASH
```

`YOUR_FULL_COMMIT_HASH`를 해당 빌드의 40자 또는 64자 소문자 16진수 리비전으로 바꾸거나, 미상이면 옵션을 생략합니다. 이는 Git 검증이 아닌 진술입니다. 로컬 변경은 별도로 설명해야 합니다.

보고 도구는 일반 진단 모드, false 기능 플래그, 제한된 소스 행 구조와 EFI 해시를 검사합니다. `matching-local-efi`는 제공된 바이트가 제공된 매니페스트와 일치한다는 뜻뿐입니다. 소스 파일 내용, PE 유효성, 매니페스트 진위, 캡처/EFI 출처 관계, 리비전이나 실제 진단 동작은 검증하지 않습니다. 기존 빌드/스테이징 검증을 대체하지 않습니다. 다른 매니페스트 형식은 거부되며 언락 매니페스트는 일반 진단 매니페스트가 아닙니다.

## 검토 후 공유

검토한 JSON을 텍스트 첨부나 JSON 코드 블록으로 기존 이슈에 추가합니다. 이는 **증거 보고서**이며 `compare-compatibility.py`가 받는 형식 2 입력이 아닙니다. 해당 비교 흐름에는 원래 검사기를 사용합니다.

> [!IMPORTANT]
> 원시 로그, 펌웨어 내용, 입력 경로와 자유 형식 빌드 필드는 출력하지 않습니다. 파일 해시와 하드웨어 모델/토폴로지 식별 정보는 포함되므로 보고서들을 연결할 수 있습니다. 공개 전에 검토하세요. PROM/IMG/EFI 파일, 비공개 매니페스트, 일련번호, UUID, 키나 개인 경로는 첨부하지 마세요. 자동 업로드는 없습니다.

진단 출력 형식, 기존 검사기 CLI/형식, 프로필 허용 조건과 적격성 정책은 변경하지 않습니다. 네이티브 FWSEC 파서나 하드웨어 시험을 실행하지 않습니다. 보고서 공유는 새로운 하드웨어 실험을 허가하지 않습니다.
