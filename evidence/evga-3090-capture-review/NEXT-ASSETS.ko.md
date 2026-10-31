# Windows에서 EVGA 펌웨어 자산 준비

[English](NEXT-ASSETS.en.md) | [한국어](NEXT-ASSETS.ko.md)

하드웨어 상태/PROM캡처는 완료되었습니다. EFI재부팅 없이 다음 런타임통합에 필요한 로컬 파일5개를 준비합니다. GitHub Desktop에서 브랜치를 업데이트하고 저장소 루트에서 실행합니다.

```powershell
py -3.14 research/profile/prepare-evga3090-assets.py
```

Git과 Python3.14이상이 필요합니다. 고정 NVIDIA소스와 nvidia-utils패키지를 다운로드하고 입력해시를 검증하며 출력5개의 생성/추출 후 저장소 크기/해시를 검증합니다. 고정 업스트림 리비전0ebdb1c6308b6a2072be5bf04513472942541956을 사용합니다. 외부 zstd프로세스만 Windows Python네이티브 tar/zstd처리로 대체하고 고정 멤버/입출력검증 및 페이로드생성은 유지합니다. 설치/모듈로드, GPU접근, EFI배치, USB/부팅변경 또는 재부팅은 없습니다. 다운로드와 디스크공간이 필요합니다.

기본 소스/캐시/출력은 Git에서 제외되는 research/profile/build/evga-assets/입니다. 비어있지 않은 출력은 덮어쓰지 않습니다. 실패 시 부분출력을 보존하고 오류를 보내세요. 기존 출력은 --verify-only로 검사합니다. NVIDIA-LICENSE/NOTICE.md를 보존하고 사용 전 검토하세요. 콘솔JSON/오류 또는 verified-assets.json만 보내고 펌웨어5개는 업로드하지 마세요. 벤치마크 재빌드는 필요 없습니다.

로컬에서5개 생성과 정확한 매니페스트 검증, 별도 verify-only, 기존출력 보존 및 손상자산 거부를 확인했습니다. 이는 출처/무결성 검증이며 GPU펌웨어수락/잠금해제 입증이 아닙니다. 기여자의 로컬 준비가 필요하며 PR로 펌웨어를 재배포하지 않습니다.

원본 생성기는 외부 zstd가 없어 실패했지만 Python3.14네이티브 어댑터로 성공했습니다. 업스트림소스는 수정하지 않았습니다. [고정 업스트림 안내](https://github.com/Zagitta/gpu-unlock/tree/0ebdb1c6308b6a2072be5bf04513472942541956/firmware). 잠금해제 EFI제공 전 정확한 EVGA식별/토폴로지/진입선택자와 실패격리를 런타임에 통합해야 합니다.
