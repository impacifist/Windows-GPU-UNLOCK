/* GPL-2.0-only. Probe interface; CAPTURE layout must match diagnostic.c. */
#pragma once
#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>
#include "../unlock/machine-profile.h"
#include "diagnostic.h"
EFI_STATUS GuProbeGpu(EFI_SYSTEM_TABLE *t,CAPTURE *c);

