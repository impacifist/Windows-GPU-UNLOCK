/* GPL-2.0-only. Read-only binding data shared by capture and research adapter. */
#pragma once
#include "snapshot.h"
#include <Protocol/PciIo.h>
#include <Protocol/DevicePath.h>
#define GU_DS_EFI_MAX_PATH 112U
#define GU_DS_EFI_MAX_PARENTS 15U

typedef struct {
  EFI_HANDLE Handle;
  EFI_PCI_IO_PROTOCOL *Io;
  UINTN PathBytes;
  UINT8 Path[GU_DS_EFI_MAX_PATH];
} GU_DS_EFI_BINDING;
typedef struct {
  GU_DS_EFI_BINDING Pin;
  EFI_PCI_IO_PROTOCOL_GET_LOCATION Location;
  EFI_PCI_IO_PROTOCOL_CONFIG Read;
  EFI_PCI_IO_PROTOCOL_ATTRIBUTES Attributes;
} GU_DS_EFI_DEVICE;
