/* GPL-2.0-only. Synthetic host RAM tests of the real Prom() service boundary.
 * No device methods, driver release or firmware execution are available.
 * The SHA-256 pin below is ONLY for this test translation unit's zero buffer. */
#if !defined(__STDC_HOSTED__) || __STDC_HOSTED__ != 1
#error PROM boundary tests require a hosted compiler
#endif
#include "../unlock/machine-profile.h"
#undef GU_PROM_SHA256
#define GU_PROM_SHA256 "30e14955ebf1352266dc2ff8067e68104607e750abb9d3b36582b8af909fcb58"
#include "full-release.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static EFI_PCI_IO_PROTOCOL Io,OtherIo;
static EFI_BOOT_SERVICES Services;
static EFI_SYSTEM_TABLE System;
static UINT32 Config[64];
static UINTN Bus;
static int Checks,Reads,Allocations,Frees,HashCorrupt,FailReadAt;
static int AllocationFault,ConfigFault,FreeFault;
static EFI_HANDLE Target=(VOID *)1;
static VOID *Allocation;
static void Check(int ok,const char *name) {
  Checks++;if(!ok){fprintf(stderr,"FAIL %s\n",name);exit(1);}
}
static EFI_STATUS EFIAPI AllocateProm(EFI_MEMORY_TYPE kind,UINTN bytes,VOID **out) {
  Check(kind==EfiBootServicesData && bytes==1048576,"only one bounded PROM buffer allocated");
  Allocations++;
  if(AllocationFault==1)Config[1]&=~2U;
  if(AllocationFault==2)Config[1]|=4U;
  if(AllocationFault==3)Config[0]^=1U;
  if(AllocationFault==4)Config[4]^=0x1000000;
  if(AllocationFault==5)Bus=2;
  if(AllocationFault==7)Io.Mem.Read=NULL;
  if(AllocationFault==8)PreparedPlan.RtxIo=&OtherIo;
  if(AllocationFault==9){*out=NULL;return EFI_OUT_OF_RESOURCES;}
  *out=Allocation=calloc(1,bytes);
  return *out?EFI_SUCCESS:EFI_OUT_OF_RESOURCES;
}
static EFI_STATUS EFIAPI FreeProm(VOID *data) {
  Check(data==Allocation && data!=NULL,"only allocated buffer freed exactly once");
  Frees++;free(data);Allocation=NULL;
  return FreeFault?EFI_ACCESS_DENIED:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI HandleProm(EFI_HANDLE target,EFI_GUID *guid,VOID **out) {
  Check(target==Target && GuGuidEqual(guid,&PciGuid),"exact target protocol rechecked");
  if(AllocationFault==6){*out=&OtherIo;return EFI_SUCCESS;}
  *out=&Io;return ConfigFault==1?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI LocateProm(EFI_PCI_IO_PROTOCOL *io,UINTN *seg,UINTN *bus,UINTN *dev,UINTN *fun) {
  Check(io==&Io,"exact interface location rechecked");
  *seg=*dev=*fun=0;*bus=Bus;
  return ConfigFault==2?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI ConfigProm(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH width,UINT32 at,UINTN count,VOID *out) {
  Check(io==&Io && width==EfiPciIoWidthUint32 && !at && count==64,"complete bounded PCI baseline read");
  memcpy(out,Config,sizeof(Config));
  return ConfigFault==3?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI ReadProm(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH width,UINT8 bar,UINT64 at,UINTN count,VOID *out) {
  Check(io==&Io && width==EfiPciIoWidthUint8 && !bar && count==65536 && at==0x300000+(UINT64)Reads*65536,"bounded sequential PROM reads");
  Check((Config[1]&6U)==2U,"PROM read requires MEM1/BME0");
  Check(!memcmp(Config,Full.Config,sizeof(Config)),"PROM read requires unchanged complete configuration");
  Reads++;memset(out,0,count);
  if(HashCorrupt)((UINT8 *)out)[0]=1;
  return Reads==FailReadAt?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static VOID Reset(UINT32 ioBit) {
  Check(!Allocation,"no retained buffer from preceding case");
  memset(&Io,0,sizeof(Io));memset(&OtherIo,0,sizeof(OtherIo));memset(&Services,0,sizeof(Services));
  memset(&System,0,sizeof(System));memset(&Full,0,sizeof(Full));memset(&PreparedPlan,0,sizeof(PreparedPlan));
  memset(Config,0,sizeof(Config));Config[0]=GU_BOARD_ID;Config[1]=2|ioBit;Config[2]=0x030000a1;
  Config[4]=(UINT32)GU_BAR0_BASE;Config[11]=GU_BOARD_SUBSYSTEM;Bus=GU_TARGET_BUS;
  memcpy(Full.Config,Config,sizeof(Config));System.BootServices=&Services;Full.Table=&System;
  PreparedPlan.RtxPci=Target;PreparedPlan.RtxIo=&Io;
  Io.GetLocation=LocateProm;Io.Pci.Read=ConfigProm;Io.Mem.Read=ReadProm;
  Services.AllocatePool=AllocateProm;Services.FreePool=FreeProm;Services.HandleProtocol=HandleProm;
  Reads=Allocations=Frees=HashCorrupt=FailReadAt=AllocationFault=ConfigFault=FreeFault=0;
}
static VOID Complete(EFI_STATUS expected,int reads) {
  Check(Prom()==expected,"exact first operation status retained");
  Check(Reads==reads,"dependent read count is exact");
  Check(Allocations==1 && Frees==(AllocationFault==9?0:1) && !Allocation,"bounded allocation always cleaned up");
  Check(!Io.Pci.Write && !Io.Mem.Write && !Io.Attributes && !Services.DisconnectController,"no write/attribute/release service exists");
}
int main(void) {
  for(UINT32 io=0;io<=1;io++) {
    Reset(io);Complete(EFI_SUCCESS,16);
    for(int fault=1;fault<=9;fault++) {
      Reset(io);AllocationFault=fault;
      EFI_STATUS expected=(fault==1)?EFI_NOT_READY:
        (fault==2 || fault==3 || fault==4 || fault==5)?EFI_UNSUPPORTED:
        fault==6?EFI_NOT_READY:(fault==7 || fault==8)?EFI_UNSUPPORTED:EFI_OUT_OF_RESOURCES;
      Complete(expected,0);
    }
    for(int fault=1;fault<=3;fault++) {
      Reset(io);ConfigFault=fault;
      Complete(fault==1?EFI_NOT_READY:fault==2?EFI_UNSUPPORTED:EFI_DEVICE_ERROR,0);
    }
    for(UINTN word=0;word<64;word++) {
      Reset(io);Config[word]^=0x100U;
      Complete((word==0 || word==2 || word==4 || word==11)?EFI_UNSUPPORTED:EFI_NOT_READY,0);
    }
    for(int read=1;read<=16;read++) {
      Reset(io);FailReadAt=read;Complete(EFI_DEVICE_ERROR,read);
      Reset(io);FailReadAt=read;FreeFault=1;Complete(EFI_DEVICE_ERROR,read);
    }
    Reset(io);HashCorrupt=1;Complete(EFI_CRC_ERROR,16);
    Reset(io);HashCorrupt=1;FreeFault=1;Complete(EFI_CRC_ERROR,16);
    Reset(io);FreeFault=1;Complete(EFI_ACCESS_DENIED,16);
    Reset(io);AllocationFault=1;FreeFault=1;Complete(EFI_NOT_READY,0);
  }
  printf("PASS PROM boundary host checks: %d; profile=%s; synthetic RAM, real SHA-256, no hardware\n",Checks,GU_PROFILE_NAME);
  return 0;
}
