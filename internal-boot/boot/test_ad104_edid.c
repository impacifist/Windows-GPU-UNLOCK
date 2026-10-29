/* GPL-2.0-only. Executes only RAM mocks, never firmware code. */
#pragma push_macro("main")
#undef main
#define main PreflightMain
#include "test_ad104_preflight.c"
#undef main
#pragma pop_macro("main")
#include <IndustryStandard/Acpi.h>
#include "diagnostic.h"
static int Writes,Flushes,Calls,Stage,KeyPhase,Reads,EdFault,Collections;
#ifdef GU_AD104_FALCON_OBSERVE_TEST
static int FalconFault,FalconIntentSeen,FalconSummarySeen;
#ifdef GU_AD104_ENTRY_INPUTS_TEST
static int EntryAuditSeen;
#endif
#ifdef GU_AD104_FALCON_REFUSAL_TEST
static int RecoveryIntentSeen,RecoverySummarySeen;
static EFI_STATUS EFIAPI RefusalAltRead(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH width,UINT8 bar,UINT64 offset,UINTN count,VOID *out){
  (void)io;(void)width;(void)bar;(void)offset;(void)count;(void)out;Check(0,"replacement reader must never run");return EFI_DEVICE_ERROR;
}
#endif
#endif
static EFI_PCI_IO_PROTOCOL Io;
static EFI_FILE_PROTOCOL File;
static EFI_SIMPLE_TEXT_INPUT_PROTOCOL Input;
static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL Output;
static UINT32 Config[64];
static UINT64 CurrentAttributes,SupportedAttributes;
#ifdef GU_AD104_DECODE_READ_TEST
static int DecodeFault,DecodeCalls,MmioCalls,DecodeFlushes;
#ifdef GU_AD104_DECODE_OFF_TEST
static int OffFault,OffCalls,OffIntentSeen;
#ifdef GU_AD104_PROM_HEADER_TEST
static int PromFault,PromIntentSeen,PromOutputSeen;
#ifdef GU_AD104_FWSEC_AUDIT_TEST
static int AuditSeen;
#endif
#endif
#endif
#endif
#ifdef GU_AD104_GOP_TEST
static int GopFault,ProtocolReads;
#ifdef GU_AD104_CONSUMERS_TEST
static int ConsumerFault,ConsumerFlushes;
#endif
#endif
#ifdef GU_AD104_TEXT_TEST
BOOLEAN GuTextOutputDisabled;
/* Test the real lifecycle/graph engine separately from the display collector,
 * which has dedicated real-collector tests in test_text_display.c. */
