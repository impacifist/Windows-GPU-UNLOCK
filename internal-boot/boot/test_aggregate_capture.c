/* GPL-2.0-only. Actual captured aggregate graph; RAM-only firmware interfaces. */
#include "full-release.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  EFI_HANDLE Agent,Image;
  UINT32 Version;
  UINTN Size,Stop;
  UINT8 CodeType,*Path;
  const char *File;
} DRIVER_CAPTURE;
#include "capture.h"

static EFI_BOOT_SERVICES Boot;
static EFI_SYSTEM_TABLE Table;
static EFI_PCI_IO_PROTOCOL Pci;
static EFI_DRIVER_BINDING_PROTOCOL Bindings[5];
static EFI_LOADED_IMAGE_PROTOCOL Images[5];
static GRAPH Original,Current;
static unsigned Checks;

static void Check(BOOLEAN ok,const char *message) {
  Checks++;
  if(!ok){fprintf(stderr,"FAIL %s\n",message);exit(1);}
}
static EFI_STATUS EFIAPI Handle(EFI_HANDLE h,EFI_GUID *g,VOID **out) {
  *out=NULL;
  if(h==FX_PCI && GuGuidEqual(g,&PciGuid))*out=&Pci;
  if(h==FX_PRIMARY && GuGuidEqual(g,&PathGuid))*out=(VOID *)CapturedPath;
  if(h==FX_PRIMARY && GuGuidEqual(g,&EdidGuid))*out=&Table;
  for(UINTN i=0;i<5;i++) {
    if(h==CapturedDrivers[i].Agent && GuGuidEqual(g,&BindingGuid))*out=&Bindings[i];
    if(h==CapturedDrivers[i].Image && GuGuidEqual(g,&ImageGuid))*out=&Images[i];
  }
  return *out?EFI_SUCCESS:EFI_NOT_FOUND;
}
static EFI_STATUS EFIAPI Locate(EFI_GUID *g,EFI_DEVICE_PATH_PROTOCOL **path,EFI_HANDLE *h) {
  if(!GuGuidEqual(g,&PciGuid) || (VOID *)*path!=(VOID *)CapturedPath)return EFI_NOT_FOUND;
  *h=FX_PCI;*path=(VOID *)(CapturedPath+24);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Location(EFI_PCI_IO_PROTOCOL *io,UINTN *seg,UINTN *bus,UINTN *dev,UINTN *fun) {
  Check(io==&Pci,"only captured PCI accessed");*seg=*dev=*fun=0;*bus=1;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Read(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 at,UINTN n,VOID *out) {
  Check(io==&Pci && w==EfiPciIoWidthUint32 && !at && n==16,"bounded captured PCI read");
  memcpy(out,CapturedConfig,n*4);return EFI_SUCCESS;
}
static PLAN Plan(void) {
  PLAN p={0};p.RtxPci=FX_PCI;p.Rtx=FX_PRIMARY;p.Console=FX_CONSOLE;
  p.RtxIo=&Pci;
  p.Input=p.Error=FX_CONSOLE;p.AggCandidate=FX_AGG;p.AggDriver=FX_GFX;
  memcpy(p.RtxConfig,CapturedConfig,sizeof(p.RtxConfig));
  memcpy(p.RtxDisplayPath,CapturedPath,sizeof(p.RtxDisplayPath));
  return p;
}
static void Reset(void) {
  Original.Count=sizeof(CapturedEdges)/sizeof(CapturedEdges[0]);
  memcpy(Original.Edges,CapturedEdges,sizeof(CapturedEdges));
  PreparedPlan=Plan();PreparedGraph=&Original;Full=(FULL){0};
  Full.Table=&Table;Full.Image=FX_EXTRA;
}
static void Accept(void) {
  Check(Consumers(&Table,&PreparedPlan,&Original)==EFI_SUCCESS,"captured aggregate console accepted");
  Check(FullConsumers(&Original)==EFI_SUCCESS,"captured release ownership accepted");
}
static void Drop(GRAPH *g,EFI_HANDLE handle,EFI_HANDLE agent) {
  UINTN n=0;
  for(UINTN i=0;i<g->Count;i++)
    if(g->Edges[i].Handle!=handle || g->Edges[i].Agent!=agent)g->Edges[n++]=g->Edges[i];
  g->Count=n;
}
int main(int argc,char **argv) {
  Check(argc==2,"fixture directory required");
  Boot.HandleProtocol=Handle;Boot.LocateDevicePath=Locate;Table.BootServices=&Boot;
  Pci.GetLocation=Location;Pci.Pci.Read=Read;
  for(UINTN i=0;i<5;i++) {
    const DRIVER_CAPTURE *d=&CapturedDrivers[i];char path[4096];
    int n=snprintf(path,sizeof(path),"%s/%s",argv[1],d->File);
    Check(n>0 && (size_t)n<sizeof(path),"bounded fixture path");
    FILE *f=fopen(path,"rb");Check(f!=NULL,"captured driver file present");
    VOID *data=malloc(d->Size);Check(data!=NULL,"driver RAM allocated");
    Check(fread(data,1,d->Size,f)==d->Size && fgetc(f)==EOF,"exact captured driver size");fclose(f);
    Images[i].ImageBase=data;Images[i].ImageSize=d->Size;Images[i].ImageCodeType=d->CodeType;
    Images[i].FilePath=(VOID *)d->Path;Images[i].DeviceHandle=i==4?FX_PCI:NULL;
    Bindings[i].DriverBindingHandle=d->Agent;Bindings[i].ImageHandle=d->Image;
    Bindings[i].Version=d->Version;Bindings[i].Stop=(VOID *)((UINT8 *)data+d->Stop);
  }
  Check(sizeof(CapturedPath)==sizeof(RtxPath) && Bytes(CapturedPath,RtxPath,sizeof(RtxPath)),"captured display path matches profile");
  GU_CAPS caps;Check(GuCheckConfig(CapturedConfig,0,1,0,0,&caps),"captured PCI identity matches profile");
  Reset();Accept();
  Check(Profile(FX_EDID,TRUE)==EFI_SUCCESS,"captured AMI code accepted");
  Check(Profile(FX_NV,FALSE)==EFI_SUCCESS,"captured NVIDIA code accepted");
  ((UINT8 *)Images[4].ImageBase)[0x280]^=1;
  Check(Profile(FX_NV,FALSE)!=EFI_SUCCESS,"changed NVIDIA code rejected");
  ((UINT8 *)Images[4].ImageBase)[0x280]^=1;
  ((UINT8 *)Images[0].ImageBase)[0x1000]^=1;
  Check(Profile(FX_EDID,TRUE)!=EFI_SUCCESS,"changed AMI code rejected");
  ((UINT8 *)Images[0].ImageBase)[0x1000]^=1;
  for(UINTN i=0;i<4;i++) {
    Bindings[i].Version++;
    Reset();Check(Consumers(&Table,&PreparedPlan,&Original)!=EFI_SUCCESS,"changed binding fingerprint rejected");
    Bindings[i].Version--;
  }
  /* Every target ownership edge is needed; removing or duplicating it must fail. */
  for(UINTN i=0;i<sizeof(CapturedEdges)/sizeof(CapturedEdges[0]);i++) {
    const EDGE *e=&CapturedEdges[i];
    if(e->Handle!=FX_PCI && e->Handle!=FX_PRIMARY && e->Handle!=FX_AGG)continue;
    if(e->Handle==FX_PCI && !GuGuidEqual(&e->Protocol,&PciGuid) && !GuGuidEqual(&e->Protocol,&PathGuid))continue;
    Reset();Original.Edges[i]=Original.Edges[--Original.Count];
    Check(Consumers(&Table,&PreparedPlan,&Original)!=EFI_SUCCESS,"missing target edge rejected");
    Reset();Original.Edges[Original.Count++]=*e;
    Check(Consumers(&Table,&PreparedPlan,&Original)!=EFI_SUCCESS || FullConsumers(&Original)!=EFI_SUCCESS,"duplicate target edge rejected before release");
  }
  Reset();Original.Edges[Original.Count++]=(EDGE){FX_PRIMARY,FX_EXTRA,FX_PRIMARY,TextGuid,16,1};
  Check(Consumers(&Table,&PreparedPlan,&Original)!=EFI_SUCCESS,"foreign consumer rejected");
  Reset();Original.Edges[Original.Count++]=(EDGE){FX_PCI,FX_NV,FX_EXTRA,PathGuid,8,1};
  Check(Consumers(&Table,&PreparedPlan,&Original)!=EFI_SUCCESS,"extra display child rejected");
  Reset();Accept();Current=Original;
  Check(FullExpected(&Current,2),"original graph retained before release");
  Check(!FullExpected(&Current,3),"EDID edge must disappear after detach");
  Drop(&Current,FX_PRIMARY,FX_EDID);
  Check(FullExpected(&Current,3),"only EDID removed at stage3");
  Check(!FullExpected(&Current,4),"NVIDIA edges must disappear after stop");
  Drop(&Current,FX_PCI,FX_NV);Drop(&Current,FX_PRIMARY,FX_COMBO);
  Check(FullExpected(&Current,4),"target edges released; unrelated graph preserved");
  Current.Edges[Current.Count++]=(EDGE){FX_PCI,FX_EXTRA,FX_PCI,PciGuid,16,1};
  Check(!FullExpected(&Current,4),"new owner after release rejected");
  for(UINTN i=0;i<5;i++)free(Images[i].ImageBase);
  printf("PASS %u captured aggregate graph/driver checks; no firmware access\n",Checks);
  return 0;
}
