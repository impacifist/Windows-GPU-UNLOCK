# 다음 참석형 테스트: 텍스트 콘솔 연결 Stop 1회

[English](NEXT-EFI-TEST.en.md) | [한국어](NEXT-EFI-TEST.ko.md)

상태: 구현/컴파일 완료, 하드웨어 미검증, USB 미배치입니다. 첫 EVGA3090 수명주기 테스트이며 언락이 아닙니다. 아래 위험을 읽고 배치/실행 여부를 선택하세요. 별도 확인 답변은 필요하지 않습니다.

## 범위와 위험

사전 캡처와 새로운 대문자 G 입력 후 IMG13의 `Stop(binding, virtualGop, 1, &systemConsoleOut)`만 1회 호출합니다. 실시간 GOP 2개와 정확한 소유 관계, 단일 EVGA3090 PCI 디스플레이 ID/서브시스템을 확인하고 콘솔 드라이버 3개의 Stop 주소, 이미지 크기, 버전, 실행 섹션 해시를 고정합니다. 저장된 IMG13의 RVA 0x6190 자식 슬롯이 캡처 ConsoleOut과 일치하여 제안 인수와 부합합니다. 호출 전 EVGA-TEXT-STOP.ARMED를 생성하고 플러시하며 기존 마커가 있으면 재시도하지 않습니다.

이후 IMG13 텍스트 소비자 관계 2개가 제거되고 나머지 GOP/소유자/코드 핀이 유지되는지 확인합니다. 그래프를 다시 캡처하고 파일을 닫은 뒤 Windows StartImage 직전에 선택 그래프를 다시 확인합니다. Stop 성공만으로 통과하지 않습니다. 모든 검사가 성공하면 미리 로드한 Windows Boot Manager를 시작합니다.

실제 펌웨어 콘솔 연결을 변경합니다. Stop은 공유 콘솔 상태를 해제하거나 헬퍼 콜백을 실행할 수 있습니다. 화면/입력 손실, 펌웨어 정지, Windows 시작 또는 GPU 초기화 실패가 가능합니다. 수동 전원 종료와 USB 제거가 필요할 수 있으며 복구는 미검증이고 자동 복원은 없습니다. Stop 시작 후 검사 실패 시 정지합니다. Stop 전 거부는 로그/파일 닫기가 성공하면 정상 Windows 시작을 시도합니다. 마커는 시도가 준비되었다는 뜻이며 호출 완료를 증명하지 않습니다.

IMG16/17, EDID, NVIDIA Stop, PCI 속성 쓰기, GPU MMIO, 리셋, DMA 시작, 펌웨어 업로드/실행, 언락, NVRAM 쓰기는 추가하지 않습니다. 인벤토리에 PCI 구성을 읽습니다. 소비자 콜백 자체의 간접 하드웨어 부작용까지 배제하는 주장은 아닙니다.

## 컴파일만 수행

업데이트된 PR96 브랜치 저장소 루트에서 본인 PC에서 확인한 Windows ESP GUID를 사용하세요.

```powershell
.\research\profile\build-evga-text-stop.ps1 -WindowsEspGuid '<your Windows ESP GUID>' -Clang 'C:\Program Files\LLVM\bin\clang.exe' -LldLink 'C:\Program Files\LLVM\bin\lld-link.exe'
```

로컬 빌드 폴더에만 기록하며 USB 복사, 부팅 설정 변경, EFI 실행은 하지 않습니다. 구성은 internal-boot/boot/build/evga-text-stop/에 격리됩니다. 빌드가 성공하고 위험을 이해한 상태로 실행하기로 선택하면 아래 배치 명령을 사용하세요. 오류 발생 시 중단하고 마커 삭제나 강제 프로필 적용은 하지 마세요.

## 원하는 시점에 배치 및 실행

기존 캡처 USB가 여전히 D:라면 실행하세요.

```powershell
.\research\profile\stage-evga-text-stop.ps1 -Drive D
```

빌드/소스 해시와 현재 PC의 ESP를 확인하고, 원본 캡처 2개가 있는 FAT32 USB 데이터 볼륨만 허용합니다. 기존 시도 마커가 있으면 거부합니다. 현재 EFI를 백업한 뒤 새 EFI를 복사하고 해시를 확인합니다. 캡처 보존, 포맷/부팅 설정 변경/재부팅 없음입니다. 문자가 바뀌었다면 실제 USB 문자를 사용하고 거부를 우회하지 마세요.

작업 저장 후 완전히 종료하고 UEFI USB를 선택합니다. `Live guards passed. NOW press fresh Shift+G once`가 표시되면 Shift+G를 1회 누릅니다. Shift+V는 필요 없고 통과하면 자동 Windows 시작을 시도합니다. G 이후 화면이 멈춰 보일 수 있습니다. 3분 내 Windows가 시작하지 않으면 전원을 끄고 USB를 제거한 뒤 정상 Windows Boot Manager를 선택합니다. 반복하거나 마커를 삭제하지 마세요. EVGA00.TXT(또는 다음 EVGAnn)와 마커를 보존하고 자동 복귀인지 수동 복구인지, 화면/입력 변화도 알려주세요.

## 로컬 검증

모의 EFI 서비스/해시/Stop을 사용하는 호스트 체크포인트 18개가 통과했습니다. 성공, 잘못된 인벤토리/코드/버전/메서드/소유자/콘솔, 기존 마커, 로그 실패, 콜백 실패, 변경 없는 사후 상태, 취소, 시간 초과, 짧은 마커 쓰기/플러시 실패, 최종 그래프 변화를 검증합니다. 캡처된 펌웨어는 실행하지 않았습니다.

기존 진단 핸드오프도 경고를 오류로 처리하여 컴파일되며 Python 테스트 78개가 통과했습니다. 새 EFI도 같은 조건으로 빌드되었습니다. PE 검사는 AMD64 EFI 앱, import/IAT/delay import 없음, NX 및 동적 베이스를 확인했습니다. 로컬 빌드는 18,944바이트, SHA256 e25e20da76331b1246db96d6e5141b6c934befb5f12cf383876c53853fb4d15b입니다. 도구 체인이 다르면 해시가 달라질 수 있으므로 실제 값을 기록하세요.

통과하더라도 해당 PC의 정확한 연결 제거와 관찰된 Windows 핸드오프만 입증합니다. 후속 소비자 검증 또는 언락 증거는 아닙니다.

수정 빌드와 첫 실행 근거: [FIRST-EFI-RESULT](FIRST-EFI-RESULT.ko.md). 위의 이전 파일 정보는 초기 빌드를 설명합니다.

최신 호출 전 거부와 단계 진단 빌드: [SECOND-EFI-RESULT](SECOND-EFI-RESULT.ko.md).

[관련 보고 및 증거의 한계](RELATED-ISSUES.ko.md)
