/* GPL-2.0-only. CPU-only shared EFI asset reader; no GPU calls. */
#pragma once
#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>
#include "sha256.h"
static EFI_STATUS GuReadVerifiedAsset(EFI_BOOT_SERVICES *bs,EFI_FILE_PROTOCOL *parent,
 const CHAR16 *path,UINTN bytes,const char *hash,VOID **out) {
 if(!bs || !bs->AllocatePool || !bs->FreePool || !parent || !parent->Open || !out || !bytes || bytes>(128U<<20))return EFI_INVALID_PARAMETER;
 *out=NULL;EFI_FILE_PROTOCOL *file=NULL;EFI_STATUS s=parent->Open(parent,&file,(CHAR16 *)path,EFI_FILE_MODE_READ,0);
 if(s!=EFI_SUCCESS || !file)return s==EFI_SUCCESS?EFI_NOT_FOUND:s;
 if(!file->Close || !file->GetInfo || !file->Read){if(file->Close)(void)file->Close(file);return EFI_UNSUPPORTED;}
 union {EFI_FILE_INFO info;UINT8 raw[512];} meta;UINTN n=sizeof(meta);EFI_GUID info=EFI_FILE_INFO_ID;
 s=file->GetInfo(file,&info,&n,&meta);
 if(s==EFI_SUCCESS && (n<SIZE_OF_EFI_FILE_INFO || n>sizeof(meta) || meta.info.Size<SIZE_OF_EFI_FILE_INFO || meta.info.Size>n || (meta.info.Attribute&EFI_FILE_DIRECTORY) || meta.info.FileSize!=bytes))s=EFI_COMPROMISED_DATA;
 VOID *data=NULL;if(s==EFI_SUCCESS)s=bs->AllocatePool(EfiLoaderData,bytes,&data);
 if(s==EFI_SUCCESS && !data)s=EFI_OUT_OF_RESOURCES;
 for(UINTN done=0;s==EFI_SUCCESS && done<bytes;) {
  UINTN want=bytes-done;if(want>1048576)want=1048576;n=want;
  s=file->Read(file,&n,(UINT8 *)data+done);if(s==EFI_SUCCESS && n!=want)s=EFI_END_OF_FILE;
  if(s==EFI_SUCCESS)done+=n;
 }
 EFI_STATUS closed=file->Close(file);if(s==EFI_SUCCESS)s=closed;
 if(s==EFI_SUCCESS && !GuHashMatches(data,bytes,hash))s=EFI_CRC_ERROR;
 if(s!=EFI_SUCCESS){if(data && bs->FreePool(data)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;return s;}
 *out=data;return EFI_SUCCESS;
}
