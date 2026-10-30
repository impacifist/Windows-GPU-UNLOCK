/* GPL-2.0-only. Host test for GuPeekCold: read-only pre-release cold check. */
#include "full-release.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int Checks,Failures;
static void Check(int ok,const char *name){Checks++;if(!ok){Failures++;fprintf(stderr,"FAIL %s\n",name);}}

typedef struct {EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR d;UINT8 end[2];} BAR;
typedef struct {EFI_PCI_IO_PROTOCOL io;UINTN seg,bus,dev,fun;UINT32 cfg[16];UINT32 regs[9];BOOLEAN failRead,failLocation,nullBar,errorBar;BAR bar;} FAKE;
static FAKE Devs[3];static UINTN DevCount;static UINTN Writes,Reads,BarQueries,ConfigReads;
static VOID *BarAllocation;static UINT32 Values[9];
static int FreeFault,DriftPhase,DriftKind,FailConfigAt,SwapInterface;
static EFI_PCI_IO_PROTOCOL Replacement;
static EFI_STATUS EFIAPI Loc(EFI_PCI_IO_PROTOCOL *io,UINTN *s,UINTN *b,UINTN *d,UINTN *f){FAKE *x=(FAKE *)io;if(x->failLocation)return EFI_DEVICE_ERROR;*s=x->seg;*b=x->bus;*d=x->dev;*f=x->fun;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI CfgRead(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 at,UINTN n,VOID *out){
  (void)w;ConfigReads++;if(at||n!=16)return EFI_INVALID_PARAMETER;
  if((int)ConfigReads==FailConfigAt){memset(out,0xff,64);return EFI_DEVICE_ERROR;}
  memcpy(out,((FAKE *)io)->cfg,64);return EFI_SUCCESS;}
static EFI_STATUS EFIAPI MemRead(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT8 bar,UINT64 at,UINTN n,VOID *out){
  Reads++;FAKE *x=(FAKE *)io;if(w!=EfiPciIoWidthUint32||bar||n!=1)return EFI_INVALID_PARAMETER;
  for(UINTN i=0;i<9;i++)if(ColdOffsets[i]==at){if(x->failRead && i==6)return EFI_DEVICE_ERROR;*(UINT32 *)out=x->regs[i];return EFI_SUCCESS;}
  return EFI_INVALID_PARAMETER;}
static EFI_STATUS EFIAPI MemWrite(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT8 bar,UINT64 at,UINTN n,VOID *in){
  (void)io;(void)w;(void)bar;(void)at;(void)n;(void)in;Writes++;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI LocateBuf(EFI_LOCATE_SEARCH_TYPE t,EFI_GUID *g,VOID *k,UINTN *n,EFI_HANDLE **out){
  (void)t;(void)k;if(!GuGuidEqual(g,&PciGuid))return EFI_NOT_FOUND;
  if(!DevCount){*n=0;*out=NULL;return EFI_NOT_FOUND;}
  *n=DevCount;*out=malloc(DevCount*sizeof(EFI_HANDLE));for(UINTN i=0;i<DevCount;i++)(*out)[i]=&Devs[i];return EFI_SUCCESS;}
static EFI_STATUS EFIAPI HandleProto(EFI_HANDLE h,EFI_GUID *g,VOID **out){
  if(!GuGuidEqual(g,&PciGuid)){*out=NULL;return EFI_UNSUPPORTED;}*out=SwapInterface?&Replacement:&((FAKE *)h)->io;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI GetBar(EFI_PCI_IO_PROTOCOL *io,UINT8 bar,UINT64 *supports,VOID **out){
  FAKE *x=(FAKE *)io;(void)supports;BarQueries++;if(bar)return EFI_INVALID_PARAMETER;
  if(x->nullBar){*out=NULL;return x->errorBar?EFI_DEVICE_ERROR:EFI_SUCCESS;}
  BAR *copy=malloc(sizeof(*copy));if(!copy)return EFI_OUT_OF_RESOURCES;
  *copy=x->bar;*out=copy;BarAllocation=copy;
  return x->errorBar?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI FreeP(VOID *p){
  int phase=p==BarAllocation?2:1;
  if(phase==2)BarAllocation=NULL;
  free(p);
  if(DriftPhase==phase) {
    if(DriftKind==1)Devs[0].cfg[1]&=~2U;
    if(DriftKind==2)Devs[0].cfg[1]|=4U;
    if(DriftKind==3)Devs[0].cfg[4]=0;
    if(DriftKind==4)Devs[0].cfg[0]^=1U;
    if(DriftKind==5)Devs[0].bus=GU_TARGET_BUS+1U;
    if(DriftKind==6)SwapInterface=1;
    if(DriftKind==7)Devs[0].io.Pci.Read=NULL;
    if(DriftKind==8)Devs[0].failLocation=TRUE;
    if(DriftKind==9)Devs[0].cfg[15]^=1U;
    if(DriftKind==10)Devs[0].seg=GU_TARGET_SEGMENT^1U;
  }
  return FreeFault==phase?EFI_DEVICE_ERROR:EFI_SUCCESS;
}

/* A power-on state for the selected profile; SS0/SS1 are any valid entry values. */
static void Card(FAKE *x){
  memset(x,0,sizeof(*x));x->io.GetLocation=Loc;x->io.Pci.Read=CfgRead;x->io.Mem.Read=MemRead;x->io.Mem.Write=MemWrite;
  x->io.GetBarAttributes=GetBar;
  x->seg=GU_TARGET_SEGMENT;x->bus=GU_TARGET_BUS;x->cfg[0]=GU_BOARD_ID;x->cfg[1]=2;x->cfg[2]=0x030000a1;x->cfg[4]=(UINT32)GU_BAR0_BASE;x->cfg[11]=GU_BOARD_SUBSYSTEM;
  x->bar.d.Desc=ACPI_ADDRESS_SPACE_DESCRIPTOR;x->bar.d.Len=sizeof(x->bar.d)-3;x->bar.d.AddrSpaceGranularity=32;
  x->bar.d.AddrRangeMin=GU_BAR0_BASE;x->bar.d.AddrRangeMax=GU_BAR0_END;x->bar.d.AddrLen=0x1000000;
  x->bar.end[0]=ACPI_END_TAG_DESCRIPTOR;
  if(x==&Devs[0]){FreeFault=DriftPhase=DriftKind=FailConfigAt=SwapInterface=0;}
  const UINT32 r[9]={GU_CHIP_ID,0x8b8f,0x3ff,0x10,0,0xffffff8f,0x40123070,0x3,GU_DEVICE_MEMORY_SIZE};memcpy(x->regs,r,sizeof(r));
}
static EFI_STATUS Run(void){
  static EFI_BOOT_SERVICES bs;static EFI_SYSTEM_TABLE t;
  memset(&bs,0,sizeof(bs));memset(&t,0,sizeof(t));memset(Values,0xee,sizeof(Values));
  Reads=BarQueries=ConfigReads=0;
  bs.LocateHandleBuffer=LocateBuf;bs.HandleProtocol=HandleProto;bs.FreePool=FreeP;t.BootServices=&bs;
  return GuPeekCold(&t,Values);
}
static void RejectBeforeMmio(EFI_STATUS expected,const char *name) {
  Check(Run()==expected,name);Check(Reads==0,"rejected prerequisite performs zero MMIO reads");
  UINT32 zero[9]={0};Check(Bytes(Values,zero,sizeof(zero)),"pre-MMIO rejection has no cold-state evidence");
}
int main(void){
  /* The decode-restore gate must observe exactly one held exclusive claim,
   * for aggregate and direct-console profiles alike. RAM graph only. */
  static GRAPH originalOwnership,currentOwnership;
  PreparedGraph=&originalOwnership;PreparedPlan.RtxPci=(EFI_HANDLE)(UINTN)1;
  Full.Image=(EFI_HANDLE)(UINTN)2;
  originalOwnership.Count=1;
  originalOwnership.Edges[0]=(EDGE){.Handle=(EFI_HANDLE)(UINTN)3,.Agent=(EFI_HANDLE)(UINTN)4,
    .Controller=(EFI_HANDLE)(UINTN)3,.Protocol=PciGuid,.Attributes=EFI_OPEN_PROTOCOL_BY_DRIVER,.Count=1};
  currentOwnership=originalOwnership;
  Check(FullExpected(&currentOwnership,4),"unclaimed graph accepted before exclusive acquisition");
  Full.Owned=TRUE;
  Check(!FullExpected(&currentOwnership,4),"missing held exclusive claim rejected");
  currentOwnership.Count=2;
  currentOwnership.Edges[1]=(EDGE){.Handle=PreparedPlan.RtxPci,.Agent=Full.Image,
    .Protocol=PciGuid,.Attributes=EFI_OPEN_PROTOCOL_EXCLUSIVE,.Count=1};
  Check(FullExpected(&currentOwnership,4),"exactly one held exclusive claim accepted");
  currentOwnership.Count=3;currentOwnership.Edges[2]=currentOwnership.Edges[1];
  Check(!FullExpected(&currentOwnership,4),"duplicate exclusive claim rejected");
  currentOwnership.Count=2;currentOwnership.Edges[1].Count=2;
  Check(!FullExpected(&currentOwnership,4),"exclusive claim open count drift rejected");
  currentOwnership.Edges[1].Count=1;currentOwnership.Edges[1].Controller=(EFI_HANDLE)(UINTN)3;
  Check(!FullExpected(&currentOwnership,4),"exclusive claim controller drift rejected");
  currentOwnership.Edges[1].Controller=NULL;currentOwnership.Edges[1].Agent=(EFI_HANDLE)(UINTN)4;
  Check(!FullExpected(&currentOwnership,4),"foreign exclusive claim rejected");
  currentOwnership.Edges[1].Agent=Full.Image;Full.Owned=FALSE;
  Check(!FullExpected(&currentOwnership,4),"unexpected exclusive claim before acquisition rejected");
  Full=(FULL){0};PreparedPlan=(PLAN){0};PreparedGraph=NULL;
  Check(CodeRange(0x100000,0x3000,0x280,0x400,0x300),"bounded driver code accepted");
  Check(!CodeRange(0,0x3000,0x280,0x400,0x300),"null image rejected before hashing");
  Check(!CodeRange(0x100000,0x3000,0x280,0,0x300),"empty code rejected before hashing");
  Check(!CodeRange(0x100000,0x200,0x280,0x400,0x300),"code RVA past image rejected");
  Check(!CodeRange(0x100000,0x300,0x280,0x400,0x300),"truncated code rejected before hashing");
  Check(!CodeRange(0x100000,0x3000,0x280,0x400,0x27f),"Stop before code rejected");
  Check(!CodeRange(0x100000,0x3000,0x280,0x400,0x680),"Stop at code end rejected");
  Check(!CodeRange(~(UINTN)0-0x100,0x3000,0x280,0x400,0x300),"image pointer overflow rejected");
  Check(!CodeRange(0x100000,0x3000,0x280,~(UINTN)0,0x300),"code length overflow rejected");
  DevCount=1;Card(&Devs[0]);Check(Run()==EFI_SUCCESS,"profile cold state with valid decoding and BAR accepted");
  Check(Reads==9 && BarQueries==1 && ConfigReads==3,"valid target checked around resource lifetime before nine reads");
  for(UINT32 ss1=0;ss1<=15;ss1++){Card(&Devs[0]);Devs[0].regs[7]=ss1;Check(Run()==EFI_SUCCESS,"any power-on SS1 0..15 accepted");}
  Card(&Devs[0]);Devs[0].regs[7]=16;Check(Run()==EFI_NOT_READY,"SS1 above 15 refused");
  Card(&Devs[0]);Devs[0].regs[4]=0x5ff2e00;Check(Run()==EFI_NOT_READY,"initialized WPR2 (warm restart) refused");
  Card(&Devs[0]);Devs[0].regs[6]=0x88888888;Check(Run()==EFI_NOT_READY,"already unlocked SS0 refused");
  Card(&Devs[0]);Devs[0].regs[6]=0xbadf5040;Check(Run()==EFI_NOT_READY,"inaccessible SS0 refused");
  Card(&Devs[0]);Devs[0].regs[3]=0;Check(Run()==EFI_NOT_READY,"running GFW falcon refused");
  Card(&Devs[0]);Devs[0].regs[8]=GU_DEVICE_MEMORY_SIZE+4;Check(Run()==EFI_NOT_READY,"other memory size refused");
  Card(&Devs[0]);Devs[0].regs[0]=GU_CHIP_ID^0x00100000U;Check(Run()==EFI_NOT_READY,"other chip refused");
  Card(&Devs[0]);Devs[0].failRead=TRUE;Check(Run()==EFI_DEVICE_ERROR,"register read failure reported");
  Card(&Devs[0]);Devs[0].cfg[11]=GU_BOARD_SUBSYSTEM^1U;Check(Run()==EFI_NOT_FOUND,"other subsystem not a target");
  Card(&Devs[0]);Devs[0].bus=GU_TARGET_BUS+1U;Check(Run()==EFI_NOT_FOUND,"card at another slot not a target");
  Card(&Devs[0]);Devs[0].seg=GU_TARGET_SEGMENT^1U;RejectBeforeMmio(EFI_NOT_FOUND,"same BDF in another segment is not a target");
  DevCount=2;Card(&Devs[0]);Card(&Devs[1]);Devs[1].fun=1;Devs[1].cfg[0]=0x1aef10de;Check(Run()==EFI_SUCCESS,"HD audio function ignored");
  Card(&Devs[1]);Devs[1].seg=GU_TARGET_SEGMENT^1U;Devs[1].regs[0]^=1U;
  Check(Run()==EFI_SUCCESS && Reads==9,"another segment with the same BDF cannot supply the cold observations");
  Card(&Devs[1]);Check(Run()==EFI_NOT_FOUND,"two identical targets refused");
  DevCount=0;Check(Run()==EFI_NOT_FOUND,"no PCI devices refused");
  DevCount=1;
  Card(&Devs[0]);Devs[0].cfg[1]=0;RejectBeforeMmio(EFI_UNSUPPORTED,"disabled memory decoding refused");Check(BarQueries==0,"bad raw decode prevents resource query");
  Card(&Devs[0]);Devs[0].cfg[1]=6;RejectBeforeMmio(EFI_UNSUPPORTED,"bus mastering refused");
  Card(&Devs[0]);Devs[0].cfg[1]=3;Check(Run()==EFI_SUCCESS,"IO decode remains allowed as in existing config policy");
  Card(&Devs[0]);Devs[0].cfg[4]=0;RejectBeforeMmio(EFI_UNSUPPORTED,"unset raw BAR refused");
  Card(&Devs[0]);Devs[0].cfg[4]^=0x1000000;RejectBeforeMmio(EFI_UNSUPPORTED,"different raw BAR refused");
  Card(&Devs[0]);Devs[0].cfg[4]|=1;RejectBeforeMmio(EFI_UNSUPPORTED,"IO BAR refused");
  Card(&Devs[0]);Devs[0].cfg[4]|=4;RejectBeforeMmio(EFI_UNSUPPORTED,"64-bit BAR refused");
  Card(&Devs[0]);Devs[0].io.GetBarAttributes=NULL;RejectBeforeMmio(EFI_UNSUPPORTED,"missing resource method refused");
  Card(&Devs[0]);Devs[0].nullBar=TRUE;RejectBeforeMmio(EFI_UNSUPPORTED,"null resource refused");
  Card(&Devs[0]);Devs[0].nullBar=Devs[0].errorBar=TRUE;RejectBeforeMmio(EFI_DEVICE_ERROR,"resource read failure refused");
  Card(&Devs[0]);Devs[0].errorBar=TRUE;RejectBeforeMmio(EFI_DEVICE_ERROR,"resource failure with allocation cleaned up");Check(!BarAllocation,"failed resource buffer freed");
  Card(&Devs[0]);Devs[0].bar.d.Desc=0;RejectBeforeMmio(EFI_UNSUPPORTED,"wrong descriptor refused");
  Card(&Devs[0]);Devs[0].bar.d.Len--;RejectBeforeMmio(EFI_UNSUPPORTED,"wrong declared descriptor length refused");
  Card(&Devs[0]);Devs[0].bar.d.ResType=1;RejectBeforeMmio(EFI_UNSUPPORTED,"IO resource refused");
  Card(&Devs[0]);Devs[0].bar.d.AddrSpaceGranularity=64;RejectBeforeMmio(EFI_UNSUPPORTED,"64-bit resource refused");
  Card(&Devs[0]);Devs[0].bar.d.AddrRangeMin++;RejectBeforeMmio(EFI_UNSUPPORTED,"resource base mismatch refused");
  Card(&Devs[0]);Devs[0].bar.d.AddrRangeMax++;RejectBeforeMmio(EFI_UNSUPPORTED,"resource end mismatch refused");
  Card(&Devs[0]);Devs[0].bar.d.AddrLen--;RejectBeforeMmio(EFI_UNSUPPORTED,"short resource length refused");
  Card(&Devs[0]);Devs[0].bar.d.AddrLen=~(UINT64)0;RejectBeforeMmio(EFI_UNSUPPORTED,"overflow-sized resource refused");
  Card(&Devs[0]);Devs[0].bar.d.AddrTranslationOffset=1;RejectBeforeMmio(EFI_UNSUPPORTED,"translated resource refused");
  Card(&Devs[0]);Devs[0].bar.end[0]=0;RejectBeforeMmio(EFI_UNSUPPORTED,"missing end tag refused");
  Card(&Devs[0]);Devs[0].bar.d.AddrRangeMax=0x00ffffff;Check(Run()==EFI_SUCCESS,"relative resource end remains accepted");
  Card(&Devs[0]);{
    UINT8 sum=0;for(UINTN i=0;i<sizeof(Devs[0].bar.d)+2;i++)sum=(UINT8)(sum+((UINT8 *)&Devs[0].bar)[i]);
    Devs[0].bar.end[1]=(UINT8)(0-sum);
    Check(Run()==EFI_SUCCESS,"valid supplied checksum accepted");
    Devs[0].bar.end[1]=(UINT8)(Devs[0].bar.end[1]+1);if(!Devs[0].bar.end[1])Devs[0].bar.end[1]=1;
    RejectBeforeMmio(EFI_CRC_ERROR,"invalid supplied checksum refused");
  }
  for(int phase=1;phase<=2;phase++) {
    Card(&Devs[0]);FreeFault=phase;RejectBeforeMmio(EFI_DEVICE_ERROR,"pool free failure prevents MMIO");
    for(int drift=1;drift<=10;drift++) {
      Card(&Devs[0]);DriftPhase=phase;DriftKind=drift;
      EFI_STATUS expected=(drift==5 || drift==7 || drift==10)?EFI_UNSUPPORTED:drift==8?EFI_DEVICE_ERROR:EFI_NOT_READY;
      RejectBeforeMmio(expected,"service-return target or raw-config drift refused");
    }
  }
  for(int at=1;at<=3;at++){Card(&Devs[0]);FailConfigAt=at;RejectBeforeMmio(at==1?EFI_NOT_FOUND:EFI_DEVICE_ERROR,"failed config read never supplies MMIO authority");}
  Card(&Devs[0]);Devs[0].io.Mem.Read=NULL;RejectBeforeMmio(EFI_UNSUPPORTED,"missing MMIO read method refused");
  Card(&Devs[0]);{
    Full.Captured=TRUE;Full.Config[0]=0x11223344;PreparedPlan.RtxPci=(EFI_HANDLE)(UINTN)123;PreparedReady=TRUE;
    FULL before=Full;PLAN plan=PreparedPlan;
    Check(Run()==EFI_SUCCESS,"peek does not depend on global release authority");
    Check(Bytes(&before,&Full,sizeof(Full)) && Bytes(&plan,&PreparedPlan,sizeof(PreparedPlan)) && PreparedReady,"peek preserves release authority state");
  }
  {
    EFI_BOOT_SERVICES bs={0};EFI_SYSTEM_TABLE t={0};UINT32 out[9];
    Check(GuPeekCold(NULL,out)==EFI_INVALID_PARAMETER,"null table refused");
    Check(GuPeekCold(&t,out)==EFI_INVALID_PARAMETER,"null services refused");
    t.BootServices=&bs;
    Check(GuPeekCold(&t,NULL)==EFI_INVALID_PARAMETER,"null output refused");
    Check(GuPeekCold(&t,out)==EFI_UNSUPPORTED,"missing enumeration services refused");
    bs.LocateHandleBuffer=LocateBuf;bs.FreePool=FreeP;
    Check(GuPeekCold(&t,out)==EFI_UNSUPPORTED,"missing handle service refused");
    bs.HandleProtocol=HandleProto;bs.FreePool=NULL;
    Check(GuPeekCold(&t,out)==EFI_UNSUPPORTED,"missing cleanup service refused");
  }
  Check(Writes==0,"no MMIO writes in any case");
  if(Failures){fprintf(stderr,"%d/%d cold pre-check cases failed\n",Failures,Checks);return 1;}
  printf("PASS %d cold pre-check cases; read-only, no MMIO writes\n",Checks);return 0;
}
