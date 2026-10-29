/* GPL-2.0-only. Protocol metadata and one admitted PCD Boolean query only. */
#include <Protocol/Pcd.h>
#include <Protocol/SimpleTextOut.h>
typedef struct {
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *Out,*Err;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL OutMethods,ErrMethods;
  EFI_SIMPLE_TEXT_OUTPUT_MODE OutMode,ErrMode;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
  EFI_GRAPHICS_OUTPUT_PROTOCOL GopMethods;
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GopMode;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION Info;
  PCD_PROTOCOL *Pcd;
  PCD_PROTOCOL_GET_BOOLEAN GetBool;
  PCD_PROTOCOL_GET_NEXT_TOKEN Next;
  PCD_PROTOCOL_GET_SIZE Size;
} OUT_STATE;
static EFI_GUID OutPcdGuid=PCD_PROTOCOL_GUID;
#ifdef GU_AD104_PCD_CAPTURE
#include "pcd-producer.h"
#endif
static VOID OutCopyTextMode(EFI_SIMPLE_TEXT_OUTPUT_MODE *to,const EFI_SIMPLE_TEXT_OUTPUT_MODE *from) {
  /* OUT_STATE was zeroed: copy fields rather than firmware padding bytes. */
  to->MaxMode=from->MaxMode;to->Mode=from->Mode;to->Attribute=from->Attribute;
  to->CursorColumn=from->CursorColumn;to->CursorRow=from->CursorRow;to->CursorVisible=from->CursorVisible;
}
static EFI_STATUS OutCollect(EFI_SYSTEM_TABLE *t,EFI_HANDLE child,OUT_STATE *o) {
  UINT8 *z=(VOID *)o;for(UINTN i=0;i<sizeof(*o);i++)z[i]=0;
  if(!t->ConOut || !t->StdErr || !t->ConOut->Mode || !t->StdErr->Mode)return EFI_UNSUPPORTED;
  o->Out=t->ConOut;o->Err=t->StdErr;o->OutMethods=*o->Out;o->ErrMethods=*o->Err;
  OutCopyTextMode(&o->OutMode,o->Out->Mode);OutCopyTextMode(&o->ErrMode,o->Err->Mode);
  EFI_STATUS s=t->BootServices->HandleProtocol(child,&GopGuidL,(VOID **)&o->Gop);
  if(s!=EFI_SUCCESS || !o->Gop || !o->Gop->Mode || !o->Gop->Mode->Info)return EFI_NOT_READY;
  o->GopMethods=*o->Gop;o->GopMode=*o->Gop->Mode;
  if(o->GopMode.SizeOfInfo<sizeof(o->Info) || o->GopMode.SizeOfInfo>4096 ||
     !o->GopMode.MaxMode || o->GopMode.Mode>=o->GopMode.MaxMode)return EFI_BAD_BUFFER_SIZE;
  o->Info=*o->GopMode.Info;
  EFI_HANDLE *hs=NULL;UINTN n=0;
  s=t->BootServices->LocateHandleBuffer(ByProtocol,&OutPcdGuid,NULL,&n,&hs);
  if(s==EFI_SUCCESS && n==1 && hs)s=t->BootServices->HandleProtocol(hs[0],&OutPcdGuid,(VOID **)&o->Pcd);
  else s=EFI_NOT_FOUND;
  if(hs && t->BootServices->FreePool(hs)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  if(s!=EFI_SUCCESS || !o->Pcd)return EFI_NOT_FOUND;
  o->GetBool=o->Pcd->GetBool;o->Next=o->Pcd->GetNextToken;o->Size=o->Pcd->GetSize;
  return o->GetBool && o->Next && o->Size?EFI_SUCCESS:EFI_UNSUPPORTED;
}
static EFI_STATUS OutToken(OUT_STATE *o,BOOLEAN *value,UINTN *steps) {
  UINTN token=0;*steps=0;
  for(UINTN i=0;i<4096;i++) {
    UINTN prior=token;EFI_STATUS s=o->Next(NULL,&token);(*steps)++;
    if(s!=EFI_SUCCESS)return s;
    if(!token || token<=prior)return EFI_COMPROMISED_DATA;
    if(token==0x4ac) {
      /* Existence plus size prevents querying an absent token. Its Boolean
       * use is established by the exact pinned IMG14 code, not size alone. */
      if(o->Size(token)!=sizeof(BOOLEAN))return EFI_BAD_BUFFER_SIZE;
      *value=o->GetBool(token);return *value<=1?EFI_SUCCESS:EFI_COMPROMISED_DATA;
    }
    if(token>0x4ac)return EFI_NOT_FOUND;
  }
  return EFI_BAD_BUFFER_SIZE;
}
static VOID OutOwner(CAPTURE *c,EFI_HANDLE *hs,UINTN n,const char *label,UINTN address) {
  Text(c,"ad104-output-callback name=");Text(c,label);Text(c," address=0x");Hex(c,address,16);
  UINTN found=0;
  for(UINTN i=0;i<n;i++) {
    EFI_LOADED_IMAGE_PROTOCOL *im=NULL;
    if(c->Table->BootServices->HandleProtocol(hs[i],&ImgGuidL,(VOID **)&im)!=EFI_SUCCESS || !im)continue;
    UINTN base=(UINTN)im->ImageBase;
    if(address && base && im->ImageSize && base<=MAX_UINTN-im->ImageSize && address>=base && address-base<im->ImageSize) {
      found++;Text(c," image-base=0x");Hex(c,base,16);Text(c," image-size=0x");Hex(c,im->ImageSize,16);
      Text(c," rva=0x");Hex(c,address-base,8);
    }
  }
  Text(c," containing-images=0x");Hex(c,found,4);End(c);
}
static VOID OutTextMode(CAPTURE *c,const char *label,const EFI_SIMPLE_TEXT_OUTPUT_MODE *m,UINTN pointer) {
  Text(c,"ad104-output-text-mode name=");Text(c,label);Text(c," pointer=0x");Hex(c,pointer,16);
  Text(c," max=0x");Hex(c,(UINT32)m->MaxMode,8);Text(c," current=0x");Hex(c,(UINT32)m->Mode,8);
  Text(c," attribute=0x");Hex(c,(UINT32)m->Attribute,8);Text(c," column=0x");Hex(c,(UINT32)m->CursorColumn,8);
  Text(c," row=0x");Hex(c,(UINT32)m->CursorRow,8);Text(c," visible=");Hex(c,m->CursorVisible,2);End(c);
}
static VOID ObserveOutput(CAPTURE *c,const PF_STATE *before,EFI_HANDLE gpu,EFI_PCI_IO_PROTOCOL *io,
                          const UINT32 *cfg,const UINTN *loc) {
  static OUT_STATE a,b,last;PF_STATE after={0};BOOLEAN first=FALSE,second=FALSE;UINTN steps1=0,steps2=0;
  EFI_SYSTEM_TABLE *t=c->Table;EFI_HANDLE *images=NULL;UINTN count=0;
  EFI_STATUS s=OutCollect(t,before->Child,&a);
  EFI_STATUS token=s==EFI_SUCCESS?OutToken(&a,&first,&steps1):s;
  if(s==EFI_SUCCESS)s=OutCollect(t,before->Child,&b);
  EFI_STATUS token2=s==EFI_SUCCESS?OutToken(&b,&second,&steps2):s;
  if(s==EFI_SUCCESS)s=OutCollect(t,before->Child,&last);
  if(s==EFI_SUCCESS && (!PfEqual(&a,&b,sizeof(a)) || !PfEqual(&a,&last,sizeof(a)) || token!=token2 ||
     (token==EFI_SUCCESS && first!=second) || steps1!=steps2))s=EFI_NOT_READY;
  EFI_STATUS fresh=PfCollect(t,gpu,PfPins,&after);
  if(s==EFI_SUCCESS && (fresh!=EFI_SUCCESS || !PfEqual(before,&after,sizeof(after)) ||
     !ProbeUnchanged(c,gpu,io,cfg,loc)))s=EFI_NOT_READY;
  Text(c,"ad104-output-observation snapshot-status=0x");Hex(c,s,16);Text(c," read-only=1 detach-authority=0");End(c);
  Text(c,"ad104-output-pcd token=0x04ac first-status=0x");Hex(c,token,16);
  Text(c," second-status=0x");Hex(c,token2,16);Text(c," steps1=0x");Hex(c,steps1,4);Text(c," steps2=0x");Hex(c,steps2,4);
  if(token==EFI_SUCCESS && token2==EFI_SUCCESS){Text(c," first-value=");Hex(c,first,2);Text(c," second-value=");Hex(c,second,2);}End(c);
  if(s!=EFI_SUCCESS){Problem(c,"ad104-output-snapshot",s);return;}
  Text(c,"ad104-output-interfaces out=0x");Hex(c,(UINTN)a.Out,16);Text(c," err=0x");Hex(c,(UINTN)a.Err,16);
  Text(c," gop=0x");Hex(c,(UINTN)a.Gop,16);Text(c," pcd=0x");Hex(c,(UINTN)a.Pcd,16);End(c);
  OutTextMode(c,"conout",&a.OutMode,(UINTN)a.OutMethods.Mode);
  OutTextMode(c,"stderr",&a.ErrMode,(UINTN)a.ErrMethods.Mode);
  Text(c,"ad104-output-mode max=0x");Hex(c,a.GopMode.MaxMode,8);Text(c," current=0x");Hex(c,a.GopMode.Mode,8);
  Text(c," info-version=0x");Hex(c,a.Info.Version,8);Text(c," width=0x");Hex(c,a.Info.HorizontalResolution,8);
  Text(c," height=0x");Hex(c,a.Info.VerticalResolution,8);Text(c," format=0x");Hex(c,a.Info.PixelFormat,8);
  Text(c," red-mask=0x");Hex(c,a.Info.PixelInformation.RedMask,8);Text(c," green-mask=0x");Hex(c,a.Info.PixelInformation.GreenMask,8);
  Text(c," blue-mask=0x");Hex(c,a.Info.PixelInformation.BlueMask,8);Text(c," reserved-mask=0x");Hex(c,a.Info.PixelInformation.ReservedMask,8);
  Text(c," stride=0x");Hex(c,a.Info.PixelsPerScanLine,8);Text(c," framebuffer-base=0x");Hex(c,a.GopMode.FrameBufferBase,16);
  Text(c," framebuffer-bytes=0x");Hex(c,a.GopMode.FrameBufferSize,16);End(c);
  s=t->BootServices->LocateHandleBuffer(ByProtocol,&ImgGuidL,NULL,&count,&images);
  if(s==EFI_SUCCESS && images && count && count<=1024) {
    OutOwner(c,images,count,"out-string",(UINTN)a.OutMethods.OutputString);
    OutOwner(c,images,count,"err-string",(UINTN)a.ErrMethods.OutputString);
    OutOwner(c,images,count,"out-set-mode",(UINTN)a.OutMethods.SetMode);
    OutOwner(c,images,count,"gop-set-mode",(UINTN)a.GopMethods.SetMode);
    OutOwner(c,images,count,"pcd-get-bool",(UINTN)a.GetBool);
#ifdef GU_AD104_PCD_CAPTURE
    EFI_STATUS captured=CapturePcdProducer(c,&a,images,count);
    Text(c,"ad104-pcd-producer status=0x");Hex(c,captured,16);End(c);
    if(captured!=EFI_SUCCESS)Problem(c,"ad104-pcd-producer",captured);
#endif
  } else Problem(c,"ad104-output-images",EFI_NOT_READY);
  if(images && t->BootServices->FreePool(images)!=EFI_SUCCESS)Problem(c,"ad104-output-images-free",EFI_DEVICE_ERROR);
  /* All pointers above are trusted UEFI protocol buffers. No framebuffer,
   * arbitrary heap record, saved address, or extracted firmware is read. */
}
