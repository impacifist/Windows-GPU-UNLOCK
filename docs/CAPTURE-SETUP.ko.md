# 캡처 전용 설정으로 시작하기

[English](CAPTURE-SETUP.en.md) | **한국어** | [전체 로컬 프로필 절차](NEW-MACHINE.en.md)

새 기여자는 프로필 생성이나 언락 전에 진단 자료부터 수집합니다. 이 절차에는
펌웨어 자산 5개나 MSVC가 필요하지 않습니다. Windows PowerShell, Python 3.10+,
LLVM `clang`/`lld-link`, 기존 FAT32 USB가 필요합니다. 스크립트는 USB 대상을
검사하지만 포맷이나 파티션 변경을 하지 않습니다.

## 처음이라면 여기서 시작하세요

1. GitHub의 **Code → Download ZIP**에서 저장소 ZIP을 내려받고 Windows의
   **모두 압축 풀기**를 사용합니다. ZIP 안에서 직접 실행하지 않습니다. Git은
   필요하지 않습니다.
2. 압축을 푼 폴더의 **Start-Capture.cmd**를 두 번 클릭합니다. 결과를 읽을 수
   있도록 텍스트 창이 열려 있습니다. 언락 버튼이 아니라 캡처 설정입니다.
3. 도구 검사 안내를 따릅니다. 누락된 도구가 있으면 공식 Python/LLVM 다운로드
   링크가 표시됩니다. 직접 설치한 뒤 설정 창을 닫고 다시 실행합니다. 표준
   폴더에 설치한 LLVM은 PATH가 갱신되지 않아도 찾습니다.
4. 지원하는 Windows EFI 파티션을 자동으로 찾습니다. USB 이름, 모델, 크기와
   드라이브 문자를 보고 선택합니다. 사용할 수 없는 항목에는 이유가 표시됩니다.
   먼저 USB 파일을 백업합니다. 표시된 대상을 확인한 후에만 **PREPARE**를
   입력합니다. Enter만 누르면 취소합니다. 포맷, 자동 재시작이나 펌웨어 설정
   변경은 하지 않습니다.
5. 부팅 전에 아래의 **지켜보면서 수집하고 복귀 확인**을 읽습니다. Windows로
   돌아올 때마다 같은 실행 파일을 열고 같은 USB를 선택합니다. 캡처 수집에서
   멈추며 언락 단계로 진행할 수 없습니다.

Windows가 내려받은 스크립트를 차단하면 멈추고 의도한 저장소 리비전을 받았는지
확인합니다. 유지관리자에게 도움을 요청하고 시스템 보안이나 실행 정책을 끄지
마세요. Python과 LLVM 설치는 여전히 필요합니다. 의존성을 자동 설치하는 도구가
아닙니다. 지원하지 않거나 모호한 부팅 구성은 파티션 ID를 추측하지 말고 도움을
요청합니다. 현재 텍스트 프롬프트는 영어이며 이 안내는 한국어도 제공합니다.

## 호스트 도구 검사

저장소 루트의 일반 터미널에서 실행합니다.

```powershell
python .\tools\check_prerequisites.py --phase capture
```

종료 코드 0은 검색/버전 검사 통과이며 빌드나 하드웨어 테스트 통과가 아닙니다.
JSON에 실제 실행 파일 경로와 버전이 기록됩니다. LLVM 버전 출력은 도구 식별에
사용하며 새로 인증한 버전 범위를 뜻하지 않습니다. Python이 실행되지 않으면
먼저 Python 3.10+를 설치합니다. 설치 후 도구를 찾지 못하면 터미널을 다시 엽니다.
명시적 경로를 사용하면 오래된 PATH에 의존하지 않습니다.

```powershell
python .\tools\check_prerequisites.py --clang 'C:\Program Files\LLVM\bin\clang.exe' --lld-link 'C:\Program Files\LLVM\bin\lld-link.exe'
```

버전 명령만 실행하며 설치, 영구 환경 변경, 컴파일 빌드나 GPU 접근은 하지
않습니다. `--phase profile`은 `vcvars64.bat`도 검색하며 `--vcvars`로 지정할 수
있습니다. 검색은 MSVC 링크 검증이나 배치 파일 실행이 아닙니다. 호스트 재현
테스트로 넘어가기 전에 [자산 안내](ASSETS.en.md)를 별도로 확인합니다.

## 캡처 프로브를 명시적으로 스테이징

Windows 시스템 EFI 파티션과 USB 드라이브 문자를 확인합니다. 다른 사람의 GUID나
드라이브 문자를 복사하지 마세요.

```powershell
Get-Partition | Where-Object IsSystem | Select-Object DiskNumber,PartitionNumber,Guid,Type
Get-Volume | Select-Object DriveLetter,FileSystemLabel,FileSystem,Size
```

GPT/EFI Windows 환경에서 자신의 값으로 바꿉니다.

```powershell
.\research\profile\new-machine.ps1 -Drive D -WindowsEspGuid 'YOUR-WINDOWS-ESP-GUID' -CaptureOnly
```

필요하면 같은 `-Clang`, `-LldLink` 경로를 전달합니다. 사전 검사 도구는 경로를
저장하지 않습니다. `-Python`은 빌드 검증 스크립트의 Python 실행 파일을 선택합니다.
일반 진단 EFI를 빌드하고 선택한 USB에 기록하며, 기존 백업 경로를 통해 이전
로더와 캡처를 보존합니다. PC를 재부팅하지 않습니다. CaptureOnly는 `-Assets`나
`-StageUnlock`과 함께 사용할 수 없습니다.

## 지켜보면서 수집하고 복귀 확인

[프로브 범위와 부팅 안내](NEW-MACHINE.en.md#steps)를 확인한 후 완전히 종료하고
펌웨어 부팅 메뉴에서 USB를 선택합니다. 프로브는 Windows 인계를 시도합니다.
필요한 캡처 횟수만큼 별도의 완전 종료에서 반복합니다. 진단 읽기도 멈추거나
Windows 복귀에 실패할 수 있습니다. 필요하면 전원을 끄고 USB를 제거한 뒤 정상
Windows Boot Manager를 선택합니다. 복구는 보장되지 않습니다. 이 안내를 완료하려고
펌웨어 보안 설정을 바꾸지 마세요.

같은 CaptureOnly 명령을 다시 실행합니다. 충분한 완료 로그가 있으면 수집 완료를
표시하고 **프로필 생성, 자산 재현, 언락 빌드/스테이징 없이** 종료합니다. 파일
완전성은 호환성 판정이 아닙니다. 기존 디렉터리 이름인 `RTX3090-UNLOCK` 아래의
`DIAGnn.TXT`, `IMGnn.BIN`, `PROM.BIN` 원본을 로컬 분석용으로 보관합니다. 공유 전
보고서를 검토하고 개인정보를 제거하며 펌웨어 덤프를 공개 첨부하지 않습니다.

캡처 이후 단계로 명시적으로 진행할 때만 전체 절차에 따라 `-Assets`와 기존
허용/재현 검사를 사용합니다. 이전 명령도 자산 없이 초기 프로브를 스테이징할 수
있지만 캡처가 준비되면 프로필 생성 전에 멈춥니다. 언락 스테이징에는 여전히
`-StageUnlock`과 자산이 필요합니다.
