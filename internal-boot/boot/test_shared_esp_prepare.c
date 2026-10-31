/* Host-only direct Prepare admission scenarios; no firmware or GPU access. */
#define GU_DIAGNOSTIC_ONLY
#define GU_EVGA_STOP_ONLY
#define GU_EVGA_SHARED_ESP_ADMISSION
#include "handoff.c"
typedef struct {HARDDRIVE_DEVICE_PATH hd;EFI_DEVICE_PATH_PROTOCOL end;} TEST_PATH;
typedef struct {USB_DEVICE_PATH usb;HARDDRIVE_DEVICE_PATH hd;EFI_DEVICE_PATH_PROTOCOL end;} TEST_USB_PATH;
static TEST_PATH correct,wrong;static TEST_USB_PATH usbPath;static EFI_HANDLE list[2];
static UINTN count=1;static BOOLEAN duplicate,usb;static int fsCalls;
static EFI_STATUS EFIAPI locate(EFI_LOCATE_SEARCH_TYPE t,EFI_GUID *g,VOID *k,UINTN *n,EFI_HANDLE **p){(void)t;(void)g;(void)k;*n=count;*p=list;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI freepool(VOID *p){(void)p;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI protocol(EFI_HANDLE h,EFI_GUID *g,VOID **p){
 if(GuGuidEqual(g,&PathGuid)){*p=(h==list[0] || duplicate)?(usb?(VOID *)&usbPath:(VOID *)&correct):(VOID *)&wrong;return EFI_SUCCESS;}
 if(GuGuidEqual(g,&FsGuid)){fsCalls++;return EFI_NOT_FOUND;}return EFI_NOT_FOUND;
}
static EFI_STATUS run(EFI_HANDLE own){OwnDevice=own;fsCalls=0;return Prepare(NULL);}
int main(void){
 EFI_BOOT_SERVICES bs={0};EFI_SYSTEM_TABLE st={0};bs.LocateHandleBuffer=locate;bs.HandleProtocol=protocol;bs.FreePool=freepool;st.BootServices=&bs;Table=&st;
 correct.hd.Header.Type=MEDIA_DEVICE_PATH;correct.hd.Header.SubType=MEDIA_HARDDRIVE_DP;correct.hd.Header.Length[0]=sizeof(HARDDRIVE_DEVICE_PATH);correct.hd.PartitionSize=1;correct.hd.MBRType=2;correct.hd.SignatureType=2;
 for(UINTN i=0;i<16;i++)correct.hd.Signature[i]=((UINT8 *)&GuWindowsPartition)[i];correct.end.Type=END_DEVICE_PATH_TYPE;correct.end.SubType=END_ENTIRE_DEVICE_PATH_SUBTYPE;correct.end.Length[0]=4;
 wrong=correct;wrong.hd.Signature[0]^=1;usbPath.usb.Header.Type=MESSAGING_DEVICE_PATH;usbPath.usb.Header.SubType=MSG_USB_DP;usbPath.usb.Header.Length[0]=sizeof(USB_DEVICE_PATH);usbPath.hd=correct.hd;usbPath.end=correct.end;
 list[0]=(EFI_HANDLE)1;list[1]=(EFI_HANDLE)2;
 if(run(list[0])!=EFI_UNSUPPORTED || fsCalls!=1)return 1;
 if(run(list[1])!=EFI_COMPROMISED_DATA || fsCalls)return 2;
 duplicate=TRUE;count=2;if(run(list[0])!=EFI_COMPROMISED_DATA || fsCalls)return 3;
 duplicate=FALSE;usb=TRUE;count=1;if(run(list[0])!=EFI_COMPROMISED_DATA || fsCalls)return 4;
 usb=FALSE;count=0;if(run(list[0])!=EFI_COMPROMISED_DATA || fsCalls)return 5;
 return 0;
}
EFI_STATUS GuCaptureGraphOnVolume(EFI_SYSTEM_TABLE *t,EFI_FILE_PROTOCOL *f,EFI_FILE_PROTOCOL *r){(void)t;(void)f;(void)r;return EFI_UNSUPPORTED;}
EFI_STATUS GuEvgaTextStop(EFI_SYSTEM_TABLE *t,EFI_FILE_PROTOCOL *f,GU_EVGA_RECORD r,BOOLEAN *b){(void)t;(void)f;(void)r;(void)b;return EFI_UNSUPPORTED;}
EFI_STATUS GuEvgaTextStopVerify(EFI_SYSTEM_TABLE *t){(void)t;return EFI_UNSUPPORTED;}
