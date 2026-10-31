# Christian MSI 텍스트 콘솔 Stop 에뮬레이션

[English](CHRISTIAN-STOP.en.md)

반환한 스크린샷에서 두 캡처의 정확한 드라이버 고정값 검사와 각각 여섯 개의 부정 사례가 통과했습니다. 실제 장비 해제 증거는 아닙니다.

새 개발자 검사는 저장된 텍스트 Stop RVA 0x10bc를 Unicorn x86-64 CPU에서 에뮬레이션합니다. 두 캡처에서 자식 0개는 EFI_SUCCESS(22명령), 자식 2개는 EFI_INVALID_PARAMETER(23명령), 자식 1개와 null 버퍼는 EFI_INVALID_PARAMETER(25명령)를 반환했습니다. 스택 복귀와 실행 주소 범위를 확인하며 실제 펌웨어 실행이나 하드웨어 접근은 없습니다. 초기 잘못된 인자 예상값 EFI_UNSUPPORTED는 0x125b 디스어셈블리와 실행 결과에 따라 EFI_INVALID_PARAMETER로 수정 후 커밋했습니다.

Unicorn이 설치된 개발 환경에서 `python research/profile/test-christian-stop-emulation.py <저장된-캡처-폴더>`로 재현합니다. 로컬 의존성이 사용자 패키지 경로에 있으므로 이 개발자 검사에는 `-I`를 사용하지 않습니다. 펌웨어 이미지는 데이터로만 취급합니다.

유효한 자식 하나 제거, 연결 목록 변경 및 프로토콜 콜백은 아직 검증하지 않았습니다. 해제 EFI 준비 전에 별도 검토/검사가 필요합니다. Christian은 추가 캡처 부팅이나 에뮬레이터 설치를 할 필요가 없습니다. 기존 캡처와 BIOS 구성을 보존하세요.
