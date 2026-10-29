/* GPL-2.0-only. Probe interface; CAPTURE layout must match diagnostic.c. */
#pragma once
#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>
#include "../unlock/machine-profile.h"
#include "diagnostic.h"
EFI_STATUS GuProbeGpu(EFI_SYSTEM_TABLE *t,CAPTURE *c);
#if defined(GU_AD104_EDID_TEST) || defined(GU_AD104_TEXT_TEST)
EFI_STATUS GuEdidExperiment(EFI_SYSTEM_TABLE *t,EFI_FILE_PROTOCOL *log);
#ifdef GU_AD104_PROM_FULL_TEST
EFI_STATUS GuEdidExperimentOnVolume(EFI_SYSTEM_TABLE *t,EFI_FILE_PROTOCOL *log,EFI_FILE_PROTOCOL *root,EFI_FILE_PROTOCOL *witness);
#endif
#endif


#ifdef GU_AD104_TEXT_TEST
extern BOOLEAN GuTextOutputDisabled;
#endif
