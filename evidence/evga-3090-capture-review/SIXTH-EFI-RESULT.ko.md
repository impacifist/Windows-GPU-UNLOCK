# 여섯 번째 EFI 결과: 첫 IMG13 멤버십 Stop 완료

참가자는 프롬프트가 유지되었고 새 Shift+G 후 화면이 사라진 뒤 Windows로 자동 부팅했다고 보고했습니다. 새 ARMED 파일도 생성되었습니다. EVGA05.TXT는 821126바이트이며 SHA256은 cbbfad6aadb858cdc6311f656c819e6707e52f274e8e0959df4348c51fbbec1a입니다.

로그에는 Stop 성공 1회, 사후 그래프 검사 성공, 완전한 캡처 2개 및 ready-windows가 있습니다. Windows 도착은 참가자 보고로 확인하며 ready-windows만으로 단정하지 않습니다. 호출 전후 소유권 및 코드 검사를 통과했습니다.

전체 기록 그래프의 독립 오프라인 비교에서 선택한 IMG13 텍스트 open 2개가 제거되었고 GOP 핸들 2개와 다른 모든 활성 open은 유지되었습니다. 핸들 추가/제거는 없습니다. 가상 GOP에서 추가 프로토콜 GUID d3b36f2c-d551-11d4-9a46-0090273fc14d가 사라졌습니다. 여기서 그 의미나 원인은 검증하지 않았습니다. 따라서 효과가 open 2개 제거에만 한정된다고 주장하면 안 됩니다. 다음 단계 검토에 포함해야 합니다.

하네스는 IMG16/17, EDID, NVIDIA Stop이나 GPU MMIO, PCI 속성 변경, 펌웨어 실행, unlock을 하지 않았습니다. 한 시스템의 단일 멤버십 테스트이며 완전한 해제, unlock, 반복 신뢰성 또는 일반 복구 검증은 아닙니다.

## 다음 테스트: 저장 결과 재검증

GitHub Desktop으로 업데이트한 뒤 저장 로그만 읽는 명령을 실행하십시오.

```powershell
python -I research/profile/inspect-evga3090-stop-result.py "D:\RTX3090-UNLOCK\EVGA05.TXT"
```

JSON을 보내십시오. ARMED 마커 및 백업을 보존하고 EFI를 반복하거나 마커를 삭제하지 마십시오. 다음 하드웨어 단계 전 IMG16과 추가 프로토콜 제거 효과를 검토합니다. 이 오프라인 결과는 새 EFI 단계 승인이 아닙니다.

실제 로그 검사가 통과했고 성공/완전성 증거를 제거한 변형 3개가 거부되었습니다. [정리된 결과](sixth-efi-run-result.json).
