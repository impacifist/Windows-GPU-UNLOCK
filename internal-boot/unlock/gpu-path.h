/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <Uefi.h>

/* Read-bound contract shared by discovery and the LOCAL console backend.
 * ACPI PNP0A03/PNP0A08 root (any captured UID), 1..16 PCI nodes, one ADR,
 * end-entire. The target endpoint remains Pci(0,0); BDF/PCI identity still
 * require independent validation. This does not authorize a new route. */
#define GU_GPU_PATH_MAX_PCI_NODES 16U
#define GU_GPU_DISPLAY_PATH_MAX_BYTES (12U+6U*GU_GPU_PATH_MAX_PCI_NODES+8U+4U)

static inline BOOLEAN GuDecodeGpuDisplayPath(const VOID *Path,UINTN MaxBytes,
                                             UINTN *ControllerBytes,UINTN *PathBytes) {
  const UINT8 *p=(const UINT8 *)Path;
  UINTN at=12,count=0;
  if(ControllerBytes)*ControllerBytes=0;
  if(PathBytes)*PathBytes=0;
  if(!p || MaxBytes<12)return FALSE;
  if(MaxBytes>GU_GPU_DISPLAY_PATH_MAX_BYTES)MaxBytes=GU_GPU_DISPLAY_PATH_MAX_BYTES;
  if(p[0]!=2 || p[1]!=1 || p[2]!=12 || p[3] || p[4]!=0xd0 || p[5]!=0x41 ||
     (p[6]!=3 && p[6]!=8) || p[7]!=0x0a)return FALSE;
  while(MaxBytes-at>=4 && p[at]==1 && p[at+1]==1) {
    if(count==GU_GPU_PATH_MAX_PCI_NODES || MaxBytes-at<6 ||
       p[at+2]!=6 || p[at+3] || p[at+4]>7 || p[at+5]>31)return FALSE;
    count++;at+=6;
  }
  if(!count || p[at-2] || p[at-1] || MaxBytes-at<12)return FALSE;
  if(p[at]!=2 || p[at+1]!=3 || p[at+2]!=8 || p[at+3] ||
     p[at+8]!=0x7f || p[at+9]!=0xff || p[at+10]!=4 || p[at+11])return FALSE;
  if(ControllerBytes)*ControllerBytes=at;
  if(PathBytes)*PathBytes=at+12;
  return TRUE;
}

/* ADR is an output address, not GPU identity. Match all root/PCI ancestry
 * bytes and ADR shape; callers snapshot and pin the complete path separately. */
static inline BOOLEAN GuSameGpuDisplayRoute(const VOID *Left,UINTN LeftBound,
                                            const VOID *Right,UINTN RightBound) {
  UINTN a=0,b=0,an=0,bn=0;
  const UINT8 *x=(const UINT8 *)Left,*y=(const UINT8 *)Right;
  if(!GuDecodeGpuDisplayPath(Left,LeftBound,&a,&an) ||
     !GuDecodeGpuDisplayPath(Right,RightBound,&b,&bn) || a!=b || an!=bn)return FALSE;
  for(UINTN i=0;i<a+4;i++)if(x[i]!=y[i])return FALSE;
  return TRUE;
}
