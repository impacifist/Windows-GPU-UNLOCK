/* SPDX-License-Identifier: GPL-2.0-only */
/* Bounded path parser only: no EFI services, devices or driver execution. */
#include "../gpu-path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned Checks;
static void Check(BOOLEAN good,const char *why) {
  Checks++;if(!good){fprintf(stderr,"FAIL GPU path: %s\n",why);exit(1);}
}
static UINTN Make(UINT8 *p,unsigned nodes,unsigned uid,BOOLEAN express) {
  memset(p,0,256);memcpy(p,(UINT8[]){2,1,12,0,0xd0,0x41,3,0x0a},8);
  if(express)p[6]=8;
  p[8]=(UINT8)uid;p[9]=(UINT8)(uid>>8);p[10]=(UINT8)(uid>>16);p[11]=(UINT8)(uid>>24);
  UINTN at=12;
  for(unsigned i=0;i<nodes;i++,at+=6) {
    memcpy(p+at,(UINT8[]){1,1,6,0,0,0},6);
    if(i+1<nodes){p[at+4]=(UINT8)(i%8);p[at+5]=(UINT8)(31-i%32);}
  }
  memcpy(p+at,(UINT8[]){2,3,8,0,0x70,3,1,0x80,0x7f,0xff,4,0},12);
  return at+12;
}
static void Refuse(const UINT8 *p,UINTN bound,const char *why) {
  UINTN controller=123,total=456;
  Check(!GuDecodeGpuDisplayPath(p,bound,&controller,&total),why);
  Check(!controller && !total,"failed parse clears output offsets");
}
int main(void) {
  UINT8 path[256],other[256];UINTN controller,total;
  for(unsigned nodes=1;nodes<=16;nodes++)for(unsigned express=0;express<=1;express++) {
    UINTN size=Make(path,nodes,0xfedcba98,express!=0);
    Check(GuDecodeGpuDisplayPath(path,size,&controller,&total),"bounded PCI chain with captured UID accepted");
    Check(controller==12+6*nodes && total==size,"exact controller consumption and total byte count");
    Check(GuDecodeGpuDisplayPath(path,sizeof(path),NULL,NULL),"read bound may extend past end-entire");
    for(UINTN short_size=0;short_size<size;short_size++)Refuse(path,short_size,"every truncated path rejected");
    memcpy(other,path,size);other[controller+4]^=1;
    Check(GuSameGpuDisplayRoute(path,size,other,size),"same GPU ancestry with another output ADR accepted");
    other[8]^=1;
    Check(!GuSameGpuDisplayRoute(path,size,other,size),"different captured root UID is not same route");
  }
  UINTN size=Make(path,3,0,FALSE);
  const unsigned hops[]={16,17,22,23};
  for(UINTN i=0;i<sizeof(hops)/sizeof(hops[0]);i++) {
    memcpy(other,path,size);other[hops[i]]^=1;
    Check(GuDecodeGpuDisplayPath(other,size,NULL,NULL),"mutated intermediate hop is still structurally legal");
    Check(!GuSameGpuDisplayRoute(path,size,other,size),"every captured intermediate PCI hop remains pinned");
  }
  memcpy(other,path,size);other[6]=8;
  Check(GuDecodeGpuDisplayPath(other,size,NULL,NULL),"alternative root HID remains structurally legal");
  Check(!GuSameGpuDisplayRoute(path,size,other,size),"captured root HID remains pinned");
  const unsigned positions[]={0,1,2,3,4,5,6,7,12,13,14,15,18,19,20,21,24,25,26,27,28,29,30,31,32,33,38,39,40,41};
  for(UINTN i=0;i<sizeof(positions)/sizeof(positions[0]);i++) {
    memcpy(other,path,size);other[positions[i]]^=0x40;
    Refuse(other,size,"malformed root, PCI, endpoint, ADR or end rejected");
    Check(!GuSameGpuDisplayRoute(path,size,other,size),"malformed route never compares equal");
  }
  memcpy(other,path,size);other[16]=8;Refuse(other,size,"PCI function above 7 rejected");
  memcpy(other,path,size);other[17]=32;Refuse(other,size,"PCI device above 31 rejected");
  memcpy(other,path,size);other[28]=1;Refuse(other,size,"target endpoint function remains zero");
  memcpy(other,path,size);other[29]=1;Refuse(other,size,"target endpoint device remains zero");
  Refuse(path,0,"zero bound rejected");Refuse(NULL,256,"null path rejected");
  Refuse(path,Make(path,0,0,FALSE),"root without PCI node rejected");
  Refuse(path,Make(path,17,0,FALSE),"PCI chain above shared bound rejected");
  size=Make(path,2,0,FALSE);memcpy(other,path,size);other[size-3]=1;
  Refuse(other,size,"multi-instance terminator rejected");
  printf("PASS GPU display path: %u checks; bounded buffers only\n",Checks);return 0;
}
