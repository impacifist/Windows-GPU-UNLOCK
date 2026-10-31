/* Host-only synthetic UEFI services; no firmware or GPU access. */
#include "shared-esp-assets-launcher.c"
typedef struct {HARDDRIVE_DEVICE_PATH hd;EFI_DEVICE_PATH_PROTOCOL end;} DISK_PATH;
typedef struct {USB_DEVICE_PATH usb;HARDDRIVE_DEVICE_PATH hd;EFI_DEVICE_PATH_PROTOCOL end;} USB_PATH;
static DISK_PATH good,wrong;static USB_PATH usb;
static EFI_HANDLE handles[2];static UINTN handleCount;static BOOLEAN usbMode,duplicate;
static EFI_LOADED_IMAGE_PROTOCOL ownImage,windowsImage;static UINT8 pathBuffer[1024];static UINTN starts,loads,unloads;static BOOLEAN wrongChild,loadFailure;
static EFI_STATUS EFIAPI Locate(EFI_LOCATE_SEARCH_TYPE type,EFI_GUID *guid,VOID *key,UINTN *count,EFI_HANDLE **out){(void)type;(void)guid;(void)key;*count=handleCount;*out=handles;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Free(VOID *p){(void)p;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Protocol(EFI_HANDLE h,EFI_GUID *g,VOID **out){
 if(GuGuidEqual(g,&LoadedGuid)){if(h==(EFI_HANDLE)3)*out=&ownImage;else if(h==(EFI_HANDLE)4)*out=&windowsImage;else return EFI_NOT_FOUND;return EFI_SUCCESS;}
 if(h==handles[0])*out=usbMode?(VOID *)&usb:(VOID *)&good;else *out=duplicate?(VOID *)&good:(VOID *)&wrong;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Allocate(EFI_MEMORY_TYPE type,UINTN bytes,VOID **out){(void)type;if(bytes>sizeof(pathBuffer))return EFI_OUT_OF_RESOURCES;*out=pathBuffer;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Output(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out,CHAR16 *text){(void)out;(void)text;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Load(BOOLEAN policy,EFI_HANDLE parent,EFI_DEVICE_PATH_PROTOCOL *path,VOID *buffer,UINTN size,EFI_HANDLE *image){
 (void)parent;(void)buffer;(void)size;loads++;if(policy || !path || loadFailure)return EFI_LOAD_ERROR;
 UINTN base=sizeof(HARDDRIVE_DEVICE_PATH);EFI_DEVICE_PATH_PROTOCOL *node=(VOID *)((UINT8 *)path+base);
 const CHAR16 expected[]=L"\\EFI\\EVGA-SHARED-ASSETS.EFI";
 if(node->Type!=MEDIA_DEVICE_PATH || node->SubType!=MEDIA_FILEPATH_DP)return EFI_LOAD_ERROR;
 for(UINTN i=0;i<sizeof(expected)/sizeof(CHAR16);i++)if(((CHAR16 *)((UINT8 *)node+4))[i]!=expected[i])return EFI_LOAD_ERROR;
 windowsImage.DeviceHandle=wrongChild?handles[1]:handles[0];*image=(EFI_HANDLE)4;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Start(EFI_HANDLE image,UINTN *size,CHAR16 **data){(void)size;(void)data;if(image!=(EFI_HANDLE)4)return EFI_INVALID_PARAMETER;starts++;return EFI_ABORTED;}
static EFI_STATUS EFIAPI Stall(UINTN us){return us==10000000?EFI_SUCCESS:EFI_INVALID_PARAMETER;}
static EFI_STATUS EFIAPI Unload(EFI_HANDLE image){(void)image;unloads++;return EFI_SUCCESS;}
int main(void){
 EFI_BOOT_SERVICES bs={0};bs.LocateHandleBuffer=Locate;bs.HandleProtocol=Protocol;bs.FreePool=Free;
 good.hd.Header.Type=MEDIA_DEVICE_PATH;good.hd.Header.SubType=MEDIA_HARDDRIVE_DP;good.hd.Header.Length[0]=sizeof(HARDDRIVE_DEVICE_PATH);
 good.hd.PartitionSize=1;good.hd.MBRType=2;good.hd.SignatureType=2;
 for(UINTN i=0;i<16;i++)good.hd.Signature[i]=((UINT8 *)&GuWindowsPartition)[i];
 good.end.Type=END_DEVICE_PATH_TYPE;good.end.SubType=END_ENTIRE_DEVICE_PATH_SUBTYPE;good.end.Length[0]=4;
 wrong=good;wrong.hd.Signature[0]^=1;handles[0]=(EFI_HANDLE)1;handles[1]=(EFI_HANDLE)2;handleCount=1;
 if(FindOwnWindowsEsp(&bs,handles[0])!=EFI_SUCCESS)return 1;
 if(FindOwnWindowsEsp(&bs,handles[1])==EFI_SUCCESS)return 2;
 handleCount=2;duplicate=TRUE;if(FindOwnWindowsEsp(&bs,handles[0])==EFI_SUCCESS)return 3;
 duplicate=FALSE;if(FindOwnWindowsEsp(&bs,handles[0])!=EFI_SUCCESS)return 4;
 usb.usb.Header.Type=MESSAGING_DEVICE_PATH;usb.usb.Header.SubType=MSG_USB_DP;usb.usb.Header.Length[0]=sizeof(USB_DEVICE_PATH);usb.hd=good.hd;usb.end=good.end;usbMode=TRUE;
 if(FindOwnWindowsEsp(&bs,handles[0])==EFI_SUCCESS)return 5;
 usbMode=FALSE;handleCount=0;if(FindOwnWindowsEsp(&bs,handles[0])==EFI_SUCCESS)return 6;
 EFI_SYSTEM_TABLE table={0};EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL con={0};table.BootServices=&bs;table.ConOut=&con;con.OutputString=Output;
 bs.Stall=Stall;bs.AllocatePool=Allocate;bs.LoadImage=Load;bs.StartImage=Start;bs.UnloadImage=Unload;ownImage.DeviceHandle=handles[0];handleCount=1;
 if(efi_main((EFI_HANDLE)3,&table)!=EFI_ABORTED || starts!=1 || loads!=1)return 7;
 wrongChild=TRUE;if(efi_main((EFI_HANDLE)3,&table)==EFI_SUCCESS || starts!=1 || unloads!=1)return 8;
 wrongChild=FALSE;loadFailure=TRUE;if(efi_main((EFI_HANDLE)3,&table)==EFI_SUCCESS || starts!=1)return 9;
 loadFailure=FALSE;duplicate=TRUE;handleCount=2;if(efi_main((EFI_HANDLE)3,&table)==EFI_SUCCESS || loads!=3 || starts!=1)return 10;
 return 0;
}
