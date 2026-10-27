# 다른 환경의 호환성 제보

[English](CONTRIBUTING.en.md) | **한국어**

다른 메인보드·CPU·GPU의 환경 정보는 공통 호환성 도구 개선과 재사용 가능한 변경을 찾는 데 도움이 됩니다. **제보를 위해 EFI 부팅·BIOS 설정 변경·언락 실행을 할 필요가 없습니다.** EFI 지원은 정확히 일치하는 [공개된 장비 프로필](COMPATIBILITY.ko.md)에 한정되며 GPU 모델명이 같다는 이유만으로 호환되지는 않습니다.

## 변경 제안 전

[기여 수용·작업 가이드라인](CONTRIBUTION-POLICY.ko.md)을 먼저 읽어주세요.
공통 수정·진단·로컬 프로필 도구를 우선하며, 성공 사례나 다른 보드의 상수만으로
main에 새 프로필을 추가하지 않습니다. 기존 참조 프로필은 유지하고 새 고정 프로필은
이유를 명시한 유지관리자의 예외 결정으로 다룹니다. 로컬 프로필 절차도 검토된 범위에
제한되므로 만능 지원으로 안내하지 않습니다.

## Windows에서 환경 수집

Python 3.9+가 설치된 일반 Windows 터미널에서 저장소 루트 기준으로 실행합니다.

```powershell
python .\tools\collect_compatibility.py
```

Windows PowerShell의 로컬 CIM 조회로 보드·CPU 모델, Windows·BIOS 버전, 화면 어댑터 모델·PCI 모델 ID·드라이버 버전을 출력합니다. 관리자 권한 요청, 다운로드, 설정 변경, GPU MMIO 접근, PROM·펌웨어 읽기, 언락 실행, 파일 생성, 자동 전송은 하지 않습니다.

파일로 보관하려면 새 이름을 정해 직접 stdout을 저장합니다.

```powershell
python .\tools\collect_compatibility.py > .\compatibility-report.json
```

셸 리다이렉션은 기존 파일을 덮어쓸 수 있습니다. `compatibility-report*.json`은 Git에서 제외합니다. **공유 전에 내용을 직접 확인하세요.** 수집기는 컴퓨터명·사용자명·serial·UUID 필드와 PnP 장치 고유 접미사를 제외하지만 모델·버전 문자열도 자신의 환경 정보입니다.

종료 코드 `0`은 선택한 Windows 정보 수집 완료, `2`는 일부 항목 누락, `1`은 수집 실패입니다. 권한 상승이나 보안 설정 해제 없이 누락 정보를 수동으로 적어도 됩니다. 수집 성공은 언락 지원을 뜻하지 않습니다. `assessment.status`는 항상 `not-assessed`, `can_unlock`은 항상 `false`입니다.

Secure Boot 상태, 실제 모니터 연결, PCI BDF/BAR, PROM, cold 상태와 사전 부팅 정리 가능 여부는 수집하지 않습니다. 가상·비 PCI 화면 어댑터는 `pci: null`로 표시합니다. WMI 드라이버 버전은 NVIDIA 배포 버전과 표기 방식이 다를 수 있습니다.

## 직접 제보하기

[호환성 이슈 양식](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/new?template=compatibility.yml)을 이용합니다. 양식이 없으면 일반 이슈도 가능합니다.

- 검토한 보고서 또는 보드·CPU·GPU·Windows·드라이버 정보를 수동으로 적습니다.
- 물리 카드의 제조사·모델과 다른 카드용 VBIOS를 기록한 이력을, 소프트웨어에 표시되는 PCI/subsystem ID·VBIOS 버전과 구분합니다. 표시되는 ID만으로 실제 PCB를 확정할 수 없습니다. 모르는 항목은 미확인으로 남기며, 제보를 위해 플래시하거나 펌웨어를 덤프하지 않습니다.
- 모니터가 어디에 연결되어 있는지, iGPU 또는 별도 화면용 GPU가 있는지 적습니다.
- 미실행·진단만 수집·기존 실험 기록 중 어느 상태인지 구분합니다. 미실행 제보도 유용합니다.
- 이미 승인받아 수행한 실험이 있다면 EFI 해시·프로필, 비식별화한 최종 단계 로그, Windows 인계와 동일 조건 측정 결과를 적습니다. 양식을 채우려고 새 언락 실험을 하지 않습니다.
- 기존 capture마다 소스 commit·로컬 diff·컴파일러 버전·화면 연결과 BIOS 설정을 연결합니다. 진단 수집, transaction/정리, Windows 화면 복구, 성능 근거를 구분합니다. 기존 측정은 입력·누산 정밀도, 행렬 크기, 라이브러리·드라이버 버전과 반복 횟수를 기록하고 cold boot·재시작·절전·최대절전을 따로 표시합니다.

펌웨어·VBIOS/PROM·BitLocker 복구 키·serial·UUID·원시 시스템 보고서를 첨부하지 않습니다. 기존 로그의 개인 경로도 확인합니다. 이 도구는 자동 업로드하지 않습니다.

실제 지원 추가에는 카드·펌웨어·메모리 배치 검토, 사전 부팅 소유권과 실패 정리, Windows 인계 및 하드웨어 측정 근거가 필요합니다. [지원 확대 설계](PORTABILITY.en.md)를 참고하세요. 원본 API 근거는 [영문 안내](CONTRIBUTING.en.md)의 Microsoft 문서에 있습니다.

[장비 검증 기록](VALIDATION.ko.md)으로 기존 관측·transaction·Windows 복구·계산·수명주기 근거를
구분할 수 있습니다. 이 문서를 채우기 위한 새 실행은 필요하지 않습니다.
