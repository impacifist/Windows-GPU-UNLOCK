/* GPL-2.0-only. Private to the fixed Stage 1 pilot, never a release permit. */
#pragma once
#if !defined(GU_RELEASE_PILOT) || GU_PILOT_STAGE != 1
#error Splitter policy requires the isolated Stage 1 pilot
#endif
#define STAGE1_POLICY_ID "stage1-splitter-policy-v1"
#include "stage1-build.h"
static struct {
  BOOLEAN Started,Ready,Confirmed,Attempted,Ended;
  EFI_HANDLE Image,Nvidia;
  PLAN Plan;
  UINT32 Config[64];
  UINT64 Attributes;
  UINT8 GraphHash[32];
  VOID *Text;
} Splitter;
#include "stage1-diagnostics.h"

static BOOLEAN Stage1Pair(UINT32 command,UINT64 attributes) {
  return (command&7U)==2U && (attributes==0x200ULL || attributes==0x300ULL);
}
static EFI_STATUS Stage1ReadSafe(const char *checkpoint) {
  VerifyCheckpoint=checkpoint;
  UINT32 cfg[64];EFI_STATUS s=Stage1ReadConfig(cfg);
  if(s==EFI_SUCCESS && ((cfg[1]&7U)!=2U || !Bytes(cfg,Full.Config,sizeof(cfg))))s=EFI_NOT_READY;
  return s;
}
static EFI_STATUS Stage1Prom(VOID) {
  EFI_BOOT_SERVICES *bs=Full.Table->BootServices;EFI_PCI_IO_PROTOCOL *io=PreparedPlan.RtxIo;
  VerifyCheckpoint="prom-allocate";
  UINT8 *data=NULL;EFI_STATUS s=bs->AllocatePool(EfiBootServicesData,1048576,(VOID **)&data);
  if(s!=EFI_SUCCESS || !data)return EFI_OUT_OF_RESOURCES;
  /* Allocation is outside the read batch. No service intervenes between this
   * final raw check and the first dependent request. Vendor calls are not atomic. */
  s=Stage1ReadSafe("prom-raw-safety");
  if(s==EFI_SUCCESS) {
    VerifyCheckpoint="prom-read";
    if(!io->Mem.Read)s=EFI_UNSUPPORTED;
    for(UINTN at=0;s==EFI_SUCCESS && at<1048576;at+=65536)s=io->Mem.Read(io,EfiPciIoWidthUint8,0,0x300000+at,65536,data+at);
    if(s==EFI_SUCCESS){VerifyCheckpoint="prom-hash";if(!GuHashMatches(data,1048576,GU_PROM_SHA256))s=EFI_CRC_ERROR;}
  }
  /* Preserve the primary failure even if cleanup fails or dispatches callbacks. */
  Stage1Remember(s);
  EFI_STATUS freed=bs->FreePool(data);
  if(s==EFI_SUCCESS){VerifyCheckpoint="prom-free";s=freed;}
  return s;
}
static EFI_STATUS Stage1Capture(VOID) {
  VerifyCheckpoint="capture-pci-config256";
  GU_CAPS caps;EFI_STATUS s=Stage1ReadConfig(Full.Config);
  if(s==EFI_SUCCESS && (!GuCheckConfig(Full.Config,0,GU_TARGET_BUS,0,0,&caps) || (Full.Config[1]&7U)!=2U))s=EFI_UNSUPPORTED;
  s=Note("capture-pci-config256",s);if(s!=EFI_SUCCESS)return s;
  for(UINTN i=0;i<64;i++)Stage1ConfigBaseline[i]=Full.Config[i];Stage1ConfigBaselineValid=TRUE;
  VerifyCheckpoint="capture-pci-log";
  s=LogConfig("pre-nv-pci",Full.Config);if(s!=EFI_SUCCESS)return s;
  VerifyCheckpoint="capture-bar0";
  s=Note("capture-bar0",CheckBar(TRUE));if(s!=EFI_SUCCESS)return s;
  /* Logging and firmware services may dispatch callbacks. Recheck raw safety
   * before each dependent batch. There is no atomicity inside vendor calls. */
  s=Stage1Prom();Stage1Remember(s);
  if(s==EFI_SUCCESS)VerifyCheckpoint="capture-prom-log";
  s=Note("capture-prom",s);if(s!=EFI_SUCCESS)return s;
  s=Stage1ReadSafe("capture-cold-raw-safety");if(s!=EFI_SUCCESS)return s;
  VerifyCheckpoint="capture-cold";
  s=Note("capture-cold",Cold(Full.Cold));if(s!=EFI_SUCCESS)return s;
  VerifyCheckpoint="capture-attributes-read";
  s=Stage1ReadAttributes(&Full.Attributes);
  s=Note("capture-attributes-read",s);if(s!=EFI_SUCCESS)return s;
  Stage1AttributeBaseline=Full.Attributes;Stage1AttributeBaselineValid=TRUE;
  VerifyCheckpoint="capture-attributes-log";
  s=Full.Log(Full.Context,"capture-attributes-value",Full.Attributes);if(s!=EFI_SUCCESS)return s;
  s=Full.Log(Full.Context,"capture-attributes-expected",0x200);if(s!=EFI_SUCCESS)return s;
  EFI_STATUS strict=Full.Attributes==0x200?EFI_SUCCESS:EFI_NOT_READY;
  /* A failed strict classification is data, not a transport result or permit. */
  s=Full.Log(Full.Context,"capture-attributes-match",strict);if(s!=EFI_SUCCESS)return s;
  s=Note(STAGE1_POLICY_ID,EFI_SUCCESS);if(s!=EFI_SUCCESS)return s;
  s=Note("stage1-source-sha256=" GU_STAGE1_SOURCE_ID,EFI_SUCCESS);if(s!=EFI_SUCCESS)return s;
  s=Full.Log(Full.Context,"stage1-strict-release-attribute-check",strict);if(s!=EFI_SUCCESS)return s;
  s=Full.Log(Full.Context,"stage1-no-release-permit",EFI_ACCESS_DENIED);if(s!=EFI_SUCCESS)return s;
  VerifyCheckpoint="stage1-eligibility";
  s=Note("stage1-eligibility",Stage1Pair(Full.Config[1],Full.Attributes)?EFI_SUCCESS:EFI_NOT_READY);
  if(s!=EFI_SUCCESS)return s;
  VerifyCheckpoint="capture-text-interface";
  s=Interface(Full.Table->BootServices,PreparedPlan.Rtx,&TextGuid,&Splitter.Text);
  if(s!=EFI_SUCCESS || !Splitter.Text)return EFI_NOT_READY;
  Splitter.Plan=PreparedPlan;Splitter.Image=Full.Image;Splitter.Nvidia=Full.NvAgent;
  for(UINTN i=0;i<64;i++)Splitter.Config[i]=Full.Config[i];
  Splitter.Attributes=Full.Attributes;
  GuSha256(Initial.Edges,Initial.Count*sizeof(EDGE),Splitter.GraphHash);
  Splitter.Ready=TRUE;return EFI_SUCCESS;
}
static EFI_STATUS Stage1Bound(UINTN stage) {
  if(!Splitter.Ready || Splitter.Ended || stage>1 ||
     (stage==0 && Splitter.Attempted) || (stage==1 && !Splitter.Attempted) ||
     Full.Captured || Full.Released || Full.Owned || Full.Restored || PreparedReady ||
     Full.Image!=Splitter.Image || Full.NvAgent!=Splitter.Nvidia ||
     !Bytes(&PreparedPlan,&Splitter.Plan,sizeof(PLAN)) ||
     !Bytes(Full.Config,Splitter.Config,sizeof(Splitter.Config)) ||
     Full.Attributes!=Splitter.Attributes || !Stage1Pair(Full.Config[1],Full.Attributes) ||
     Initial.Count>MAX_RECORDS)return EFI_ACCESS_DENIED;
  UINT8 hash[32];GuSha256(Initial.Edges,Initial.Count*sizeof(EDGE),hash);
  return Bytes(hash,Splitter.GraphHash,sizeof(hash))?EFI_SUCCESS:EFI_ACCESS_DENIED;
}
