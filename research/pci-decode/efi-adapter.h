/* GPL-2.0-only. Research library only; no EFI entry point or shipped build. */
#pragma once
#include "decode-sync.h"
#include "efi-binding.h"
#define GU_DS_EFI_MAX_OPENERS 64U

/* One zero-initialized context. Treat initialized fields as private/immutable.
 * Caller retains the exclusive claim, input storage and interface lifetimes,
 * serializes the operation, and separately authorizes any future physical use.
 * Observing an open record does not acquire or lock a protocol. */
typedef struct {
  BOOLEAN InitUsed,Ready,RunUsed,Busy,WriteSpent,OwnershipFault;
  BOOLEAN OwnershipObserved,CleanupObserved,DisableInvoked;
  EFI_BOOT_SERVICES *Boot;
  EFI_HANDLE_PROTOCOL HandleProtocol;
  EFI_LOCATE_DEVICE_PATH LocateDevicePath;
  EFI_OPEN_PROTOCOL_INFORMATION OpenProtocolInformation;
  EFI_FREE_POOL FreePool;
  EFI_HANDLE OwnerImage;
  const GU_DS_EFI_BINDING *InputBindings;
  const GU_DS_SNAPSHOT *InputExpected;
  const VOID *CallerContext,*ActiveTransaction;
  EFI_STATUS OwnershipStatus,CleanupStatus,DisableIoStatus;
  GU_DS_EFI_DEVICE Devices[1+GU_DS_EFI_MAX_PARENTS];
  GU_DS_SNAPSHOT Expected;
  BOOLEAN (*DurableLog)(VOID *Context,const char *Event);
  VOID *LogContext;
} GU_DS_EFI_CONTEXT;

/* Bindings[0] is GPU; following bindings are its immediate root-to-leaf parents.
 * Init is pure validation/copying and calls no firmware or log callback. */
EFI_STATUS GuDsEfiInitialize(GU_DS_EFI_CONTEXT *Context,EFI_BOOT_SERVICES *Boot,EFI_HANDLE OwnerImage,
  const GU_DS_EFI_BINDING *Bindings,UINTN Count,const GU_DS_SNAPSHOT *Expected,
  BOOLEAN (*DurableLog)(VOID *,const char *),VOID *LogContext);
GU_DS_RESULT GuDsEfiRun(GU_DS_EFI_CONTEXT *Context,GU_DS_TRANSACTION *Transaction);
