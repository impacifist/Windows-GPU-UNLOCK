# 30차 결과: BCR 거부와 의도된 중지

[English](AD104-THIRTIETH-RESULT.en.md) | [한국어](AD104-THIRTIETH-RESULT.ko.md)

2026-10-27 실제30차는 기존 레지스터/PROM/RAM 검사를 마친 뒤 선택 읽기9회를 시도했다. GSP ENGINE0x1103c0은0, BCR_CTRL0x111668은 콜백 상태0/값0xBADF5620이었다.9회 전송 상태는 모두0이었다. 기존 데이터 검사가 EFI_NOT_READY를 반환했고 이후 로그는 manual-power-off-required-no-key-can-resume/handoff-stopped를 명시한다. STATUS도 handoff-stopped-manual-power-off를 기록한다. 이는 MMIO 호출 자체의 멈춤 증거가 아니라 읽기 반환 후 의도된 소프트웨어 중지이다.

사용자는 멈췄던 것으로 기억하지만 마지막 실행은 확실하지 않다. 로그로30차 바이너리와 중지 경로는 확인되지만 정확한 수동 복구 순서는 미확인이다. 두 번째 선택 배치, 새V, 디코딩 해제, Windows StartImage는 도달하지 않았다. 추가9회/총262329회 Mem.Read였다. 리셋, 코어 전환, GPU 제어 쓰기, 펌웨어 업로드/실행, 잠금 해제는 없었다. 중지 전 디코딩을 명시적으로 해제하지 않았다.

원본36개 파일을 비공개로 복사하고 해시를 검증했다. USB 원본은 검증된 다음 스테이징까지 유지한다. DIAG427897바이트 SHA256084bfa4e0c94c651b48523cc485acb454dda03d84c7af0778eb688f7d7a77a83. EFI SHA256d49aa002a509404d9bd8495005d8f022d977be5d315fabb0270b860ad9da349e. PROM은 accf2a054de0ec394078f933f74356ed7c4057ee0bd1ce6734cacda54ca9e1e1로 동일하다.

DMACTL(25차), RISC-V CPUCTL(28/29차), BCR(30차)은 전송 성공에도 BADF5620을 반환했다. 공통 원인, 정확한 권한 수준, 무해함은 입증하지 못했다. 별도 승인된29차 CPUCTL 복구가 일반 BCR 복구 권한은 아니다. 거부 데이터를 코어 상태로 인정하거나 해당 레지스터를 재시도하지 않는다.

다음31차는 거부된 세 주소를 모두 제외하고 기존 엔진/기능 관측을 유지하며 이미 측정한 실제 FB/VGA/MMU/SS1 입력을 CPU에서만 감사한다. 참조 정책 차이를 설명하지만 검사를 완화하지 않는다. 전체 배치는 해시로 고정하고 파싱한 펌웨어 자산이 필요하며 파일 크기는 파싱된 이미지 크기 대신 쓸 수 없다. 체크리스트15/26(58%)는 유지한다. [31차](AD104-THIRTY-FIRST-BOOT.ko.md).
