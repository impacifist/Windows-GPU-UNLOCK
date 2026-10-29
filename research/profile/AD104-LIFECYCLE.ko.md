# AD104 수명주기 조사 및 다섯 번째 진단

[English](AD104-LIFECYCLE.en.md) | 한국어

## 해제 코드 이식 불일치

[오프라인 감사 증거](ad104-release-port-evidence.json)는 고정된 외부 해제 코드를
그대로 사용할 수 없는 세 가지 이유를 재현합니다.

| 속성 | 캡처 호스트 | AD104 참조 해제 코드 |
| --- | --- | --- |
| PCI 위치 | `0000:0b:00.0` | `0000:01:00.0` 요구 |
| PCI command | `0007`, BME 활성화 | BME 활성화 거부 |
| PCI 소유자 버전 | `0x24` | `0x7000b` |
| 소유자 이미지 바이트 | 18,432 | 162,016 |
| Stop RVA | `0x7c4` | `0x6ec` |

참조: [gpu-unlock-ada의 고정된 msi_release.c](https://github.com/JustAResearcher/gpu-unlock-ada/blob/4e7fefcc4923f9e083d5324dc75a7d2f906af77f/windows/ad104-msi/msi_release.c).
로컬 full-release 구현도 PC별 위치, MEM1/BME0, 드라이버 지문 및 예상 그래프
변화를 고정합니다. ROM 안의 네이티브 NVIDIA EFI 이미지는 관찰된 PCI 소유자가
아닙니다. 상수 몇 개를 바꾸는 것만으로 검증된 이식이 되지 않습니다.

## 캡처된 Stop 콜백

비공개 IMG29 스냅샷을 정적으로 분석했습니다. SHA-256은
`6978c561cfc119be29c3832d47fc51e471b02e86dc84b3a4eac6524a65de502c`이며
기록된 Stop RVA는 `0x7c4`입니다. 자식 수가 0인 분기에서 PCI I/O 오프셋
`0x78`을 operation 4로 호출하고, 반환값에 `0x700` 마스크와 `0x18` OR를 적용한
뒤 같은 메서드를 operation 3으로 호출하는 명령이 있습니다. 번들 EDK II 헤더로
호스트 ABI 검사를 컴파일하여 `0x78`이 Attributes, operation 4가 Supported,
operation 3이 Disable임을 확인했습니다. `0x700`은 I/O 디코딩, 메모리 디코딩과
버스 마스터링이며 `0x18`은 VGA 메모리 및 VGA I/O입니다. 이 분기는 디코딩과
DMA 권한 변경을 요청할 수 있습니다. 이는 정적 경로 해석이며 관찰된 Stop 실행
결과가 아닙니다.

RVA `0xcd0`의 자식 정리 도우미에는 프로토콜 정리와 레거시 INT10 비디오 서비스에
부합하는 호출이 있습니다.
[TianoCore의 보관된 BIOS 비디오 수명주기](https://github.com/tianocore/edk2-archive/blob/master/IntelFrameworkModulePkg/Csm/BiosThunk/VideoDxe/BiosVideo.c)와
유사하지만 OEM 동작의 동일성을 입증하지 않습니다. 두 번째 바인딩은 이미지 핸들이
null이지만 Stop RVA `0x2b38`이 IMG29 안에 있습니다. null 이미지 핸들만으로
관련 없는 드라이버이거나 누락되었다고 판단해서는 안 됩니다. 이번 작업에서 Stop
콜백이나 DisconnectController를 실행하지 않았습니다.

하드웨어 접근 없이 스냅샷 감사를 재현합니다.

```powershell
python research/profile/audit-release-port.py C:\private\DIAG00.TXT `
  --output C:\private\release-audit.json
```

도구는 이 캡처 계열 전용이며 런타임 권한을 부여하지 않습니다. CRLF 처리와
모호한 입력 거부를 포함해 프로필 Python 테스트 19개가 통과했습니다. 해제 순서나
복구를 인증하는 감사가 아닙니다.

## 다섯 번째 EFI 테스트: 장치 전체 속성 질의

상태 변경 전에 `PciIo.Attributes(Get)`과 `PciIo.Attributes(Supported)`를 수집합니다.
네 번째 부팅의 GetBarAttributes 지원 필드는 BAR별 정보이며 이 장치 전체 질의에
대한 답은 아닙니다. 새 리소스 진단은 속성 마스크 0으로 Get/Supported만 호출하고
전후의 인터페이스/BDF/PCI 구성 전체를 다시 확인합니다. 두 상태를 기록하며 두 호출이
성공하고 ID가 유지될 때만 마스크를 공개합니다.

예상 기록은 `ad104-pci-attributes ... current=... supported=... query-only=1`입니다.
값은 정적 Stop 경로의 후보 마스크를 구체화할 수 있지만 Stop이 해당 분기에 도달하거나
성공하거나 BME를 해제하거나 Windows 복구를 유지한다는 증거는 아닙니다.
Set/Enable/Disable, GPU MMIO, ROM 버퍼 복사, 드라이버 해제, 펌웨어 업로드 또는
언락은 포함하지 않습니다. 기존 BME 허용 조건도 유지합니다.

빌드 옵션은 `-Ad104Msi -Ad104Metadata -Ad104Resources -WindowsMbr`입니다.
구현 `588323e`, EFI 23,040바이트, SHA-256:
`2949d1f816b0b201c98320d0b762c931571cdf1749bee0c377f07a47292a91b7`.
AMD64 PE 검사 통과(imports/IAT/delay imports 없음, NX/dynamic base).
호스트 검사는 리소스/속성 66개, 심층 AD104 151개, 메타데이터 24개, ROM 버퍼
113개, MBR 66개, GPT 4개, 거부된 빌드 조합 20개, Python 19개입니다.
새 산출물의 실제 테스트는 아직 수행하지 않았습니다. `capture-complete=0`은
예상 결과입니다.

준비 전에 이전 파일을 보관합니다. 캡처 USB를 연결하고 작업을 저장한 뒤
`shutdown /s /t 0`을 실행합니다. 완전히 꺼진 뒤 10초 기다리고 ASUS Delete →
F8 → 8GB USB DISK 2.0의 UEFI 항목을 선택합니다. 테스트 키는 필요하지 않습니다.
4분 안에 Windows로 돌아오지 않으면 전원을 끄고 USB를 제거한 뒤 일반 Windows
Boot Manager로 부팅합니다. 자동 복귀는 보장하지 않습니다. 다시 부팅하기 전에 새
파일을 보존합니다. BIOS 그래픽/CSM 설정을 바꾸거나 질의 성공을 해제 허용으로
해석하지 마십시오.
