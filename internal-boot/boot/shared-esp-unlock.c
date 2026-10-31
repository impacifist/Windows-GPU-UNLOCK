/* GPL-2.0-only. Shared-ESP one-shot actual transaction entry; experimental. */
#if defined(GU_EVGA_PACKAGE) && (!defined(GU_SHARED_ESP_UNLOCK) || !defined(GU_EVGA_STOP_ONLY) || !defined(GU_EVGA_PROBE_RESOURCES) || !defined(GU_EVGA_UNLOCK) || defined(GU_AUTOMATIC_BOOT) || defined(GU_HEADLESS_RESEARCH) || defined(GU_FULL_UNLOCK) || defined(GU_SHARED_ESP_SECOND))
#error Attended package requires the exact EVGA adapter and forbids automatic/legacy mode combinations
#endif
#if !defined(GU_EVGA_PACKAGE) && (!defined(GU_SHARED_ESP_UNLOCK) || !defined(GU_EVGA_STOP_ONLY) || !defined(GU_EVGA_PROBE_RESOURCES) || !defined(GU_EVGA_SHARED_ESP_ADMISSION) || defined(GU_USB_MEDIA) || !defined(GU_EVGA_UNLOCK))
#error SULK01 requires the isolated shared-ESP unlock, never USB
#endif
#ifdef GU_EVGA_PACKAGE
#define GU_SULK_PATH GU_PACKAGE_RECEIPT
#define GU_SULK_PENDING GU_PACKAGE_PENDING
#define GU_SULK_STARTED GU_PACKAGE_STARTED
#elif defined(GU_SHARED_ESP_SECOND)
#define GU_SULK_PATH L"\\RTX3090-UNLOCK\\SULK02.ARMED"
#define GU_SULK_PENDING "PENDING SULK02\r\n"
#define GU_SULK_STARTED "STARTED SULK02 - DO NOT REPEAT - SEE SULK02.TXT\r\n"
#else
#define GU_SULK_PATH L"\\RTX3090-UNLOCK\\SULK01.ARMED"
#define GU_SULK_PENDING "PENDING SULK01\r\n"
#define GU_SULK_STARTED "STARTED SULK01 - DO NOT REPEAT - SEE SULK01.TXT\r\n"
#endif
#define efi_main unlock_handoff_entry
#include "handoff.c"
#undef efi_main
#ifdef GU_EVGA_PACKAGE
#include "evga-package-media.h"
#endif
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
 const CHAR16 path[]=GU_SULK_PATH;
 EFI_STATUS s=root->Open(root,receipt,(CHAR16 *)path,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE,0);
 if(s!=EFI_SUCCESS || !*receipt)return s==EFI_SUCCESS?EFI_DEVICE_ERROR:s;
 UINT8 data[512],extra;UINTN bytes=sizeof(data),tail=1;
 s=(*receipt)->Read(*receipt,&bytes,data);
 if(s==EFI_SUCCESS && bytes!=sizeof(data))s=EFI_BAD_BUFFER_SIZE;
 if(s==EFI_SUCCESS)s=(*receipt)->Read(*receipt,&tail,&extra);
 if(s==EFI_SUCCESS && tail)s=EFI_BAD_BUFFER_SIZE;
 const char pending[]=GU_SULK_PENDING;
 if(s==EFI_SUCCESS)for(UINTN i=0;i<sizeof(data);i++){
  UINT8 expected=i<sizeof(pending)-1?(UINT8)pending[i]:0;
  if(data[i]!=expected){s=EFI_ACCESS_DENIED;break;}
 }
 if(s!=EFI_SUCCESS){(*receipt)->Close(*receipt);*receipt=NULL;}
 return s;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *table){
 if(!table || !table->BootServices || !table->ConOut)return EFI_INVALID_PARAMETER;
 Table=table;Self=image;EFI_BOOT_SERVICES *bs=table->BootServices;
 EFI_LOADED_IMAGE_PROTOCOL *loaded=NULL;EFI_STATUS s=bs->HandleProtocol(image,&LoadedGuid,(VOID **)&loaded);
 if(s!=EFI_SUCCESS || !loaded || !loaded->DeviceHandle)return EFI_SECURITY_VIOLATION;
 OwnDevice=loaded->DeviceHandle;EFI_HANDLE pinned=NULL;EFI_DEVICE_PATH_PROTOCOL *path=NULL;
#ifdef GU_EVGA_PACKAGE
 if(PathFor(OwnDevice,&path)!=EFI_SUCCESS || !GuPackageMediaAllowed(path))return EFI_SECURITY_VIOLATION;
 pinned=OwnDevice;
#if GU_PACKAGE_LAYOUT == 1
 EFI_HANDLE unique=NULL;s=FindPartition(&GuWindowsPartition,&unique);if(s!=EFI_SUCCESS || unique!=pinned)return EFI_SECURITY_VIOLATION;
#elif GU_PACKAGE_LAYOUT == 3
 EFI_HANDLE unique=NULL;s=FindPartition(&GuInternalPartition,&unique);if(s!=EFI_SUCCESS || unique!=pinned)return EFI_SECURITY_VIOLATION;
#endif
#else
 BOOLEAN usb=FALSE,partition=FALSE;
 s=FindPartition(&GuWindowsPartition,&pinned);
 if(s!=EFI_SUCCESS || pinned!=OwnDevice || PathFor(pinned,&path)!=EFI_SUCCESS || !GuInspectPath(path,&GuWindowsPartition,NULL,&usb,&partition) || usb || !partition)return EFI_SECURITY_VIOLATION;
#endif
 EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;EFI_FILE_PROTOCOL *root=NULL,*marker=NULL;
 s=bs->HandleProtocol(pinned,&FsGuid,(VOID **)&fs);if(s!=EFI_SUCCESS || !fs)return EFI_DEVICE_ERROR;
 s=fs->OpenVolume(fs,&root);if(s!=EFI_SUCCESS || !root)return EFI_DEVICE_ERROR;
 s=OpenReceipt(root,&marker);
 if(s==EFI_SUCCESS)s=WriteReceipt(marker,GU_SULK_STARTED);
 if(marker){EFI_STATUS closed=marker->Close(marker);if(s==EFI_SUCCESS)s=closed;}
 EFI_STATUS closed=root->Close(root);if(s==EFI_SUCCESS)s=closed;if(s!=EFI_SUCCESS)return s;
 Say(L"ATTENDED EVGA GPU UNLOCK: ONE TRANSACTION.\r\nFresh Shift+G after guards starts driver release, GPU/PCI writes, DMA, firmware execution and FLR.\r\nDisplay may go black; failure may require power off. No automatic retry.\r\n");
 return unlock_handoff_entry(image,table);
}
