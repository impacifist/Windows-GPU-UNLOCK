/* Windows GPU UNLOCK internal boot experiment, 2026-10-20. GPL-2.0-only. */
#pragma once
#include <Uefi.h>
#include <Protocol/DevicePath.h>

#if defined(GU_EVGA_PACKAGE)
/* Partition and receipt constants are force-included from the per-attempt build header. */
#elif defined(GU_SHARED_ESP_UNLOCK) || defined(GU_SHARED_ESP_UNLOCK_CONFIG)
#include "build/shared-esp-unlock/platform-config.h"
#elif defined(GU_SHARED_ESP_QUAL)
#include "build/shared-esp-qualification/platform-config.h"
#elif defined(GU_SHARED_ESP_INTEGRATION)
#include "build/shared-esp-integration/platform-config.h"
#elif defined(GU_SHARED_ESP_RETAIN)
#include "build/shared-esp-retain/platform-config.h"
#elif defined(GU_SHARED_ESP_ASSETS)
#include "build/shared-esp-assets/platform-config.h"
#elif defined(GU_SHARED_ESP_AUDIT)
#include "build/shared-esp-audit/platform-config.h"
#elif defined(GU_EVGA_STOP_ONLY)
#include "build/evga-text-stop/platform-config.h"
#else
#include "platform-config.h" /* Generated locally by Configure.ps1. */
#endif
static const EFI_GUID GuInternalPartition = GU_INTERNAL_PARTITION_INITIALIZER;
static const EFI_GUID GuWindowsPartition = GU_WINDOWS_PARTITION_INITIALIZER;

static inline BOOLEAN GuGuidEqual(const VOID *Left,const VOID *Right) {
  const UINT8 *a=Left,*b=Right;
  for(UINTN i=0;i<16;i++)if(a[i]!=b[i])return FALSE;
  return TRUE;
}

/* Firmware-provided paths only. Reject multi-instance, malformed and repeated HD nodes. */
static inline BOOLEAN GuInspectPath(const EFI_DEVICE_PATH_PROTOCOL *Path,const EFI_GUID *Guid,
                                    UINTN *Bytes,BOOLEAN *Usb,BOOLEAN *Partition) {
  UINTN total=0,hd=0;BOOLEAN usb=FALSE,match=FALSE;
  if(!Path)return FALSE;
  for(UINTN nodes=0;nodes<64 && total+4<=4096;nodes++) {
    UINTN n=Path->Length[0]+((UINTN)Path->Length[1]<<8);
    if(n<4 || n>4096-total)return FALSE;
    if(Path->Type==END_DEVICE_PATH_TYPE) {
      if(n!=4 || Path->SubType!=END_ENTIRE_DEVICE_PATH_SUBTYPE)return FALSE;
      if(Bytes)*Bytes=total+4;if(Usb)*Usb=usb;if(Partition)*Partition=match;
      return TRUE;
    }
    if(Path->Type==MESSAGING_DEVICE_PATH && Path->SubType==MSG_USB_DP && n==sizeof(USB_DEVICE_PATH))usb=TRUE;
    if(Path->Type==MEDIA_DEVICE_PATH && Path->SubType==MEDIA_HARDDRIVE_DP) {
      if(n!=sizeof(HARDDRIVE_DEVICE_PATH) || ++hd!=1)return FALSE;
      const HARDDRIVE_DEVICE_PATH *drive=(const VOID *)Path;
      if(Guid && drive->MBRType==2 && drive->SignatureType==2 && drive->PartitionSize &&
         GuGuidEqual(drive->Signature,Guid))match=TRUE;
    }
    total+=n;Path=(const VOID *)((const UINT8 *)Path+n);
  }
  return FALSE;
}

static inline BOOLEAN GuPathIsPartition(const EFI_DEVICE_PATH_PROTOCOL *Path,const EFI_GUID *Guid) {
  BOOLEAN match=FALSE;
  return GuInspectPath(Path,Guid,NULL,NULL,&match) && match;
}

static inline BOOLEAN GuAllowedMediaPath(const EFI_DEVICE_PATH_PROTOCOL *Path,BOOLEAN InternalAllowed) {
  BOOLEAN usb=FALSE,partition=FALSE;
  return GuInspectPath(Path,&GuInternalPartition,NULL,&usb,&partition) && (usb || (InternalAllowed && partition));
}
