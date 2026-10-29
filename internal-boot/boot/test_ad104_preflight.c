/* GPL-2.0-only. Real discovery and graph checker with RAM-only EFI services. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef NULL
#include <Uefi.h>
#ifndef NULL
#define NULL ((VOID *)0)
#endif
#include <Protocol/PciIo.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/GraphicsOutput.h>
#include "../unlock/sha256.h"
static EFI_GUID BindGuidL=EFI_DRIVER_BINDING_PROTOCOL_GUID;
static EFI_GUID ImgGuidL=EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID PciGuidL=EFI_PCI_IO_PROTOCOL_GUID;
static EFI_GUID GopGuidL=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
#include "ad104-preflight.h"
static EFI_BOOT_SERVICES Bs;
static EFI_SYSTEM_TABLE T;
static PF_PIN Pins[4];static char Hashes[4][65];
static EFI_DRIVER_BINDING_PROTOCOL Drivers[5];static EFI_LOADED_IMAGE_PROTOCOL Images[4];
static UINT8 Data[4][0x8000];static PF_EDGE Edges[8];
static int Checks,Fault,Missing,ExtraMissing,ThirdMissing,FourthMissing,FifthMissing;
static int HideChild;
static void Check(int ok,const char *s){Checks++;if(!ok){fprintf(stderr,"FAIL %s fault=%d missing=%d\n",s,Fault,Missing);exit(1);}}
static EFI_STATUS EFIAPI FreeMock(VOID *p){free(p);return EFI_SUCCESS;}
static EFI_STATUS EFIAPI LocateMock(EFI_LOCATE_SEARCH_TYPE type,EFI_GUID *g,VOID *key,UINTN *n,EFI_HANDLE **out) {
  (void)key;
  if(type==ByProtocol && !PfEqual(g,&BindGuidL,sizeof(*g)))return EFI_NOT_FOUND;
  *n=type==AllHandles?(HideChild?7:8):5;
  *out=malloc(*n*sizeof(EFI_HANDLE));if(!*out)return EFI_OUT_OF_RESOURCES;
  for(UINTN i=0;i<*n;i++)(*out)[i]=(VOID *)(UINTN)(type==AllHandles?(HideChild?(i==0?1:i==1?3:i+8):(i<3?i+1:i+7)):i+10);
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI HandleMock(EFI_HANDLE h,EFI_GUID *g,VOID **out) {
  UINTN v=(UINTN)h;
  if(PfEqual(g,&BindGuidL,sizeof(*g)) && v>=10 && v<=14){*out=&Drivers[v-10];return EFI_SUCCESS;}
  if(PfEqual(g,&ImgGuidL,sizeof(*g)) && v>=20 && v<=23){*out=&Images[v-20];return EFI_SUCCESS;}
  return EFI_NOT_FOUND;
}
static EFI_STATUS EFIAPI ProtocolsMock(EFI_HANDLE h,EFI_GUID ***out,UINTN *n) {
  *out=malloc(8*sizeof(EFI_GUID *));if(!*out)return EFI_OUT_OF_RESOURCES;
  *n=0;
  for(UINTN i=0;i<8;i++)if(Edges[i].Handle==h){
    BOOLEAN seen=FALSE;for(UINTN j=0;j<*n;j++)if(PfEqual((*out)[j],&Edges[i].Protocol,sizeof(EFI_GUID)))seen=TRUE;
    if(!seen)(*out)[(*n)++]=&Edges[i].Protocol;
  }
  if(!*n)(*out)[(*n)++]=&BindGuidL;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI OpensMock(EFI_HANDLE h,EFI_GUID *g,EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **out,UINTN *n) {
  *out=calloc(10,sizeof(**out));if(!*out)return EFI_OUT_OF_RESOURCES;
  *n=0;
  for(UINTN i=0;i<8;i++)if((int)i!=Missing && (int)i!=ExtraMissing && (int)i!=ThirdMissing && (int)i!=FourthMissing && (int)i!=FifthMissing && Edges[i].Handle==h && PfEqual(g,&Edges[i].Protocol,sizeof(*g))) {
    (*out)[(*n)++]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){Edges[i].Agent,Edges[i].Controller,Edges[i].Attributes,1};
  }
  if(Fault==5 && h==(VOID *)1)(*out)[(*n)++]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){(VOID *)99,h,16,1};
  if(Fault==6 && h==(VOID *)10)(*out)[(*n)++]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){(VOID *)10,h,16,1};
  if(Fault==7 && *n)(*out)[0].OpenCount=2;
  return EFI_SUCCESS;
}
static void Setup(int fault,int missing) {
  HideChild=0;Fault=fault;Missing=missing;ExtraMissing=ThirdMissing=FourthMissing=FifthMissing=-1;memset(&Bs,0,sizeof(Bs));memset(&T,0,sizeof(T));
  Bs.LocateHandleBuffer=LocateMock;Bs.HandleProtocol=HandleMock;Bs.ProtocolsPerHandle=ProtocolsMock;
  Bs.OpenProtocolInformation=OpensMock;Bs.FreePool=FreeMock;T.BootServices=&Bs;
  T.ConsoleInHandle=T.ConsoleOutHandle=T.StandardErrorHandle=(VOID *)3;
  const UINTN sizes[]={0x6000,0x8000,0x2000,0x4000};
  for(UINTN i=0;i<4;i++) {
    memset(Data[i],(int)i+1,sizeof(Data[i]));UINT8 digest[32];GuSha256(Data[i]+0x100,8,digest);
    for(UINTN j=0;j<32;j++){Hashes[i][j*2]="0123456789abcdef"[digest[j]>>4];Hashes[i][j*2+1]="0123456789abcdef"[digest[j]&15];}Hashes[i][64]=0;
    Pins[i]=(PF_PIN){sizes[i],0x100,8,0x150,(UINT32)i+1,i==0?EfiRuntimeServicesCode:EfiBootServicesCode,Hashes[i]};
    memset(&Images[i],0,sizeof(Images[i]));Images[i].ImageBase=Data[i];Images[i].ImageSize=sizes[i];Images[i].ImageCodeType=Pins[i].CodeType;
    memset(&Drivers[i],0,sizeof(Drivers[i]));Drivers[i].ImageHandle=(VOID *)(20+i);Drivers[i].DriverBindingHandle=(VOID *)(10+i);
    Drivers[i].Version=Pins[i].Version;Drivers[i].Stop=(EFI_DRIVER_BINDING_STOP)(Data[i]+0x150);
  }
  memset(&Drivers[4],0,sizeof(Drivers[4]));Drivers[4].Version=0x10;Drivers[4].DriverBindingHandle=(VOID *)14;
  Drivers[4].Stop=(EFI_DRIVER_BINDING_STOP)(Data[3]+0x2b38);
  EFI_GUID text={0x387477c2,0x69c7,0x11d2,{0x8e,0x39,0,0xa0,0xc9,0x69,0x72,0x3b}};
  EFI_GUID edid={0x1c0c34f6,0xd380,0x41fa,{0xa0,0x49,0x8a,0xd0,0x6c,0x1a,0x66,0xaa}};
  EFI_GUID ptr={0x31878c87,0xb75,0x11d5,{0x9a,0x4f,0,0x90,0x27,0x3f,0xc1,0x4d}};
  EFI_GUID abs={0x8d59d32b,0xc655,0x4ae9,{0x9b,0x15,0xf2,0x59,4,0x99,0x2a,0x43}};
  Edges[0]=(PF_EDGE){(VOID *)1,(VOID *)13,(VOID *)1,PciGuidL,16};
  Edges[1]=(PF_EDGE){(VOID *)1,(VOID *)13,(VOID *)2,PciGuidL,8};
  Edges[2]=(PF_EDGE){(VOID *)2,(VOID *)11,(VOID *)2,text,16};
  Edges[3]=(PF_EDGE){(VOID *)2,(VOID *)11,(VOID *)3,text,8};
  Edges[4]=(PF_EDGE){(VOID *)2,(VOID *)12,(VOID *)2,GopGuidL,16};
  Edges[5]=(PF_EDGE){(VOID *)2,(VOID *)10,(VOID *)2,edid,16};
  Edges[6]=(PF_EDGE){(VOID *)3,(VOID *)10,(VOID *)3,ptr,16};
  Edges[7]=(PF_EDGE){(VOID *)3,(VOID *)10,(VOID *)3,abs,16};
  if(fault==1)Data[1][0x100]^=1;
  if(fault==2)Drivers[0].Stop=(EFI_DRIVER_BINDING_STOP)(Data[0]+0x151);
  if(fault==3)Drivers[4].ImageHandle=(VOID *)20;
  if(fault==4)T.ConsoleInHandle=(VOID *)4;
  if(fault==8)Images[0].ImageCodeType=EfiBootServicesCode;
}
int main(void) {
  PF_STATE state;Setup(0,-1);
  Check(PfCollect(&T,(VOID *)1,Pins,&state)==EFI_SUCCESS,"fresh discovery accepts exact synthetic five bindings and eight edges");
  Check(state.NullBinding==(VOID *)14 && state.Child==(VOID *)2,"shared null binding and child discovered");
  for(int f=1;f<=8;f++){Setup(f,-1);Check(PfCollect(&T,(VOID *)1,Pins,&state)!=EFI_SUCCESS,"fingerprint, code type, null binding, input or graph faults rejected");}
  for(int e=0;e<8;e++){Setup(0,e);Check(PfCollect(&T,(VOID *)1,Pins,&state)!=EFI_SUCCESS,"every missing active relationship rejected");}
  Setup(0,5);memset(&state,0,sizeof(state));
  Check(PfDrivers(&T,Pins,&state)==EFI_SUCCESS && PfGraphForState(&T,(VOID *)1,&state,TRUE)==EFI_SUCCESS && state.MatchedEdges==7,"post checker accepts only missing EDID edge");
  Setup(0,-1);memset(&state,0,sizeof(state));
  Check(PfDrivers(&T,Pins,&state)==EFI_SUCCESS && PfGraphForState(&T,(VOID *)1,&state,TRUE)!=EFI_SUCCESS,"post checker rejects remaining EDID edge");
  for(int e=0;e<8;e++)if(e!=5){Setup(0,e);memset(&state,0,sizeof(state));Check(PfDrivers(&T,Pins,&state)==EFI_SUCCESS && PfGraphForState(&T,(VOID *)1,&state,TRUE)!=EFI_SUCCESS,"post checker rejects unrelated missing edge");}
  printf("PASS AD104 runtime preflight: %d checks, RAM-only firmware mocks\n",Checks);
  return 0;
}
