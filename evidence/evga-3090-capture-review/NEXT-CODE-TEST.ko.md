# 다음 테스트: 저장된 콘솔 드라이버 코드 지문

[English](NEXT-CODE-TEST.en.md) | [한국어](NEXT-CODE-TEST.ko.md)

두 캡처의 콘솔 연결 검사 9개가 통과했습니다. 이번 검사는 실제 BY_DRIVER 소유자를 따라 저장된 이미지 덤프를 선택합니다. AMD64 PE 구조, 범위, Stop 함수가 포함된 실행 섹션을 확인하고 해당 섹션 내 재배치를 거부한 뒤 코드 해시, Stop RVA, 버전, 덤프 이름을 비교합니다. 같은 펌웨어 식별자를 여러 바인딩이 사용할 수 있으므로 소유 관계로 인스턴스를 선택합니다.

업데이트된 PR96 브랜치의 저장소 루트에서 실행하세요.

```powershell
python -I research/profile/inspect-evga3090-console-code.py "D:\RTX3090-UNLOCK"
```

예상 결과는 console_code_matches_across_captures=true이며 hardware_accessed, write_eligible, unlock_proven은 모두 false입니다. 전체 JSON 출력을 보내주세요. 누락되거나 잘못된 로그/이미지, 모호한 소유 관계, 선택 섹션 내 재배치 또는 지문 차이는 실패로 처리합니다. 실패하면 파일을 보존하고 프로필을 강제하지 마세요.

업로드된 두 캡처는 로컬에서 통과했습니다. 그래픽 콘솔 IMG17의 Stop RVA는 0x88c, 해시 대상은 6544바이트입니다. 텍스트 콘솔 IMG13은 0x104c, 22048바이트입니다. 정확한 해시는 [저장 결과](console-code-test-results.json)에 있습니다. 이는 저장된 실행 코드의 재현성을 확인할 뿐 출처 인증이나 Stop 동작의 안전성을 검증하지 않습니다. 두 로그는 동일한 IMG 파일을 사용하므로 부팅 사이에 코드가 변하지 않았다는 독립적 증거는 아닙니다.

재부팅, 새 EFI, BIOS 변경, 펌웨어 실행, GPU 접근은 필요 없습니다. 저장된 파일을 읽고 JSON만 출력합니다. 기존 토폴로지 허용 거부는 그대로 유지합니다. 다음 단계는 정확한 콘솔 구현의 정적 분석과 오프라인 수명주기 모델 검토이며, 이후에 새 하드웨어 테스트를 제안합니다.
