/* GPL-2.0-only. Production handoff/capture integration; no GPU mutation code linked. */
#if !defined(GU_DIAGNOSTIC_ONLY) || !defined(GU_EVGA_SHARED_ESP_ADMISSION) || defined(GU_EVGA_STOP_ONLY) || defined(GU_EVGA_UNLOCK) || defined(GU_USB_MEDIA) || defined(GU_DIAGNOSTIC_PROBE)
#error Integrated capture is an isolated shared-ESP diagnostic only
#endif
#define efi_main handoff_unused_entry
#include "handoff.c"
#undef efi_main
typedef struct {const CHAR16 *path;UINTN bytes;const char *hash;} GU_ASSET_SPEC;
static const GU_ASSET_SPEC AssetSpecs[]={
 {L"\\RTX3090-UNLOCK\\ASSETS\\booter_load_ga102.bin",61256,"056cbc064a888f80717726c54d7f64fcd68044d123870c95a74f37f4975dcaa4"},
 {L"\\RTX3090-UNLOCK\\ASSETS\\v67-ga102-610.43.03.bin",64000,"2a0aa1f86d0b6afff5214d641d87d46dc98ef06ed908ff05d0ddbad5ffb7e197"},
 {L"\\RTX3090-UNLOCK\\ASSETS\\gsp_ga10x.bin",84277400,"572373620a37418f24dc16b5031c39338778c3257e48e8408de9a57291b24f3a"},
 {L"\\RTX3090-UNLOCK\\ASSETS\\gsp_boot_ga102.bin",24576,"3df651acb174fd5f5561a1996965b192f50c5d763168446ed0d4ead19d895768"},
 {L"\\RTX3090-UNLOCK\\ASSETS\\gsp_boot_desc_ga102.bin",84,"cc5a3fc1f4c47f2fe107b1b68f0176d8a448e6dec84b575e5560ccef5262bf15"}
};
static EFI_STATUS WriteReceipt(EFI_FILE_PROTOCOL *file,const char *message){
 UINT8 data[512]={0},readback[512];UINTN bytes=sizeof(data);
 for(UINTN i=0;message[i] && i<sizeof(data)-1;i++)data[i]=(UINT8)message[i];
 EFI_STATUS s=file->SetPosition(file,0);
 if(s==EFI_SUCCESS)s=file->Write(file,&bytes,data);
 if(s==EFI_SUCCESS && bytes!=sizeof(data))s=EFI_BAD_BUFFER_SIZE;
 if(s==EFI_SUCCESS)s=file->Flush(file);
 if(s==EFI_SUCCESS)s=file->SetPosition(file,0);
 bytes=sizeof(readback);if(s==EFI_SUCCESS)s=file->Read(file,&bytes,readback);
 if(s==EFI_SUCCESS && bytes!=sizeof(readback))s=EFI_BAD_BUFFER_SIZE;
 if(s==EFI_SUCCESS)for(UINTN i=0;i<sizeof(data);i++)if(data[i]!=readback[i]){s=EFI_COMPROMISED_DATA;break;}
 return s;
}
static EFI_STATUS OpenReceipt(EFI_FILE_PROTOCOL *root,EFI_FILE_PROTOCOL **receipt){
 const CHAR16 path[]=L"\\RTX3090-UNLOCK\\INTG01.ARMED";
 EFI_STATUS s=root->Open(root,receipt,(CHAR16 *)path,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE,0);
 if(s!=EFI_SUCCESS || !*receipt)return s==EFI_SUCCESS?EFI_DEVICE_ERROR:s;
 UINT8 data[512],extra;UINTN bytes=sizeof(data),tail=1;
 s=(*receipt)->Read(*receipt,&bytes,data);
 if(s==EFI_SUCCESS && bytes!=sizeof(data))s=EFI_BAD_BUFFER_SIZE;
 if(s==EFI_SUCCESS)s=(*receipt)->Read(*receipt,&tail,&extra);
 if(s==EFI_SUCCESS && tail)s=EFI_BAD_BUFFER_SIZE;
 const char pending[]="PENDING INTG01\r\n";
 if(s==EFI_SUCCESS)for(UINTN i=0;i<sizeof(data);i++){
  UINT8 expected=i<sizeof(pending)-1?(UINT8)pending[i]:0;
  if(data[i]!=expected){s=EFI_ACCESS_DENIED;break;}
 }
 if(s!=EFI_SUCCESS){(*receipt)->Close(*receipt);*receipt=NULL;}
 return s;
}
static EFI_STATUS LoadRetained(EFI_BOOT_SERVICES *bs,EFI_FILE_PROTOCOL *root,const GU_ASSET_SPEC *spec,VOID **data){
 EFI_FILE_PROTOCOL *file=NULL;UINTN got=spec->bytes,tail=1;UINT8 extra;
 EFI_STATUS s=root->Open(root,&file,(CHAR16 *)spec->path,EFI_FILE_MODE_READ,0);
 if(s!=EFI_SUCCESS || !file)return s==EFI_SUCCESS?EFI_DEVICE_ERROR:s;
 s=bs->AllocatePool(EfiBootServicesData,spec->bytes,data);
 if(s==EFI_SUCCESS && !*data)s=EFI_OUT_OF_RESOURCES;
 if(s==EFI_SUCCESS){s=file->Read(file,&got,*data);if(s==EFI_SUCCESS && got!=spec->bytes)s=EFI_BAD_BUFFER_SIZE;}
 if(s==EFI_SUCCESS){s=file->Read(file,&tail,&extra);if(s==EFI_SUCCESS && tail)s=EFI_BAD_BUFFER_SIZE;}
 if(s==EFI_SUCCESS && !GuHashMatches(*data,spec->bytes,spec->hash))s=EFI_COMPROMISED_DATA;
 EFI_STATUS closed=file->Close(file);return s==EFI_SUCCESS?closed:s;
}
static EFI_STATUS RetainAll(EFI_BOOT_SERVICES *bs,EFI_FILE_PROTOCOL *root,const GU_ASSET_SPEC *specs,UINTN count){
 VOID *data[5]={0};if(count!=5)return EFI_INVALID_PARAMETER;
 EFI_STATUS s=EFI_SUCCESS;
 for(UINTN i=0;i<count && s==EFI_SUCCESS;i++)s=LoadRetained(bs,root,&specs[i],&data[i]);
 /* Second pass proves all five allocations remain readable together. */
 for(UINTN i=0;i<count && s==EFI_SUCCESS;i++)if(!GuHashMatches(data[i],specs[i].bytes,specs[i].hash))s=EFI_COMPROMISED_DATA;
 for(UINTN i=0;i<count;i++)if(data[i]){EFI_STATUS freed=bs->FreePool(data[i]);if(s==EFI_SUCCESS)s=freed;}
 return s;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *table){
 if(!table || !table->BootServices || !table->ConOut)return EFI_INVALID_PARAMETER;
 Table=table;Self=image;EFI_BOOT_SERVICES *bs=table->BootServices;
 if(!bs->SetWatchdogTimer || bs->SetWatchdogTimer(0,0,0,NULL)!=EFI_SUCCESS)return EFI_UNSUPPORTED;
 EFI_LOADED_IMAGE_PROTOCOL *loaded=NULL;EFI_STATUS s=bs->HandleProtocol(image,&LoadedGuid,(VOID **)&loaded);
 if(s!=EFI_SUCCESS || !loaded || !loaded->DeviceHandle)return EFI_SECURITY_VIOLATION;
 OwnDevice=loaded->DeviceHandle;EFI_HANDLE pinned=NULL;EFI_DEVICE_PATH_PROTOCOL *path=NULL;BOOLEAN usb=FALSE,partition=FALSE;
 s=FindPartition(&GuWindowsPartition,&pinned);
 if(s!=EFI_SUCCESS || pinned!=OwnDevice || PathFor(pinned,&path)!=EFI_SUCCESS || !GuInspectPath(path,&GuWindowsPartition,NULL,&usb,&partition) || usb || !partition)return EFI_SECURITY_VIOLATION;
 EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;EFI_FILE_PROTOCOL *gateRoot=NULL,*marker=NULL;
 s=bs->HandleProtocol(pinned,&FsGuid,(VOID **)&fs);if(s!=EFI_SUCCESS || !fs)return EFI_DEVICE_ERROR;
 s=fs->OpenVolume(fs,&gateRoot);if(s!=EFI_SUCCESS || !gateRoot)return EFI_DEVICE_ERROR;
 s=OpenReceipt(gateRoot,&marker);
 if(s==EFI_SUCCESS)s=WriteReceipt(marker,"STARTED INTG01 - NO GPU MUTATION - DO NOT REPEAT\r\n");
 EFI_STATUS closed=gateRoot->Close(gateRoot);if(s==EFI_SUCCESS)s=closed;
 if(s!=EFI_SUCCESS){if(marker)marker->Close(marker);return s;}
 Say(L"INTG01 ENTERED. Production Prepare/log/capture; no GPU mutation.\r\n");
 s=Prepare(NULL);BOOLEAN prepared=s==EFI_SUCCESS;
 if(s==EFI_SUCCESS)s=Record(NULL,"intg01-prepare-complete",s);
 if(s==EFI_SUCCESS){s=RetainAll(bs,Root,AssetSpecs,5);EFI_STATUS recorded=Record(NULL,"intg01-retained-assets-released",s);if(s==EFI_SUCCESS)s=recorded;}
 if(s==EFI_SUCCESS){s=GuCaptureGraphOnVolume(Table,Log,Root);EFI_STATUS recorded=Record(NULL,"intg01-capture-result",s);if(s==EFI_SUCCESS)s=recorded;}
 EFI_STATUS result=s;
 EFI_STATUS written=WriteReceipt(marker,result==EFI_SUCCESS?"COMPLETE INTG01 - PREPARE ASSETS CAPTURE - NO GPU MUTATION\r\n":"FAILED INTG01 - PRESERVE LOG - NO GPU MUTATION - DO NOT REPEAT\r\n");
 closed=marker->Close(marker);if(written==EFI_SUCCESS)written=closed;
 if(result==EFI_SUCCESS)result=written;
 if(Log){s=Record(NULL,"intg01-ready-windows",result);if(result==EFI_SUCCESS)result=s;}
 Say(result==EFI_SUCCESS?L"INTG01 COMPLETE. Log/capture saved. Windows in 10 seconds.\r\n":L"INTG01 FAILED. Preserve files. Windows handoff if prepared.\r\n");
 if(!bs->Stall || bs->Stall(10000000)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
 if(prepared && written==EFI_SUCCESS)return Windows(NULL);
 if(Log && Root)(void)CloseBootFiles(NULL,NULL);
 else if(Root){Root->Close(Root);Root=NULL;}
 return result;
}
