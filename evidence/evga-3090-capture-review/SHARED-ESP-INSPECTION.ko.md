# 공유 ESP: 읽기 전용 부팅 조사

[English](SHARED-ESP-INSPECTION.en.md) | [한국어](SHARED-ESP-INSPECTION.ko.md)

Movian은 기존 Windows/CachyOS ESP에 별도 부팅 항목을 추가하는 방식을 선호합니다. 현재 EVGA EFI는 USB 매체를 사용하며 트랜잭션 후 고정된 Windows Boot Manager를 직접 시작합니다. 따라서 해당 실행에서는 Linux 선택을 건너뛰지만 기존 이중 부팅 구성을 삭제하거나 변경한다는 뜻은 아닙니다. 기존 내부 코드는 잠금 해제 이미지와 Windows에 동일한 파티션을 사용하는 것을 거부합니다. 공유 ESP EVGA 프로필은 구현되거나 시험되지 않았습니다.

Movian은 Windows에서만 잠금 해제가 필요하며 일반적인 CachyOS 부팅은 유지되어야 한다고 명확히 했습니다. 따라서 기존 Linux 부팅 파일과 일반 부팅 선택을 보존한다면 Windows 전용 잠금 해제 항목은 Windows로 직접 인계할 수 있습니다. Linux 잠금 해제나 잠금 해제 후 OS 메뉴는 요청되지 않았습니다.

다음 조사는 기존 시스템 볼륨의 파일 시스템과 총/여유 용량을 조회하고 `bcdedit /enum firmware`로 펌웨어 부팅 항목을 나열합니다. ESP를 마운트하거나 파일을 나열/수정하지 않고, 펌웨어 항목이나 부팅 순서를 변경하거나 재부팅하지 않습니다. 펌웨어 항목 읽기에는 관리자 PowerShell이 필요할 수 있습니다. 거부되면 오류를 보내 주세요. 오류가 나면 중단합니다.

브랜치를 업데이트한 뒤 저장소 루트에서 실행합니다.

```powershell
powershell -NoProfile -File ./research/profile/inspect-shared-esp.ps1
```

공유 전에 출력을 확인하세요. 펌웨어 항목에 식별자, 장치 정보 및 부팅 경로가 포함될 수 있습니다. 비공개 정보를 가리고 가능하면 항목 설명과 EFI 경로는 남겨 주세요. 결과는 현재 부팅 구성의 단서일 뿐 로더 동작, 안전한 공유 쓰기 또는 복구를 증명하지 않습니다. 이 결과만으로 파티션을 조정하거나 런처를 설치하지 마세요.

호스트 모의 시험은 성공 및 항목 조회 실패 사례를 통과했습니다. 연구용 시스템에서 읽기 전용 실행은 성공하지 못했으므로 Movian의 시스템에서는 아직 검증되지 않았습니다.

이후 Movian은 성공한 출력을 제공했습니다. 시스템 볼륨은 FAT32이며 전체 11,585,944,576바이트 중 10,425,717,760바이트가 남아 있습니다. 펌웨어 목록에는 Limine 항목이 첫 번째로 표시되고 Windows Boot Manager 항목이 있으며 둘 다 보고된 동일 볼륨을 가리킵니다. 다른 Windows 항목과 USB 항목도 있습니다. 표시된 경로는 `\EFI\limine\limine_x64.efi` 및 `\EFI\Microsoft\Boot\bootmgfw.efi`입니다. 이는 기여자가 제공한 Windows 출력이며 파일 내용, 실제 사용된 부팅 경로 또는 안전한 공유 ESP 쓰기를 독립적으로 증명하지 않습니다. Windows 전용 잠금 해제 설계는 기존 항목과 일반 CachyOS 부팅을 보존해야 합니다.
