/* GPL-2.0-only. Bounded first-failure evidence; no services while collecting. */
#pragma once
typedef struct {
  const char *Phase,*Checkpoint;
  UINTN Stage;
  EFI_STATUS Status,ConfigStatus,AttributeStatus;
  BOOLEAN Failed,Attempted,ConfigAttempted,ConfigValid,AttributesAttempted,AttributesValid;
  UINT32 Config[64];
  UINT64 Attributes;
} STAGE1_DIAGNOSTIC;
static STAGE1_DIAGNOSTIC Stage1Working,Stage1Failure;
static BOOLEAN Stage1Reported,Stage1ConfigBaselineValid,Stage1AttributeBaselineValid;
static UINT32 Stage1ConfigBaseline[64];
static UINT64 Stage1AttributeBaseline;

static VOID Stage1Begin(const char *phase,UINTN stage) {
  Stage1Working=(STAGE1_DIAGNOSTIC){.Phase=phase,.Stage=stage,
    .ConfigStatus=EFI_NOT_STARTED,.AttributeStatus=EFI_NOT_STARTED};
}
static VOID Stage1Remember(EFI_STATUS status) {
  if(status==EFI_SUCCESS || Stage1Failure.Failed)return;
  Stage1Working.Failed=TRUE;Stage1Working.Status=status;
  Stage1Working.Checkpoint=VerifyCheckpoint;
  Stage1Working.Attempted=Splitter.Attempted;
  Stage1Failure=Stage1Working;
}
/* Same location, transport and identity gates as ReadConfigFull. Preserve data
 * only after Pci.Read succeeds, even if the subsequent identity gate rejects. */
static EFI_STATUS Stage1ReadConfig(UINT32 *cfg) {
  Stage1Working.ConfigAttempted=Stage1Working.ConfigValid=FALSE;
  Stage1Working.ConfigStatus=EFI_NOT_STARTED;
  EFI_PCI_IO_PROTOCOL *io=NULL;
  EFI_STATUS s=Interface(Full.Table->BootServices,PreparedPlan.RtxPci,&PciGuid,(VOID **)&io);
  if(s!=EFI_SUCCESS || io!=PreparedPlan.RtxIo || !io->GetLocation || !io->Pci.Read)return EFI_NOT_READY;
  UINTN seg=0,bus=0,dev=0,fun=0;
  s=io->GetLocation(io,&seg,&bus,&dev,&fun);
  if(s!=EFI_SUCCESS || seg || bus!=GU_TARGET_BUS || dev || fun)return EFI_UNSUPPORTED;
  Stage1Working.ConfigAttempted=TRUE;
  s=io->Pci.Read(io,EfiPciIoWidthUint32,0,64,cfg);
  Stage1Working.ConfigStatus=s;
  if(s==EFI_SUCCESS) {
    Stage1Working.ConfigValid=TRUE;
    for(UINTN i=0;i<64;i++)Stage1Working.Config[i]=cfg[i];
    if(cfg[0]!=GU_BOARD_ID || cfg[11]!=GU_BOARD_SUBSYSTEM || cfg[2]!=0x030000a1 || cfg[4]!=(UINT32)GU_BAR0_BASE || (cfg[1]&4))s=EFI_UNSUPPORTED;
  }
  return s;
}
static EFI_STATUS Stage1ReadAttributes(UINT64 *attrs) {
  Stage1Working.AttributesAttempted=TRUE;Stage1Working.AttributesValid=FALSE;
  EFI_STATUS s=PreparedPlan.RtxIo->Attributes(PreparedPlan.RtxIo,EfiPciIoAttributeOperationGet,0,attrs);
  Stage1Working.AttributeStatus=s;
  if(s==EFI_SUCCESS){Stage1Working.AttributesValid=TRUE;Stage1Working.Attributes=*attrs;}
  return s;
}
static EFI_STATUS Stage1Field(GU_PREPARE_LOG log,VOID *ctx,const char *prefix,const char *value) {
  char label[128];UINTN n=0;
  for(UINTN i=0;prefix[i];i++){if(n==sizeof(label)-1)return EFI_BAD_BUFFER_SIZE;label[n++]=prefix[i];}
  for(UINTN i=0;value && value[i];i++){if(n==sizeof(label)-1)return EFI_BAD_BUFFER_SIZE;label[n++]=value[i];}
  label[n]=0;return log(ctx,label,Stage1Failure.Status);
}
/* Entry calls this once, at application TPL, before closing the primary log.
 * Never re-read hardware, report failed buffers, or replace the original error. */
VOID PilotReportFailure(GU_PREPARE_LOG log,VOID *ctx,EFI_STATUS status) {
  if(status==EFI_SUCCESS || Stage1Reported || !log)return;
  Stage1Reported=TRUE;
  if(!Stage1Failure.Failed){Stage1Begin("entry",Completed);VerifyCheckpoint="entry-flow";Stage1Remember(status);}
  const STAGE1_DIAGNOSTIC *d=&Stage1Failure;
#define FIELD(name,value) do {if(log(ctx,"stage1-failure-" name,(value))!=EFI_SUCCESS)return;} while(0)
  FIELD("status",d->Status);
  if(Stage1Field(log,ctx,"stage1-failure-phase=",d->Phase)!=EFI_SUCCESS)return;
  if(Stage1Field(log,ctx,"stage1-failure-checkpoint=",d->Checkpoint)!=EFI_SUCCESS)return;
  FIELD("stage",d->Stage);FIELD("attempted",d->Attempted);
  FIELD("config-attempted",d->ConfigAttempted);FIELD("config-read-status",d->ConfigStatus);
  FIELD("config-valid",d->ConfigValid);FIELD("config-baseline-valid",Stage1ConfigBaselineValid);
  if(d->ConfigValid) {
    for(UINTN i=0;i<64;i++) {
      char label[]="stage1-failure-pci-00";
      label[19]="0123456789abcdef"[i>>4];label[20]="0123456789abcdef"[i&15];
      if(log(ctx,label,d->Config[i])!=EFI_SUCCESS)return;
    }
    if(Stage1ConfigBaselineValid) {
      UINTN i=0;while(i<64 && d->Config[i]==Stage1ConfigBaseline[i])i++;
      FIELD("config-different",i<64);
      if(i<64){FIELD("config-offset",i*4);FIELD("config-expected",Stage1ConfigBaseline[i]);FIELD("config-observed",d->Config[i]);}
    }
  }
  FIELD("attributes-attempted",d->AttributesAttempted);FIELD("attributes-read-status",d->AttributeStatus);
  FIELD("attributes-valid",d->AttributesValid);FIELD("attributes-baseline-valid",Stage1AttributeBaselineValid);
  if(Stage1AttributeBaselineValid)FIELD("attributes-expected",Stage1AttributeBaseline);
  if(d->AttributesValid)FIELD("attributes-observed",d->Attributes);
  FIELD("record-complete",EFI_SUCCESS);
#undef FIELD
}
