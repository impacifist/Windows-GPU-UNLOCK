/* GPL-2.0-only. Copy a firmware-owned RAM buffer, never GPU BAR/PROM access. */
#define GU_FIRMROM_BYTES 0x24a00U
static BOOLEAN RomRam(EFI_BOOT_SERVICES *bs,VOID *map,UINTN capacity,UINTN address,UINTN bytes) {
  UINTN size=capacity,key=0,stride=0;UINT32 version=0;
  if(!bs->GetMemoryMap || !address || address>MAX_UINTN-bytes)return FALSE;
  EFI_STATUS status=bs->GetMemoryMap(&size,map,&key,&stride,&version);
  if(status!=EFI_SUCCESS || size>capacity || stride<sizeof(EFI_MEMORY_DESCRIPTOR) ||
     !size || size%stride || version!=EFI_MEMORY_DESCRIPTOR_VERSION)return FALSE;
  UINTN matches=0;
  for(UINTN at=0;at<size;at+=stride) {
    EFI_MEMORY_DESCRIPTOR *d=(VOID *)((UINT8 *)map+at);
    if(!d->NumberOfPages || d->NumberOfPages>MAX_UINT64/4096)continue;
    UINT64 length=d->NumberOfPages*4096;
    if(d->PhysicalStart>MAX_UINT64-length)continue;
    UINT64 end=d->PhysicalStart+length;
    if(address>=end || address+bytes<=d->PhysicalStart)continue;
    if(address<d->PhysicalStart || address+bytes>end || (d->Attribute&(EFI_MEMORY_RUNTIME|EFI_MEMORY_RP)) ||
       (d->Type!=EfiLoaderCode && d->Type!=EfiLoaderData &&
        d->Type!=EfiBootServicesCode && d->Type!=EfiBootServicesData))return FALSE;
    matches++;
  }
  return matches==1;
}
static VOID FirmwareRom(CAPTURE *c,EFI_HANDLE handle,EFI_PCI_IO_PROTOCOL *io,
                        EFI_FILE_PROTOCOL *root,const UINT32 *cfg) {
  UINTN loc[4];UINT8 *copy=NULL;VOID *map=NULL;
  VOID *rom=io->RomImage;UINT64 bytes=io->RomSize;EFI_STATUS status=EFI_NOT_READY;
  if(!rom || bytes!=GU_FIRMROM_BYTES || !root || !root->Open ||
     !io->GetLocation || io->GetLocation(io,&loc[0],&loc[1],&loc[2],&loc[3])!=EFI_SUCCESS ||
     loc[0]>65535 || loc[1]>255 || loc[2]>31 || loc[3]>7)goto done;
  if(c->Table->BootServices->AllocatePool(EfiBootServicesData,GU_FIRMROM_BYTES,(VOID **)&copy)!=EFI_SUCCESS || !copy)goto done;
  if(c->Table->BootServices->AllocatePool(EfiBootServicesData,65536,&map)!=EFI_SUCCESS || !map)goto done;
  if(!ProbeUnchanged(c,handle,io,cfg,loc) || io->RomImage!=rom || io->RomSize!=bytes)goto done;
  if(!RomRam(c->Table->BootServices,map,65536,(UINTN)rom,GU_FIRMROM_BYTES)) {
    Problem(c,"firmrom-not-admitted-ram",EFI_UNSUPPORTED);goto done;
  }
  if(io->RomImage!=rom || io->RomSize!=bytes)goto done;
  const volatile UINT8 *source=rom;
  for(UINTN i=0;i<GU_FIRMROM_BYTES;i++)copy[i]=source[i];
  /* Validate only the bounded first option-ROM/PCIR header. The full image chain
   * is for offline analysis; this is not a physical PROM or unlock profile. */
  UINTN pcir=(UINTN)copy[0x18]+((UINTN)copy[0x19]<<8);
  if(copy[0]!=0x55 || copy[1]!=0xaa || pcir<0x1a || pcir>GU_FIRMROM_BYTES-0x18 ||
     copy[pcir]!='P' || copy[pcir+1]!='C' || copy[pcir+2]!='I' || copy[pcir+3]!='R' ||
     copy[pcir+4]!=0xde || copy[pcir+5]!=0x10 || copy[pcir+6]!=0x82 || copy[pcir+7]!=0x27) {
    Problem(c,"firmrom-header",EFI_COMPROMISED_DATA);goto done;
  }
  UINTN pcirBytes=(UINTN)copy[pcir+0x0a]+((UINTN)copy[pcir+0x0b]<<8);
  UINTN imageBytes=((UINTN)copy[pcir+0x10]+((UINTN)copy[pcir+0x11]<<8))*512U;
  if(pcirBytes<0x18 || pcirBytes>GU_FIRMROM_BYTES-pcir ||
     !imageBytes || imageBytes>GU_FIRMROM_BYTES || pcir+pcirBytes>imageBytes) {
    Problem(c,"firmrom-header-length",EFI_COMPROMISED_DATA);goto done;
  }
  if(!ProbeUnchanged(c,handle,io,cfg,loc) || io->RomImage!=rom || io->RomSize!=bytes ||
     !RomRam(c->Table->BootServices,map,65536,(UINTN)rom,GU_FIRMROM_BYTES))goto done;
  if(io->RomImage!=rom || io->RomSize!=bytes)goto done;
  for(UINTN i=0;i<GU_FIRMROM_BYTES;i++)if(copy[i]!=source[i])goto done;
  if(!ProbeUnchanged(c,handle,io,cfg,loc) || io->RomImage!=rom || io->RomSize!=bytes)goto done;
  if(!NewDump(c,root,L"\\RTX3090-UNLOCK\\FIRMROM.BIN"))goto done;
  EFI_FILE_PROTOCOL *file=NULL;
  status=root->Open(root,&file,L"\\RTX3090-UNLOCK\\FIRMROM.BIN",EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0);
  if(status!=EFI_SUCCESS)goto done;
  if(!file || file==root){status=EFI_DEVICE_ERROR;goto done;}
  if(!file->Write || !file->Flush || !file->Close) {
    if(file->Close)(void)file->Close(file);
    status=EFI_UNSUPPORTED;goto done;
  }
  UINTN written=GU_FIRMROM_BYTES;
  status=file->Write(file,&written,copy);
  if(status==EFI_SUCCESS && written!=GU_FIRMROM_BYTES)status=EFI_END_OF_FILE;
  if(file->Flush(file)!=EFI_SUCCESS)status=EFI_DEVICE_ERROR;
  if(file->Close(file)!=EFI_SUCCESS)status=EFI_DEVICE_ERROR;
  if(status==EFI_SUCCESS) {
    UINT8 hash[32];GuSha256(copy,GU_FIRMROM_BYTES,hash);
    Text(c,"firmware-rom-buffer dump=FIRMROM.BIN bytes=0x00024a00 sha256=");
    for(UINTN i=0;i<32;i++)Hex(c,hash[i],2);
    Text(c," unlock-eligibility=0");End(c);
  }
done:
  if(status!=EFI_SUCCESS)Problem(c,"firmrom-incomplete",status);
  if(map && c->Table->BootServices->FreePool(map)!=EFI_SUCCESS)Problem(c,"firmrom-map-free",EFI_DEVICE_ERROR);
  if(copy && c->Table->BootServices->FreePool(copy)!=EFI_SUCCESS)Problem(c,"firmrom-copy-free",EFI_DEVICE_ERROR);
}
