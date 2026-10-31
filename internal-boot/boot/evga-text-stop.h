/* GPL-2.0-only. Exact captured console membership experiment; no unlock. */
#pragma once
#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>
typedef EFI_STATUS (*GU_EVGA_RECORD)(VOID *,const char *,EFI_STATUS);
EFI_STATUS GuEvgaTextStop(EFI_SYSTEM_TABLE *,EFI_FILE_PROTOCOL *,GU_EVGA_RECORD,BOOLEAN *);
EFI_STATUS GuEvgaTextStopVerify(EFI_SYSTEM_TABLE *);
