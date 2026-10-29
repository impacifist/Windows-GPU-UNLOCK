# 여덟 번째 AD104 부팅: 출력 상태와 화면 재설정 조건

[English](AD104-EIGHTH-BOOT.en.md) | 한국어

성공한 EDID 실험의 읽기 전용 후속 작업이다. 드라이버 연결을 해제하지 않는다.
[텍스트 정리 분석](AD104-TEXT-LIFECYCLE.ko.md)에서 빠진 콘솔 콜백 위치,
텍스트/GOP 모드 상태 및 조건부 GOP SetMode를 제어하는 Boolean을 확인한다.
이 질문에 일곱 번째 변경 EFI를 다시 사용하지 않는다.

정확한 실시간 사전 검사 후 ConOut/StdErr 인터페이스 메서드와 텍스트 모드 필드,
탐색한 화면 자식의 GOP 메서드, 모드 정보, 픽셀 마스크, 프레임버퍼 주소/크기를
복사한다. 프레임버퍼 내용이나 임의의 힙/목록 레코드는 읽지 않는다. 펌웨어가
제공한 프로토콜 버퍼와 로드 이미지 할당은 신뢰하는 계약으로 남는다.

유일한 native PCD 프로토콜을 요구하고 GetNextToken으로 native 토큰을 열거한다.
최대 4,096번 호출하며 0이 아닌 값이 엄격히 증가해야 한다. 정확한 `0x4ac`가
존재하고 GetSize가 Boolean 한 바이트를 보고할 때만 GetBool(`0x4ac`)를 조회한다.
Boolean 사용은 고정한 IMG14 코드에서 확인했으며 크기만으로 타입을 입증하지는
않는다. PCD setter, SetSku, 화면 메서드 또는 드라이버 Stop은 호출하지 않는다.
토큰이 없거나 유효하지 않으면 Boolean 조회 없이 상태를 기록한다. 다른 토큰의
값이나 구성 데이터베이스 내용은 덤프하지 않는다.

두 번의 조회가 일치해야 하며 이를 둘러싼 인터페이스/모드 스냅샷 세 개를 비교한다.
이후 새 소유 관계와 PCI 구성 검사도 수행한다. 로그는 다음을 구분한다.

```text
ad104-output-observation snapshot-status=...
ad104-output-pcd token=0x04ac first-status=... second-status=...
```

스냅샷 성공이 토큰 조회 성공을 뜻하지는 않는다. 두 조회가 성공할 때만 값을 기록하며
실패를 false로 해석하지 않는다. true는 관측 당시 조건부 SetMode 경로를 뒷받침할
뿐 앞선 정리 과정이 모두 그 경로까지 도달함을 입증하지 않는다. false도 안전한
텍스트 해제를 뜻하지 않는다. 플랫폼별 의미 있는 이름은 아직 모른다.

텍스트 모드, GOP 모드/프레임버퍼 메타데이터와 선택한 콜백의 로드 이미지
base/size/RVA를 기록한다. 대상은 콘솔/오류 OutputString, 콘솔 SetMode,
GOP SetMode와 PCD GetBool이다. 이미지 범위 포함은 위치 근거이며 실행 섹션
포함, 안전한 호출 또는 모든 콘솔 콜백의 소유 관계를 입증하지 않는다.
원시 주소는 비공개 USB/호스트 캡처에 남긴다.

구현 `0997398`의 EFI는 32,768바이트이며 SHA-256은
`7f9cba828a34660723e389465ffd9a190ce8ead20dc08d11477c741d0d5946b1`이다.
빌드 옵션은 `-Ad104Msi -Ad104Metadata -Ad104Resources -Ad104Preflight
-Ad104OutputObservation -WindowsMbr`이다. 매니페스트의 CanDetach/CanUnlock/
CanReadGpuMmio/CanDereferenceRomImage/WritesFirmwareVariables는 모두 false,
CanQueryPcdBoolean은 true다. EDID 변경 테스트와 함께 컴파일할 수 없다.
네이티브 AMD64 PE 검사는 import/IAT/delay import 없음과 NX/dynamic base를
확인했다. 호스트 시험 통과 수는 출력 관측 78, 토폴로지 27, EDID 수명 주기 84,
인계 15, deep AD104 151, 메타데이터 24, ROM 버퍼 113, 리소스 66, MBR 66,
GPT 4, 거부한 컴파일 조합 24, Python 31이다. 실제 실행의 증거는 아니다.

준비 전에 일곱 번째 캡처와 EFI/매니페스트를 호스트와 USB 모두에 보존한다.
작업을 저장하고 USB를 연결한 채 `shutdown /s /t 0`을 실행한다. 꺼진 뒤 10초
기다리고 ASUS Delete → F8 → **UEFI: USB DISK 2.0**(8GB)을 선택한다.
**테스트 키 입력은 필요 없다.** 메타데이터의 `capture-complete=0` 및
`EFI_NOT_READY`는 예상된 결과이며 이후 Windows 시작을 시도한다. 5분 안에
복귀하지 않으면 전원을 끄고 USB를 제거한 뒤 일반 Windows Boot Manager를
선택한다. 펌웨어 조회 콜백은 멈출 수 있으며 복귀를 보장하지 않는다.
한 번만 부팅하고 다음 시험 전에 캡처를 보존한다.
