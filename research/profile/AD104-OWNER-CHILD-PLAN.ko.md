# 다음 후보: PCI 소유자의 자식 정리

[English](AD104-OWNER-CHILD-PLAN.en.md) | 한국어

## 정정 및 구현된 후보

아래의 기존 DisconnectController 제안은 폐기되었습니다. EDK II의
[CoreDisconnectController](https://github.com/tianocore/edk2/blob/master/MdeModulePkg/Core/Dxe/Hand/DriverSupport.c)는
선택한 자식이 유일한 자식이면 자식 없는 부모 Stop을 이어서 호출할 수 있습니다.
NULL이 아닌 자식만 지정해도 자식 콜백을 격리하지 못합니다. 이는 참조 구현의
발견이며 이 OEM 코어의 정확한 구현을 입증하지는 않습니다.

새 선택적 후보는 새로 고정한 바인딩의 `Stop(binding, gpu, 1, &child)`를 직접
호출합니다. 로컬 DriverBinding.h는 시작된 컨트롤러와 생성된 유효 자식이라는
조건하에 직접 호출을 허용합니다. 현재 BY_DRIVER/유일 BY_CHILD_CONTROLLER 관계,
탐색된 유효 핸들, 정확한 이미지/코드/Stop 지문과 두 번의 사전 상태 비교를
요구합니다. Start는 호출하지 않으며 과거 Start 실행은 현재 관계에서 추론하고
별도로 추적한 것은 아닙니다.

[열두 번째 시험 계약](AD104-TWELFTH-BOOT.ko.md)을 참조하세요. 구현, RAM 수명주기
검사 193개와 네이티브 빌드는 완료했지만 실제 실행은 하지 않았습니다. USB 준비는
새 직접 호출/GOP 제거 위험 경계 수락을 기다립니다. 아래는 정정 이유를 위한
이전 조사 제안입니다.

상태: 조사 계약입니다. 구현, USB 준비 또는 실제 소유자 콜백 실행을 주장하지
않습니다. [열한 번째 결과](AD104-ELEVENTH-RESULT.ko.md)는 소비자 해제를 입증하지만
PCI command `0007`과 소유자/자식 관계는 남아 있습니다.

다음 최소 후보는 세 소비자 결과를 새로 확인한 뒤 정확히
`DisconnectController(gpu, pinned_IMG29_binding, discovered_child)`를 호출하는
것입니다. 로컬 `deps/edk2/MdePkg/Include/Uefi/UefiSpec.h` 선언은 NULL이 아닌
ChildHandle과 NULL(모든 자식)을 구분합니다. 실제 펌웨어 전달 경로를 먼저
확인해야 하며 `DisconnectController(gpu, binding, NULL)`로 대체하면 안 됩니다.

[정적 분석](AD104-LIFECYCLE.ko.md)의 IMG29 RVA `0xcd0` 자식 정리는 프로토콜 제거와
레거시 INT10 서비스를 포함합니다. 별도의 자식 없는 Stop `0x7c4`는
`(Supported & 0x700) | 0x18` 속성 Disable을 요청할 수 있으며 이 장치에서는
`0x718`로 예상합니다. 부모 Stop과 디코딩 활성화는 별도 실험입니다.

시험 전 필요한 설계:

- 새 코드 지문, 정확한 BDF/컨트롤러/바인딩/자식, 네 관계, 공유 바이트 0,
  포인터 개수 각각 1, 전체 PCI 상태와 Windows 부트 이미지가 두 번 일치해야
  합니다. 일회성 의도를 flush하고 호출 직전에 다시 확인합니다.
- 새로운 수집기는 GOP 자식 제거 후에도 동작하며 저장한 GOP, mode, text 또는
  드라이버 내부 할당을 역참조하면 안 됩니다. 기존 화면 수집기는 살아 있는
  GOP가 필요하므로 사후 수집기로 사용할 수 없습니다.
- 예상 관계는 PCI BY_DRIVER와 포인터 두 개로 총 세 개입니다. PCI
  BY_CHILD_CONTROLLER는 없어야 합니다. 자식 프로토콜 부재는 새 펌웨어 탐색으로
  확인하며 오래된 인터페이스를 읽지 않습니다.
- command `0007`, 속성 `0718`, 지원 `1b77b` 및 전체 PCI 구성이 유지될 것으로
  예상합니다. 이는 실제 시험이 필요한 예측입니다.
- 오류 콜백 후에도 독립적으로 반복 수집합니다. 부분 제거, 잘못된 관계,
  포인터/입력/구성 변경 또는 기록 실패는 다음 단계와 Windows 인계를 차단합니다.
  재시도/재연결, 속성 쓰기, MMIO, 리셋, 부모 Stop 또는 unlock은 포함하지 않습니다.
- RAM 시험은 사전 조건 변경, 부분 정리, 해제된 인터페이스 미접근, 콜백 오류,
  기록 및 최종 입력 실패를 포함해야 합니다.

새 위험 경계: 자식 정리는 레거시 비디오를 재초기화하고 이후 부팅 코드가 쓰는
GOP 인터페이스를 제거/해제할 수 있습니다. 소비자 전용 시험은 복귀했어도 화면/
입력이 사라지거나 펌웨어가 멈추고 Windows가 시작되지 않을 수 있습니다.
수동 전원 차단과 USB 제거가 필요할 수 있으며 복구는 입증되지 않았습니다.
참석 부팅용 자식 정리 시험을 준비하기 전에 이 구체적인 경계를 제시해야 합니다.

자식 전용 시험이 성공해도 PCI 소유자와 BME는 남습니다. 별도로 허용한 부모 해제와
독립적인 cold-entry 재확인만이 남은 [unlock 입증 단계](AD104-UNLOCK-EVIDENCE.ko.md)를
진행할 수 있습니다.
