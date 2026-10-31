/* GPL-2.0-only. Read and hash five assets; no GPU protocols or filesystem writes. */
#define efi_main shared_audit_entry
#include "shared-esp-audit.c"
#undef efi_main
#include "../unlock/sha256.h"

typedef struct {const CHAR16 *path;UINTN bytes;const char *hash;} GU_ASSET_SPEC;
static const GU_ASSET_SPEC AssetSpecs[]={
 {L"\\RTX3090-UNLOCK\\ASSETS\\booter_load_ga102.bin",61256,"056cbc064a888f80717726c54d7f64fcd68044d123870c95a74f37f4975dcaa4"},
 {L"\\RTX3090-UNLOCK\\ASSETS\\v67-ga102-610.43.03.bin",64000,"2a0aa1f86d0b6afff5214d641d87d46dc98ef06ed908ff05d0ddbad5ffb7e197"},
 {L"\\RTX3090-UNLOCK\\ASSETS\\gsp_ga10x.bin",84277400,"572373620a37418f24dc16b5031c39338778c3257e48e8408de9a57291b24f3a"},
 {L"\\RTX3090-UNLOCK\\ASSETS\\gsp_boot_ga102.bin",24576,"3df651acb174fd5f5561a1996965b192f50c5d763168446ed0d4ead19d895768"},
 {L"\\RTX3090-UNLOCK\\ASSETS\\gsp_boot_desc_ga102.bin",84,"cc5a3fc1f4c47f2fe107b1b68f0176d8a448e6dec84b575e5560ccef5262bf15"}
};
static EFI_STATUS CheckOneAsset(EFI_BOOT_SERVICES *bs,EFI_FILE_PROTOCOL *root,const GU_ASSET_SPEC *spec){
 EFI_FILE_PROTOCOL *file=NULL;VOID *data=NULL;UINTN got=spec->bytes;UINT8 extra;UINTN tail=1;
 EFI_STATUS s=root->Open(root,&file,(CHAR16 *)spec->path,EFI_FILE_MODE_READ,0);
 if(s!=EFI_SUCCESS || !file)return s==EFI_SUCCESS?EFI_DEVICE_ERROR:s;
 s=bs->AllocatePool(EfiBootServicesData,spec->bytes,&data);
 if(s==EFI_SUCCESS && !data)s=EFI_OUT_OF_RESOURCES;
 if(s==EFI_SUCCESS){s=file->Read(file,&got,data);if(s==EFI_SUCCESS && got!=spec->bytes)s=EFI_BAD_BUFFER_SIZE;}
 if(s==EFI_SUCCESS){s=file->Read(file,&tail,&extra);if(s==EFI_SUCCESS && tail)s=EFI_BAD_BUFFER_SIZE;}
 if(s==EFI_SUCCESS && !GuHashMatches(data,spec->bytes,spec->hash))s=EFI_COMPROMISED_DATA;
 EFI_STATUS closed=file->Close(file);if(s==EFI_SUCCESS)s=closed;
 if(data){EFI_STATUS freed=bs->FreePool(data);if(s==EFI_SUCCESS)s=freed;}
 return s;
}
static EFI_STATUS CheckEspAssets(EFI_BOOT_SERVICES *bs,EFI_HANDLE device){
 EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;EFI_FILE_PROTOCOL *root=NULL;
 EFI_STATUS s=bs->HandleProtocol(device,&FsGuid,(VOID **)&fs);
 if(s!=EFI_SUCCESS || !fs)return s==EFI_SUCCESS?EFI_DEVICE_ERROR:s;
 s=fs->OpenVolume(fs,&root);if(s!=EFI_SUCCESS || !root)return s==EFI_SUCCESS?EFI_DEVICE_ERROR:s;
 for(UINTN i=0;i<sizeof(AssetSpecs)/sizeof(AssetSpecs[0]) && s==EFI_SUCCESS;i++)s=CheckOneAsset(bs,root,&AssetSpecs[i]);
 EFI_STATUS closed=root->Close(root);return s==EFI_SUCCESS?closed:s;
}
#ifndef GU_ASSET_ENTRY
#define GU_ASSET_ENTRY efi_main
#endif
EFI_STATUS EFIAPI GU_ASSET_ENTRY(EFI_HANDLE image,EFI_SYSTEM_TABLE *table){
 if(!table || !table->BootServices || !table->ConOut)return EFI_INVALID_PARAMETER;
 EFI_BOOT_SERVICES *bs=table->BootServices;EFI_LOADED_IMAGE_PROTOCOL *loaded=NULL;
 EFI_STATUS s=bs->HandleProtocol(image,&LoadedGuid,(VOID **)&loaded);
 if(s!=EFI_SUCCESS || !loaded || !loaded->DeviceHandle)return EFI_SECURITY_VIOLATION;
 s=FindOwnWindowsEsp(bs,loaded->DeviceHandle);if(s!=EFI_SUCCESS)return s;
 table->ConOut->OutputString(table->ConOut,L"ASSET PREFLIGHT ENTERED. Reading five ESP assets; no GPU access or writes.\r\n");
 s=CheckEspAssets(bs,loaded->DeviceHandle);
 if(s==EFI_SUCCESS)table->ConOut->OutputString(table->ConOut,L"FIVE ESP ASSETS VERIFIED. No unlock attempted. Windows in 10 seconds.\r\n");
 else table->ConOut->OutputString(table->ConOut,L"ESP ASSET CHECK FAILED. No unlock attempted. Windows in 10 seconds.\r\n");
 if(!bs->Stall)return EFI_UNSUPPORTED;
 EFI_STATUS waited=bs->Stall(10000000);if(waited!=EFI_SUCCESS)return waited;
 /* Read failure still permits the already-proven Windows handoff. */
 return shared_audit_entry(image,table);
}
