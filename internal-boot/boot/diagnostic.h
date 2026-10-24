/* GPL-2.0-only. Observe the UEFI handle database; never detach a device. */
#pragma once
#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>
typedef struct CAPTURE_S {
  EFI_SYSTEM_TABLE *Table;
  EFI_FILE_PROTOCOL *File;
  EFI_STATUS Io;
  BOOLEAN Complete;
  UINTN Used,Total;
  char Buffer[4096];
} CAPTURE;
EFI_STATUS GuCaptureGraph(EFI_SYSTEM_TABLE *Table, EFI_FILE_PROTOCOL *Log);
