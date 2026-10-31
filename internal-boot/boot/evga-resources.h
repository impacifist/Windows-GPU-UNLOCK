#pragma once
#include "evga-text-stop.h"
#include <Protocol/PciIo.h>
EFI_STATUS GuEvgaResourcesPrepare(EFI_SYSTEM_TABLE *,EFI_FILE_PROTOCOL *,EFI_HANDLE,GU_EVGA_RECORD);
#ifdef GU_EVGA_UNLOCK
EFI_HANDLE GuEvgaResourcesImage(VOID);
BOOLEAN GuEvgaResourcesLogReady(VOID);
EFI_STATUS GuEvgaResourcesRelease(EFI_SYSTEM_TABLE *);
#endif
EFI_STATUS GuEvgaResourcesClaim(EFI_SYSTEM_TABLE *,EFI_HANDLE,EFI_PCI_IO_PROTOCOL *,GU_EVGA_RECORD);
