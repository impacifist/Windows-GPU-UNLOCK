/* Windows GPU UNLOCK internal boot experiment, 2026-10-20. GPL-2.0-only. */
#pragma once
#include <Uefi.h>
#include <Protocol/DevicePath.h>

#include "platform-config.h" /* Generated locally by Configure.ps1. */
#if defined(GU_DIAGNOSTIC_MBR) && !defined(GU_WINDOWS_MBR_SIGNATURE)
#error Missing locally pinned MBR diagnostic configuration
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

#ifdef GU_WINDOWS_MBR_SIGNATURE
#if !defined(GU_DIAGNOSTIC_ONLY) || !defined(GU_USB_PROBE) || defined(GU_FULL_UNLOCK) || defined(GU_CONSOLE_PREPARE) || defined(GU_HEADLESS_RESEARCH) || defined(GU_AUTOMATIC_BOOT) || defined(GU_UEFI_PORT) || defined(GU_INTERNAL_BOOT) || defined(GU_EXPERIMENTAL_3090)
#error MBR Windows volume selection is diagnostic-only
#endif
/* An MBR signature is identification metadata, not boot-image authentication.
 * The diagnostic separately hashes the exact boot-manager bytes it loads. */
static inline BOOLEAN GuWindowsVolumePath(const EFI_DEVICE_PATH_PROTOCOL *Path) {
  BOOLEAN usb=FALSE,match=FALSE;
  if(!GuInspectPath(Path,NULL,NULL,&usb,NULL) || usb)return FALSE;
  for(UINTN nodes=0;nodes<64;nodes++) {
    UINTN n=Path->Length[0]+((UINTN)Path->Length[1]<<8);
    if(Path->Type==END_DEVICE_PATH_TYPE)return match;
    if(Path->Type==MESSAGING_DEVICE_PATH &&
       (Path->SubType==MSG_USB_DP || Path->SubType==MSG_USB_CLASS_DP || Path->SubType==MSG_USB_WWID_DP))return FALSE;
    if(Path->Type==MEDIA_DEVICE_PATH && Path->SubType==MEDIA_HARDDRIVE_DP) {
      const HARDDRIVE_DEVICE_PATH *d=(const VOID *)Path;
      UINT32 signature=(UINT32)d->Signature[0]|((UINT32)d->Signature[1]<<8)|
        ((UINT32)d->Signature[2]<<16)|((UINT32)d->Signature[3]<<24);
      if(d->MBRType!=1 || d->SignatureType!=1 || !signature ||
         signature!=GU_WINDOWS_MBR_SIGNATURE || !d->PartitionNumber ||
         d->PartitionNumber!=GU_WINDOWS_MBR_PARTITION || !d->PartitionSize ||
         d->PartitionStart>MAX_UINT64-d->PartitionSize ||
         d->PartitionStart!=GU_WINDOWS_MBR_START || d->PartitionSize!=GU_WINDOWS_MBR_SIZE)return FALSE;
      for(UINTN j=4;j<16;j++)if(d->Signature[j])return FALSE;
      match=TRUE;
    }
    Path=(const VOID *)((const UINT8 *)Path+n);
  }
  return FALSE;
}
#else
static inline BOOLEAN GuWindowsVolumePath(const EFI_DEVICE_PATH_PROTOCOL *Path) {
  return GuPathIsPartition(Path,&GuWindowsPartition);
}
#endif

static inline BOOLEAN GuAllowedMediaPath(const EFI_DEVICE_PATH_PROTOCOL *Path,BOOLEAN InternalAllowed) {
  BOOLEAN usb=FALSE,partition=FALSE;
  return GuInspectPath(Path,&GuInternalPartition,NULL,&usb,&partition) && (usb || (InternalAllowed && partition));
}
