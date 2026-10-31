# 관련 이슈와 EVGA3090에 적용 가능한 내용

[English](RELATED-ISSUES.en.md) | [한국어](RELATED-ISSUES.ko.md)

2026년 10월 27일 이슈 본문과 관련 댓글을 검토했습니다. 다른 시스템의 원본 캡처를 독립 검증한 결과가 아니라 보고된 증거의 비교입니다.

| 보고 | 공통점 | 차이와 의미 |
|---|---|---|
| [#95 ASRock B760 / ZOTAC3090](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/95) | 물리 디스플레이 1개, GOP 2개, 경로와 프레임버퍼가 없는 집계 GOP, ConOut 변수 부재, topology_scope 거부. | 가장 가까운 별도 토폴로지 보고이나 BIOS와 콘솔 바이너리가 다릅니다. PROM/nvgop 일치만으로 콘솔 Stop을 검증할 수 없습니다. |
| [#75 ASUS B660M / 3090 FE](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/75) | 경로 없는 집계 GOP, framebuffer/info 크기 0, 공유 콘솔, ConOut 부재 및 생성기 거부. | GPU/펌웨어 및 변동 레지스터가 다릅니다. 구조 유사성은 코드나 해제 경로의 동일성이 아닙니다. |
| [#52 ASUS Z690 / RTX3090](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/52#issuecomment-6427173800) | 물리 GOP → combo → 가상 GOP/텍스트 → 시스템 콘솔, 별도 EDID 소유자. IMG17과 같은 파일 GUID. | combo 이미지 0x2160 대 Movian 0x2180. GUID 및 Stop 오프셋 일치가 코드 해시를 대체하지 못합니다. ConOut 부재와 집계 토폴로지 거부는 별도 문제입니다. |
| [#80 MSI Z690 / RTX3060 Ti](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/80) | 트랜잭션 전 준비 거부와 소유권 기대치 불일치. | 물리 GOP/텍스트가 같은 핸들이며 GOP 자식 open이 없습니다. Movian의 GOP 2개 체인과 다릅니다. 공통 경로를 강제로 적용하지 않습니다. |
| [#87 ASUS B760 / ZOTAC3060 GA104](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/87) | 단일 GPU 및 경로 없는 집계 GOP. | GOP+EDID 자식과 EDID 전용 자식 2개가 있어 별도 정리 의무가 있습니다. 현재 단일 멤버십 테스트 범위 밖입니다. 미검토 envelope 거부도 있습니다. |
| [#32 EVGA3090 / ASUS Z790](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/32) | 같은 GPU 제조사/계열 및 Z790, 성공 트랜잭션과 Tensor 벤치마크 보고. | 서브시스템 3842:3982 대 Movian 3842:3387, 보드/BIOS 및 iGPU 출력 경로가 다릅니다. 해당 시스템의 성공이며 Movian용 프로필이 아닙니다. |
| [#4 ASUS B660M / RTX3070 Ti](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/4) | 실기 검증된 집계 콘솔 해제와 Windows 복귀 보고. | GPU와 고정 펌웨어가 다릅니다. 검토된 집계 경로의 가능성을 보여주지만 범용 AMI 지원은 아닙니다. |

## 구현에 대한 결론

GOP 2개는 물리 GPU 2개 대신 물리 GOP와 펌웨어 집계 계층일 수 있습니다. ConOut NVRAM 변수 부재는 시스템 테이블 콘솔 부재를 뜻하지 않습니다. 생성기의 비집계 허용 범위 거부와 실시간 하네스의 프로토콜 조회 실패는 따로 진단해야 합니다.

기존 `internal-boot/boot/prepare-console.c`는 ComboFile, GfxFile 및 텍스트 소유자를 구분하고 정확한 바인딩과 추가 자식/런타임 관계를 검사합니다. Movian의 IMG17/IMG16/IMG13은 큰 구조가 유사하지만 프로필 상수와 전체 해제 순서를 검증 없이 복사하면 안 됩니다.

현재 수정은 매번 ProtocolsPerHandle로 시스템 콘솔 GOP 0개와 텍스트 GUID 1개를 요구하며 다른 GOP/소유권/코드 검사는 유지합니다. 검토한 이슈에서 EVGA03/04의 정확한 현상, 즉 목록에는 GOP가 없는데 HandleProtocol(GOP)이 성공과 포인터를 반환하는 사례는 확인하지 못했습니다. 원인은 미해결입니다. 이 변경이 펌웨어 API 전체의 결함이나 Stop 안전성을 입증하지는 않습니다.

다음 실기 단계는 기존 단일 IMG13 멤버십 Stop입니다. 나머지 실시간 검사와 새 G 입력을 통과한 뒤 관계 변화와 Windows 복귀를 확인합니다. 관련 이슈가 IMG16/17, EDID 또는 부모 Stop 추가의 근거는 아닙니다.

## Ada 논의의 복구 주장 정정

[Yakold의 #54 최신 정정](https://github.com/impacifist/Windows-GPU-UNLOCK/issues/54#issuecomment-6455834943)은 검증된 PciIo 매핑 및 실패 전용 halt/FLR 최종 정리 주장을 철회합니다. 실행 EFI, 쌍 로그 및 원래 ESP 자산은 남아 있지 않고 정리 코드 조각도 실행 바이너리에 연결되지 않습니다. 자산 버전과 소스 식별자는 연구 단서이며 복구 검증이 아닙니다. 이전 주장을 EVGA 복구 위험을 낮추는 근거로 사용하지 않습니다. 이는 별도 아키텍처/트랜잭션 문제이며 현재 콘솔 조회 불일치의 설명도 아닙니다.
