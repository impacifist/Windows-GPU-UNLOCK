# 후속 풀 리퀘스트 설명

[English](AD104-FOLLOWUP-PR.en.md) | 한국어

제목: 보호 조건을 갖춘 MSI AD104 캡처 및 진단 전용 MBR 인계 추가

대상: 업스트림 `main`. 소스: `feature/4070ti-capture-preparation`.
공개 PR: [#44](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/44).

관련 연구: [#26](https://github.com/impacifist/Windows-GPU-UNLOCK/pull/26).
이 브랜치는 해당 연구의 후속 작업이지만 기존 기본 상태 GPU 도구에 의존하지
않습니다.

범용 진단은 이 MSI AD104 ID에 대해 심층 관찰을 수집할 수 없었고, GPT 전용
부팅 볼륨 가정은 이 UEFI/MBR Windows 설치와 맞지 않았습니다. 이 변경은 보호
조건을 갖춘 캡처 전용 AD104 모드와 진단 전용 MBR 인계를 추가합니다. 현재
시스템 파티션을 고정하고 `LoadImage`에 전달할 정확한 부팅 관리자 바이트의
해시를 확인합니다. 언락 프로필을 추가하지 않으며 지원 보드의 허용 조건도
변경하지 않습니다.

첫 실제 부팅에서 예상한 4070 Ti를 발견하고 프로토콜 그래프와 드라이버 이미지
스냅샷 32개를 보존했습니다. PCI command `0007`은 버스 마스터링 비활성화
조건을 만족하지 않아 AD104 MMIO/PROM 읽기 전에 거부되었습니다. 캡처는
불완전하며 언락 성공이나 Windows 자동 복귀를 주장하지 않습니다.
[첫 부팅 결과](AD104-FIRST-BOOT.ko.md)와
[증거 요약](ad104-first-boot-evidence.json)을 참고하십시오.

구현 검증 기록: RAM 전용 AD104 검사 148개, MBR 검사 66개, GPT 회귀 검사
4개, 거부된 컴파일 조합 16개, 프로필 Python 테스트 12개 및 네이티브 AMD64
EFI PE 검증입니다. 실제 증거는 비공개로 보관했으며 USB/복사본 해시와 덤프
크기가 일치했습니다. 원본 펌웨어, 파티션 고정값 및 생성 바이너리는 제외합니다.

기존 #26 설명은 부팅 전 실행을 하지 않았던 당시 연구 범위에 관한 것입니다.
이 후속 작업은 이후의 실제 진단 증거를 추가합니다. 기존 기본 상태 GPU
검사가 언락을 입증한 것처럼 해석해서는 안 됩니다.
