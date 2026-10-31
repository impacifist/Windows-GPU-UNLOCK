# Visible shared ESP audit test / 공유 ESP 표시 감사 테스트

This is an observation-only EFI handoff test for the pinned EVGA Windows ESP. It performs no GPU access, unlock, EFI writes at runtime, or firmware entry changes. Staging writes one new internal ESP file and replaces the USB fallback loader only after preserving its backup. Firmware execution can still hang or fail to boot; retain the normal Windows boot option and a recovery USB. Secure Boot may reject the unsigned EFI; do not change firmware security settings for this test.

이 테스트는 지정된 EVGA Windows ESP에서 EFI 실행 및 Windows 전달만 확인합니다. GPU 접근이나 잠금 해제를 하지 않으며 실행 중 ESP 또는 펌웨어 설정을 변경하지 않습니다. 준비 단계에서 내부 ESP에 새 파일 하나를 추가하고 USB 부트 파일은 백업 후 교체합니다. 실행이 멈추거나 부팅에 실패할 수 있으므로 일반 Windows 부팅 항목과 복구 USB를 유지하십시오. Secure Boot가 서명되지 않은 EFI를 거부할 수 있습니다. 이 테스트를 위해 보안 설정을 변경하지 마십시오.

Run from the repository root in PowerShell. Confirm the partition GUID and USB drive against local inventory; the GUID below is the pinned contributor ESP, and `X` is a placeholder for the USB. Staging requires administrator access. Never use a Windows/internal drive for `-Drive`.

저장소 루트의 PowerShell에서 실행하십시오. 로컬 디스크 목록에서 ESP GUID와 USB 드라이브 문자를 확인하십시오. 아래 GUID는 지정된 기여자 ESP이며 `X`는 실제 USB 문자로 바꿔야 합니다. 준비 작업에는 관리자 권한이 필요합니다. 내부/Windows 드라이브를 지정하지 마십시오.

```powershell
$esp = [guid]'c36c3e86-02aa-4f76-af1b-96e327f9d1c1'
./research/profile/build-shared-esp-launcher.ps1 -WindowsEspGuid $esp
./research/profile/test-shared-esp-audit.ps1
./research/profile/test-shared-esp-launcher.ps1
./research/profile/test-stage-shared-esp-audit.ps1
./research/profile/test-stage-shared-esp-launcher.ps1
./research/profile/stage-shared-esp-audit.ps1 -WindowsEspGuid $esp
./research/profile/stage-shared-esp-launcher.ps1 -WindowsEspGuid $esp -Drive X -ReuseExisting
```

Internal target: `EFI/EVGA-SHARED-AUDIT-VISIBLE.EFI`. Existing audit files are preserved, and an existing visible target is refused. Launcher staging checks the exact new audit hash from its build manifest before touching the USB. Keep both manifests and record the USB backup SHA256 before boot. An existing backup with different content causes refusal; preserve it and use a separate USB rather than deleting it.

내부 대상은 `EFI/EVGA-SHARED-AUDIT-VISIBLE.EFI`입니다. 기존 감사 파일을 보존하며 새 대상도 이미 있으면 중단합니다. USB 변경 전에 빌드 매니페스트의 새 감사 파일 해시를 검증합니다. 매니페스트 두 개를 보관하고 부팅 전에 USB 백업 SHA256을 기록하십시오. 기존 백업이 다르면 중단하므로 삭제하지 말고 별도 USB를 사용하십시오.

Choose the USB once in the firmware boot menu and record video through Windows login. Expected sequence: `AUDIT LOADED FROM PINNED ESP` (10 seconds), then `AUDIT ENTERED ON PINNED ESP` (10 seconds), then Windows Boot Manager. The first message proves only loading; the second proves audit entry on the pinned ESP. Neither proves GPU compatibility or unlock. Missing output is inconclusive because firmware console routing may hide it. Report the exact last visible message, delays, errors, and whether Windows login completed.

펌웨어 일회성 부팅 메뉴에서 USB를 선택하고 Windows 로그인까지 촬영하십시오. 예상 순서는 `AUDIT LOADED FROM PINNED ESP` (10초), `AUDIT ENTERED ON PINNED ESP` (10초), Windows Boot Manager입니다. 첫 문구는 로드, 둘째 문구는 지정 ESP에서 감사 진입만 입증합니다. GPU 호환성이나 잠금 해제 증거가 아닙니다. 화면이 없으면 펌웨어 콘솔 경로 문제일 수 있으므로 결론을 내릴 수 없습니다. 마지막 문구, 대기 시간, 오류 및 Windows 로그인 완료 여부를 알려주십시오.

Restore the USB using its recorded original hash:

기록한 원본 해시로 USB를 복원하십시오:

```powershell
./research/profile/restore-shared-esp-launcher.ps1 -Drive X -OriginalSha256 '<recorded backup SHA256>'
```

Local PE verification and mocked tests cover admission, visible delay, exact path, staging hash refusal, no overwrite, USB backup and restore. They do not establish physical firmware execution or Windows boot success.

로컬 PE 검증과 모의 테스트는 ESP 선택, 표시 지연, 정확한 경로, 해시 거부, 덮어쓰기 방지 및 USB 백업/복원을 확인합니다. 실제 펌웨어 실행이나 Windows 부팅 성공은 아직 검증하지 않았습니다.
