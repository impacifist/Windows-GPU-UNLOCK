# 다음 시험: 오프라인 콘솔 연결 재생

[English](NEXT-TEST.en.md) | [한국어](NEXT-TEST.ko.md)

이 시험은 재부팅, 펌웨어 자산, BIOS 변경이나 새 EFI가 필요 없다. 저장된 캡처를 읽고 두 번째 GOP가 관측된 펌웨어 콘솔 연결에 속하는지 검사한다. 프로필을 생성하거나 기존 토폴로지 허용 검사를 바꾸지 않는다.

PR96 브랜치의 저장소 루트에서 실행한다.

```powershell
python -I research/profile/inspect-evga3090-topology.py "D:\RTX3090-UNLOCK"
```

제출된 파일에서는 각 캡처가 expected_console_chain_observed=true, write_eligible=false, unlock_proven=false, hardware_accessed=false를 출력해야 한다. 종료0은 관측 연결 일치이며 잠금 해제 빌드 허용이 아니다. 다른 검사가 나오면 BIOS 변경이나 프로필 강제 대신 JSON을 보존/공유한다. 누락/잘못된 형식/모호한 기록은 실패한다.

## 기존 파일이 입증하는 범위

DIAG00/01은9개 검사에 일치한다. PCI 디스플레이1개, GOP2개, RTX PCI 경로 아래 GOP1개, 해당 GOP의 유효한 framebuffer, 다른 GOP의 장치 경로 부재와 framebuffer0, 두 GOP를 잇는 count1 그래픽 콘솔 BY_DRIVER/BY_CHILD 관계, 두 번째 GOP에서 SystemTable ConsoleOut으로 이어지는 count1 텍스트 콘솔 관계, 마지막 콘솔의 텍스트 출력과 GOP 부재다.

연결 바인딩 ID는55987e62-1df6-41d1-9062-334fcac3e54d(그래픽 콘솔 역할),628a497d-2bf6-4264-8741-069dbd3399d6(텍스트 콘솔 역할)이다. 캡처된 식별 정보이지 드라이버 코드 인증/실행 자격은 아니다. 경로/framebuffer가 없는 GOP와 동일 에이전트 자식 관계는 펌웨어 가상 콘솔 계층의 증거다. GOP2개라는 수치만으로 두 번째 물리 GPU나 활성 iGPU를 식별하지 않는다. 이 시험은 어떤 펌웨어 생산자/소비자도 분리하지 않는다.

[재생 결과](topology-test-results.json)는 원시 핸들/framebuffer 주소 없이 캡처 SHA256과 논리 결과를 기록한다. 기존 topology_scope 거부를 설명하지만 제거하지 않는다. ConOut 변수 부재와 집계 콘솔은 현재 자동 경로 밖이다. 지원 변경은 별도 구현/재생 검토가 필요하다. 서로 다른 파일 해시만으로 별도 완전 종료 부팅이나 Windows 도착을 독립 입증하지 못한다.

검증: 잘못된/공유 드라이버, 소유 횟수, 바인딩 ID, 콘솔/framebuffer 누락, 미완료 입력, CRLF 처리를 포함한 합성 회귀10개가 통과했다. 실제 엄격 파서 재생도 업로드 로그2개에서 통과했다. 새 하드웨어 시험은 없으며 기존 캡처 기준을 검토하는 동안 BIOS는 유지한다.
