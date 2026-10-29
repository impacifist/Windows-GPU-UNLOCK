# 스물여덟 번째 결과: RISCV CPUCTL 센티널과 수동 복구

[English](AD104-TWENTY-EIGHTH-RESULT.en.md) | [한국어](AD104-TWENTY-EIGHTH-RESULT.ko.md)

2026-10-27 테스트28은 기존 PROM/RAM 검사를 완료하고 첫14개 배치에서9회 읽었습니다. 처음7개는27과 같은 chip194000a1, FalconCPUCTL10, 메일박스0/0, IRQ10, HWCFG247b7, HWCFG80420100입니다. 새 GSP ENGINE0x1103c0은0, RISCV CPUCTL0x111388은0xbadf5620을 반환했으며9개 콜백 상태는 모두0입니다.

유지된 BADF 검사가 EFI_NOT_READY(0x8000000000000006)를 반환했습니다. 이후 experiment-result/manual-power-off-required-no-key-can-resume/handoff-stopped와 STATUS의 수동 전원 차단 정지가 기록됐습니다. 읽기가 반환된 뒤 보호 정지한 증거이며 CPU/MMIO 호출 자체가 멈췄다는 증거는 아닙니다. BCR_CTRL0x111668, 첫 배치 나머지, 두 번째 배치, Shift+V, 디코딩 해제, Windows StartImage에 도달하지 않았습니다. 추가28회 중9회(전체262329회)만 읽었고 정지 전에 디코딩을 명시적으로 해제하지 않았습니다.

사용자는 강제 종료 후 Windows 수동 부팅을 보고했으므로 자동 복구는 실패했습니다. 이후 RTX4070Ti/드라이버610.88을 감지했으나 계산/부하 검증은 없었습니다. ENGINE0만으로 준비 상태를 판단할 수 없고 거부된 CPUCTL은 active/halted 상태를 알려주지 않습니다. BCR/코어 선택은 미관측이며 제어/리셋/코어전환/시작·중지/쓰기/업로드/실행/언락은 없습니다.

36개 파일 원본/복사본 SHA256을 검증했습니다. DIAG429249바이트 SHA2567a454d2ce9f4053f4cb2ff443d0c439fc156960d23da101f1ff38d6dfdb840d9. EFI는5cb2e6d75b1ff7dda0872b903077bd06349544eec9ae3999f4fdf65fde3a0316와 같습니다. USB 원본/출력은 유지되며 다시 실행하지 마십시오. 이번 업데이트에서 새EFI는 스테이징하지 않았습니다.

25의 DMACTL 및28의 RISCV CPUCTL 거부는 다음 시도 전에 레지스터 접근 의미의 오프라인 조사가 필요함을 보여줍니다. 값을 정상 상태로 허용하거나 검사를 완화하지 않았으며 공통 원인은 입증되지 않았습니다. 정확한 장치 진입, SS1/VGA 정책, 펌웨어 실행은 미해결이고 체크리스트15/26(58%) 유지입니다.
