/* GPL-2.0-only. Read-only current-boot collector; no entry point or correction. */
#pragma once
#include "efi-binding.h"

typedef struct {
  EFI_HANDLE Handle;
  UINTN Segment,Bus,PathBytes;
  UINT32 PciId,Subsystem,Bar0;
  UINT8 Path[GU_DS_EFI_MAX_PATH];
} GU_DS_CAPTURE_TARGET;

/* One private zero-initialized context. Do not reset/recreate it to retry.
 * Input/context/Boot storage must be disjoint and live throughout the call.
 * Alias/reuse rejection leaves existing storage unchanged. Require this call's
 * EFI_SUCCESS; Ready alone may describe an earlier capture. Ready is observational
 * only: no claim, GPU qualification or write approval. */
typedef struct {
  BOOLEAN Used,Busy,Ready;
  EFI_STATUS Status;
  UINTN Count;
  GU_DS_EFI_BINDING Bindings[1+GU_DS_EFI_MAX_PARENTS];
  GU_DS_SNAPSHOT Snapshot;
} GU_DS_CAPTURE;

EFI_STATUS GuDsEfiCapture(GU_DS_CAPTURE *Context,EFI_BOOT_SERVICES *Boot,const GU_DS_CAPTURE_TARGET *Target);