#define GU_TEXT_DISPLAY_H
typedef struct {UINTN Stable;VOID *Pcd;EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE Mode;VOID *GetBool,*Next,*Size;} TEXT_DISPLAY;
static EFI_STATUS TextDisplayCollect(EFI_SYSTEM_TABLE *t,EFI_HANDLE child,TEXT_DISPLAY *out) {
  (void)t;(void)child;out->Stable=1;out->Pcd=(VOID *)40;
#ifdef GU_AD104_CONSUMERS_TEST
  if(Stage==3 && ConsumerFault==10)out->Stable=2;
#endif
#ifdef GU_AD104_GOP_TEST
  if(Stage>=2 && GopFault==10)out->Stable=2;
#endif
  return EdFault==19?EFI_NOT_READY:EFI_SUCCESS;
}
static EFI_STATUS TextPcdCollect(EFI_SYSTEM_TABLE *t,TEXT_DISPLAY *out) {
  (void)t;out->Pcd=(VOID *)40;return EFI_SUCCESS;
}
static int QuietReady,EarlyDrained;
#endif
VOID Text(CAPTURE *c,const char *s){
#ifdef GU_AD104_FALCON_REFUSAL_TEST
  if(strstr(s,"falcon-observation-result") && FalconFault==655)Io.Mem.Read=RefusalAltRead;
  if(strstr(s,"refusal-recovery-admission") && FalconFault==654)Io.Mem.Read=NULL;
  if(strstr(s,"ad104-falcon recovery-intent="))RecoveryIntentSeen=1;
  if(strstr(s,"recovery-rechecks-complete=1"))RecoverySummarySeen=1;
#endif
#ifdef GU_AD104_FALCON_OBSERVE_TEST
#ifdef GU_AD104_ENTRY_INPUTS_TEST
  if(strstr(s,"ad104-entry-inputs fb="))EntryAuditSeen=1;
#endif
  if(strstr(s,"ad104-falcon intent="))FalconIntentSeen=1;
  if(strstr(s,"ad104-falcon mailbox-dma-stable=") || strstr(s,"ad104-falcon mailbox-only-stable="))FalconSummarySeen=1;
#endif
#ifdef GU_AD104_FWSEC_AUDIT_TEST
  if(strstr(s,"ad104-fwsec-audit prom-match="))AuditSeen=1;
#endif
#ifdef GU_AD104_PROM_HEADER_TEST
  if(strstr(s,"ad104-prom-header intent="))PromIntentSeen=1;
  if(strstr(s,"ad104-prom-header offset="))PromOutputSeen=1;
#ifdef GU_AD104_PROM_FULL_TEST
  if(strstr(s,"ad104-prom-full intent=") && PromFault==37)Config[1]|=4;
#endif
#endif
#ifdef GU_AD104_DECODE_OFF_TEST
  if(strstr(s,"intent=one-disable-io-memory"))OffIntentSeen=1;
#endif
#ifdef GU_AD104_DECODE_READ_TEST
  /* Faithful auto-write behavior with a small buffer to force service calls. */
  while(*s && c->Io==EFI_SUCCESS) {
    if(c->Used==128) {
      UINTN n=c->Used;c->Io=c->File->Write(c->File,&n,c->Buffer);
      if(c->Io==EFI_SUCCESS && n!=c->Used)c->Io=EFI_DEVICE_ERROR;
      c->Used=0;if(c->Io!=EFI_SUCCESS)return;
    }
    c->Buffer[c->Used++]=*s++;
  }
#else
  while(*s && c->Used<sizeof(c->Buffer))c->Buffer[c->Used++]=*s++;
#endif
}
VOID Hex(CAPTURE *c,UINT64 v,UINTN n){(void)v;(void)n;Text(c,"x");}
VOID End(CAPTURE *c){Text(c,"\n");}
#include "ad104-edid.h"
static EFI_STATUS EFIAPI EdHandle(EFI_HANDLE h,EFI_GUID *g,VOID **out) {
#ifdef GU_AD104_GOP_TEST
  EFI_GUID text={0x387477c2,0x69c7,0x11d2,{0x8e,0x39,0,0xa0,0xc9,0x69,0x72,0x3b}};
  EFI_GUID companion={0xe1e4a857,0xc970,0x4075,{0xa4,0xda,0xe9,0xc4,0x1b,0x69,0xad,0xfc}};
  if(h==(VOID *)2 && (PfEqual(g,&text,sizeof(*g)) || PfEqual(g,&companion,sizeof(*g)))) {
    BOOLEAN extra=PfEqual(g,&companion,sizeof(*g));*out=NULL;
    if(Stage>=2 && GopFault==11)return EFI_DEVICE_ERROR;
    if((Stage>=2 && !(GopFault==4 && extra)) || (GopFault==1 && extra))return EFI_UNSUPPORTED;
    if(!extra)ProtocolReads++;
    UINTN shift=GopFault==12 && ProtocolReads>=2?0x1000:0;
    *out=(VOID *)(UINTN)((extra?(GopFault==2?0x10102:0x10101):0x10000)+shift);return EFI_SUCCESS;
  }
#endif
  if(h==(VOID *)1 && PfEqual(g,&PciGuidL,sizeof(*g))){*out=&Io;return EFI_SUCCESS;}
  return HandleMock(h,g,out);
}
static EFI_STATUS EFIAPI ReadConfig(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 off,UINTN count,VOID *p) {
  (void)io;(void)w;Collections++;memcpy(p,(UINT8 *)Config+off,count*4);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Location(EFI_PCI_IO_PROTOCOL *io,UINTN *a,UINTN *b,UINTN *c,UINTN *d) {
  (void)io;*a=0;*b=11;*c=*d=0;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Bar(EFI_PCI_IO_PROTOCOL *io,UINT8 index,UINT64 *support,VOID **out) {
  (void)io;(void)index;UINT8 *b=calloc(1,sizeof(EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR)+2);
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *d=(VOID *)b;
  d->Desc=ACPI_ADDRESS_SPACE_DESCRIPTOR;d->Len=sizeof(*d)-3;
  d->AddrSpaceGranularity=32;d->AddrRangeMin=0xfb000000;d->AddrRangeMax=0xfbffffff;d->AddrLen=0x1000000;
  b[sizeof(*d)]=ACPI_END_TAG_DESCRIPTOR;*out=b;*support=0;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Attributes(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION op,UINT64 value,UINT64 *out) {
  (void)io;(void)value;
  if(op==EfiPciIoAttributeOperationGet){*out=CurrentAttributes;return EFI_SUCCESS;}
  if(op==EfiPciIoAttributeOperationSupported){*out=SupportedAttributes;return EFI_SUCCESS;}
#ifdef GU_AD104_DECODE_READ_TEST
#ifdef GU_AD104_DECODE_OFF_TEST
  if(op==EfiPciIoAttributeOperationDisable) {
    Check(value==0x300 && !out && DecodeOffAttempted && MmioCalls==
#ifdef GU_AD104_FALCON_OBSERVE_TEST
#ifdef GU_AD104_FALCON_REFUSAL_TEST
    (FalconRecoveryEligible?262343:262320+2*FalconWords)
#else
    262320+2*FalconWords
#endif
#elif defined(GU_AD104_PROM_FULL_TEST)
    262320
#elif defined(GU_AD104_PROM_HEADER_TEST)
    174
#else
    44
#endif
    ,"one disable only after complete capture");
    OffCalls++;if(OffFault==1)return EFI_DEVICE_ERROR;
    Stage=7;Config[1]&=~3U;CurrentAttributes=0;
    if(OffFault==3)Config[1]|=4;
    if(OffFault==4)CurrentAttributes=0x100;
    if(OffFault==5)Config[4]++;
    return OffFault==2?EFI_DEVICE_ERROR:EFI_SUCCESS;
  }
#endif
  if(op==EfiPciIoAttributeOperationEnable) {
    Check(value==0x300 && !out && DecodeAttempted,"only one explicit IO/MEM Enable300");
    DecodeCalls++;if(DecodeFault==1)return EFI_DEVICE_ERROR;
    Stage=6;Config[1]|=3;CurrentAttributes=DecodeFault==3?0:0x300;
    if(DecodeFault==4)Config[1]|=4;
    if(DecodeFault==5)CurrentAttributes|=0x18;
    if(DecodeFault==6)Config[4]++;
    if(DecodeFault==7)Io.Mem.Read=NULL;
    if(DecodeFault==8)Io.Attributes=NULL;
    return DecodeFault==2?EFI_DEVICE_ERROR:EFI_SUCCESS;
  }
#endif
  Check(0,"attribute write forbidden");return EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI Write(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *p) {
  (void)f;(void)n;(void)p;Writes++;
#ifdef GU_AD104_FWSEC_AUDIT_TEST
  if(AuditSeen && PromFault==55)return EFI_DEVICE_ERROR;
  if(AuditSeen && PromFault==56)Config[1]|=4;
  if(AuditSeen && PromFault==57)Missing=-1;
#endif
#ifdef GU_AD104_PROM_HEADER_TEST
  if(PromOutputSeen && PromFault==14)return EFI_DEVICE_ERROR;
  if(PromOutputSeen && PromFault==15)Config[1]|=4;
#endif
#ifdef GU_AD104_DECODE_OFF_TEST
  if(Stage==6 && MmioCalls==44 && OffFault==6)return EFI_DEVICE_ERROR;
  if(Stage==7 && OffFault==7)return EFI_DEVICE_ERROR;
  if(Stage==7 && OffFault==8)Config[1]|=4;
  if(Stage==6 && MmioCalls==44 && OffFault==9)Io.Attributes=NULL;
#endif
#ifdef GU_AD104_DECODE_READ_TEST
  if(Stage==6 && MmioCalls==22) {
    if(DecodeFault==24)return EFI_DEVICE_ERROR;
    if(DecodeFault==25)Config[1]|=4;
  }
#endif
#ifdef GU_AD104_CONSUMERS_TEST
  if(Stage==3 && ConsumerFault==8)return EFI_DEVICE_ERROR;
#endif
return EdFault==1 || (EdFault==15 && Stage)?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI FlushFile(EFI_FILE_PROTOCOL *f){(void)f;Flushes++;
#ifdef GU_AD104_ENTRY_INPUTS_TEST
  if(EntryAuditSeen){if(FalconFault==801)return EFI_DEVICE_ERROR;if(FalconFault==802)Config[1]|=4;if(FalconFault==803)Io.Mem.Read=NULL;if(FalconFault==804)Missing=-1;}
#endif
#ifdef GU_AD104_FALCON_REFUSAL_TEST
  if(RecoveryIntentSeen && MmioCalls==262329){if(FalconFault==624)return EFI_DEVICE_ERROR;if(FalconFault==650)Config[1]|=4;}
  if(RecoveryIntentSeen && MmioCalls==262336){if(FalconFault==651)Config[1]|=4;if(FalconFault==652)Missing=-1;}
  if(RecoverySummarySeen && FalconFault==625)Config[1]|=4;
#endif
#ifdef GU_AD104_FALCON_OBSERVE_TEST
  if(FalconIntentSeen && MmioCalls==262320){
    if(FalconFault==30)return EFI_DEVICE_ERROR;
    if(FalconFault==31)Config[1]|=4;
    if(FalconFault==32)Io.Mem.Read=NULL;
    if(FalconFault==33)Missing=-1;
  }
  if(FalconIntentSeen && MmioCalls==262320+FalconWords){
    if(FalconFault==34)return EFI_DEVICE_ERROR;
    if(FalconFault==35)Config[1]|=4;
    if(FalconFault==36)Missing=-1;
    if(FalconFault==37)Io.Mem.Read=NULL;
  }
  if(FalconSummarySeen){if(FalconFault==38)Config[1]|=4;if(FalconFault==39)return EFI_DEVICE_ERROR;}
#endif
#ifdef GU_AD104_PROM_HEADER_TEST
  if(PromIntentSeen && !PromHeaderAttempted) {
    if(PromFault==10)return EFI_DEVICE_ERROR;
    if(PromFault==11)Config[1]|=4;
    if(PromFault==12)Io.Mem.Read=NULL;
    if(PromFault==13)Missing=-1;
  }
#endif
#ifdef GU_AD104_DECODE_OFF_TEST
  if(OffIntentSeen && !OffCalls) {
    if(OffFault==11)return EFI_DEVICE_ERROR;
    if(OffFault==12)Config[1]|=4;
    if(OffFault==13)Io.Attributes=NULL;
  }
#endif
#ifdef GU_AD104_CONSUMERS_TEST
  if(Stage==2 && ++ConsumerFlushes==2 && ConsumerFault==9)return EFI_DEVICE_ERROR;
#endif
#ifdef GU_AD104_GOP_TEST
  if(GopFault==8 && Stage==2)return EFI_DEVICE_ERROR;
#endif
  return EdFault==2 || (EdFault==16 && Stage)?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI Reset(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *i,BOOLEAN b){(void)i;(void)b;Check(0,"no reset");return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI ReadKey(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *i,EFI_INPUT_KEY *k) {
  (void)i;Reads++;
#ifdef GU_AD104_GOP_TEST
  if(GopFault==9 && Stage==2 && KeyPhase==2)return EFI_NOT_READY;
#endif
  if(!KeyPhase || EdFault==3 || (EdFault==8 && KeyPhase==2))return EFI_NOT_READY;
  k->ScanCode=0;k->UnicodeChar=(CHAR16)(KeyPhase==1?
#ifdef GU_AD104_TEXT_TEST
#ifdef GU_AD104_GOP_TEST
    'G'
#else
    'T'
#endif
#else
    'E'
#endif
    :'V');
  if(EdFault==4)k->UnicodeChar='x';
#ifdef GU_AD104_DECODE_OFF_TEST
  if(Stage==6 && OffFault==10)k->UnicodeChar='x';
#endif
  if(EdFault==18 && KeyPhase==2)Config[1]=3;
  KeyPhase=0;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Print(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *o,CHAR16 *s) {
#ifdef GU_AD104_TEXT_TEST
  Check(!GuTextOutputDisabled,"no text output after detach attempt");
#endif
  (void)o;(void)s;KeyPhase=Stage?2:1;
  if(EdFault==10 && !Stage)Config[3]++;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Stall(UINTN us){(void)us;
#ifdef GU_AD104_DELAYED_READ_TEST
  if(us==1000000) {
    if(OffFault==14)return EFI_DEVICE_ERROR;
    if(OffFault==15)Config[1]|=4;
    if(OffFault==16)Missing=-1;
    if(OffFault==17)Io.Mem.Read=NULL;
  }
#endif
#ifdef GU_AD104_TEXT_TEST
  if(us==100000 && Stage && !QuietReady){KeyPhase=2;QuietReady=1;}
#endif
  return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Disconnect(EFI_HANDLE child,EFI_HANDLE agent,EFI_HANDLE deeper) {
#ifdef GU_AD104_GOP_TEST
#ifdef GU_AD104_CONSUMERS_TEST
  if(Calls==2) {
    Check(child==(VOID *)2 && agent==(VOID *)10 && !deeper,"only IMG08 EDID third");
    Check(ConsumerEdidAttempted && Flushes>=4,"third intent persisted");
    Calls++;Stage=3;
    if(ConsumerFault==1)return EFI_DEVICE_ERROR;
    FourthMissing=ConsumerFault==3?-1:5;Data[0][0x546a]=ConsumerFault==4?1:0;
    if(ConsumerFault==5)*(UINT64 *)(Data[0]+0x5530)=0;
    if(ConsumerFault==6)Config[1]=3;
    if(ConsumerFault==7)FourthMissing=6;
    if(ConsumerFault==11)Input.WaitForKey=(VOID *)2;
    return ConsumerFault==2?EFI_DEVICE_ERROR:EFI_SUCCESS;
  }
#endif
  if(Calls==1) {
    Check(child==(VOID *)2 && agent==(VOID *)12 && !deeper,"only IMG17 consumer after text");
    Check(GuTextOutputDisabled && GopAttempted && Flushes>=3,"quiet one-shot GOP intent persisted");
    Calls++;Stage=2;ThirdMissing=GopFault==5?-1:4;
    if(GopFault==6)Config[1]=3;
    if(GopFault==7)Io.Pci.Read=NULL;
    return GopFault==3?EFI_DEVICE_ERROR:EFI_SUCCESS;
  }
#endif
  Check(child==(VOID *)2 && agent==(VOID *)
#ifdef GU_AD104_TEXT_TEST
    11
#else
    10
#endif
    && !deeper,"only exact requested binding/child");
  Check(Flushes>=2 && Writes>=2 && EdAttempted,"durable intent and attempted before callback");
  Calls++;Stage=1;
#ifdef GU_AD104_TEXT_TEST
  Check(GuTextOutputDisabled,"output disabled before firmware call");
  Missing=2;ExtraMissing=3;KeyPhase=2;EarlyDrained=1;
  *(UINT64 *)(Data[1]+0x7160)=0;*(UINT64 *)(Data[1]+0x7168)=0;*(UINT64 *)(Data[1]+0x7170)=0;
  *(UINT64 *)(Data[1]+0x7040)=40;
  if(EdFault==24)*(UINT64 *)(Data[1]+0x7160)=1;
  if(EdFault==25)*(UINT64 *)(Data[1]+0x7040)=0;
#else
  Missing=5;Data[0][0x546a]=0;
#endif
  if(EdFault==11 || EdFault==12){Io.GetBarAttributes=NULL;return EdFault==12?EFI_DEVICE_ERROR:EFI_SUCCESS;}
  if(EdFault==13 || EdFault==14){Io.Pci.Read=NULL;return EdFault==14?EFI_DEVICE_ERROR:EFI_SUCCESS;}
  if(EdFault==5)return EFI_DEVICE_ERROR;
  if(EdFault==6)Missing=6;
  if(EdFault==7)Config[1]=3;
  if(EdFault==9)Data[0][0x546a]=
#ifdef GU_AD104_TEXT_TEST
    0;
#else
    1;
#endif
  if(EdFault==17)Missing=-1;
  return EFI_SUCCESS;
}
static CAPTURE *EdSetup(int fault) {
  Setup(0,-1);CurrentAttributes=0x718;SupportedAttributes=0x1b77b;EdFault=fault;EdAttempted=FALSE;Writes=Flushes=Calls=Stage=KeyPhase=Reads=Collections=0;
#ifdef GU_AD104_GOP_TEST
  GopAttempted=FALSE;GopFault=ProtocolReads=0;
#ifdef GU_AD104_CONSUMERS_TEST
  ConsumerEdidAttempted=FALSE;ConsumerFault=ConsumerFlushes=0;
#endif
#endif
#ifdef GU_AD104_TEXT_TEST
  GuTextOutputDisabled=FALSE;QuietReady=EarlyDrained=0;
  *(UINT64 *)(Data[1]+0x7160)=1;*(UINT64 *)(Data[1]+0x7168)=99;*(UINT64 *)(Data[1]+0x7170)=99;
  *(UINT64 *)(Data[1]+0x6f60)=3;*(UINT64 *)(Data[1]+0x7040)=0;
  if(fault==20)*(UINT64 *)(Data[1]+0x7160)=2;
  if(fault==21)*(UINT64 *)(Data[1]+0x7170)=98;
  if(fault==22)*(UINT64 *)(Data[1]+0x6f60)=4;
  if(fault==23)*(UINT64 *)(Data[1]+0x7040)=41;
#endif
  Bs.HandleProtocol=EdHandle;Bs.DisconnectController=Disconnect;Bs.Stall=Stall;
  memset(&Io,0,sizeof(Io));Io.Pci.Read=ReadConfig;Io.GetLocation=Location;Io.GetBarAttributes=Bar;Io.Attributes=Attributes;
  memset(Config,0,sizeof(Config));Config[0]=0x278210de;Config[1]=0x100007;Config[2]=0x030000a1;
  Config[4]=0xfb000000;Config[11]=0x51321462;
  memset(&File,0,sizeof(File));File.Write=Write;File.Flush=FlushFile;
  memset(&Input,0,sizeof(Input));Input.Reset=Reset;Input.ReadKeyStroke=ReadKey;Input.WaitForKey=(VOID *)1;
  memset(&Output,0,sizeof(Output));Output.OutputString=Print;
  T.ConIn=&Input;T.ConOut=T.StdErr=&Output;
  Data[0][0x546a]=1;*(UINT64 *)(Data[0]+0x5530)=1;*(UINT64 *)(Data[0]+0x5550)=1;
  static CAPTURE c;memset(&c,0,sizeof(c));c.Table=&T;c.File=&File;c.Io=EFI_SUCCESS;return &c;
}
int main(void) {
  CAPTURE *c=EdSetup(0);
  Check(EdRun(c,(VOID *)1,&Io,Pins)==EFI_SUCCESS,"one exact transition permits Windows consideration");
  Check(Calls==1 && Stage==1,"single invocation");
#ifdef GU_AD104_TEXT_TEST
  Check(EarlyDrained && QuietReady,"early queued V discarded and later fresh V accepted");
#endif
  Check(EdRun(c,(VOID *)1,&Io,Pins)==EFI_ALREADY_STARTED && Calls==1,"retry forbidden");
  for(int f=1;f<=18;f++) {
    c=EdSetup(f);Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS,"logging/input/callback/post failure blocks Windows");
    Check(Calls==(f<=4 || f==10?0:1),"precondition failures never invoke callback");
    if(f>=5 && f!=10)Check(Collections>=(f==13 || f==14?3:5),"callback errors and post failures still run independent collections");
  }
  c=EdSetup(0);Config[0]^=1;Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !Calls,"wrong GPU rejected");
  c=EdSetup(0);Data[1][0x100]^=1;Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !Calls,"wrong code rejected");
  c=EdSetup(0);Missing=7;Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !Calls,"missing pre-edge rejected");
  c=EdSetup(0);T.ConIn=NULL;Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !Calls,"no input rejected");
#ifdef GU_AD104_TEXT_TEST
  c=EdSetup(19);Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !Calls,"missing captured display admission prevents text attempt");
  for(int f=20;f<=25;f++) {
    c=EdSetup(f);Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS,"wrong text list/console/cache state rejected");
    Check(Calls==(f>=24),"invalid pre-state never reaches text callback");
  }
#endif
  printf("PASS " ED_LOG " lifecycle: %d checks, RAM-only mocks\n",Checks);return 0;
}
