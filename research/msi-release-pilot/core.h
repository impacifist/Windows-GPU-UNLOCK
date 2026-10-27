/* GPL-2.0-only. No payload, reset or chainload API. */
#pragma once
#include <Uefi.h>
#include "../../internal-boot/boot/prepare-console.h"
EFI_STATUS PilotPreflight(EFI_SYSTEM_TABLE *,EFI_HANDLE,GU_PREPARE_LOG,VOID *);
EFI_STATUS PilotAdvance(UINTN);
EFI_STATUS PilotVerify(UINTN);
BOOLEAN PilotInputStable(VOID);
EFI_STATUS PilotEvidence(VOID);
EFI_STATUS PilotConfirm(VOID);
VOID PilotEnd(VOID);
EFI_STATUS PilotVerifyAt(UINTN,const char *);
VOID PilotReportFailure(GU_PREPARE_LOG,VOID *,EFI_STATUS);
