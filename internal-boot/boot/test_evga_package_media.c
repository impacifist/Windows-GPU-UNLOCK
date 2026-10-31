/* Synthetic device paths only. No firmware/hardware execution. */
#include "media.h"
#include "evga-package-media.h"
#pragma pack(push,1)
typedef struct {HARDDRIVE_DEVICE_PATH hd;EFI_DEVICE_PATH_PROTOCOL end;} DISK;
typedef struct {USB_DEVICE_PATH usb;DISK disk;} USB_DISK;
#pragma pack(pop)
static void Init(DISK *d,const EFI_GUID *g){
 UINT8 *b=(VOID *)d;for(UINTN i=0;i<sizeof(*d);i++)b[i]=0;
 d->hd.Header.Type=4;d->hd.Header.SubType=1;d->hd.Header.Length[0]=sizeof(d->hd);
 d->hd.PartitionSize=1;d->hd.MBRType=2;d->hd.SignatureType=2;
 for(UINTN i=0;i<16;i++)d->hd.Signature[i]=((const UINT8 *)g)[i];
 d->end.Type=127;d->end.SubType=255;d->end.Length[0]=4;
}
int main(void){
 DISK win,own,other;EFI_GUID unknown={7,7,7,{7,7,7,7,7,7,7,7}};
 Init(&win,&GuWindowsPartition);Init(&own,&GuInternalPartition);Init(&other,&unknown);
 USB_DISK usb;Init(&usb.disk,&unknown);usb.usb.Header.Type=3;usb.usb.Header.SubType=5;usb.usb.Header.Length[0]=sizeof(USB_DEVICE_PATH);usb.usb.Header.Length[1]=0;
 if(GuPackageMediaAllowed(NULL))return 1;
 if(GuPackageMediaAllowed(&other.hd.Header))return 2;
 if(GuPackageMediaAllowed(&win.hd.Header)!=(GU_PACKAGE_LAYOUT==1))return 3;
 if(GuPackageMediaAllowed(&usb.usb.Header)!=(GU_PACKAGE_LAYOUT==2))return 4;
#if GU_PACKAGE_LAYOUT == 3
 if(!GuPackageMediaAllowed(&own.hd.Header))return 5;
#endif
 Init(&usb.disk,&GuWindowsPartition);if(GuPackageMediaAllowed(&usb.usb.Header))return 6;
 win.hd.Header.Length[0]=3;if(GuPackageMediaAllowed(&win.hd.Header))return 7;
 return 0;
}
