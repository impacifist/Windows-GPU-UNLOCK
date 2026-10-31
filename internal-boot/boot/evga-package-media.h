/* GPL-2.0-only. Explicit boot-volume policy; never broadens machine/GPU pins. */
#pragma once
#if GU_PACKAGE_LAYOUT < 1 || GU_PACKAGE_LAYOUT > 3
#error Invalid attended package layout
#endif
#if GU_PACKAGE_LAYOUT == 1 && (!defined(GU_EVGA_SHARED_ESP_ADMISSION) || defined(GU_USB_MEDIA))
#error Shared package must use only shared admission
#endif
#if GU_PACKAGE_LAYOUT == 2 && (!defined(GU_USB_MEDIA) || defined(GU_EVGA_SHARED_ESP_ADMISSION))
#error USB package must use only USB admission
#endif
#if GU_PACKAGE_LAYOUT == 3 && (defined(GU_USB_MEDIA) || defined(GU_EVGA_SHARED_ESP_ADMISSION))
#error Separate package must not admit USB or Windows ESP
#endif
static BOOLEAN GuPackageMediaAllowed(const EFI_DEVICE_PATH_PROTOCOL *path){
 BOOLEAN usb=FALSE,windows=FALSE;
 if(!GuInspectPath(path,&GuWindowsPartition,NULL,&usb,&windows))return FALSE;
#if GU_PACKAGE_LAYOUT == 1
 return !usb && windows;
#elif GU_PACKAGE_LAYOUT == 2
 return usb && !windows;
#else
 BOOLEAN own=FALSE;
 if(!GuInspectPath(path,&GuInternalPartition,NULL,&usb,&own))return FALSE;
 return !usb && own && !windows;
#endif
}
