/* GPL-2.0-only. Callback-free capability pin for read-only probe observations. */
#pragma once
#include "diagnostic.h"
#include <Protocol/PciIo.h>
typedef struct {
  EFI_SYSTEM_TABLE *Table;
  EFI_BOOT_SERVICES *Boot;
  EFI_HANDLE_PROTOCOL Handle;
  EFI_LOCATE_HANDLE_BUFFER Enumerate;
  EFI_LOCATE_DEVICE_PATH Locate;
  EFI_ALLOCATE_POOL Allocate;
  EFI_FREE_POOL Free;
  EFI_PCI_IO_PROTOCOL *Io;
  EFI_PCI_IO_PROTOCOL_GET_LOCATION Location;
  EFI_PCI_IO_PROTOCOL_CONFIG Read;
  EFI_PCI_IO_PROTOCOL_IO_MEM Mem;
  EFI_PCI_IO_PROTOCOL_GET_BAR_ATTRIBUTES Bar;
  EFI_PCI_IO_PROTOCOL_ATTRIBUTES Attributes;
} GU_PROBE_PIN;
static inline BOOLEAN GuProbeTakePin(CAPTURE *c,EFI_PCI_IO_PROTOCOL *io,GU_PROBE_PIN *p) {
  if(!c || !c->Table || !c->Table->BootServices || !io || !p)return FALSE;
  EFI_BOOT_SERVICES *b=c->Table->BootServices;
  *p=(GU_PROBE_PIN){c->Table,b,b->HandleProtocol,b->LocateHandleBuffer,b->LocateDevicePath,b->AllocatePool,b->FreePool,
    io,io->GetLocation,io->Pci.Read,io->Mem.Read,io->GetBarAttributes,io->Attributes};
  return p->Handle && p->Enumerate && p->Allocate && p->Free && p->Location && p->Read && p->Mem && p->Bar;
}
static inline BOOLEAN GuProbePinSame(const CAPTURE *c,const GU_PROBE_PIN *p) {
  return c->Table==p->Table && c->Table->BootServices==p->Boot && p->Boot->HandleProtocol==p->Handle &&
    p->Boot->LocateHandleBuffer==p->Enumerate && p->Boot->LocateDevicePath==p->Locate &&
    p->Boot->AllocatePool==p->Allocate && p->Boot->FreePool==p->Free && p->Io->GetLocation==p->Location &&
    p->Io->Pci.Read==p->Read && p->Io->Mem.Read==p->Mem && p->Io->GetBarAttributes==p->Bar && p->Io->Attributes==p->Attributes;
}
