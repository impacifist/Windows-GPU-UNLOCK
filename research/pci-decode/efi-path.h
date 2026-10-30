/* GPL-2.0-only. Bounded controller paths shared by the research libraries. */
#pragma once
#include "efi-binding.h"
#include "../../internal-boot/unlock/gpu-path.h"

static inline BOOLEAN GuDsControllerParents(const UINT8 *path,UINTN bytes,UINTN *parents) {
  static const UINT8 end[]={0x7f,0xff,4,0};
  UINT8 display[GU_GPU_DISPLAY_PATH_MAX_BYTES];UINTN controller=0,total=0;
  if(parents)*parents=0;
  if(!path || !parents || bytes<4 || bytes>GU_DS_EFI_MAX_PATH)return FALSE;
  for(UINTN i=0;i<4;i++)if(path[bytes-4+i]!=end[i])return FALSE;
  UINTN body=bytes-4;
  if(body<24 || body+12>sizeof(display) || (body-12)%6)return FALSE;
  for(UINTN i=0;i<body;i++)display[i]=path[i];
  for(UINTN i=0;i<12;i++)display[body+i]=0;
  display[body]=2;display[body+1]=3;display[body+2]=8;
  for(UINTN i=0;i<4;i++)display[body+8+i]=end[i];
  if(!GuDecodeGpuDisplayPath(display,body+12,&controller,&total) || controller!=body || total!=body+12)return FALSE;
  *parents=(body-12)/6-1;
  return *parents>=1 && *parents<=GU_DS_EFI_MAX_PARENTS;
}
