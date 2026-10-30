/* GPL-2.0-only. Actual refresh/target predicate with RAM EFI services only. */
#define main legacy_port_main
#include "test_port.c"
#undef main
#include "../ownership.h"

static unsigned Checks,Reads,Locations,Handles,DropLocation,SkipOffset,ChangeLocation,ChangeMethod,ChangeHandle;
static EFI_STATUS HandleStatus,LocationStatus,ReadStatus;
static BOOLEAN OmitHandle,OmitRead;
static unsigned LocationStatusAt,DropLocationAt,WrongLocation,CorruptRead,ChangeBs,ChangeResolver,ChangeWrite,ChangeGlobal,ReentryStage;
static EFI_GUID TargetGuid=EFI_PCI_IO_PROTOCOL_GUID;
static void Check(int ok,const char *name) {Checks++;if(!ok){fprintf(stderr,"FAIL %s\n",name);exit(1);}}
static EFI_STATUS EFIAPI IdentityRead(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 at,UINTN n,VOID *out) {
  Check(p==&pci && w==EfiPciIoWidthUint32 && n==1 && (at==0 || at==8 || at==12 || at==44),"read only the four bounded identity/header words");
  Reads++;
  if(!OmitRead || at!=SkipOffset)memcpy(out,config+at,4);
  if(ChangeMethod && Reads==ChangeMethod)pci.Pci.Read=NULL;
  if(ChangeHandle && Reads==ChangeHandle)Gu.handle=(EFI_HANDLE)0x8888;
  if(ChangeBs && Reads==ChangeBs)Gu.bs=NULL;
  if(ChangeResolver && Reads==ChangeResolver)bs.HandleProtocol=NULL;
  if(ChangeWrite && Reads==ChangeWrite)pci.Pci.Write=NULL;
  if(ChangeGlobal && Reads==ChangeGlobal)Gu.pci=&pci;
  if(CorruptRead==Reads)*(UINT32 *)out^=at==8?0x01000000U:at==12?0x10000U:1U;
  if(ReentryStage==3)Check(RETURN_ERROR(GuRefreshTargetPci()),"read callback reentry refused");
  return ReadStatus;
}
static EFI_STATUS EFIAPI IdentityLocation(EFI_PCI_IO_PROTOCOL *p,UINTN *s,UINTN *b,UINTN *d,UINTN *f) {
  Check(p==&pci,"only selected PCI interface queried");Locations++;
  unsigned drop=(!DropLocationAt || Locations==DropLocationAt)?DropLocation:0;
  if(drop!=1)*s=GU_TARGET_SEGMENT;
  if(drop!=2)*b=GU_TARGET_BUS;
  if(drop!=3)*d=0;
  if(drop!=4)*f=0;
  if(WrongLocation==1)*s=GU_TARGET_SEGMENT+0x10000U;
  if(WrongLocation==2)*b=GU_TARGET_BUS+1;
  if(WrongLocation==3)*d=1;
  if(WrongLocation==4)*f=1;
  if(ChangeLocation && Locations>=2)*s=GU_TARGET_SEGMENT^1U;
  if(ReentryStage==2)Check(RETURN_ERROR(GuRefreshTargetPci()),"location callback reentry refused");
  return LocationStatusAt && Locations!=LocationStatusAt?EFI_SUCCESS:LocationStatus;
}
static EFI_STATUS EFIAPI IdentityHandle(EFI_HANDLE h,EFI_GUID *g,VOID **out) {
  Check(h==(EFI_HANDLE)0x1234 && !memcmp(g,&TargetGuid,sizeof(*g)),"resolve only the exact selected handle/protocol");Handles++;
  if(!OmitHandle)*out=&pci;
  if(ReentryStage==1)Check(RETURN_ERROR(GuRefreshTargetPci()),"resolve callback reentry refused");
  return HandleStatus;
}
static void IdentitySetup(void) {
  memset(&fake,0,sizeof(fake));memset(&bs,0,sizeof(bs));memset(&pci,0,sizeof(pci));memset(config,0,sizeof(config));
  Reads=Locations=Handles=DropLocation=SkipOffset=ChangeLocation=ChangeMethod=ChangeHandle=0;
  HandleStatus=LocationStatus=ReadStatus=EFI_SUCCESS;OmitHandle=OmitRead=FALSE;
  LocationStatusAt=DropLocationAt=WrongLocation=CorruptRead=ChangeBs=ChangeResolver=ChangeWrite=ChangeGlobal=ReentryStage=0;
  bs.HandleProtocol=IdentityHandle;pci.GetLocation=IdentityLocation;pci.Pci.Read=IdentityRead;pci.Pci.Write=writeconfig;
  *(UINT32 *)(config+0)=GU_BOARD_ID;*(UINT32 *)(config+8)=0x030000a1;*(UINT32 *)(config+44)=GU_BOARD_SUBSYSTEM;
  GuInit(&bs,&pci);Gu.handle=(EFI_HANDLE)0x1234;
}
static void Reject(void) {
  Check(RETURN_ERROR(GuRefreshTargetPci()),"incomplete or uncertain identity returns an error");
  Check(Gu.pci==NULL,"failed refresh cannot retain the stale target");
  Check(!fake.writes && !fake.pciwrites && !fake.allocs && !fake.maps,"refresh performs no MMIO/config/DMA mutation");
}
int main(void) {
  IdentitySetup();Check(GuRefreshTargetPci()==EFI_SUCCESS && Gu.pci==&pci,"complete exact identity accepted");
  Check(!fake.writes && !fake.pciwrites,"successful refresh is read-only");
  for(unsigned command=0;command<8;command++) {
    IdentitySetup();config[4]=(UINT8)command;
    Check(GuRefreshTargetPci()==EFI_SUCCESS,"identity refresh preserves BME-on/MEM-off containment access");
  }
  IdentitySetup();EFI_PCI_IO_PROTOCOL prior={0};Gu.pci=&prior;
  Check(GuRefreshTargetPci()==EFI_SUCCESS && Gu.pci==&pci,"valid replacement interface can be resolved for containment");
  IdentitySetup();OmitRead=TRUE;SkipOffset=12;Reject();
  for(unsigned stage=0;stage<3;stage++)for(unsigned warning=0;warning<2;warning++) {
    IdentitySetup();EFI_STATUS status=warning?EFI_WARN_UNKNOWN_GLYPH:EFI_DEVICE_ERROR;
    if(stage==0)HandleStatus=status;if(stage==1)LocationStatus=status;if(stage==2)ReadStatus=status;
    Reject();
  }
  IdentitySetup();OmitHandle=TRUE;Reject();
  for(unsigned field=1;field<=4;field++){IdentitySetup();DropLocation=field;Reject();}
  for(unsigned i=0;i<4;i++){IdentitySetup();OmitRead=TRUE;const UINT32 offsets[]={0,8,12,44};SkipOffset=offsets[i];Reject();}
  for(unsigned i=1;i<=4;i++){IdentitySetup();ChangeMethod=i;Reject();IdentitySetup();ChangeHandle=i;Reject();}
  IdentitySetup();ChangeLocation=1;Reject();
  for(unsigned field=1;field<=4;field++) {
    IdentitySetup();DropLocation=field;DropLocationAt=2;Reject();
    IdentitySetup();WrongLocation=field;Reject();IdentitySetup();CorruptRead=field;Reject();
  }
  for(unsigned warning=0;warning<2;warning++) {
    IdentitySetup();LocationStatusAt=2;LocationStatus=warning?EFI_WARN_UNKNOWN_GLYPH:EFI_DEVICE_ERROR;Reject();
  }
  for(unsigned at=1;at<=4;at++) {
    IdentitySetup();ChangeBs=at;Reject();IdentitySetup();ChangeResolver=at;Reject();
    IdentitySetup();ChangeWrite=at;Reject();IdentitySetup();ChangeGlobal=at;Reject();
  }
  for(unsigned stage=1;stage<=3;stage++){IdentitySetup();ReentryStage=stage;Reject();}
  IdentitySetup();pci.GetLocation=NULL;Reject();IdentitySetup();pci.Pci.Read=NULL;Reject();IdentitySetup();pci.Pci.Write=NULL;Reject();
  IdentitySetup();bs.HandleProtocol=NULL;Reject();
  IdentitySetup();Gu.bs=NULL;Reject();
  IdentitySetup();Gu.handle=NULL;Reject();
  printf("PASS %u fresh PCI identity assertions; actual ownership/port code, RAM-only\n",Checks);
  return 0;
}
