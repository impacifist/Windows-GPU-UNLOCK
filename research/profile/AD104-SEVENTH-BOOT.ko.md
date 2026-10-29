# 일곱 번째 AD104 부팅: EDID 연결 해제 한 번

[English](AD104-SEVENTH-BOOT.en.md) | 한국어

첫 수명 주기 변경 테스트이며 unlock 테스트가 아니다. 운영자는 화면/입력 손실,
펌웨어 정지, 수동 전원 차단, Windows 복귀 실패 가능성을 명시적으로 수락했다.
복구는 검증되지 않았으며 이 PC에는 내장 GPU가 없다.
[전체 계약](AD104-EDID-EXPERIMENT.ko.md)을 참고한다.

EFI는 먼저 기준 자료를 저장하고 정확한 식별 정보, 코드, 여덟 관계 그래프,
공유 상태, 전체 PCI 구성, BAR 및 속성을 새로 검사한다. MBR 인계는 준비된
Windows 부팅 이미지 바이트를 고정하고 검증한다. 첫 안내에서 **Shift+E**
(대문자 `E`)를 눌러 탐색한 GOP 자식과 IMG08 바인딩만 대상으로 하는 한 번의
`DisconnectController`를 허용한다. 다른 키나 입력 시간 초과는 호출 없이 정지한다.
이 테스트는 연결 해제 전 정지하더라도 자동 Windows 대체 부팅을 하지 않는다.

실행 의도 기록을 flush한 뒤 한 번만 호출한다. 반환 상태에 관계없이 두 번의
독립적인 사후 수집을 수행한다. 예상 상태는 EDID 관계만 사라져 여덟 관계가
일곱 개가 되고 공유 바이트는 `1→0`, 포인터 카운트는 `1/1` 유지, 나머지 고정
상태는 변화 없음이다. 검사와 콜백 상태가 통과한 경우에만 다음 안내가
**Shift+V**(대문자 `V`)를 요청한다. 변경 후 실제 키보드 동작을 확인하는 단계다.
새 사후 검사, 결과 기록, 파일 닫기가 모두 성공해야 Windows 시작을 시도한다.
재시도, 재연결, PCI 소유자 해제, GPU MMIO, ROM 복사, 속성 쓰기, NVRAM 변경,
unlock 트랜잭션은 없다.

각 키 안내는 60초 분량의 폴링 예산이며 펌웨어 콜백 자체는 멈출 수 있다.
키를 누르고 있거나 미리 입력하지 않는다. 화면이 사라지면 보이지 않는 상태에서
입력하거나 부팅을 반복하지 않는다. 정지하거나 Windows가 5분 안에 복귀하지 않으면
전원을 끄고 USB를 제거한 뒤 일반 Windows Boot Manager를 선택한다. 이는 복구
시도이지 보장이 아니다. 다음 EFI 실행 전에 USB 파일을 보존해야 하며 결과가
없다고 테스트를 두 번 실행하면 안 된다.

작업을 저장하고 캡처 USB를 연결한 채 `shutdown /s /t 0`을 실행한다. 전원이
꺼진 뒤 10초 기다린다. 전원을 켜면서 ASUS Delete, F8을 누르고
**UEFI: USB DISK 2.0**(8GB)을 선택한다. BIOS 설정은 바꾸지 않는다.

구현 `b45ddd6`에서 `-Ad104Msi -Ad104Metadata -Ad104Resources -Ad104Preflight
-Ad104EdidTest -WindowsMbr`로 빌드했다. EFI는 33,280바이트이며 SHA-256은
`5503b5fc3cda5cac2d9bc852d669ec509890f94800a205510da54b099b9d69f7`이다.
매니페스트는 의도적으로 `CanDetach=true`, `CanUnlock=false`,
`CanReadGpuMmio=false`, `CanDereferenceRomImage=false`, `HardwareTested=false`를
보고한다. 네이티브 AMD64 PE 검증은 import/IAT/delay import 없음과 NX/dynamic
base를 확인했다. RAM 모의 검사 통과 수는 수명 주기 84, 토폴로지 27, 인계 15,
deep AD104 151, 메타데이터 24, ROM 버퍼 113, 리소스 66, MBR 66, GPT 4,
거부한 컴파일 조합 22 및 Python 시험 31이다. 실제 복구를 입증하지는 않는다.

준비 전에 여섯 번째 캡처와 이전 EFI/매니페스트를 검증된 호스트 및 USB 보관
폴더에 남겨야 한다. 기존 `RTX3090-UNLOCK` 폴더는 이 RTX 4070 Ti의 진단 출력
폴더 이름으로 유지하며 3090 프로필을 사용하는 것은 아니다.
