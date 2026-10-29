/* GPL-2.0-only. Fixed 1 MiB PROM transport capture; no firmware execution. */
#include "../unlock/sha256.h"
static BOOLEAN PromFullAttempted;
#ifdef GU_AD104_FWSEC_AUDIT_TEST
#include "fwsec-audit.h"
static VOID PromAuditLog(CAPTURE *c,const UINT8 *buffer) {
  GU_FWSEC_AUDIT audit;EFI_STATUS status=GuAuditCapturedFwsec(buffer,0x100000,&audit);
  EdStatus(c,"fwsec-ram-audit",status);
  Text(c,"ad104-fwsec-audit prom-match=");Hex(c,audit.PromMatch,1);Text(c," parsed=");Hex(c,audit.Parsed,1);
  Text(c," geometry-match=");Hex(c,audit.GeometryMatch,1);Text(c," image-match=");Hex(c,audit.ImageMatch,1);
  Text(c," signature-region-match=");Hex(c,audit.SignatureMatch,1);
  Text(c," firmware-authenticated=0 cold-entry-admitted=0 execution-authority=0\r\n");
  Text(c,"ad104-fwsec-audit image-off=0x");Hex(c,audit.Descriptor.image_off,8);
  Text(c," image-bytes=0x");Hex(c,audit.Descriptor.image_size,8);Text(c," sig-off=0x");Hex(c,audit.Descriptor.sig_off,8);
  Text(c," sig-bytes=0x");Hex(c,audit.Descriptor.sig_bytes,8);End(c);
}
#endif
static BOOLEAN PromFullState(CAPTURE *c,EFI_HANDLE gpu,EFI_PCI_IO_PROTOCOL *io,const PF_PIN *pins,
                             const ED_STATE *parent,EFI_PCI_IO_PROTOCOL_IO_MEM read) {
  static ED_STATE state;
  return io->Mem.Read==read && ParentCollect(c,gpu,io,pins,parent->Drivers.Child,&state)==EFI_SUCCESS &&
    DecodePost(parent,&state) && OwnerGone(c->Table,parent->Drivers.Child)==EFI_SUCCESS;
}
static BOOLEAN PromFileSame(EFI_FILE_PROTOCOL *f,const EFI_FILE_PROTOCOL *ops) {
  return f->Write==ops->Write && f->Read==ops->Read && f->Flush==ops->Flush &&
    f->SetPosition==ops->SetPosition && f->Close==ops->Close;
}
static EFI_STATUS PromSave(CAPTURE *c,UINT8 *page,UINT8 *verify,const UINT8 *expected) {
  EFI_FILE_PROTOCOL *existing=NULL;
  EFI_STATUS absent=c->DumpRoot->Open(c->DumpRoot,&existing,L"\\RTX3090-UNLOCK\\PROM.BIN",EFI_FILE_MODE_READ,0);
  if(absent==EFI_SUCCESS){if(existing && existing!=c->DumpRoot && existing!=c->File && existing!=c->Witness && existing->Close)(void)existing->Close(existing);return EFI_ALREADY_STARTED;}
  if(absent!=EFI_NOT_FOUND || existing)return EFI_DEVICE_ERROR;
  EFI_FILE_PROTOCOL *f=NULL;EFI_STATUS s=c->DumpRoot->Open(c->DumpRoot,&f,L"\\RTX3090-UNLOCK\\PROM.BIN",
    EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0);
  if(!f || f==c->DumpRoot || f==c->File || f==c->Witness)return EFI_DEVICE_ERROR;
  if(s!=EFI_SUCCESS){if(f->Close)(void)f->Close(f);return s;}
  EFI_FILE_PROTOCOL ops=*f;
  if(!ops.Write || !ops.Read || !ops.Flush || !ops.SetPosition || !ops.Close){if(ops.Close)(void)ops.Close(f);return EFI_UNSUPPORTED;}
  UINTN n=0x100000;s=ops.Write(f,&n,page);
  if(s==EFI_SUCCESS && (n!=0x100000 || !PromFileSame(f,&ops)))s=EFI_DEVICE_ERROR;
  if(s==EFI_SUCCESS)s=ops.Flush(f);
  if(s==EFI_SUCCESS && !PromFileSame(f,&ops))s=EFI_NOT_READY;
  if(s==EFI_SUCCESS)s=ops.SetPosition(f,0);
  if(s==EFI_SUCCESS && !PromFileSame(f,&ops))s=EFI_NOT_READY;
  if(s==EFI_SUCCESS){n=0x100000;s=ops.Read(f,&n,verify);if(s==EFI_SUCCESS && (n!=0x100000 || !PromFileSame(f,&ops)))s=EFI_DEVICE_ERROR;}
  if(s==EFI_SUCCESS){UINT8 extra=0;n=1;s=ops.Read(f,&n,&extra);if(s==EFI_SUCCESS && (n || !PromFileSame(f,&ops)))s=EFI_BAD_BUFFER_SIZE;}
  if(s==EFI_SUCCESS){UINT8 a[32],b[32];GuSha256(page,0x100000,a);GuSha256(verify,0x100000,b);if(!PfEqual(a,expected,32) || !PfEqual(b,expected,32))s=EFI_CRC_ERROR;}
  EFI_STATUS closed=ops.Close(f);return s==EFI_SUCCESS?closed:s;
}
static EFI_STATUS PromFull(CAPTURE *c,EFI_HANDLE gpu,EFI_PCI_IO_PROTOCOL *io,const PF_PIN *pins,const ED_STATE *parent,const DECODE_READS *samples) {
  (void)samples;
  EFI_PCI_IO_PROTOCOL_IO_MEM read=io->Mem.Read;UINT8 *buffer=NULL;EFI_FILE_PROTOCOL *existing=NULL;
  if(PromFullAttempted || PromPrefixGpu!=gpu || PromPrefixIo!=io || !c->DumpRoot || !c->DumpRoot->Open ||
     !c->Witness || c->DumpRoot==c->File || c->DumpRoot==c->Witness || c->Witness==c->File || !read || !c->Table->BootServices->AllocatePool || !c->Table->BootServices->FreePool)return EFI_NOT_READY;
  PromFullAttempted=TRUE;
  EFI_STATUS s=c->DumpRoot->Open(c->DumpRoot,&existing,L"\\RTX3090-UNLOCK\\PROM.BIN",EFI_FILE_MODE_READ,0);
  if(s==EFI_SUCCESS){if(existing && existing!=c->DumpRoot && existing!=c->File && existing!=c->Witness && existing->Close)(void)existing->Close(existing);return EFI_ALREADY_STARTED;}
  if(s!=EFI_NOT_FOUND || existing)return EFI_DEVICE_ERROR;
  s=c->Table->BootServices->AllocatePool(EfiBootServicesData,0x200000,(VOID **)&buffer);
  if(s!=EFI_SUCCESS || !buffer)return s==EFI_SUCCESS?EFI_OUT_OF_RESOURCES:s;
  Text(c,"ad104-prom-full intent=one-1mib-window base=0x00300000 dwords=262144 no-execution=1 no-retry=1\r\n");
  s=EdFlush(c);UINTN attempted=0;UINT32 signature=0,chip=0;
  if(s==EFI_SUCCESS && !PromFullState(c,gpu,io,pins,parent,read))s=EFI_NOT_READY;
  if(s==EFI_SUCCESS)for(UINTN i=0;i<0x100000/4;i++) {
    if(io->Mem.Read!=read){s=EFI_NOT_READY;break;}
    s=read(io,EfiPciIoWidthUint32,0,0x300000+i*4,1,((UINT32 *)buffer)+i);attempted=i+1;
    if(s!=EFI_SUCCESS || io->Mem.Read!=read){if(s==EFI_SUCCESS)s=EFI_NOT_READY;break;}
    if(!i && ((((UINT32 *)buffer)[0]&0xffffU)!=0xaa55U || !PfEqual(buffer,PromPrefix,4))){s=EFI_COMPROMISED_DATA;break;}
  }
  if(s==EFI_SUCCESS && !PfEqual(buffer,PromPrefix,512))s=EFI_COMPROMISED_DATA;
  if(s==EFI_SUCCESS){s=read(io,EfiPciIoWidthUint32,0,0x300000,1,&signature);if(s==EFI_SUCCESS && (signature!=((UINT32 *)buffer)[0] || io->Mem.Read!=read))s=EFI_NOT_READY;}
  if(s==EFI_SUCCESS){s=read(io,EfiPciIoWidthUint32,0,0,1,&chip);if(s==EFI_SUCCESS && (chip!=0x194000a1U || io->Mem.Read!=read))s=EFI_NOT_READY;}
  if(!PromFullState(c,gpu,io,pins,parent,read) && s==EFI_SUCCESS)s=EFI_NOT_READY;
  UINT8 digest[32]={0};if(s==EFI_SUCCESS)GuSha256(buffer,0x100000,digest);
#ifdef GU_AD104_FWSEC_AUDIT_TEST
  /* Observation only: mismatch is recorded, not converted into authority or a
   * new hardware failure. Preserve transport evidence and normal safe recovery. */
  if(s==EFI_SUCCESS){PromAuditLog(c,buffer);s=EdFlush(c);if(s==EFI_SUCCESS && !PromFullState(c,gpu,io,pins,parent,read))s=EFI_NOT_READY;}
#endif
#ifdef GU_AD104_FWSEC_REHEARSAL_TEST
  if(s==EFI_SUCCESS){
    GU_FWSEC_REHEARSAL rehearsal;EFI_STATUS result=GuRehearseCapturedFwsec(buffer,0x100000,buffer+0x100000,0x100000,&rehearsal);
    UINT8 after[32];GuSha256(buffer,0x100000,after);rehearsal.SourceUnchanged=PfEqual(after,digest,32);
    EdStatus(c,"fwsec-ram-rehearsal",result);
    Text(c,"ad104-fwsec-rehearsal source-unchanged=");Hex(c,rehearsal.SourceUnchanged,1);
    Text(c," patch-match=");Hex(c,rehearsal.PatchMatch,1);Text(c," selector-matrix=");Hex(c,rehearsal.SelectorMatrix,1);
    Text(c," scratch-intact=");Hex(c,rehearsal.ScratchIntact,1);
    Text(c," changed-bytes=0x");Hex(c,rehearsal.ChangedBytes,8);
    Text(c," synthetic-frts=0x100000000 live-fuse-read=0 uploaded=0 executed=0 authority=0\r\n");
    Text(c,"ad104-fwsec-rehearsal patched-ram-sha256=");for(UINTN i=0;i<32;i++)Hex(c,rehearsal.PatchedHash[i],2);End(c);
    s=EdFlush(c);if(s==EFI_SUCCESS && (!rehearsal.ScratchIntact || !PfEqual(after,digest,32)))s=EFI_COMPROMISED_DATA;
    if(s==EFI_SUCCESS && !PromFullState(c,gpu,io,pins,parent,read))s=EFI_NOT_READY;
  }
#endif
#ifdef GU_AD104_FWSEC_SIGNATURE_TEST
  if(s==EFI_SUCCESS){
    if(!samples || samples->Attempted!=22 || samples->Status[20]!=EFI_SUCCESS || DecodeOffsets[20]!=0x8241e0)s=EFI_NOT_READY;
    if(s==EFI_SUCCESS){
    GU_FWSEC_SIGNATURE_AUDIT sig;EFI_STATUS result=GuRehearseObservedSignature(buffer,0x100000,buffer+0x100000,0x100000,DecodeOffsets[20],samples->Values[20],&sig);
    EdStatus(c,"fwsec-sampled-signature",result);
    Text(c,"ad104-fwsec-signature sampled-before-prom=1 offset=0x008241e0 raw=0x");Hex(c,samples->Values[20],8);
    Text(c," version=0x");Hex(c,sig.Version,2);Text(c," sig-offset=0x");Hex(c,sig.SignatureOffset,8);
    Text(c," supported=");Hex(c,sig.Supported,1);Text(c," image-match=");Hex(c,sig.ImageMatch,1);
    Text(c," changed-bytes=0x");Hex(c,sig.ChangedBytes,8);Text(c," authenticated=0 uploaded=0 executed=0 authority=0\r\n");
    Text(c,"ad104-fwsec-signature selected-bytes-sha256=");for(UINTN i=0;i<32;i++)Hex(c,sig.SignatureHash[i],2);End(c);
    Text(c,"ad104-fwsec-signature assembled-ram-sha256=");for(UINTN i=0;i<32;i++)Hex(c,sig.AssembledHash[i],2);End(c);
    UINT8 after[32];GuSha256(buffer,0x100000,after);
    s=EdFlush(c);if(s==EFI_SUCCESS && (!sig.Base.ScratchIntact || !PfEqual(after,digest,32)))s=EFI_COMPROMISED_DATA;
    if(s==EFI_SUCCESS && !PromFullState(c,gpu,io,pins,parent,read))s=EFI_NOT_READY;
    }
  }
#endif
  if(s==EFI_SUCCESS)s=PromSave(c,buffer,buffer+0x100000,digest);
  EFI_STATUS freed=c->Table->BootServices->FreePool(buffer);
  if(s==EFI_SUCCESS)s=freed;
  if(!PromFullState(c,gpu,io,pins,parent,read) && s==EFI_SUCCESS)s=EFI_NOT_READY;
  EdStatus(c,"prom-full-transport",s);EdStatus(c,"prom-full-free",freed);
  Text(c,"ad104-prom-full attempted=0x");Hex(c,attempted,8);Text(c," transport-complete=");Hex(c,s==EFI_SUCCESS?1:0,1);
  Text(c," firmware-validity=unreviewed unlock=0");End(c);
  if(s==EFI_SUCCESS){Text(c,"ad104-prom-full bytes=0x00100000 sha256=");for(UINTN i=0;i<32;i++)Hex(c,digest[i],2);End(c);}
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  if(s!=EFI_SUCCESS)return s;
  EFI_STATUS final=PromFullState(c,gpu,io,pins,parent,read)?EFI_SUCCESS:EFI_NOT_READY;
  EdStatus(c,"prom-full-final-state",final);
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  if(final!=EFI_SUCCESS)return final;
  return PromFullState(c,gpu,io,pins,parent,read)?EFI_SUCCESS:EFI_NOT_READY;
}
