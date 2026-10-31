/* GPL-2.0-only. USB entry that launches the staged no-GPU ESP audit. */
#define efi_main audit_unused_entry
#include "shared-esp-audit.c"
#undef efi_main

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *table) {
 if(!table || !table->BootServices || !table->ConOut)return EFI_INVALID_PARAMETER;
 EFI_BOOT_SERVICES *bs=table->BootServices;EFI_HANDLE *handles=NULL,esp=NULL;UINTN count=0,matches=0;
 table->ConOut->OutputString(table->ConOut,L"Shared ESP audit launcher: no GPU access or firmware changes.\r\n");
 EFI_STATUS s=bs->LocateHandleBuffer(ByProtocol,&FsGuid,NULL,&count,&handles);
 if(s!=EFI_SUCCESS || (count && !handles))return EFI_DEVICE_ERROR;
 for(UINTN i=0;i<count;i++){
  EFI_DEVICE_PATH_PROTOCOL *path=NULL;BOOLEAN usb=FALSE,partition=FALSE;
  if(bs->HandleProtocol(handles[i],&PathGuid,(VOID **)&path)==EFI_SUCCESS && path &&
     GuInspectPath(path,&GuWindowsPartition,NULL,&usb,&partition) && partition){
   if(usb){if(handles)bs->FreePool(handles);return EFI_SECURITY_VIOLATION;}
   esp=handles[i];matches++;
  }
 }
 if(handles)bs->FreePool(handles);
 if(matches!=1)return EFI_SECURITY_VIOLATION;
 const CHAR16 name[]=L"\\EFI\\EVGA-SHARED-INTEGRATION.EFI";EFI_DEVICE_PATH_PROTOCOL *path=NULL;
 s=EspFilePath(bs,esp,name,sizeof(name),&path);if(s!=EFI_SUCCESS)return s;
 EFI_HANDLE child=NULL;s=bs->LoadImage(FALSE,image,path,NULL,0,&child);bs->FreePool(path);
 if(s!=EFI_SUCCESS || !child)return s==EFI_SUCCESS?EFI_LOAD_ERROR:s;
 EFI_LOADED_IMAGE_PROTOCOL *loaded=NULL;s=bs->HandleProtocol(child,&LoadedGuid,(VOID **)&loaded);
 if(s!=EFI_SUCCESS || !loaded || loaded->DeviceHandle!=esp){bs->UnloadImage(child);return EFI_SECURITY_VIOLATION;}
 table->ConOut->OutputString(table->ConOut,L"AUDIT LOADED FROM PINNED ESP. Starting in 10 seconds.\r\n");
 if(!bs->Stall){bs->UnloadImage(child);return EFI_UNSUPPORTED;}
 s=bs->Stall(10000000);if(s!=EFI_SUCCESS){bs->UnloadImage(child);return s;}
 return bs->StartImage(child,NULL,NULL);
}
