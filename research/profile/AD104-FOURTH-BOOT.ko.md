# 네 번째 AD104 테스트: BAR0 리소스 관찰

[English](AD104-FOURTH-BOOT.en.md) | 한국어

질문: 정확한 GPU에 대해 펌웨어가 보고하는 BAR0 설명자가 기존 심층 캡처의
예상 형식과 일치하는가? 첫 부팅에서는 설명자 질의 전에 BME 조건으로 중단했습니다.
이번 별도 메타데이터 모드는 `GetBarAttributes`와 PCI 구성/위치 질의만 호출하며
GPU MMIO 메서드는 호출하지 않습니다. BME를 그대로 유지합니다. ROM 버퍼 복사를
비활성화하고 컴파일 시 리소스 모드와의 결합도 거부합니다. 드라이버 해제,
펌웨어 업로드 또는 언락을 하지 않습니다.

빌드: `build-probe.ps1 -Ad104Msi -Ad104Metadata -Ad104Resources -WindowsMbr`.
출력 `internal-boot/boot/build/probe-ad104-resources/BOOTX64.EFI`는
23,040바이트이며 SHA-256은
`cf3bd58ef496604d653fc4d21da6030f27eb326dea49b5131ca590026d80014d`입니다.
구현 `c671d99`에서 빌드했고 네이티브 AMD64 EFI PE 검증을 통과했습니다.
imports/IAT/delay imports가 없으며 NX와 dynamic base가 활성화되어 있습니다.
아직 실제 부팅하지 않았습니다.

질의 전에 정확한 장치 ID와 디스플레이 대상 하나를 검사합니다. 질의 전후에
동일한 인터페이스, BDF, PCI 구성 256바이트 전체를 확인합니다. 상태, 설명자
유효성, granularity, 최소/최대값, 길이, 변환값 및 지원 플래그를 기록한 뒤
반환된 풀 할당을 해제합니다. UEFI 프로토콜은 할당 길이를 제공하지 않으므로
기존 허용 조건 코드와 마찬가지로 펌웨어의 설명자 목록 할당 계약을 신뢰합니다.

`descriptor-valid=1`과 `envelope-shape=1`은 설명자에 관한 값일 뿐 전체 MMIO
허용을 뜻하지 않습니다. 메모리 디코딩, BME, 정렬, 장치 ID 안정성 및 다른
검사는 별도입니다. 오류나 설명자 부재도 유효한 부정 관찰입니다.
`capture-complete=0`과 최종 `EFI_NOT_READY`는 예상 결과이며 언락 허용 근거가
되지 않습니다.

검사 통과: 리소스 40개, 심층 AD104 151개, 메타데이터 24개, 펌웨어 버퍼 113개,
MBR 66개, GPT 4개, 거부된 컴파일 조합 20개, Python 테스트 17개입니다.
리소스 테스트는 BME 활성화, 형식 불일치, 체크섬, null 설명자, 잘못된 길이/종료
태그, 콜백 오류, ID 변동 및 잘못되거나 여러 개인 장치를 검사합니다.

준비 전에 이전 캡처와 EFI를 보관하고 활성 `RTX3090-UNLOCK` 폴더를 비웁니다.
작업을 저장하고 캡처 USB를 연결한 채 `shutdown /s /t 0`을 실행합니다.
완전히 꺼진 뒤 10초 기다리고 ASUS Delete → F8 → 8GB USB DISK 2.0의 UEFI
항목을 선택합니다. 테스트 키 입력은 필요하지 않습니다. 4분 안에 Windows로
돌아오지 않으면 전원을 끄고 USB를 제거한 뒤 일반 Windows Boot Manager로
부팅합니다. 자동 복귀는 보장하지 않습니다. 복귀 후 새 로그/이미지를 보존하기
전에 다시 부팅하지 마십시오. BIOS 모드 변경은 필요하지 않습니다.
