/* GPL-2.0-only. Actual generic diagnostic + parent collector; RAM-only EFI. */
#define GU_PROBE_PARENT_FIXTURE
#include "test_probe_generic.c"
#undef main
#include "probe-parent.h"
#ifndef GU_PARENT_TEST_SEGMENT
#define GU_PARENT_TEST_SEGMENT 0
#endif
static EFI_GUID ParentPathGuid=EFI_DEVICE_PATH_PROTOCOL_GUID;
static EFI_PCI_IO_PROTOCOL ParentIo;
static UINT8 GpuPath[]={2,1,12,0,0xd0,0x41,3,0x0a,0,0,0,0,1,1,6,0,0,3,1,1,6,0,0,0,0x7f,0xff,4,0};
static UINT8 BridgePath[]={2,1,12,0,0xd0,0x41,3,0x0a,0,0,0,0,1,1,6,0,0,3,0x7f,0xff,4,0};
static unsigned ParentFault,ParentHits;
static UINT32 Command;
static BOOLEAN Replaced;
static EFI_STATUS EFIAPI ParentLogWrite(EFI_FILE_PROTOCOL *file,UINTN *n,VOID *data) {
  if(ParentFault==10)Pci.Pci.Read=NULL;
  if(ParentFault==11)return EFI_DEVICE_ERROR;
  return LogWrite(file,n,data);
}
static EFI_STATUS EFIAPI ParentRead(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 at,UINTN n,VOID *data) {
  if(io==&Pci || io==&ChangedPci) {
    if((ParentFault==17 || ParentFault==18) && S.PromAllocs && w==EfiPciIoWidthUint32 && !at && n==64) {
      UINT32 complete[64];EFI_STATUS s=GenericRead(io,w,at,n,complete);complete[1]=Command;
      memcpy(data,complete,ParentFault==17?sizeof(complete)-4:sizeof(complete)-1);return s;
    }
    EFI_STATUS s=GenericRead(io,w,at,n,data);
    if(s==EFI_SUCCESS && w==EfiPciIoWidthUint32 && at==0 && n==64)((UINT32 *)data)[1]=Command;
    return s;
  }
  Check(io==&ParentIo && w==EfiPciIoWidthUint32 && !at && n==64,"parent config is bounded read-only");
  UINT32 *cfg=data;for(UINTN i=0;i<64;i++)cfg[i]=0;
  cfg[0]=0x14831022;cfg[1]=7;cfg[2]=0x06040000;cfg[3]=0x10000;cfg[6]=(UINT32)(TestBus<<8)|(UINT32)(TestBus<<16);
  if(ParentFault==9)cfg[6]^=0x100;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI ParentLocation(EFI_PCI_IO_PROTOCOL *io,UINTN *s,UINTN *b,UINTN *d,UINTN *f) {
  if(io==&ParentIo){*s=TestSegment;*b=0;*d=3;*f=0;return EFI_SUCCESS;}
  if(ParentFault>=13 && ParentFault<=16 && S.PromAllocs) {
    if(ParentFault!=13)*s=TestSegment;if(ParentFault!=14)*b=TestBus;
    if(ParentFault!=15)*d=0;if(ParentFault!=16)*f=0;return EFI_SUCCESS;
  }
  return GenericLocation(io,s,b,d,f);
}
static EFI_STATUS EFIAPI ParentAttrs(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION op,UINT64 mask,UINT64 *out) {
  Check(!mask && out && (op==EfiPciIoAttributeOperationGet || op==EfiPciIoAttributeOperationSupported),"parent feature has no attribute write capability");
  *out=op==EfiPciIoAttributeOperationSupported || io==&ParentIo?0x700:0x300;
  if(io==&ParentIo && op==EfiPciIoAttributeOperationSupported) {
    ParentHits++;
    if(ParentFault==1)Pci.Pci.Read=NULL;
    if(ParentFault==2)Pci.GetLocation=NULL;
    if(ParentFault==3)Pci.Mem.Read=NULL;
    if(ParentFault==4)Bs.HandleProtocol=NULL;
    if(ParentFault==5)Replaced=TRUE;
    if(ParentFault==6)TestBus++;
    if(ParentFault==7)Table.BootServices=NULL;
    if(ParentFault==8)Bs.FreePool=NULL;
  }
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI ParentHandle(EFI_HANDLE h,EFI_GUID *g,VOID **out) {
  if(!memcmp(g,&ParentPathGuid,sizeof(*g))) {
    if(h==PciHandle){*out=GpuPath;return EFI_SUCCESS;}
    if(h==(VOID *)60){*out=BridgePath;return EFI_SUCCESS;}
  }
  if(h==(VOID *)60 && !memcmp(g,&WantPci,sizeof(*g))){*out=&ParentIo;return EFI_SUCCESS;}
  if(Replaced && h==PciHandle && !memcmp(g,&WantPci,sizeof(*g))){*out=&ChangedPci;return EFI_SUCCESS;}
  return GenericHandle(h,g,out);
}
static EFI_STATUS EFIAPI ParentLocate(EFI_GUID *g,EFI_DEVICE_PATH_PROTOCOL **path,EFI_HANDLE *h) {
  Check(!memcmp(g,&WantPci,sizeof(*g)),"only PCI prefix resolution");
  if(!memcmp(*path,GpuPath,sizeof(GpuPath))){*h=PciHandle;*path=(VOID *)((UINT8 *)*path+sizeof(GpuPath)-4);return EFI_SUCCESS;}
  if(!memcmp(*path,BridgePath,sizeof(BridgePath))){*h=(VOID *)60;*path=(VOID *)((UINT8 *)*path+sizeof(BridgePath)-4);return EFI_SUCCESS;}
  return EFI_NOT_FOUND;
}
static void ParentSetup(unsigned fault) {
  GenericSetup(G_OK);ParentFault=fault;ParentHits=0;Command=2;Replaced=FALSE;
  TestSegment=GU_PARENT_TEST_SEGMENT;
  Bs.HandleProtocol=ParentHandle;Bs.LocateDevicePath=ParentLocate;
  Pci.Pci.Read=ParentRead;Pci.GetLocation=ParentLocation;Pci.Attributes=ParentAttrs;
  ParentIo=(EFI_PCI_IO_PROTOCOL){0};ParentIo.Pci.Read=ParentRead;ParentIo.GetLocation=ParentLocation;ParentIo.Attributes=ParentAttrs;
  ChangedPci=Pci;
}
int main(void) {
  ParentSetup(0);
  Check(!Pci.Pci.Write && !Pci.Mem.Write && !Pci.Io.Write && !Bs.OpenProtocol && !Bs.DisconnectController && !Rt.SetVariable,
    "the host producer has no device mutation, acquisition, release or NVRAM service");
  Check(GuCaptureGraphOnVolume(&Table,&LogFile,&RootFile)==EFI_SUCCESS,"actual producer captures parent baseline");
  Check(Logged("parent-baseline status=0x0000000000000000 available=1 parents=0x01"),"one complete optional baseline is logged");
  Check(Logged("parent-baseline-config index=0x00") && Logged("parent-baseline-config index=0x01"),"target and parent use distinct config syntax");
  unsigned configs=0;const char *cursor=S.Output;
  while((cursor=strstr(cursor,"cfg dwords="))!=NULL){configs++;cursor++;}
  Check(configs==1,"parent records cannot masquerade as another target config snapshot");
  Check(S.RegReads==16 && S.PromReads==PROM_DWORDS && ParentHits==2,"new reads do not increase the bounded GPU MMIO batch");
  for(unsigned fault=1;fault<=8;fault++) {
    ParentSetup(fault);
    Check(GuCaptureGraphOnVolume(&Table,&LogFile,&RootFile)==EFI_NOT_READY,"parent callback drift fails the actual capture");
    Check(!S.RegReads && !S.PromReads && !Logged("capture-complete=1"),"drift is rejected without any GPU register/PROM read");
  }
  ParentSetup(9);
  Check(GuCaptureGraphOnVolume(&Table,&LogFile,&RootFile)==EFI_SUCCESS && Logged("available=0 parents=0x00"),"unsupported parent chain is optional, not a profile grant");
  for(unsigned fault=13;fault<=18;fault++) {
    ParentSetup(fault);
    Check(GuCaptureGraphOnVolume(&Table,&LogFile,&RootFile)==EFI_NOT_READY,"missing final BDF/config output cannot become a complete capture");
    Check(!S.RegReads && !S.PromReads,"partial-output success fails before any GPU deep reads");
  }
  ParentSetup(0);Command=3;
  Check(GuCaptureGraphOnVolume(&Table,&LogFile,&RootFile)==EFI_SUCCESS && Logged("available=0 parents=0x00"),"legacy command3 capture remains usable without this research subset");
  ParentSetup(0);TestId=0x24c910de;
  Check(GuCaptureGraphOnVolume(&Table,&LogFile,&RootFile)==EFI_SUCCESS && Logged("available=0 parents=0x00"),"production24c9 admission does not widen the research collector");
  for(unsigned fault=10;fault<=12;fault++) {
    ParentSetup(fault);LogFile.Write=ParentLogWrite;
    static CAPTURE direct;static GU_DS_CAPTURE observation;
    memset(&direct,0,sizeof(direct));memset(&observation,0,sizeof(observation));
    direct.Table=&Table;direct.File=&LogFile;direct.Complete=TRUE;
    direct.Used=sizeof(direct.Buffer)-1;direct.Total=direct.Used;memset(direct.Buffer,'x',direct.Used);
    UINT32 cfg[64];Check(ParentRead(&Pci,EfiPciIoWidthUint32,0,64,cfg)==EFI_SUCCESS,"direct helper target fixture");
    if(fault==12)direct.Io=EFI_DEVICE_ERROR;
    EFI_STATUS result=GuProbeParentBaseline(&direct,&observation,PciHandle,&Pci,cfg,TestSegment,TestBus);
    Check(result==(fault==10?EFI_COMPROMISED_DATA:EFI_DEVICE_ERROR),"parent log flush drift/error is not promoted to success");
    Check(!S.RegReads && !S.PromReads,"parent logging and collection never perform GPU deep reads");
    if(fault==12)Check(!observation.Used && !ParentHits,"failed log prevents any supplementary collection");
  }
  printf("PASS %d parent baseline/actual-producer checks; every EFI effect is simulated in RAM\n",Checks);return 0;
}
