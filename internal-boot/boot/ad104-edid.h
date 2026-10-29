/* GPL-2.0-only. One attended consumer disconnect; no PCI owner release or unlock. */
#ifdef GU_AD104_TEXT_TEST
#include "text-display.h"
#ifdef GU_AD104_GOP_TEST
#define ED_LOG "ad104-display"
#else
#define ED_LOG "ad104-text"
#endif
#else
#define ED_LOG "ad104-edid"
#endif
typedef struct {
  PF_STATE Drivers;
  UINT32 Config[64];
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR Bar;
  UINT64 BarSupport,Attributes,Supported;
  EFI_HANDLE InputHandle,OutputHandle,ErrorHandle;
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL *Input;
  EFI_INPUT_RESET Reset;
  EFI_INPUT_READ_KEY Read;
  EFI_EVENT Wait;
  VOID *Output,*Error;
#ifdef GU_AD104_TEXT_TEST
  TEXT_DISPLAY Display;
  UINT64 TextCount,TextFirst,TextLast,TextConsole,TextPcd;
#endif
} ED_STATE;
static BOOLEAN EdAttempted;

static EFI_STATUS EdFlush(CAPTURE *c) {
  if(c->Io!=EFI_SUCCESS)return c->Io;
  if(c->Used) {
    UINTN n=c->Used;c->Io=c->File->Write(c->File,&n,c->Buffer);
    if(c->Io==EFI_SUCCESS && n!=c->Used)c->Io=EFI_DEVICE_ERROR;
    c->Used=0;
  }
  if(c->Io==EFI_SUCCESS)c->Io=c->File->Flush(c->File);
  return c->Io;
}
static VOID EdStatus(CAPTURE *c,const char *stage,EFI_STATUS s) {
  Text(c,ED_LOG " stage=");Text(c,stage);Text(c," status=0x");Hex(c,s,16);End(c);
}
static VOID EdDescribe(CAPTURE *c,const ED_STATE *s,const char *stage) {
  Text(c,ED_LOG "-state stage=");Text(c,stage);
  Text(c," edges=0x");Hex(c,s->Drivers.MatchedEdges,2);
  Text(c," shared-byte=0x");Hex(c,s->Drivers.SharedFlag,2);
  Text(c," pointer-count1=0x");Hex(c,s->Drivers.PointerCounts[0],16);
  Text(c," pointer-count2=0x");Hex(c,s->Drivers.PointerCounts[1],16);
  Text(c," attributes=0x");Hex(c,s->Attributes,16);
  Text(c," supported=0x");Hex(c,s->Supported,16);End(c);
  Text(c,ED_LOG "-cfg stage=");Text(c,stage);Text(c," dwords=");
  for(UINTN i=0;i<64;i++){Hex(c,s->Config[i],8);Text(c," ");}End(c);
#ifdef GU_AD104_TEXT_TEST
  Text(c,"ad104-text-loaded-state stage=");Text(c,stage);
  Text(c," list-count=0x");Hex(c,s->TextCount,16);Text(c," list-first=0x");Hex(c,s->TextFirst,16);
  Text(c," list-last=0x");Hex(c,s->TextLast,16);Text(c," cached-console=0x");Hex(c,s->TextConsole,16);
  Text(c," cached-pcd=0x");Hex(c,s->TextPcd,16);End(c);
  Text(c,"ad104-text-display stage=");Text(c,stage);Text(c," gop=0x");Hex(c,(UINTN)s->Display.Gop,16);
  Text(c," mode=0x");Hex(c,s->Display.Mode.Mode,8);Text(c," framebuffer=0x");Hex(c,s->Display.Mode.FrameBufferBase,16);
  Text(c," framebuffer-bytes=0x");Hex(c,s->Display.Mode.FrameBufferSize,16);End(c);
#endif
}
static EFI_STATUS EdCollectMode(EFI_SYSTEM_TABLE *t,EFI_HANDLE gpu,EFI_PCI_IO_PROTOCOL *io,
                            const PF_PIN *pins,BOOLEAN post,UINTN removedMask,EFI_HANDLE knownChild,BOOLEAN noGop,ED_STATE *out) {
  UINT8 *z=(VOID *)out;for(UINTN i=0;i<sizeof(*out);i++)z[i]=0;
  EFI_STATUS first=PfDrivers(t,pins,&out->Drivers);
  #ifdef GU_AD104_TEXT_TEST
  if(first==EFI_SUCCESS)first=PfGraphKnown(t,gpu,&out->Drivers,post?removedMask:0,knownChild);
  EFI_STATUS display=noGop?TextPcdCollect(t,&out->Display):
    (out->Drivers.Child?TextDisplayCollect(t,out->Drivers.Child,&out->Display):EFI_NOT_READY);
  if(first==EFI_SUCCESS)first=display;
  if(out->Drivers.Base[1] && pins[1].Size>=0x7178) {
    const volatile UINT8 *base=out->Drivers.Base[1];
    out->TextCount=*(const volatile UINT64 *)(base+0x7160);
    out->TextFirst=*(const volatile UINT64 *)(base+0x7168);out->TextLast=*(const volatile UINT64 *)(base+0x7170);
    out->TextConsole=*(const volatile UINT64 *)(base+0x6f60);out->TextPcd=*(const volatile UINT64 *)(base+0x7040);
    if(out->TextConsole!=(UINTN)t->ConsoleOutHandle ||
       (!post && (out->TextCount!=1 || !out->TextFirst || out->TextFirst!=out->TextLast ||
                  (out->TextPcd && out->TextPcd!=(UINTN)out->Display.Pcd))) ||
       (post && (out->TextCount || out->TextFirst || out->TextLast || out->TextPcd!=(UINTN)out->Display.Pcd)))
      if(first==EFI_SUCCESS)first=EFI_NOT_READY;
  } else if(first==EFI_SUCCESS)first=EFI_NOT_READY;
#else
  (void)noGop;
  if(first==EFI_SUCCESS)first=PfGraphKnown(t,gpu,&out->Drivers,post?removedMask:0,knownChild);
#endif
  /* Continue independent observations even when the graph is unexpected. */
  EFI_PCI_IO_PROTOCOL *fresh=NULL;UINTN loc[4]={0};
  EFI_STATUS s=t->BootServices->HandleProtocol(gpu,&PciGuidL,(VOID **)&fresh);
  if(s!=EFI_SUCCESS || fresh!=io)return first!=EFI_SUCCESS?first:EFI_NOT_READY;
  s=io->Pci.Read?io->Pci.Read(io,EfiPciIoWidthUint32,0,64,out->Config):EFI_UNSUPPORTED;
  if(s!=EFI_SUCCESS && first==EFI_SUCCESS)first=s;
  s=io->GetLocation?io->GetLocation(io,&loc[0],&loc[1],&loc[2],&loc[3]):EFI_UNSUPPORTED;
  if((s!=EFI_SUCCESS || loc[0] || loc[1]!=11 || loc[2] || loc[3]) && first==EFI_SUCCESS)first=EFI_NOT_READY;
  VOID *raw=NULL;
  s=io->GetBarAttributes?io->GetBarAttributes(io,0,&out->BarSupport,&raw):EFI_UNSUPPORTED;
  if(s==EFI_SUCCESS && raw) {
    EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *d=raw;
    if(d->Desc==ACPI_ADDRESS_SPACE_DESCRIPTOR && d->Len==sizeof(*d)-3) {
      UINT8 sum=0;const UINT8 *end=(const UINT8 *)(d+1);
      for(UINTN i=0;i<sizeof(*d)+2;i++)sum=(UINT8)(sum+((UINT8 *)raw)[i]);
      if(end[0]==ACPI_END_TAG_DESCRIPTOR && (!end[1] || !sum))out->Bar=*d;
      else if(first==EFI_SUCCESS)first=EFI_UNSUPPORTED;
    } else if(first==EFI_SUCCESS)first=EFI_UNSUPPORTED;
  } else if(first==EFI_SUCCESS)first=EFI_NOT_READY;
  if(raw && t->BootServices->FreePool(raw)!=EFI_SUCCESS && first==EFI_SUCCESS)first=EFI_DEVICE_ERROR;
  s=io->Attributes?io->Attributes(io,EfiPciIoAttributeOperationGet,0,&out->Attributes):EFI_UNSUPPORTED;
  if(s!=EFI_SUCCESS && first==EFI_SUCCESS)first=s;
  s=io->Attributes?io->Attributes(io,EfiPciIoAttributeOperationSupported,0,&out->Supported):EFI_UNSUPPORTED;
  if(s!=EFI_SUCCESS && first==EFI_SUCCESS)first=s;
  out->InputHandle=t->ConsoleInHandle;out->OutputHandle=t->ConsoleOutHandle;out->ErrorHandle=t->StandardErrorHandle;
  out->Input=t->ConIn;out->Output=t->ConOut;out->Error=t->StdErr;
  if(t->ConIn){out->Reset=t->ConIn->Reset;out->Read=t->ConIn->ReadKeyStroke;out->Wait=t->ConIn->WaitForKey;}
  if(!out->Input || !out->Reset || !out->Read || !out->Wait || !out->Output || !out->Error)
    if(first==EFI_SUCCESS)first=EFI_UNSUPPORTED;
  return first;
}
static EFI_STATUS EdCollectMask(EFI_SYSTEM_TABLE *t,EFI_HANDLE gpu,EFI_PCI_IO_PROTOCOL *io,
                            const PF_PIN *pins,BOOLEAN post,UINTN removedMask,ED_STATE *out) {
  return EdCollectMode(t,gpu,io,pins,post,removedMask,NULL,FALSE,out);
}
static EFI_STATUS EdCollect(EFI_SYSTEM_TABLE *t,EFI_HANDLE gpu,EFI_PCI_IO_PROTOCOL *io,
                            const PF_PIN *pins,BOOLEAN post,ED_STATE *out) {
  return EdCollectMask(t,gpu,io,pins,post,
#ifdef GU_AD104_TEXT_TEST
    0x0c,
#else
    0x20,
#endif
    out);
}
static BOOLEAN EdInitial(const ED_STATE *s) {
  return s->Config[0]==0x278210deU && s->Config[11]==0x51321462U && s->Config[2]==0x030000a1U &&
    (s->Config[1]&0xffffU)==7 && s->Config[4]==0xfb000000U && s->Attributes==0x718 && s->Supported==0x1b77b &&
    !s->BarSupport && !s->Bar.ResType && s->Bar.AddrSpaceGranularity==32 &&
    s->Bar.AddrRangeMin==0xfb000000 && s->Bar.AddrRangeMax==0xfbffffff &&
    s->Bar.AddrLen==0x1000000 && !s->Bar.AddrTranslationOffset &&
    s->Drivers.MatchedEdges==8 && s->Drivers.SharedFlag==1 &&
    s->Drivers.PointerCounts[0]==1 && s->Drivers.PointerCounts[1]==1;
}
static BOOLEAN EdPost(const ED_STATE *before,const ED_STATE *after) {
  ED_STATE expected=*before;
#ifdef GU_AD104_TEXT_TEST
  expected.Drivers.MatchedEdges=6;
  expected.TextCount=expected.TextFirst=expected.TextLast=0;expected.TextPcd=(UINTN)before->Display.Pcd;
#else
  expected.Drivers.MatchedEdges=7;expected.Drivers.SharedFlag=0;
#endif
  return PfEqual(&expected,after,sizeof(expected));
}
static EFI_STATUS EdKey(EFI_SYSTEM_TABLE *t,CHAR16 wanted,const CHAR16 *prompt) {
  if(!t->ConIn || !t->ConIn->ReadKeyStroke || (prompt && (!t->ConOut || !t->ConOut->OutputString)) || !t->BootServices->Stall)
    return EFI_UNSUPPORTED;
  EFI_INPUT_KEY k;EFI_STATUS s=EFI_SUCCESS;
  /* Bounded drain prevents a held/queued key from authorizing a later stage. */
  for(UINTN i=0;i<32;i++) {
    s=t->ConIn->ReadKeyStroke(t->ConIn,&k);
    if(s==EFI_NOT_READY)break;
    if(s!=EFI_SUCCESS)return s;
  }
  if(s!=EFI_NOT_READY)return EFI_NOT_READY;
  if(prompt){s=t->ConOut->OutputString(t->ConOut,(CHAR16 *)prompt);if(s!=EFI_SUCCESS)return s;}
  for(UINTN i=0;i<600;i++) {
    s=t->ConIn->ReadKeyStroke(t->ConIn,&k);
    if(s==EFI_SUCCESS)return !k.ScanCode && k.UnicodeChar==wanted?EFI_SUCCESS:EFI_ABORTED;
    if(s!=EFI_NOT_READY)return s;
    s=t->BootServices->Stall(100000);if(s!=EFI_SUCCESS)return s;
  }
  return EFI_TIMEOUT;
}
#ifdef GU_AD104_GOP_TEST
#include "gop-step.h"
#ifdef GU_AD104_CONSUMERS_TEST
#include "edid-step.h"
#endif
#endif
#ifdef GU_AD104_OWNER_CHILD_TEST
#include "owner-child.h"
#endif
static EFI_STATUS EdRun(CAPTURE *c,EFI_HANDLE gpu,EFI_PCI_IO_PROTOCOL *io,const PF_PIN *pins) {
  EFI_SYSTEM_TABLE *t=c->Table;static ED_STATE a,b,p,q;
  if(EdAttempted)return EFI_ALREADY_STARTED;
  if(!io || !io->Pci.Read || !io->GetLocation || !io->GetBarAttributes || !io->Attributes ||
     !t->BootServices->DisconnectController)return EFI_UNSUPPORTED;
  EFI_STATUS s=EdCollect(t,gpu,io,pins,FALSE,&a);
  EdDescribe(c,&a,"initial");EdStatus(c,"initial",s);
  if(s!=EFI_SUCCESS || !EdInitial(&a))return EFI_NOT_READY;
#ifdef GU_AD104_TEXT_TEST
#ifdef GU_AD104_GOP_TEST
  #ifdef GU_AD104_CONSUMERS_TEST
#ifdef GU_AD104_OWNER_CHILD_TEST
#ifdef GU_AD104_PARENT_TEST
#ifdef GU_AD104_DECODE_READ_TEST
#ifdef GU_AD104_DECODE_OFF_TEST
#ifdef GU_AD104_FWSEC_AUDIT_TEST
  if(t->ConOut && t->ConOut->OutputString)
    t->ConOut->OutputString(t->ConOut,L"RAM AUDIT ONLY: parse/hash captured FWSEC bytes.\r\nNo firmware execution or cold-entry approval.\r\nAudit mismatch alone does not block recovery.\r\n");
#endif
#ifdef GU_AD104_PROM_FULL_TEST
  s=EdKey(t,'G',L"COMBINED FULL PROM CAPTURE TEST\r\nFive stops, decode enable, delayed registers,\r\n512-byte header, then full 1 MiB PROM window.\r\n262144 full-window DWORD reads plus bookends.\r\nSave/readback/hash; then decode-off and Windows.\r\nNew MMIO may hang. Recovery is unproven.\r\nNo reset, firmware execution, unlock or retry.\r\nShift+G accepts ALL stages.\r\nScreen stops updating. Capture can take longer.\r\nWait 10 sec; tap Shift+V every 2 sec up to 3 min.\r\nFresh V permits decode-off then Windows.\r\nNo Windows after 10 min: power off, remove USB,\r\nboot normal Windows Boot Manager. Run once.\r\n");
#else
#ifdef GU_AD104_PROM_HEADER_TEST
  if(t->ConOut && t->ConOut->OutputString)
    t->ConOut->OutputString(t->ConOut,L"NEW: 512-byte BAR PROM header at 0x300000.\r\n128 new reads may have side effects or hang.\r\nNot a full ROM dump. Recovery unproven.\r\n");
#endif
#ifdef GU_AD104_DELAYED_READ_TEST
  if(t->ConOut && t->ConOut->OutputString)
    t->ConOut->OutputString(t->ConOut,L"DELAYED STABILITY: nominal 1-second batch gap.\r\nSame registers and writes as test 18.\r\nTests stability; no cold-entry authorization.\r\n");
#endif
  s=EdKey(t,'G',L"CAPTURE THEN DECODE-OFF TEST\r\nNo unlock. Recovery is unproven.\r\nFive stops, IO/MEM enable, two read batches.\r\nThen ONE IO/MEM Disable(0x300).\r\nBME/VGA stay off. No reset or rollback.\r\nVerify exact post-parent state before Windows.\r\nShift+G accepts ALL stages.\r\nDisplay stops updating after G.\r\nWait 5 sec; tap Shift+V every 2 sec for 1 min.\r\nV authorizes decode-off then Windows.\r\nNo Windows after 5 min: power off, remove USB,\r\nboot normal Windows Boot Manager.\r\nRun once. Return captures before retry.\r\n");
#endif
#else
  s=EdKey(t,'G',L"REGISTER + WINDOWS HANDOFF TEST\r\nNo unlock. Recovery is unproven.\r\nFive stops; enable IO/MEM, keep BME off.\r\nRead 21 registers twice; check chip ID.\r\nSkip 0x840100. Keep all error guards.\r\n\r\nShift+G: accept and start all stages.\r\nThen the display stops updating.\r\nA blinking cursor does NOT show progress.\r\nWait 5 seconds; tap Shift+V every 2 seconds.\r\nDo this for up to ONE minute.\r\nV works only after every check passes.\r\n\r\nNo Windows after FIVE minutes?\r\nPower off manually. Remove this USB.\r\nBoot normal Windows Boot Manager.\r\nNo automatic reset, retry or rollback.\r\nReturn captures before another attempt.\r\n");
#endif
#else
  s=EdKey(t,'G',L"PARENT STOP TEST: prior four stages THEN direct IMG29 zero-child Stop. May disable IO/MEM/BME/VGA. No restore/unlock. Recovery unproven.\r\nG permits all FIVE stages. Display/input/Windows may fail. Wait five seconds then tap uppercase V every two seconds up to one minute. No second prompt.\r\n");
#endif
#else
  s=EdKey(t,'G',L"OWNER CHILD TEST: text, GOP consumer, EDID, then DIRECT IMG29 one-child Stop. No parent Stop. Recovery unproven.\r\nG permits all FOUR stages. GOP may be freed; legacy video may reset. Wait five seconds then tap uppercase V every two seconds up to one minute. No second prompt.\r\n");
#endif
#else
  s=EdKey(t,'G',L"CONSUMER TEST: text THEN GOP consumer THEN EDID detach. PCI owner retained. Recovery unproven.\r\nUppercase G permits all THREE stages. Wait five seconds then tap uppercase V every two seconds up to one minute. No second prompt. Any other key stops.\r\n");
#endif
#else
  s=EdKey(t,'G',L"DISPLAY TEST: PCD captured. Text detach THEN one GOP-consumer detach. PCI owner retained. Recovery unproven.\r\nPress uppercase G to permit BOTH stages. Then wait five seconds and tap uppercase V every two seconds up to one minute until Windows starts.\r\nNo second prompt; screen may blank. Any other key stops.\r\n");
#endif
#else
  s=EdKey(t,'T',L"TEXT TEST: PCD capture saved; display/input may be lost and recovery is unproven.\r\nPress uppercase T for ONE text disconnect. After T, wait five seconds, then tap uppercase V every two seconds until Windows starts (up to one minute).\r\nThere is no further prompt. Early queued keys are discarded; a fresh V is required after postchecks. Any other key stops.\r\n");
#endif
#else
  s=EdKey(t,'E',L"EDID TEST: display/input may be lost; manual power-off may be needed.\r\nPress uppercase E within 60 seconds to attempt ONE EDID disconnect. Any other key cancels.\r\n");
#endif
  EdStatus(c,"input-before",s);if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  if(s!=EFI_SUCCESS)return s;
  s=EdCollect(t,gpu,io,pins,FALSE,&b);
  if(s!=EFI_SUCCESS || !PfEqual(&a,&b,sizeof(a)))return EFI_NOT_READY;
#ifdef GU_AD104_TEXT_TEST
  Text(c,"ad104-text intent=one-disconnect target=gop-child agent=img14 pcd-capture-required=1 no-retry=1 no-output-after=1\r\n");
#else
  Text(c,"ad104-edid intent=one-disconnect target=gop-child agent=img08 no-retry=1 no-unlock=1\r\n");
#endif
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  /* Recollect after logging to detect intervening changes. No further I/O
   * between this agreeing collection and the single callback invocation. */
  s=EdCollect(t,gpu,io,pins,FALSE,&b);
  if(s!=EFI_SUCCESS || !PfEqual(&a,&b,sizeof(a)))return EFI_NOT_READY;
  EdAttempted=TRUE;
#ifdef GU_AD104_TEXT_TEST
  GuTextOutputDisabled=TRUE;
  EFI_STATUS callback=t->BootServices->DisconnectController(a.Drivers.Child,a.Drivers.Agent[1],NULL);
#else
  EFI_STATUS callback=t->BootServices->DisconnectController(a.Drivers.Child,a.Drivers.Agent[0],NULL);
#endif
  EFI_STATUS post=EdCollect(t,gpu,io,pins,TRUE,&p);
  EFI_STATUS repeat=EdCollect(t,gpu,io,pins,TRUE,&q);
  EdStatus(c,"disconnect-return",callback);EdStatus(c,"post",post);EdStatus(c,"post-repeat",repeat);
  EdDescribe(c,&p,"post");EdDescribe(c,&q,"post-repeat");
  BOOLEAN good=callback==EFI_SUCCESS && post==EFI_SUCCESS && repeat==EFI_SUCCESS &&
    EdPost(&a,&p) && EdPost(&a,&q);
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  if(!good)return EFI_NOT_READY;
#ifdef GU_AD104_GOP_TEST
  s=GopStep(c,gpu,io,pins,&a,&p,&b);
  if(s!=EFI_SUCCESS)return s;
#ifdef GU_AD104_CONSUMERS_TEST
  s=ConsumerEdidStep(c,gpu,io,pins,&b,&q);
  if(s!=EFI_SUCCESS)return s;
  b=q;
#ifdef GU_AD104_OWNER_CHILD_TEST
  return OwnerChildStep(c,gpu,io,pins,&b);
#endif
#endif
#endif
#ifdef GU_AD104_TEXT_TEST
  s=EdKey(t,'V',NULL);
#else
  s=EdKey(t,'V',L"EDID callback returned and state checks passed.\r\nPress uppercase V within 60 seconds to verify input and continue to Windows. Any other key stops.\r\n");
#endif
  EdStatus(c,"input-after",s);
  /* Input callbacks may themselves change shared state: check again. */
#ifdef GU_AD104_GOP_TEST
  post=EdCollectMask(t,gpu,io,pins,TRUE,
#ifdef GU_AD104_CONSUMERS_TEST
    0x3c,
#else
    0x1c,
#endif
    &p);
  EFI_STATUS protocols=GopProtocols(t,b.Drivers.Child,TRUE,NULL);
  EdStatus(c,"final-protocols",protocols);
  if(s!=EFI_SUCCESS || post!=EFI_SUCCESS || protocols!=EFI_SUCCESS || !
#ifdef GU_AD104_CONSUMERS_TEST
    ConsumerEdidPost(&b,&p)
#else
    GopPost(&b,&p)
#endif
    )s=EFI_NOT_READY;
#else
  post=EdCollect(t,gpu,io,pins,TRUE,&p);
  if(s!=EFI_SUCCESS || post!=EFI_SUCCESS || !EdPost(&a,&p))s=EFI_NOT_READY;
#endif
  EdStatus(c,"final-post",post);EdDescribe(c,&p,"final-post");
  EdStatus(c,"windows-eligible",s);
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  return s;
}
