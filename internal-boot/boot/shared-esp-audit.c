/* GPL-2.0-only. Observation-only same-ESP admission and Windows handoff. */
#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include "media.h"

static EFI_GUID LoadedGuid=EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID PathGuid=EFI_DEVICE_PATH_PROTOCOL_GUID;
static EFI_GUID FsGuid=EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_GUID * volatile RelocationAnchor=&LoadedGuid;

/* Refuse USB ancestry, missing/duplicate partition handles and mismatched own image. */
static EFI_STATUS FindOwnWindowsEsp(EFI_BOOT_SERVICES *bs,EFI_HANDLE own) {
 EFI_HANDLE *handles=NULL;UINTN count=0,matches=0;EFI_HANDLE match=NULL;
 EFI_STATUS s=bs->LocateHandleBuffer(ByProtocol,&FsGuid,NULL,&count,&handles);
 if(s!=EFI_SUCCESS)return s;
 if(count && !handles)return EFI_DEVICE_ERROR;
 for(UINTN i=0;i<count;i++){
  EFI_DEVICE_PATH_PROTOCOL *path=NULL;BOOLEAN usb=FALSE,partition=FALSE;
  s=bs->HandleProtocol(handles[i],&PathGuid,(VOID **)&path);
  if(s==EFI_SUCCESS && path && GuInspectPath(path,&GuWindowsPartition,NULL,&usb,&partition) && partition){
   if(usb){bs->FreePool(handles);return EFI_SECURITY_VIOLATION;}
   matches++;match=handles[i];
  }
 }
 if(handles)bs->FreePool(handles);
 return matches==1 && match==own?EFI_SUCCESS:EFI_SECURITY_VIOLATION;
}

static EFI_STATUS EspFilePath(EFI_BOOT_SERVICES *bs,EFI_HANDLE device,const CHAR16 *name,UINTN nameBytes,EFI_DEVICE_PATH_PROTOCOL **out){
 EFI_DEVICE_PATH_PROTOCOL *base=NULL;UINTN bytes=0;BOOLEAN usb=FALSE,partition=FALSE;
 EFI_STATUS s=bs->HandleProtocol(device,&PathGuid,(VOID **)&base);
 if(s!=EFI_SUCCESS || !base || !GuInspectPath(base,&GuWindowsPartition,&bytes,&usb,&partition) || usb || !partition)return EFI_SECURITY_VIOLATION;
 UINTN nodeBytes=4+nameBytes,total=bytes-4+nodeBytes+4;UINT8 *data=NULL;
 s=bs->AllocatePool(EfiBootServicesData,total,(VOID **)&data);if(s!=EFI_SUCCESS || !data)return EFI_OUT_OF_RESOURCES;
 for(UINTN i=0;i<bytes-4;i++)data[i]=((UINT8 *)base)[i];
 EFI_DEVICE_PATH_PROTOCOL *node=(VOID *)(data+bytes-4);
 node->Type=MEDIA_DEVICE_PATH;node->SubType=MEDIA_FILEPATH_DP;node->Length[0]=(UINT8)nodeBytes;node->Length[1]=(UINT8)(nodeBytes>>8);
 for(UINTN i=0;i<nameBytes;i++)data[bytes+i]=((const UINT8 *)name)[i];
 EFI_DEVICE_PATH_PROTOCOL *end=(VOID *)(data+bytes-4+nodeBytes);
 end->Type=END_DEVICE_PATH_TYPE;end->SubType=END_ENTIRE_DEVICE_PATH_SUBTYPE;end->Length[0]=4;end->Length[1]=0;
 *out=(VOID *)data;return EFI_SUCCESS;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *table){
 if(!table || !table->BootServices || !table->ConOut || RelocationAnchor!=&LoadedGuid)return EFI_INVALID_PARAMETER;
 EFI_BOOT_SERVICES *bs=table->BootServices;EFI_LOADED_IMAGE_PROTOCOL *loaded=NULL;
 table->ConOut->OutputString(table->ConOut,L"Shared ESP audit: no GPU access, no EFI writes. Windows handoff follows.\r\n");
 EFI_STATUS s=bs->HandleProtocol(image,&LoadedGuid,(VOID **)&loaded);
 if(s!=EFI_SUCCESS || !loaded || !loaded->DeviceHandle)return EFI_SECURITY_VIOLATION;
 EFI_HANDLE own=loaded->DeviceHandle;s=FindOwnWindowsEsp(bs,own);if(s!=EFI_SUCCESS)return s;
#ifdef GU_AUDIT_VISIBLE
 table->ConOut->OutputString(table->ConOut,L"AUDIT ENTERED ON PINNED ESP. Loading Windows in 10 seconds.\r\n");
 if(!bs->Stall)return EFI_UNSUPPORTED;
 s=bs->Stall(10000000);if(s!=EFI_SUCCESS)return s;
#endif
 const CHAR16 name[]=L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi";
 EFI_DEVICE_PATH_PROTOCOL *path=NULL;s=EspFilePath(bs,own,name,sizeof(name),&path);if(s!=EFI_SUCCESS)return s;
 EFI_HANDLE windows=NULL;s=bs->LoadImage(FALSE,image,path,NULL,0,&windows);bs->FreePool(path);
 if(s!=EFI_SUCCESS || !windows)return s==EFI_SUCCESS?EFI_LOAD_ERROR:s;
 EFI_LOADED_IMAGE_PROTOCOL *winLoaded=NULL;s=bs->HandleProtocol(windows,&LoadedGuid,(VOID **)&winLoaded);
 if(s!=EFI_SUCCESS || !winLoaded || winLoaded->DeviceHandle!=own){bs->UnloadImage(windows);return EFI_SECURITY_VIOLATION;}
 table->ConOut->OutputString(table->ConOut,L"Pinned shared ESP verified. Starting Windows Boot Manager.\r\n");
 return bs->StartImage(windows,NULL,NULL);
}
