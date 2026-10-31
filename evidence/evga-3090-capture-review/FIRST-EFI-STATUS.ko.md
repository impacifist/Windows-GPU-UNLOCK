# 첫 EVGA 텍스트 연결 테스트: 기여자 배치 보고

[English](FIRST-EFI-STATUS.en.md) | [한국어](FIRST-EFI-STATUS.ko.md)

2026년 10월 27일 21:51 Movian은 Discord에서 로컬 빌드 및 USB 배치 성공을 보고했습니다. PE 검사는 18,944바이트, AMD64 EFI 앱, efi_main 진입점, import/IAT/delay import 없음, NX 및 동적 베이스를 보고했습니다. 빌드와 배치 파일의 보고된 SHA256은 모두 다음과 같습니다.

`d76a85d10d9c045281b6430541d23dc4fd01eef14185fb8ceec0fe337b14b2c7`

배치 도구는 D:/EFI/BOOT/BOOTX64.EFI 복사, 기존 캡처 EFI의 BOOTX64.before-evga-text-stop.efi 백업, 캡처 보존을 보고했습니다. 기여자가 제공한 출력이며 그의 USB나 바이너리를 독립적으로 검사한 것은 아닙니다. 로컬 Swift 도구 체인 빌드와 해시가 다르다는 이유만으로 실패를 의미하지 않습니다. 기여자는 자신의 LLVM 설치를 사용했으며 도구 체인 간 바이트 동일성을 주장하지 않습니다.

현재 근거는 빌드/배치 보고뿐입니다. 이 EFI의 실제 Stop 결과나 Windows 복귀 증거는 아직 없습니다. 다음 단계는 [테스트 안내](NEXT-EFI-TEST.ko.md)의 참석형 실행 1회 후 EVGAnn.TXT와 자동/수동 Windows 복귀 보고를 받는 것입니다. 배치 성공만으로 범위를 넓히거나 일회성 마커를 삭제하지 마세요.

Movian은 GitHub Desktop으로 브랜치를 업데이트합니다. 요청하지 않는 한 향후 채팅에서 CLI pull 명령을 생략합니다. 빌드 배너도 문서의 운영자 선택 방식에 맞췄습니다. 출력 문구만 바꾼 수정이므로 이미 배치된 EFI는 변하지 않으며 재빌드할 필요가 없습니다.
