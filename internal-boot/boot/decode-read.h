/* GPL-2.0-only. Enable IO/MEM only, then bounded read-only BAR0 observations. */
static BOOLEAN DecodeAttempted;
/* Exclude inaccessible 0x840100; bookend each batch with chip identity. */
static const UINT32 DecodeOffsets[]={0,0x118128,0x118234,0x110100,0x1fa824,0x1fa828,
  0x1fa7cc,0x1fa7c4,0x9a0148,0x823804,0x823814,0x823818,0x82381c,0x823820,
  0x1183a4,0x625f04,0x1fa7c8,0x1fa82c,0x1fa830,0x824148,0x8241e0,0};
static BOOLEAN DecodePost(const ED_STATE *before,const ED_STATE *after) {
  ED_STATE expected=*before;expected.Config[1]|=3U;expected.Attributes=0x300;
  return PfEqual(&expected,after,sizeof(expected));
}
typedef struct {UINT32 Values[22];EFI_STATUS Status[22];UINTN Attempted;} DECODE_READS;
#ifdef GU_AD104_ENTRY_INPUTS_TEST
#include "entry-inputs.h"
#endif
static EFI_STATUS DecodeBatch(EFI_PCI_IO_PROTOCOL *io,DECODE_READS *out,EFI_PCI_IO_PROTOCOL_IO_MEM read) {
  EFI_STATUS result=EFI_SUCCESS;
  if(!read || io->Mem.Read!=read)return EFI_UNSUPPORTED;
  for(UINTN i=0;i<22;i++) {
    UINT32 value=MAX_UINT32;
    if(io->Mem.Read!=read)return EFI_NOT_READY;
    EFI_STATUS s=read(io,EfiPciIoWidthUint32,0,DecodeOffsets[i],1,&value);
    out->Values[i]=value;out->Status[i]=s;out->Attempted=i+1;
    if(io->Mem.Read!=read || s!=EFI_SUCCESS || value==MAX_UINT32 || (value&0xffff0000U)==0xbadf0000U ||
       ((!i || i==21) && value!=0x194000a1U)){result=s==EFI_SUCCESS?EFI_NOT_READY:s;break;}
  }
  return result;
}
static EFI_STATUS DecodeEmit(CAPTURE *c,const DECODE_READS *out,UINTN batch) {
  for(UINTN i=0;i<out->Attempted;i++) {
    Text(c,"ad104-register batch=0x");Hex(c,batch,2);Text(c," offset=0x");Hex(c,DecodeOffsets[i],8);
    Text(c," status=0x");Hex(c,out->Status[i],16);Text(c," value=0x");Hex(c,out->Values[i],8);End(c);
  }
  Text(c,"ad104-register batch=0x");Hex(c,batch,2);Text(c," attempted=0x");Hex(c,out->Attempted,2);End(c);
  return EdFlush(c);
}
#ifdef GU_AD104_PROM_HEADER_TEST
static BOOLEAN PromHeaderAttempted;
#ifdef GU_AD104_PROM_FULL_TEST
static UINT8 PromPrefix[512];static EFI_HANDLE PromPrefixGpu;static EFI_PCI_IO_PROTOCOL *PromPrefixIo;
#endif
static UINT16 Prom16(const UINT8 *p){return (UINT16)(p[0]|((UINT16)p[1]<<8));}
static EFI_STATUS PromHeader(CAPTURE *c,EFI_HANDLE gpu,EFI_PCI_IO_PROTOCOL *io,
                             const PF_PIN *pins,const ED_STATE *parent) {
  UINT32 words[128]={0},chip=0,signature=0;UINTN attempted=0;static ED_STATE state;
  EFI_PCI_IO_PROTOCOL_IO_MEM read=io->Mem.Read;
  if(PromHeaderAttempted || !read)return EFI_ALREADY_STARTED;
  Text(c,"ad104-prom-header intent=512-byte-window base=0x00300000 full-prom=0 no-execution=1 no-retry=1\r\n");
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  EFI_STATUS s=ParentCollect(c,gpu,io,pins,parent->Drivers.Child,&state);
  if(s!=EFI_SUCCESS || !DecodePost(parent,&state) || io->Mem.Read!=read ||
     OwnerGone(c->Table,parent->Drivers.Child)!=EFI_SUCCESS)return EFI_NOT_READY;
  PromHeaderAttempted=TRUE;
  for(UINTN i=0;i<128;i++) {
    if(io->Mem.Read!=read){s=EFI_NOT_READY;break;}
    s=read(io,EfiPciIoWidthUint32,0,0x300000+i*4,1,&words[i]);attempted=i+1;
    if(s!=EFI_SUCCESS || io->Mem.Read!=read){if(s==EFI_SUCCESS)s=EFI_NOT_READY;break;}
    if(!i && ((words[0]&0xffffU)!=0xaa55U || (words[0]&0xffff0000U)==0xbadf0000U)){s=EFI_COMPROMISED_DATA;break;}
  }
  if(s==EFI_SUCCESS && attempted==128) {
    s=read(io,EfiPciIoWidthUint32,0,0x300000,1,&signature);
    if(s==EFI_SUCCESS && (io->Mem.Read!=read || signature!=words[0]))s=EFI_NOT_READY;
    if(s==EFI_SUCCESS)s=read(io,EfiPciIoWidthUint32,0,0,1,&chip);
    if(s==EFI_SUCCESS && (io->Mem.Read!=read || chip!=0x194000a1U))s=EFI_NOT_READY;
  }
  EFI_STATUS post=ParentCollect(c,gpu,io,pins,parent->Drivers.Child,&state);
  EFI_STATUS gone=OwnerGone(c->Table,parent->Drivers.Child);
  if(post!=EFI_SUCCESS || !DecodePost(parent,&state) || gone!=EFI_SUCCESS)s=EFI_NOT_READY;
  const UINT8 *bytes=(const UINT8 *)words;UINT16 pcir=attempted==128?Prom16(bytes+0x18):0;
  if(s==EFI_SUCCESS && (pcir>512-24 || pcir<0x1a || bytes[pcir]!='P' || bytes[pcir+1]!='C' ||
     bytes[pcir+2]!='I' || bytes[pcir+3]!='R' || Prom16(bytes+pcir+4)!=0x10de ||
     Prom16(bytes+pcir+6)!=0x2782 || Prom16(bytes+pcir+10)<24 || Prom16(bytes+pcir+10)>512-pcir ||
     !Prom16(bytes+pcir+16) || (UINT32)Prom16(bytes+pcir+16)*512U>0x100000 ||
     (UINT32)Prom16(bytes+pcir+16)*512U<(UINT32)pcir+Prom16(bytes+pcir+10)))s=EFI_COMPROMISED_DATA;
  EdStatus(c,"prom-header-read",s);Text(c,"ad104-prom-header attempted=0x");Hex(c,attempted,4);
  Text(c," pcir=0x");Hex(c,pcir,4);Text(c," full-prom=0\r\n");
  /* Only complete, admitted bytes are emitted, never a failed or partial dump. */
  if(s==EFI_SUCCESS)for(UINTN i=0;i<128;i++) {
    Text(c,"ad104-prom-header offset=0x");Hex(c,i*4,4);Text(c," value=0x");Hex(c,words[i],8);End(c);
  }
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  if(s!=EFI_SUCCESS)return s;
  s=ParentCollect(c,gpu,io,pins,parent->Drivers.Child,&state);
  BOOLEAN good=s==EFI_SUCCESS && DecodePost(parent,&state) && io->Mem.Read==read &&
    OwnerGone(c->Table,parent->Drivers.Child)==EFI_SUCCESS;
#ifdef GU_AD104_PROM_FULL_TEST
  if(good){for(UINTN i=0;i<512;i++)PromPrefix[i]=bytes[i];PromPrefixGpu=gpu;PromPrefixIo=io;}
#endif
  return good?EFI_SUCCESS:EFI_NOT_READY;
}
#endif
#ifdef GU_AD104_PROM_FULL_TEST
#include "prom-full.h"
#ifdef GU_AD104_FALCON_OBSERVE_TEST
#include "falcon-observe.h"
#endif
#endif
#ifdef GU_AD104_DECODE_OFF_TEST
static BOOLEAN DecodeOffAttempted;
static EFI_STATUS DecodeOff(CAPTURE *c,EFI_HANDLE gpu,EFI_PCI_IO_PROTOCOL *io,const PF_PIN *pins,
                            const ED_STATE *parent,const ED_STATE *decoded) {
  static ED_STATE pre,one,two;
  if(DecodeOffAttempted || !io->Attributes)return EFI_ALREADY_STARTED;
  EFI_STATUS s=ParentCollect(c,gpu,io,pins,parent->Drivers.Child,&pre);
  if(s!=EFI_SUCCESS || !PfEqual(decoded,&pre,sizeof(pre)) || !DecodePost(parent,&pre) ||
     OwnerGone(c->Table,parent->Drivers.Child)!=EFI_SUCCESS)return EFI_NOT_READY;
  Text(c,"ad104-display intent=one-disable-io-memory mask=0x300 target=verified-post-parent no-retry=1 no-more-mmio=1\r\n");
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  s=ParentCollect(c,gpu,io,pins,parent->Drivers.Child,&pre);
  if(s!=EFI_SUCCESS || !PfEqual(decoded,&pre,sizeof(pre)) || !io->Attributes ||
     OwnerGone(c->Table,parent->Drivers.Child)!=EFI_SUCCESS)return EFI_NOT_READY;
  DecodeOffAttempted=TRUE;
  EFI_STATUS callback=io->Attributes(io,EfiPciIoAttributeOperationDisable,0x300,NULL);
  EFI_STATUS a=ParentCollect(c,gpu,io,pins,parent->Drivers.Child,&one);
  EFI_STATUS b=ParentCollect(c,gpu,io,pins,parent->Drivers.Child,&two);
  EFI_STATUS gone=OwnerGone(c->Table,parent->Drivers.Child);
  EdStatus(c,"decode-off-return",callback);EdStatus(c,"decode-off-post",a);EdStatus(c,"decode-off-repeat",b);
  EdDescribe(c,&one,"decode-off-post");EdDescribe(c,&two,"decode-off-repeat");
  BOOLEAN good=callback==EFI_SUCCESS && a==EFI_SUCCESS && b==EFI_SUCCESS && gone==EFI_SUCCESS &&
    PfEqual(parent,&one,sizeof(one)) && PfEqual(parent,&two,sizeof(two));
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  if(!good)return EFI_NOT_READY;
  s=ParentCollect(c,gpu,io,pins,parent->Drivers.Child,&pre);
  gone=OwnerGone(c->Table,parent->Drivers.Child);
  return s==EFI_SUCCESS && gone==EFI_SUCCESS && PfEqual(parent,&pre,sizeof(pre))?EFI_SUCCESS:EFI_NOT_READY;
}
#endif
static EFI_STATUS DecodeStep(CAPTURE *c,EFI_HANDLE gpu,EFI_PCI_IO_PROTOCOL *io,
                             const PF_PIN *pins,const ED_STATE *before) {
  EFI_SYSTEM_TABLE *t=c->Table;static ED_STATE pre,one,two,final;
  DECODE_READS a={0},b={0};EFI_PCI_IO_PROTOCOL_IO_MEM read=io->Mem.Read;
  if(DecodeAttempted || !ParentAttempted || !GuTextOutputDisabled)return EFI_ALREADY_STARTED;
  EFI_STATUS s=ParentCollect(c,gpu,io,pins,before->Drivers.Child,&pre);
  if(s!=EFI_SUCCESS || pre.Drivers.MatchedEdges!=2 || (pre.Config[1]&0xffff) || pre.Attributes ||
     (pre.Supported&0x300)!=0x300 || !PfEqual(before,&pre,sizeof(pre)) || !io->Attributes || !io->Mem.Read ||
     OwnerGone(t,pre.Drivers.Child)!=EFI_SUCCESS)return EFI_NOT_READY;
  Text(c,"ad104-display intent=enable-only-io-memory mask=0x300 no-bme-vga=1 register-reads-only=1 no-unlock=1\r\n");
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  s=ParentCollect(c,gpu,io,pins,before->Drivers.Child,&pre);
  if(s!=EFI_SUCCESS || !PfEqual(before,&pre,sizeof(pre)) || !io->Mem.Read ||
     OwnerGone(t,pre.Drivers.Child)!=EFI_SUCCESS)return EFI_NOT_READY;
  DecodeAttempted=TRUE;
  EFI_STATUS callback=io->Attributes(io,EfiPciIoAttributeOperationEnable,0x300,NULL);
  EFI_STATUS x=ParentCollect(c,gpu,io,pins,pre.Drivers.Child,&one);
  EFI_STATUS y=ParentCollect(c,gpu,io,pins,pre.Drivers.Child,&two);
  EFI_STATUS gone=OwnerGone(t,pre.Drivers.Child);
  EdStatus(c,"decode-return",callback);EdStatus(c,"decode-post",x);EdStatus(c,"decode-repeat",y);
  EdDescribe(c,&one,"decode-post");EdDescribe(c,&two,"decode-repeat");
  BOOLEAN good=callback==EFI_SUCCESS && x==EFI_SUCCESS && y==EFI_SUCCESS && gone==EFI_SUCCESS &&
    DecodePost(&pre,&one) && DecodePost(&pre,&two);
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  if(!good)return EFI_NOT_READY;
  /* Recheck after log services, before each batch of bounded MMIO reads. */
  x=ParentCollect(c,gpu,io,pins,pre.Drivers.Child,&one);
  if(x!=EFI_SUCCESS || !DecodePost(&pre,&one) || OwnerGone(t,pre.Drivers.Child)!=EFI_SUCCESS)return EFI_NOT_READY;
  x=DecodeBatch(io,&a,read);
  EFI_STATUS emit=DecodeEmit(c,&a,1);
#ifdef GU_AD104_DELAYED_READ_TEST
  if(emit==EFI_SUCCESS && x==EFI_SUCCESS) {
    Text(c,"ad104-register delay-request-us=1000000 wall-clock-unverified=1 no-additional-registers=1\r\n");
    emit=EdFlush(c);
    if(emit==EFI_SUCCESS)x=t->BootServices->Stall?t->BootServices->Stall(1000000):EFI_UNSUPPORTED;
    EdStatus(c,"inter-batch-delay",x);
    if(emit==EFI_SUCCESS)emit=EdFlush(c);
  }
#endif
  y=ParentCollect(c,gpu,io,pins,pre.Drivers.Child,&one);
  if(emit==EFI_SUCCESS && x==EFI_SUCCESS && y==EFI_SUCCESS && DecodePost(&pre,&one) && OwnerGone(t,pre.Drivers.Child)==EFI_SUCCESS)
    x=DecodeBatch(io,&b,read);
  else x=EFI_NOT_READY;
  emit=DecodeEmit(c,&b,2);
  y=ParentCollect(c,gpu,io,pins,pre.Drivers.Child,&two);
  good=emit==EFI_SUCCESS && a.Attempted==22 && b.Attempted==22 && x==EFI_SUCCESS && y==EFI_SUCCESS && DecodePost(&pre,&two) &&
    OwnerGone(t,pre.Drivers.Child)==EFI_SUCCESS && PfEqual(a.Values,b.Values,sizeof(a.Values));
  EdStatus(c,"register-capture",good?EFI_SUCCESS:EFI_NOT_READY);
  Text(c,"ad104-register cold-entry-admission=unreviewed transaction-executed=0\r\n");
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  if(!good)return EFI_NOT_READY;
#ifdef GU_AD104_PROM_HEADER_TEST
  s=PromHeader(c,gpu,io,pins,&pre);if(s!=EFI_SUCCESS)return s;
#ifdef GU_AD104_PROM_FULL_TEST
  s=PromFull(c,gpu,io,pins,&pre,&b);if(s!=EFI_SUCCESS)return s;
#endif
#endif
#ifdef GU_AD104_FALCON_OBSERVE_TEST
  s=FalconObserve(c,gpu,io,pins,&pre,&b);
#ifdef GU_AD104_FALCON_REFUSAL_TEST
  if(s!=EFI_SUCCESS){
    EdStatus(c,"falcon-observation-result",s);
    if(s!=EFI_NOT_READY || !FalconRefusalObserved)return s;
    s=FalconRefusalAdmission(c,gpu,io,pins,&pre,&b,read);
    EdStatus(c,"refusal-recovery-admission",s);if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
    if(s!=EFI_SUCCESS || !PromFullState(c,gpu,io,pins,&pre,read))return EFI_NOT_READY;
  }
#else
  if(s!=EFI_SUCCESS)return s;
#endif
#endif
#ifdef GU_AD104_ENTRY_INPUTS_TEST
  GU_ENTRY_INPUTS audit={0};
  if(!PromFullState(c,gpu,io,pins,&pre,read) || !GuAuditEntryInputs(DecodeOffsets,b.Values,b.Status,b.Attempted,&audit))return EFI_NOT_READY;
  Text(c,"ad104-entry-inputs fb=0x");Hex(c,audit.Fb,16);Text(c," raw-vga=0x");Hex(c,audit.RawVga,16);
  Text(c," policy-vga=0x");Hex(c,audit.PolicyVga,16);Text(c," mmu-low=0x");Hex(c,audit.MmuLow,16);
  Text(c," mmu-high=0x");Hex(c,audit.MmuHigh,16);Text(c," mmu-rounded-top=0x");Hex(c,audit.MmuTop,16);
  Text(c," vga-valid=");Hex(c,audit.VgaValid,1);Text(c," mmu-range-valid=");Hex(c,audit.MmuRangeValid,1);Text(c," top-lock=");Hex(c,audit.TopLock,1);End(c);
  Text(c,"ad104-entry-inputs ss1=0x");Hex(c,b.Values[13],8);Text(c," vga=0x");Hex(c,b.Values[15],8);
  Text(c," reference-ss1-match=");Hex(c,audit.ReferenceSs1,1);Text(c," reference-vga-match=");Hex(c,audit.ReferenceVga,1);
  Text(c," sample-time=pre-prom arithmetic-only=1 assets-qualified=0 full-layout-unavailable=1 profile-admitted=0 execution-authority=0\r\n");
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  if(!PromFullState(c,gpu,io,pins,&pre,read))return EFI_NOT_READY;
#endif
  s=EdKey(t,'V',NULL);
  x=ParentCollect(c,gpu,io,pins,pre.Drivers.Child,&final);gone=OwnerGone(t,pre.Drivers.Child);
  EdStatus(c,"decode-input-after",s);EdStatus(c,"decode-final-post",x);EdDescribe(c,&final,"decode-final-post");
  if(s!=EFI_SUCCESS || x!=EFI_SUCCESS || gone!=EFI_SUCCESS || !DecodePost(&pre,&final))s=EFI_NOT_READY;
#ifdef GU_AD104_DECODE_OFF_TEST
  if(s==EFI_SUCCESS)s=DecodeOff(c,gpu,io,pins,&pre,&final);
#endif
#ifdef GU_AD104_FALCON_REFUSAL_TEST
  if(s==EFI_SUCCESS && FalconRecoveryEligible){
    s=GuRefusalWitness("refusal-decode-off-complete",EFI_SUCCESS);
    if(s==EFI_SUCCESS){x=ParentCollect(c,gpu,io,pins,pre.Drivers.Child,&final);gone=OwnerGone(t,pre.Drivers.Child);
      if(x!=EFI_SUCCESS || gone!=EFI_SUCCESS || !PfEqual(&pre,&final,sizeof(pre)))s=EFI_NOT_READY;}
  }
#endif
  EdStatus(c,"windows-eligible",s);if(EdFlush(c)!=EFI_SUCCESS)return c->Io;return s;
}
