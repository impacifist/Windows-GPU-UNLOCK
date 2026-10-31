/* GPL-2.0-only. Retained memory and one-shot receipt test. No GPU access. */
#define GU_ASSET_ENTRY asset_unused_entry
#include "shared-esp-assets.c"
#undef GU_ASSET_ENTRY

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
 const CHAR16 path[]=L"\\RTX3090-UNLOCK\\RETAIN01.TXT";
 EFI_STATUS s=root->Open(root,receipt,(CHAR16 *)path,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE,0);
 if(s!=EFI_SUCCESS || !*receipt)return s==EFI_SUCCESS?EFI_DEVICE_ERROR:s;
 UINT8 data[512],extra;UINTN bytes=sizeof(data),tail=1;
 s=(*receipt)->Read(*receipt,&bytes,data);
 if(s==EFI_SUCCESS && bytes!=sizeof(data))s=EFI_BAD_BUFFER_SIZE;
 if(s==EFI_SUCCESS)s=(*receipt)->Read(*receipt,&tail,&extra);
 if(s==EFI_SUCCESS && tail)s=EFI_BAD_BUFFER_SIZE;
 const char pending[]="PENDING RETAIN01\r\n";
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
static EFI_STATUS RetainOnEsp(EFI_BOOT_SERVICES *bs,EFI_HANDLE device){
 EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;EFI_FILE_PROTOCOL *root=NULL,*receipt=NULL;
 EFI_STATUS s=bs->HandleProtocol(device,&FsGuid,(VOID **)&fs);
 if(s!=EFI_SUCCESS || !fs)return s==EFI_SUCCESS?EFI_DEVICE_ERROR:s;
 s=fs->OpenVolume(fs,&root);if(s!=EFI_SUCCESS || !root)return s==EFI_SUCCESS?EFI_DEVICE_ERROR:s;
 s=OpenReceipt(root,&receipt);
 if(s==EFI_SUCCESS)s=WriteReceipt(receipt,"STARTED RETAIN01 - NO GPU - DO NOT REPEAT\r\n");
 if(s==EFI_SUCCESS){
  s=RetainAll(bs,root,AssetSpecs,5);
  EFI_STATUS recorded=WriteReceipt(receipt,s==EFI_SUCCESS?
   "COMPLETE RETAIN01 - FIVE ASSETS RETAINED REHASHED FREED - NO GPU\r\n":
   "FAILED RETAIN01 - ASSET OR RELEASE ERROR - NO GPU - DO NOT REPEAT\r\n");
  if(s==EFI_SUCCESS)s=recorded;
 }
 if(receipt){EFI_STATUS closed=receipt->Close(receipt);if(s==EFI_SUCCESS)s=closed;}
 EFI_STATUS closed=root->Close(root);return s==EFI_SUCCESS?closed:s;
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *table){
 if(!table || !table->BootServices || !table->ConOut)return EFI_INVALID_PARAMETER;
 EFI_BOOT_SERVICES *bs=table->BootServices;EFI_LOADED_IMAGE_PROTOCOL *loaded=NULL;
 EFI_STATUS s=bs->HandleProtocol(image,&LoadedGuid,(VOID **)&loaded);
 if(s!=EFI_SUCCESS || !loaded || !loaded->DeviceHandle)return EFI_SECURITY_VIOLATION;
 s=FindOwnWindowsEsp(bs,loaded->DeviceHandle);if(s!=EFI_SUCCESS)return s;
 table->ConOut->OutputString(table->ConOut,L"RETAIN01 ENTERED. One receipt write test; no GPU access.\r\n");
 s=RetainOnEsp(bs,loaded->DeviceHandle);
 table->ConOut->OutputString(table->ConOut,s==EFI_SUCCESS?
  L"RETAIN01 COMPLETE: five assets retained/rehashed/freed, receipt verified. Windows in 10 seconds.\r\n":
  L"RETAIN01 FAILED OR ALREADY USED. Preserve receipt; do not repeat. Windows in 10 seconds.\r\n");
 if(!bs->Stall)return EFI_UNSUPPORTED;
 EFI_STATUS waited=bs->Stall(10000000);if(waited!=EFI_SUCCESS)return waited;
 return shared_audit_entry(image,table);
}
