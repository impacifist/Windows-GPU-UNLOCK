# 첫 실제 EFI 실행: 호출 전 캡처 거부 및 수정 테스트

[English](FIRST-EFI-RESULT.en.md) | [한국어](FIRST-EFI-RESULT.ko.md)

Movian은 3분 이상 대기 후 수동 전원 재시작으로 정상 Windows 부팅과 화면을 확인했다고 보고했습니다. 다운로드한 EVGA00.TXT는 408,651바이트이며 SHA256은 c0bc72185c59d24e685b875bb58b6701c01c84eaeb165aa848cad48b13d8cc43입니다. 원본은 추적 대상 밖에 로컬 보존하고 [정제 결과](first-efi-run-result.json)에 해시와 결과를 기록했습니다.

로그의 불완전 사유는 gop-info 하나이며 EFI_NOT_READY(0x8000000000000006), capture-complete=0, 같은 상태의 handoff-stopped로 끝납니다. evga-text-stop-before 및 evga-text-stop-return은 없습니다. 소스는 초기 캡처 성공 후에만 GuEvgaTextStop에 진입하므로 드라이버 Stop 호출에 도달하지 않았습니다. 이는 호출 전 소프트웨어 거부이며 Stop이 멈췄다는 증거가 아닙니다. 수명주기 성공이나 언락은 입증되지 않았습니다.

## 수정

일반 프로브는 이미 경로/프레임버퍼가 없고 SizeOfInfo=0인 GOP를 기록했지만 새 EVGA 빌드는 이 경우를 빠뜨렸습니다. 수정된 진단은 프레임버퍼 주소/크기가 모두 0이고 장치 경로 조회가 EFI_UNSUPPORTED인 GOP에 한해 모드 정보 크기 0을 허용합니다. 크기 0인 Info 포인터는 역참조하지 않습니다. 물리 GOP, 0이 아닌 짧은 메타데이터, 경로 존재/오류, null Mode는 여전히 실패합니다. 실시간 코드/소유 관계 검사와 1회 호출 범위는 유지합니다.

새 이미지는 Stop 전 거부를 기록하고 Stop이 시작되지 않았으며 로그/파일 닫기가 성공하면 미리 로드한 정상 Windows 진입을 시도합니다. Stop 시작 후 실패는 여전히 재시도/복원 없이 정지합니다. USB 배치는 기존 백업이 있으면 번호가 붙은 새 백업을 선택하여 원래 캡처 EFI와 실패한 테스트 EFI를 모두 보존합니다. 시도 마커가 있으면 여전히 거부하며 삭제하면 안 됩니다.

검증: 호스트 체크포인트 18개, 실제 진단 경로 회귀 사례 9개, Python 테스트 78개, 경고를 오류로 처리하는 EFI 빌드가 통과했습니다. PE 검사는 AMD64 EFI 앱, import/IAT/delay import 없음, NX/동적 베이스입니다. 수정 로컬 파일은 19,456바이트, SHA256 d9c1cb8dea987cf64ba6e1bab822cb048cb69117007b843a6d4b03d92eb427b9입니다. 기여자 도구 체인 해시는 다를 수 있으며 자신의 빌드 매니페스트와 배치 파일이 일치해야 합니다.

## 다음 실행

GitHub Desktop으로 같은 브랜치를 업데이트하고 이전 빌드 명령으로 재빌드한 뒤 stage-evga-text-stop.ps1 -Drive D를 다시 실행하세요. EVGA00.TXT와 모든 백업을 보존하세요. 시도 마커 또는 다른 오류가 보고되면 우회하지 말고 결과를 보내주세요. [NEXT-EFI-TEST](NEXT-EFI-TEST.ko.md)의 동일한 부팅/Shift+G 안내와 위험이 적용됩니다. 별도 확인 답변은 필요 없습니다. 다음 EVGAnn.TXT와 자동/수동 Windows 복귀 여부를 보내주세요.
