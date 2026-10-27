/* GPL-2.0-only. Local observation policy, never a write-enabled profile. */
#pragma once
#include <Uefi.h>
#include <Protocol/DevicePath.h>
#define LOCAL_GPU_ID 0x220810deU
#define LOCAL_SUBSYSTEM 0x88031043U
#define LOCAL_PROM_BYTES 0x100000U
#define LOCAL_BAR_MIN_BYTES 0x8241e4U
static inline BOOLEAN LocalUsbPath(const EFI_DEVICE_PATH_PROTOCOL *p) {
  BOOLEAN usb=FALSE;UINTN used=0;
  if(!p)return FALSE;
  for(UINTN i=0;i<64 && used+4<=4096;i++) {
    UINTN n=p->Length[0]+((UINTN)p->Length[1]<<8);
    if(n<4 || n>4096-used)return FALSE;
    if(p->Type==END_DEVICE_PATH_TYPE)
      return n==4 && p->SubType==END_ENTIRE_DEVICE_PATH_SUBTYPE && usb;
    if(p->Type==MESSAGING_DEVICE_PATH &&
       ((p->SubType==MSG_USB_DP && n==sizeof(USB_DEVICE_PATH)) ||
        (p->SubType==MSG_USB_CLASS_DP && n==sizeof(USB_CLASS_DEVICE_PATH)) ||
        (p->SubType==MSG_USB_WWID_DP && n>=sizeof(USB_WWID_DEVICE_PATH))))usb=TRUE;
    used+=n;p=(const VOID *)((const UINT8 *)p+n);
  }
  return FALSE;
}
