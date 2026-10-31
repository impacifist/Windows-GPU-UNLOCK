# EVGA19 잠금 해제 트랜잭션 결과

[English](EVGA19-RESULT.en.md) | [한국어](EVGA19-RESULT.ko.md)

Movian은 Shift+G 후 화면이 몇 번 깜박이고 Windows가 자동으로 시작됐다고 보고했습니다. 제공된 EVGA19 로그에는 승인, 펌웨어 실행, PCIe FLR 전후 SS0 `0x88888888` 및 SS1 `0x8`, 오류가 기록되지 않은 트랜잭션 결과 0, 정지된 리소스 및 남은 매핑 없음이 기록됐습니다. 정리와 Windows 준비 상태도 성공했습니다. FLR 후 SS1 값은 로그 두 줄에 걸쳐 있습니다.

이는 해당 시스템에서 기록된 트랜잭션 성공과 사용자가 보고한 Windows 복귀입니다. 성능 향상이나 부팅 후 GPU 정상 동작을 독립적으로 증명하지는 않습니다. 다음 단계는 가능하면 같은 Windows 세션에서 [기준 측정](BENCHMARK-BASELINE.ko.md)과 동일한 실행 파일과 설정으로 벤치마크를 실행하는 것입니다.

원본 로그는 공개하지 않습니다. SHA-256: `dfd6a0e0e58b8bc794e87a6c1b0e774ac674d5b483985ba36439b53fbc597537`.
