/* Development adapter only. Not yet linked into an EFI package. */
#pragma once
#include "port.h"
typedef BOOLEAN (*GU_EVGA_LOG_READY)(VOID);
/* Callback must verify the released EVGA console graph and, when Owned is
 * true, exactly one exclusive PCI open for Image with no other active owner. */
typedef BOOLEAN (*GU_EVGA_GRAPH_CHECK)(EFI_HANDLE Parent,EFI_HANDLE Image,BOOLEAN Owned);
#if defined(GU_EVGA_TEST_ONLY)
VOID GuEvgaTestReset(VOID);
#endif
/* Caller must have completed the pinned EVGA release and enabled MEMORY only.
 * Caller must hold on every error: this function never resumes Windows. Assets
 * must already be loaded in GuAssets using the shared verified reader. */
EFI_STATUS GuEvgaTransaction(EFI_SYSTEM_TABLE *Table,EFI_HANDLE Image,EFI_HANDLE Parent,
 EFI_PCI_IO_PROTOCOL *Pci,GU_EVGA_GRAPH_CHECK GraphCheck,GU_EVGA_LOG_READY LogReady);
