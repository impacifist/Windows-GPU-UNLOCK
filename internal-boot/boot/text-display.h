/* GPL-2.0-only. Never calls text output or GOP mutation methods. */
#ifndef GU_TEXT_DISPLAY_H
#define GU_TEXT_DISPLAY_H
typedef struct {
  EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
  EFI_GRAPHICS_OUTPUT_PROTOCOL Methods;
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE Mode;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION Info;
  PCD_PROTOCOL *Pcd;
  PCD_PROTOCOL_GET_BOOLEAN GetBool;
  PCD_PROTOCOL_GET_NEXT_TOKEN Next;
  PCD_PROTOCOL_GET_SIZE Size;
} TEXT_DISPLAY;
static EFI_STATUS TextGopCollect(EFI_SYSTEM_TABLE *t,EFI_HANDLE child,TEXT_DISPLAY *out) {
  EFI_STATUS s=t->BootServices->HandleProtocol(child,&GopGuidL,(VOID **)&out->Gop);
  if(s!=EFI_SUCCESS || !out->Gop || !out->Gop->Mode || !out->Gop->Mode->Info)return EFI_NOT_READY;
  out->Methods=*out->Gop;out->Mode=*out->Gop->Mode;
  if(out->Mode.SizeOfInfo<sizeof(out->Info) || out->Mode.SizeOfInfo>4096 ||
     out->Mode.MaxMode!=5 || out->Mode.Mode!=2)return EFI_UNSUPPORTED;
  out->Info=*out->Mode.Info;
  if(out->Info.HorizontalResolution!=1024 || out->Info.VerticalResolution!=768 ||
     out->Info.PixelFormat!=PixelBlueGreenRedReserved8BitPerColor || out->Info.PixelsPerScanLine!=1024 ||
     out->Mode.FrameBufferBase!=0xd0000000ULL || out->Mode.FrameBufferSize!=0x300000)return EFI_UNSUPPORTED;
  return EFI_SUCCESS;
}
static EFI_STATUS TextPcdCollect(EFI_SYSTEM_TABLE *t,TEXT_DISPLAY *out) {
  EFI_STATUS s;
  OUT_STATE p={0};
  if(t->BootServices->LocateProtocol(&OutPcdGuid,NULL,(VOID **)&p.Pcd)!=EFI_SUCCESS || !p.Pcd)return EFI_NOT_READY;
  p.GetBool=p.Pcd->GetBool;p.Next=p.Pcd->GetNextToken;p.Size=p.Pcd->GetSize;
  if(!p.GetBool || !p.Next || !p.Size || !PcdVerifyCaptured(t,&p))return EFI_NOT_READY;
#ifdef GU_AD104_GOP_TEST
  if(!PcdKnownText(PcdProducerImage->ImageBase,PcdProducerImage->ImageSize))return EFI_UNSUPPORTED;
#endif
  BOOLEAN value=TRUE;UINTN steps=0;s=OutToken(&p,&value,&steps);
  if(s!=EFI_SUCCESS || value || !PcdVerifyCaptured(t,&p) || p.Pcd->GetBool!=p.GetBool ||
     p.Pcd->GetNextToken!=p.Next || p.Pcd->GetSize!=p.Size)return EFI_NOT_READY;
  out->Pcd=p.Pcd;out->GetBool=p.GetBool;out->Next=p.Next;out->Size=p.Size;
  return EFI_SUCCESS;
}
static EFI_STATUS TextDisplayCollect(EFI_SYSTEM_TABLE *t,EFI_HANDLE child,TEXT_DISPLAY *out) {
  EFI_STATUS s=TextGopCollect(t,child,out);if(s!=EFI_SUCCESS)return s;
  s=TextPcdCollect(t,out);if(s!=EFI_SUCCESS)return s;
  TEXT_DISPLAY after={0};s=TextGopCollect(t,child,&after);
  if(s!=EFI_SUCCESS || after.Gop!=out->Gop || !PfEqual(&after.Methods,&out->Methods,sizeof(out->Methods)) ||
     !PfEqual(&after.Mode,&out->Mode,sizeof(out->Mode)) || !PfEqual(&after.Info,&out->Info,sizeof(out->Info)))return EFI_NOT_READY;
  return EFI_SUCCESS;
}

#endif
