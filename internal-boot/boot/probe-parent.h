/* GPL-2.0-only. Supplementary read-only parent PCI observations; never approval. */
#pragma once
#include "probe-pin.h"
#include "../../research/pci-decode/efi-capture.h"
EFI_STATUS GuProbeParentBaseline(CAPTURE *Log,GU_DS_CAPTURE *Observation,EFI_HANDLE Handle,
  EFI_PCI_IO_PROTOCOL *Io,const UINT32 *Config,UINTN Segment,UINTN Bus);
