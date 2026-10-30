/* GPL-2.0-only. Host-only simulated shared GPU console; no real EFI calls. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef NULL
#include "diagnostic.c"

static char Output[128*1024];
static UINTN OutputBytes;
static int Scenario,LocateCalls,ReadCalls,StopCalls,Checks;
static VOID *FailFree;
static int FreeFailures;
static EFI_HANDLE H[4]={(VOID *)1,(VOID *)2,(VOID *)3,(VOID *)4};
static EFI_GUID Dummy={0x12345678,0,0,{0}};
static EFI_DEVICE_PATH_PROTOCOL EndPath={0x7f,0xff,{4,0}};
static EFI_GRAPHICS_OUTPUT_MODE_INFORMATION ModeInfo={0};
static EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE Mode={0};
static EFI_GRAPHICS_OUTPUT_PROTOCOL Gop={0};
static EFI_PCI_IO_PROTOCOL Pci={0};
static EFI_DRIVER_BINDING_PROTOCOL Driver={0};
static EFI_LOADED_IMAGE_PROTOCOL Image={0};
static EFI_STATUS EFIAPI Allocate(EFI_MEMORY_TYPE type,UINTN n,VOID **out) {
  (void)type;*out=malloc(n);if(Scenario==15 && !FailFree && !FreeFailures)FailFree=*out;
  return *out?EFI_SUCCESS:EFI_OUT_OF_RESOURCES;
}
static EFI_STATUS EFIAPI Free(VOID *p) {
  BOOLEAN bad=p==FailFree;free(p);
  if(bad){FailFree=NULL;FreeFailures++;return EFI_DEVICE_ERROR;}
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Write(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *p) {
  (void)f;
  if(Scenario==8){if(*n)(*n)--;return EFI_SUCCESS;}
  if(OutputBytes+*n>=sizeof(Output))return EFI_VOLUME_FULL;
  memcpy(Output+OutputBytes,p,*n);OutputBytes+=*n;Output[OutputBytes]=0;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI FileFlush(EFI_FILE_PROTOCOL *f) {(void)f;return Scenario==9?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI Locate(EFI_LOCATE_SEARCH_TYPE search,EFI_GUID *g,VOID *key,UINTN *n,EFI_HANDLE **out) {
  (void)g;(void)key;if(search!=AllHandles)abort();LocateCalls++;
  *out=malloc(sizeof(H));memcpy(*out,H,sizeof(H));*n=4;
  if((Scenario==13 && LocateCalls==2) || (Scenario==14 && LocateCalls==1))FailFree=*out;
  if(Scenario==1)*n=MAX_HANDLES+1;
  if(Scenario==2 && LocateCalls==2)(*out)[1]=(VOID *)9;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Protocols(EFI_HANDLE h,EFI_GUID ***out,UINTN *n) {
  *out=malloc(3*sizeof(EFI_GUID *));
  if(Scenario==12 && !FreeFailures)FailFree=*out;
  if(h==H[0] || h==H[1]){(*out)[0]=&PathGuid;(*out)[1]=&GopGuid;(*out)[2]=&PciGuid;*n=3;}
  else if(h==H[2]){(*out)[0]=&DriverGuid;(*out)[1]=&ImageGuid;*n=2;}
  else {(*out)[0]=&Dummy;*n=1;}
  if(Scenario==3)*n=MAX_PROTOCOLS+1;
  if(Scenario==4)return EFI_DEVICE_ERROR;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Opens(EFI_HANDLE h,EFI_GUID *g,EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **out,UINTN *n) {
  (void)g;*n=2;*out=calloc(2,sizeof(**out));
  if(Scenario==11 && !FreeFailures)FailFree=*out;
  (*out)[0].AgentHandle=H[2];(*out)[0].ControllerHandle=h;
  (*out)[0].Attributes=EFI_OPEN_PROTOCOL_BY_DRIVER;(*out)[0].OpenCount=1;
  (*out)[1].AgentHandle=H[3];(*out)[1].ControllerHandle=H[3];
  (*out)[1].Attributes=EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER;(*out)[1].OpenCount=1;
  if(Scenario==5)*n=MAX_OPENS+1;
  if(Scenario==6)return EFI_DEVICE_ERROR;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Handle(EFI_HANDLE h,EFI_GUID *g,VOID **p) {
  (void)h;
  if(!memcmp(g,&PathGuid,sizeof(*g)))*p=&EndPath;
  else if(!memcmp(g,&GopGuid,sizeof(*g)))*p=&Gop;
  else if(!memcmp(g,&PciGuid,sizeof(*g)))*p=&Pci;
  else if(!memcmp(g,&DriverGuid,sizeof(*g)))*p=&Driver;
  else if(!memcmp(g,&ImageGuid,sizeof(*g)))*p=&Image;
  else return EFI_NOT_FOUND;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Variable(CHAR16 *name,EFI_GUID *g,UINT32 *attrs,UINTN *n,VOID *data) {
  (void)name;(void)g;
  UINT8 path[]={2,3,8,0,0,3,1,128,127,1,4,0,2,3,8,0,112,3,1,128,127,255,4,0};
  if(Scenario==7)path[2]=0;
  *attrs=7;
  if(!data){*n=sizeof(path);return EFI_BUFFER_TOO_SMALL;}
  if(*n<sizeof(path))return EFI_BUFFER_TOO_SMALL;
  memcpy(data,path,sizeof(path));*n=sizeof(path);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Location(EFI_PCI_IO_PROTOCOL *io,UINTN *seg,UINTN *bus,UINTN *dev,UINTN *fun) {
  (void)io;*seg=0;*bus=1;*dev=0;*fun=0;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Read(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH width,UINT32 at,UINTN n,VOID *data) {
  (void)io;if(width!=EfiPciIoWidthUint32 || at!=0 || n!=16)abort();
  memset(data,0,n*4);((UINT32 *)data)[0]=0x220410de;ReadCalls++;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI DriverStop(EFI_DRIVER_BINDING_PROTOCOL *d,EFI_HANDLE h,UINTN n,EFI_HANDLE *children) {
  (void)d;(void)h;(void)n;(void)children;StopCalls++;abort();
}
static void Check(int condition,const char *name) {
  Checks++;if(!condition){fprintf(stderr,"FAIL %s scenario=%d\n",name,Scenario);exit(1);}
}
static EFI_STATUS Run(int scenario) {
  Scenario=scenario;OutputBytes=0;Output[0]=0;LocateCalls=0;ReadCalls=0;StopCalls=0;
  FailFree=NULL;FreeFailures=0;
  EFI_BOOT_SERVICES bs={0};EFI_RUNTIME_SERVICES rt={0};EFI_SYSTEM_TABLE table={0};EFI_FILE_PROTOCOL file={0};
  bs.AllocatePool=Allocate;bs.FreePool=Free;bs.LocateHandleBuffer=Locate;
  bs.ProtocolsPerHandle=Protocols;bs.OpenProtocolInformation=Opens;bs.HandleProtocol=Handle;
  rt.GetVariable=Variable;table.BootServices=&bs;table.RuntimeServices=&rt;
  table.ConsoleOutHandle=H[3];table.StandardErrorHandle=H[3];table.ConsoleInHandle=H[3];
  file.Write=Write;file.Flush=FileFlush;
  ModeInfo.HorizontalResolution=1280;ModeInfo.VerticalResolution=720;
  ModeInfo.PixelFormat=PixelBlueGreenRedReserved8BitPerColor;ModeInfo.PixelsPerScanLine=1280;
  Mode.Info=&ModeInfo;Mode.SizeOfInfo=sizeof(ModeInfo);Mode.MaxMode=1;
  Mode.FrameBufferBase=0xfc00000000;Mode.FrameBufferSize=3686400;Gop.Mode=&Mode;
  if(scenario==10)Gop.Mode=NULL;
  Pci.GetLocation=Location;Pci.Pci.Read=Read;
  Driver.ImageHandle=H[2];Driver.DriverBindingHandle=H[2];Driver.Stop=DriverStop;
  Image.DeviceHandle=H[0];Image.FilePath=&EndPath;Image.ImageSize=12345;
  EFI_STATUS s=GuCaptureGraph(&table,&file);
  Check(StopCalls==0,"driver Stop never called");return s;
}
int main(void) {
  Check(Run(0)==EFI_SUCCESS,"shared console graph succeeds");
  Check(strstr(Output,"capture-complete=1")!=NULL,"complete marker");
  Check(strstr(Output,"attributes=0x0000000000000008")!=NULL,"child edge retained");
  Check(strstr(Output,"path-node raw=7f010400")!=NULL,"multi-instance separator retained");
  Check(strstr(Output,"pci config-dwords=220410de")!=NULL && ReadCalls==2,"GPU identification is read-only");
  Check(strstr(Output,"driver image=")!=NULL && strstr(Output,"image-file-path-node")!=NULL,"driver/image provenance");
  Check(strstr(Output,"gop pixel-format=0x0000000000000001 info-bytes=0x0000000000000024")!=NULL,
        "GOP pixel format and info allocation size are captured from the interface");
  for(int i=1;i<=7;i++) {
    Check(Run(i)==EFI_NOT_READY,"truncated or failed snapshot is incomplete");
    Check(strstr(Output,"capture-complete=0")!=NULL,"incomplete marker");
  }
  Check(Run(8)==EFI_DEVICE_ERROR,"short write fails");
  Check(Run(9)==EFI_DEVICE_ERROR,"flush failure fails");
  Check(strstr(Output,"capture-complete=1")!=NULL,"footer cannot prove final flush success");
  Check(Run(10)==EFI_NOT_READY,"null GOP mode is incomplete");
  const char *reasons[]={"opens-free","protocols-free","after-free","handles-free","variable-free"};
  for(int i=11;i<=15;i++) {
    Check(Run(i)==EFI_NOT_READY,"cleanup failure makes graph incomplete");
    Check(FreeFailures==1 && strstr(Output,reasons[i-11])!=NULL,"cleanup failure reason retained");
    Check(strstr(Output,"capture-complete=0")!=NULL,"cleanup failure cannot be complete");
  }
  Check(GuCaptureGraph(NULL,NULL)==EFI_INVALID_PARAMETER,"null input rejected");
  printf("PASS diagnostic host checks: %d; no device/driver mutation services provided\n",Checks);
  return 0;
}
