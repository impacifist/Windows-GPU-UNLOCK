/* GPL-2.0-only. Real four-stage engine and graph with synthetic RAM services. */
#define GU_AD104_TEXT_TEST
#define GU_AD104_GOP_TEST
#define GU_AD104_CONSUMERS_TEST
#define GU_AD104_OWNER_CHILD_TEST
#pragma push_macro("main")
#undef main
#define main OriginalMain
#include "test_ad104_edid.c"
#undef main
#pragma pop_macro("main")
static int OwnerFault,OwnerCalls,AfterFreeReads,OwnerFlushes;
#ifdef GU_AD104_PARENT_TEST
static int ParentFault,ParentCalls,ParentFlushes,ParentPostFlushes;
#endif
static EFI_STATUS EFIAPI OwnerStop(EFI_DRIVER_BINDING_PROTOCOL *d,EFI_HANDLE gpu,UINTN n,EFI_HANDLE *child) {
#ifdef GU_AD104_PARENT_TEST
  if(n==0) {
    Check(d==&Drivers[3] && gpu==(VOID *)1 && !child && ParentAttempted,"exact zero-child parent ABI");
    ParentCalls++;
    if(ParentFault==1)return EFI_DEVICE_ERROR;
    Stage=5;Missing=ParentFault==3?-1:0;Config[1]&=~7U;CurrentAttributes=0;
    if(ParentFault==4)Config[1]|=4;
    if(ParentFault==5)CurrentAttributes=0x718;
    if(ParentFault==6){Config[1]|=3;CurrentAttributes=0x300;}
    if(ParentFault==7)Config[4]++;
    if(ParentFault==8)*(UINT64 *)(Data[0]+0x5530)=0;
    if(ParentFault==9)Input.WaitForKey=(VOID *)2;
    if(ParentFault==15)Config[1]^=0x10000;
    if(ParentFault==16)HideChild=0;
    if(ParentFault==19)Io.Pci.Read=NULL;
    if(ParentFault==20)SupportedAttributes^=0x100;
    return ParentFault==2?EFI_DEVICE_ERROR:EFI_SUCCESS;
  }
#endif
  Check(d==&Drivers[3] && gpu==(VOID *)1 && n==1 && child && *child==(VOID *)2,"direct one-child ABI only, never zero-child Stop");
  Check(Calls==3 && OwnerChildAttempted && GuTextOutputDisabled,"prior stages and one-shot intent");
  OwnerCalls++;
  if(OwnerFault==1)return EFI_DEVICE_ERROR;
  Stage=4;HideChild=OwnerFault==3?0:1;FifthMissing=OwnerFault==4?-1:1;
  if(OwnerFault==5)Missing=0;
  if(OwnerFault==6)Config[1]=0;
  if(OwnerFault==7)*(UINT64 *)(Data[0]+0x5530)=0;
  if(OwnerFault==8)Input.WaitForKey=(VOID *)2;
  return OwnerFault==2?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI OwnerHandle(EFI_HANDLE h,EFI_GUID *g,VOID **out) {
#ifdef GU_AD104_PARENT_TEST
  if(Stage==5 && ParentFault==21 && h==(VOID *)1 && PfEqual(g,&PciGuidL,sizeof(*g))){*out=NULL;return EFI_NOT_FOUND;}
#endif
  if(Stage>=4 && h==(VOID *)2){AfterFreeReads++;Check(0,"no protocol access on freed child");}
  return EdHandle(h,g,out);
}
static EFI_STATUS EFIAPI OwnerFlush(EFI_FILE_PROTOCOL *f) {
#ifdef GU_AD104_PARENT_TEST
  if(Stage==4 && ++ParentFlushes==2) {
    if(ParentFault==10)return EFI_DEVICE_ERROR;
    if(ParentFault==11)Drivers[3].Stop=(VOID *)((UINTN)OwnerStop+1);
  }
  if(Stage==5 && ++ParentPostFlushes==2) {
    if(ParentFault==14)return EFI_DEVICE_ERROR;
#ifdef GU_AD104_DECODE_READ_TEST
    if(DecodeFault==14)return EFI_DEVICE_ERROR;
#endif
  }
#ifdef GU_AD104_DECODE_READ_TEST
  if(Stage==6) {
    DecodeFlushes++;
    if(DecodeFault==23 && DecodeFlushes==2)Config[1]|=4;
    if((DecodeFault==16 && DecodeFlushes==2) || (DecodeFault==17 && DecodeFlushes==3))return EFI_DEVICE_ERROR;
  }
#endif
#endif
  if(Stage==3 && ++OwnerFlushes==2) {
    if(OwnerFault==9)return EFI_DEVICE_ERROR;
    if(OwnerFault==13)Drivers[3].Stop=(VOID *)((UINTN)OwnerStop+1);
  }
  if(Stage==4 && OwnerFault==12)return EFI_DEVICE_ERROR;
  return FlushFile(f);
}
static EFI_STATUS EFIAPI OwnerKey(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *i,EFI_INPUT_KEY *k) {
  EFI_STATUS s=ReadKey(i,k);
#ifdef GU_AD104_PARENT_TEST
  if(s==EFI_SUCCESS && Stage==5) {
    if(ParentFault==12)Config[1]|=7;
    if(ParentFault==13)k->UnicodeChar='x';
  }
#endif
#ifdef GU_AD104_DECODE_READ_TEST
  if(s==EFI_SUCCESS && Stage==6 && DecodeFault==15)Config[1]|=4;
#endif
  if(s==EFI_SUCCESS && Stage==4) {
    if(OwnerFault==10)Config[1]=3;
    if(OwnerFault==11)k->UnicodeChar='x';
  }
  return s;
}
static CAPTURE *OwnerSetup(int fault) {
  CAPTURE *c=EdSetup(0);OwnerFault=fault;OwnerCalls=AfterFreeReads=OwnerFlushes=0;OwnerChildAttempted=FALSE;
  /* Pin an actual callable mock as this synthetic loaded image's Stop entry. */
  Images[3].ImageBase=(VOID *)(UINTN)OwnerStop;Pins[3].Code=0;Pins[3].Bytes=8;Pins[3].Stop=0;
  UINT8 digest[32];GuSha256((VOID *)(UINTN)OwnerStop,8,digest);
  for(UINTN i=0;i<32;i++){Hashes[3][i*2]="0123456789abcdef"[digest[i]>>4];Hashes[3][i*2+1]="0123456789abcdef"[digest[i]&15];}
  Drivers[3].Stop=OwnerStop;Drivers[4].Stop=(VOID *)((UINTN)OwnerStop+0x2b38);
  Bs.HandleProtocol=OwnerHandle;File.Flush=OwnerFlush;Input.ReadKeyStroke=OwnerKey;return c;
}
int main(void) {
  CAPTURE *c=OwnerSetup(0);
  Check(EdRun(c,(VOID *)1,&Io,Pins)==EFI_SUCCESS && OwnerCalls==1 && !AfterFreeReads,"four-stage quiet handoff qualifies");
  Check(OwnerChildStep(c,(VOID *)1,&Io,Pins,NULL)==EFI_ALREADY_STARTED && OwnerCalls==1,"direct stop retry refused");
  for(int f=1;f<=13;f++) {
    c=OwnerSetup(f);
    Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS,"partial callback, freed handle, edges, PCI, input, logging fail closed");
    Check(OwnerCalls==(f==9 || f==13?0:1),"intent flush failure prevents direct callback");
    Check(!AfterFreeReads,"no freed child protocol queries even after errors");
  }
  c=OwnerSetup(0);GopFault=3;
  Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !OwnerCalls,"GOP failure prevents owner callback");
  c=OwnerSetup(0);ConsumerFault=2;
  Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !OwnerCalls,"EDID failure prevents owner callback");
  printf("PASS direct owner child: %d checks, RAM-only mocks\n",Checks);return 0;
}
