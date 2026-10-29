/* GPL-2.0-only. Capture the one observed native PCD producer, never execute it. */
#define PCD_PRODUCER_BYTES 0x2e40U
static BOOLEAN PcdProducerCaptured;
static EFI_HANDLE PcdProducerHandle;
static EFI_LOADED_IMAGE_PROTOCOL *PcdProducerImage;
static UINT8 PcdProducerHash[32];
static PCD_PROTOCOL *PcdProducerProtocol;
static PCD_PROTOCOL_GET_BOOLEAN PcdProducerBool;
static PCD_PROTOCOL_GET_NEXT_TOKEN PcdProducerNext;
static PCD_PROTOCOL_GET_SIZE PcdProducerSize;
static UINT16 PcdU16(const UINT8 *b){return (UINT16)(b[0]|((UINT16)b[1]<<8));}
static UINT32 PcdU32(const UINT8 *b){return b[0]|((UINT32)b[1]<<8)|((UINT32)b[2]<<16)|((UINT32)b[3]<<24);}
static BOOLEAN PcdExecutable(const UINT8 *b,UINTN n,UINTN rva) {
  if(n<64 || b[0]!='M' || b[1]!='Z')return FALSE;
  UINTN pe=PcdU32(b+60);
  if(pe>n-24 || PcdU32(b+pe)!=0x4550 || PcdU16(b+pe+4)!=0x8664)return FALSE;
  UINTN count=PcdU16(b+pe+6),opt=PcdU16(b+pe+20),table=pe+24+opt;
  if(!count || count>96 || opt<64 || table>n || count>(n-table)/40 ||
     PcdU16(b+pe+24)!=0x20b || PcdU32(b+pe+80)!=n)return FALSE;
  UINTN found=0;
  for(UINTN i=0;i<count;i++) {
    const UINT8 *s=b+table+40*i;UINTN va=PcdU32(s+12),bytes=PcdU32(s+8);
    if(!bytes)bytes=PcdU32(s+16);
    if(va>n || bytes>n-va)return FALSE;
    if(rva>=va && rva-va<bytes) {
      if(!(PcdU32(s+36)&0x20000000U))return FALSE;
      found++;
    }
  }
  return found==1;
}
static BOOLEAN PcdFilePath(const EFI_DEVICE_PATH_PROTOCOL *path) {
  static const UINT8 guid[]={0x57,0x72,0xcf,0x80,0xab,0x87,0xf9,0x47,0xa3,0xfe,0xd5,0x0b,0x76,0xd8,0x95,0x41};
  const UINT8 *b=(const VOID *)path;
  return b && b[0]==4 && b[1]==6 && PcdU16(b+2)==20 && PfEqual(b+4,guid,16) &&
    b[20]==0x7f && b[21]==0xff && PcdU16(b+22)==4;
}
static BOOLEAN PcdMatches(EFI_LOADED_IMAGE_PROTOCOL *im,const OUT_STATE *o) {
  if(!im || !im->ImageBase || im->ImageSize!=PCD_PRODUCER_BYTES ||
     im->ImageCodeType!=EfiBootServicesCode || (UINTN)im->ImageBase>MAX_UINTN-PCD_PRODUCER_BYTES ||
     !PcdFilePath(im->FilePath))return FALSE;
  UINTN base=(UINTN)im->ImageBase,targets[]={(UINTN)o->GetBool,(UINTN)o->Next,(UINTN)o->Size};
  if(targets[0]<base || targets[0]-base!=0x4f4)return FALSE;
  for(UINTN i=0;i<3;i++)if(targets[i]<base || targets[i]-base>=PCD_PRODUCER_BYTES ||
    !PcdExecutable(im->ImageBase,im->ImageSize,targets[i]-base))return FALSE;
  return TRUE;
}
static BOOLEAN PcdStable(CAPTURE *c,EFI_HANDLE handle,EFI_LOADED_IMAGE_PROTOCOL *original,
                         const OUT_STATE *o,const UINT8 *copy) {
  EFI_LOADED_IMAGE_PROTOCOL *fresh=NULL;PCD_PROTOCOL *protocol=NULL;
  if(c->Table->BootServices->HandleProtocol(handle,&ImgGuidL,(VOID **)&fresh)!=EFI_SUCCESS || fresh!=original ||
     c->Table->BootServices->LocateProtocol(&OutPcdGuid,NULL,(VOID **)&protocol)!=EFI_SUCCESS || protocol!=o->Pcd ||
     protocol->GetBool!=o->GetBool || protocol->GetNextToken!=o->Next || protocol->GetSize!=o->Size ||
     !PcdMatches(fresh,o))return FALSE;
  for(UINTN i=0;i<PCD_PRODUCER_BYTES;i++)if(copy[i]!=((volatile UINT8 *)fresh->ImageBase)[i])return FALSE;
  return TRUE;
}
static EFI_STATUS CapturePcdProducer(CAPTURE *c,const OUT_STATE *o,EFI_HANDLE *images,UINTN count) {
  PcdProducerCaptured=FALSE;
  EFI_BOOT_SERVICES *bs=c->Table->BootServices;
  if(c->Io!=EFI_SUCCESS || !c->DumpRoot || !c->DumpRoot->Open || !bs->AllocatePool || !bs->LocateProtocol || count>1024)return EFI_UNSUPPORTED;
  EFI_LOADED_IMAGE_PROTOCOL *target=NULL;EFI_HANDLE owner=NULL;UINTN found=0;
  for(UINTN i=0;i<count;i++) {
    EFI_LOADED_IMAGE_PROTOCOL *im=NULL;
    if(bs->HandleProtocol(images[i],&ImgGuidL,(VOID **)&im)==EFI_SUCCESS && PcdMatches(im,o)) {
      target=im;owner=images[i];found++;
    }
  }
  if(found!=1)return EFI_NOT_FOUND;
  CHAR16 name[]=L"\\RTX3090-UNLOCK\\PCDPROD.BIN";
  if(!NewDump(c,c->DumpRoot,name))return EFI_ALREADY_STARTED;
  UINT8 *copy=NULL;EFI_STATUS s=bs->AllocatePool(EfiBootServicesData,PCD_PRODUCER_BYTES,(VOID **)&copy);
  if(s!=EFI_SUCCESS || !copy)return EFI_OUT_OF_RESOURCES;
  for(UINTN i=0;i<PCD_PRODUCER_BYTES;i++)copy[i]=((volatile UINT8 *)target->ImageBase)[i];
  if(!PcdStable(c,owner,target,o,copy)){s=EFI_NOT_READY;goto done;}
  EFI_FILE_PROTOCOL *file=NULL;
  s=c->DumpRoot->Open(c->DumpRoot,&file,name,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0);
  if(s!=EFI_SUCCESS || !file){if(s==EFI_SUCCESS)s=EFI_DEVICE_ERROR;goto done;}
  if(file==c->DumpRoot){s=EFI_COMPROMISED_DATA;goto done;}
  if(!file->Write || !file->Flush || !file->Close){s=EFI_UNSUPPORTED;if(file->Close)(void)file->Close(file);goto done;}
  /* Opening the output can run firmware callbacks; recheck before writing. */
  if(!PcdStable(c,owner,target,o,copy))s=EFI_NOT_READY;
  else {
    UINTN bytes=PCD_PRODUCER_BYTES;s=file->Write(file,&bytes,copy);
    if(s==EFI_SUCCESS && bytes!=PCD_PRODUCER_BYTES)s=EFI_DEVICE_ERROR;
    EFI_STATUS flush=file->Flush(file);if(s==EFI_SUCCESS)s=flush;
  }
  {EFI_STATUS close=file->Close(file);if(s==EFI_SUCCESS)s=close;}
  if(s==EFI_SUCCESS && !PcdStable(c,owner,target,o,copy))s=EFI_NOT_READY;
  if(s==EFI_SUCCESS) {
    UINT8 digest[32];GuSha256(copy,PCD_PRODUCER_BYTES,digest);
    Text(c,"ad104-pcd-producer file=PCDPROD.BIN bytes=0x00002e40 sha256=");
    for(UINTN i=0;i<32;i++)Hex(c,digest[i],2);Text(c," stable-copy=1 executed=0");End(c);
    for(UINTN i=0;i<32;i++)PcdProducerHash[i]=digest[i];
  }
done:
  if(bs->FreePool(copy)!=EFI_SUCCESS && s==EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  if(s==EFI_SUCCESS){
    PcdProducerHandle=owner;PcdProducerImage=target;PcdProducerProtocol=o->Pcd;
    PcdProducerBool=o->GetBool;PcdProducerNext=o->Next;PcdProducerSize=o->Size;PcdProducerCaptured=TRUE;
  }
  return s;
}
#ifdef GU_AD104_TEXT_TEST
static BOOLEAN PcdVerifyCaptured(EFI_SYSTEM_TABLE *t,const OUT_STATE *o) {
  EFI_LOADED_IMAGE_PROTOCOL *im=NULL;UINT8 hash[32];
  if(!PcdProducerCaptured || o->Pcd!=PcdProducerProtocol || o->GetBool!=PcdProducerBool ||
     o->Next!=PcdProducerNext || o->Size!=PcdProducerSize ||
     t->BootServices->HandleProtocol(PcdProducerHandle,&ImgGuidL,(VOID **)&im)!=EFI_SUCCESS ||
     im!=PcdProducerImage || !PcdMatches(im,o))return FALSE;
  GuSha256(im->ImageBase,im->ImageSize,hash);
  return PfEqual(hash,PcdProducerHash,sizeof(hash));
}
#endif
#ifdef GU_AD104_GOP_TEST
static BOOLEAN PcdKnownText(const UINT8 *base,UINTN size) {
  return size==PCD_PRODUCER_BYTES && PcdExecutable(base,size,0x4f4) &&
    PfHash(base+0x2a0,0x236d,"cb64fe58020cf7512de99c86d8284baca00dd679109680ea1af371081e3b6367");
}
#endif
