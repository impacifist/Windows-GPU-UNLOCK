/* GPL-2.0-only. Pure policy: restore ONLY missing original IO/MEM bits. */
#pragma once
#include "../unlock/machine-profile.h"
#include <Uefi.h>
#include <Protocol/PciIo.h>
static inline BOOLEAN GuDecodeRestoreMask(const UINT32 *before,const UINT32 *after,UINT64 *mask) {
  if(!before || !after || !mask)return FALSE;
  *mask=0;
  if(before[0]!=GU_BOARD_ID || before[11]!=GU_BOARD_SUBSYSTEM || !(before[1]&2) || (before[1]&4) || (after[1]&4))return FALSE;
  UINT32 missing=before[1]&~after[1]&3;
  for(UINTN i=0;i<64;i++)if((after[i]|(i==1?missing:0))!=before[i])return FALSE;
  *mask=(missing&1?EFI_PCI_IO_ATTRIBUTE_IO:0)|(missing&2?EFI_PCI_IO_ATTRIBUTE_MEMORY:0);
  return TRUE;
}
