/* GPL-2.0-only. Shares reviewed guards; only PilotAdvance reaches effects. */
#include "core.h"
#ifndef GU_PILOT_CORE_INCLUDED
#include "read-state.c"
#endif
static UINTN Completed;
static BOOLEAN Faulted;
static const char *VerifyCheckpoint;
static GRAPH Initial,Filtered,Current;
static EFI_STATUS Observe(VOID);
#include "stage1-policy.h"
#define CHECKPOINT(label) (VerifyCheckpoint=(label))
BOOLEAN PilotInputStable(VOID) {
  EFI_SYSTEM_TABLE *t=Full.Table;PLAN *p=&PreparedPlan;
  return t && t->ConIn && t->ConIn==p->Keys && t->ConsoleInHandle==p->Input &&
    t->ConsoleOutHandle==p->Console && t->StandardErrorHandle==p->Error &&
    t->ConOut==p->ConsoleText && t->StdErr==p->ErrorText &&
    (UINTN)t->ConIn->Reset==p->KeyReset && (UINTN)t->ConIn->ReadKeyStroke==p->KeyRead &&
    t->ConIn->WaitForKey==p->KeyWait;
}

static EFI_STATUS Stage1VerifyState(UINTN stage) {
  VerifyCheckpoint="verify-splitter-authority";
  EFI_STATUS bound=Stage1Bound(stage);if(bound!=EFI_SUCCESS)return bound;
  VerifyCheckpoint="verify-stage-input";
  if(stage!=Completed || !PilotInputStable())return EFI_NOT_READY;
  VerifyCheckpoint="verify-alive";
  EFI_STATUS s=Alive(Full.Table,&PreparedPlan);
  if(s==EFI_SUCCESS){VerifyCheckpoint="verify-snapshot";s=Snapshot(Full.Table->BootServices,&Current);}
  if(s==EFI_SUCCESS)VerifyCheckpoint="verify-graph";
  if(s==EFI_SUCCESS && !(ExpectedGraph(&PreparedPlan,&Initial,&Current,stage)))s=EFI_NOT_READY;
  if(s==EFI_SUCCESS && stage<3) {
    VerifyCheckpoint="verify-text-interface";VOID *text=NULL;EFI_STATUS ts=Interface(Full.Table->BootServices,PreparedPlan.Rtx,&TextGuid,&text);
    if(stage<2 && (ts!=EFI_SUCCESS || !text))s=EFI_NOT_READY;
    if(s==EFI_SUCCESS && text!=Splitter.Text)s=EFI_NOT_READY;
    if(stage==2 && !((ts==EFI_UNSUPPORTED || ts==EFI_NOT_FOUND) && !text))s=EFI_NOT_READY;
  }
  UINT32 cfg[64];UINT64 attrs=0;
  if(s==EFI_SUCCESS){VerifyCheckpoint="verify-config-read";
    s=Stage1ReadConfig(cfg);
    if(s!=EFI_SUCCESS && Stage1Working.ConfigValid)VerifyCheckpoint="verify-config-identity";
  }
  if(s==EFI_SUCCESS)VerifyCheckpoint="verify-config-match";
  if(s==EFI_SUCCESS && !(Bytes(Full.Config,cfg,sizeof(cfg))))s=EFI_NOT_READY;
  if(s==EFI_SUCCESS){VerifyCheckpoint="verify-attributes-read";
    s=Stage1ReadAttributes(&attrs);
  }
  UINT64 expected=Full.Attributes;
  if(s==EFI_SUCCESS)VerifyCheckpoint="verify-attributes-match";
  if(s==EFI_SUCCESS && attrs!=expected)s=EFI_NOT_READY;
  if(s==EFI_SUCCESS){VerifyCheckpoint="verify-bar";s=CheckBar(FALSE);}
  /* Do not read GPU MMIO while memory decode is disabled after driver stop. */
  if(s==EFI_SUCCESS && (cfg[1]&2)) {
    s=Stage1ReadSafe("verify-cold-raw-safety");if(s!=EFI_SUCCESS)return s;
    VerifyCheckpoint="verify-cold-read";UINT32 cold[9];s=Cold(cold);
    if(s==EFI_SUCCESS)VerifyCheckpoint="verify-cold-match";
    if(s==EFI_SUCCESS && !Bytes(cold,Full.Cold,sizeof(cold)))s=EFI_NOT_READY;
    if(s==EFI_SUCCESS)s=Stage1Prom();
  }
  if(s==EFI_SUCCESS && stage<3)VerifyCheckpoint="verify-splitter-code";
  if(s==EFI_SUCCESS && stage<3)s=Driver(Full.Table->BootServices,PreparedPlan.OutAgent,&OutFile,GU_CONSOLE_TEXT_SIZE,GU_CONSOLE_TEXT_STOP,GU_CONSOLE_TEXT_VERSION,EfiBootServicesCode);
  if(s==EFI_SUCCESS && stage<3)VerifyCheckpoint="verify-graphics-code";
  if(s==EFI_SUCCESS && stage<3)s=Driver(Full.Table->BootServices,PreparedPlan.GfxAgent,&GfxFile,GU_CONSOLE_GFX_SIZE,GU_CONSOLE_GFX_STOP,GU_CONSOLE_GFX_VERSION,EfiBootServicesCode);
  if(s==EFI_SUCCESS && stage<3)VerifyCheckpoint="verify-nvidia-code";
  if(s==EFI_SUCCESS && stage<3)s=Profile(Full.NvAgent,FALSE);
  if(s==EFI_SUCCESS){VerifyCheckpoint="verify-final-input";if(!PilotInputStable())s=EFI_NOT_READY;}
  return s;
}
EFI_STATUS PilotVerifyAt(UINTN stage,const char *phase) {
  Stage1Begin(phase,stage);
  EFI_STATUS s=Stage1VerifyState(stage);
  if(s!=EFI_SUCCESS){Stage1Remember(s);Splitter.Ready=FALSE;Faulted=TRUE;}
  return s;
}
EFI_STATUS PilotVerify(UINTN stage) {return PilotVerifyAt(stage,"verification");}
EFI_STATUS PilotPreflight(EFI_SYSTEM_TABLE *t,EFI_HANDLE image,GU_PREPARE_LOG log,VOID *ctx) {
  /* A second preflight in this boot cannot renew a consumed or failed run. */
  if(Splitter.Started || Splitter.Ended){Splitter.Ready=FALSE;Faulted=TRUE;return EFI_ACCESS_DENIED;}
  Splitter.Started=TRUE;
  Stage1Failure=(STAGE1_DIAGNOSTIC){0};Stage1Reported=FALSE;
  Stage1ConfigBaselineValid=Stage1AttributeBaselineValid=FALSE;
  Stage1Begin("preflight",0);VerifyCheckpoint="preflight-input";
  Completed=0;Faulted=FALSE;PreparedReady=FALSE;PreparedPlan=(PLAN){0};
  Full=(FULL){.Table=t,.Image=image,.Log=log,.Context=ctx};
  if(!t || !t->BootServices || !image || !log || !t->ConIn || !t->ConOut || !t->StdErr)return EFI_INVALID_PARAMETER;
  EFI_BOOT_SERVICES *bs=t->BootServices;
  if(!bs->LocateHandleBuffer || !bs->HandleProtocol || !bs->ProtocolsPerHandle || !bs->OpenProtocolInformation || !bs->AllocatePool || !bs->FreePool || !bs->RaiseTPL || !bs->RestoreTPL || !bs->LocateDevicePath)return EFI_UNSUPPORTED;
  PLAN *p=&PreparedPlan;p->Console=t->ConsoleOutHandle;p->Input=t->ConsoleInHandle;p->Error=t->StandardErrorHandle;
  p->ConsoleText=t->ConOut;p->ErrorText=t->StdErr;p->Keys=t->ConIn;
  p->KeyReset=(UINTN)t->ConIn->Reset;p->KeyRead=(UINTN)t->ConIn->ReadKeyStroke;p->KeyWait=t->ConIn->WaitForKey;
  if(!p->KeyReset || !p->KeyRead || !p->KeyWait)return EFI_UNSUPPORTED;
  CHECKPOINT("pilot-displays");EFI_STATUS s=Note("pilot-displays",Displays(t,p,TRUE));
  if(s==EFI_SUCCESS){CHECKPOINT("preflight-snapshot");s=Snapshot(bs,&Initial);}
  if(s==EFI_SUCCESS){CHECKPOINT("pilot-consumers");s=Note("pilot-consumers",Consumers(t,p,&Initial));}
  if(s==EFI_SUCCESS)CHECKPOINT("preflight-pci-services");
  if(s==EFI_SUCCESS && (!p->RtxIo->Attributes || !p->RtxIo->Mem.Read || !p->RtxIo->GetBarAttributes))s=EFI_UNSUPPORTED;
  Filtered.Count=0;
  for(UINTN i=0;i<Initial.Count;i++)if(!Removed(p,&Initial.Edges[i],2))Filtered.Edges[Filtered.Count++]=Initial.Edges[i];
  PreparedGraph=&Initial;
  if(s==EFI_SUCCESS){CHECKPOINT("pilot-nvidia-graph");s=Note("pilot-nvidia-graph",FullConsumers(&Filtered));}
  if(s==EFI_SUCCESS){CHECKPOINT("pilot-nvidia-code");s=Note("pilot-nvidia-code",Profile(Full.NvAgent,FALSE));}
  /* Stage 1 has its own splitter-only authority. Shared release permits and
   * Stage 0 observations keep their original independent policies. */
  if(s==EFI_SUCCESS)s=Stage1Capture();
  if(s==EFI_SUCCESS) {
    s=PilotVerifyAt(0,"preflight");
    s=Note(VerifyCheckpoint,s);
    if(s==EFI_SUCCESS)s=Note("pilot-baseline-verify",EFI_SUCCESS);
  }
  if(s==EFI_SUCCESS){CHECKPOINT("pilot-baseline-observe");s=Note("pilot-baseline-observe",Observe());}
  if(s!=EFI_SUCCESS){Stage1Remember(s);Splitter.Ready=FALSE;Faulted=TRUE;}
  return s;
}
EFI_STATUS PilotConfirm(VOID) {
  if(Faulted || Splitter.Confirmed || Stage1Bound(0)!=EFI_SUCCESS){Splitter.Ready=FALSE;Faulted=TRUE;return EFI_ACCESS_DENIED;}
  EFI_STATUS s=PilotVerifyAt(0,"confirmation");if(s==EFI_SUCCESS)Splitter.Confirmed=TRUE;
  else {Splitter.Ready=FALSE;Faulted=TRUE;}return s;
}
VOID PilotEnd(VOID) {
  Splitter.Ready=Splitter.Confirmed=FALSE;Splitter.Ended=TRUE;Faulted=TRUE;
}
EFI_STATUS PilotAdvance(UINTN stage) {
  if(Faulted || stage!=1 || Completed || !Splitter.Confirmed || Splitter.Attempted) {
    Faulted=TRUE;Splitter.Ready=FALSE;return EFI_ACCESS_DENIED;
  }
  Faulted=TRUE;
  if(stage!=1 || !Splitter.Confirmed || Splitter.Attempted || Stage1Bound(0)!=EFI_SUCCESS)return EFI_ACCESS_DENIED;
  EFI_BOOT_SERVICES *bs=Full.Table->BootServices;
  if(!bs->DisconnectController)return EFI_UNSUPPORTED;
  EFI_STATUS s=Note("pilot-text-intent",EFI_SUCCESS);
  if(s==EFI_SUCCESS)s=Full.Log(Full.Context,"stage1-disconnect-controller",(UINTN)Splitter.Plan.Rtx);
  if(s==EFI_SUCCESS)s=Full.Log(Full.Context,"stage1-disconnect-agent",(UINTN)Splitter.Plan.OutAgent);
  if(s==EFI_SUCCESS)s=Full.Log(Full.Context,"stage1-disconnect-child",0);
  if(s!=EFI_SUCCESS)return s;
  EFI_TPL old=bs->RaiseTPL(TPL_NOTIFY);
  s=PilotVerifyAt(Completed,"pre-disconnect-raised");bs->RestoreTPL(old);
  /* Revalidate again at application TPL after queued callbacks ran. There is
   * still no atomicity guarantee across firmware service calls themselves. */
  if(s==EFI_SUCCESS)s=PilotVerifyAt(Completed,"pre-disconnect");
  if(s!=EFI_SUCCESS)return s;
  if(!PilotInputStable())return EFI_NOT_READY;
  EFI_HANDLE controller=PreparedPlan.Rtx,agent=PreparedPlan.OutAgent;
  if(controller!=Splitter.Plan.Rtx || agent!=Splitter.Plan.OutAgent)return EFI_ACCESS_DENIED;
  Splitter.Attempted=TRUE;Splitter.Confirmed=FALSE;
  Stage1Begin("disconnect",stage);VerifyCheckpoint="disconnect-controller";
  s=bs->DisconnectController(controller,agent,NULL);
  Stage1Remember(s);
  EFI_STATUS written=Full.Log(Full.Context,"pilot-disconnect-return",s);
  if(written==EFI_SUCCESS)written=Observe();
  if(s!=EFI_SUCCESS)return s;if(written!=EFI_SUCCESS)return written;
  Completed=stage;
  s=PilotVerifyAt(stage,"post-disconnect");
  return s;
}

EFI_STATUS PilotEvidence(VOID) {
  EFI_STATUS s=PilotVerifyAt(Completed,"evidence");if(s!=EFI_SUCCESS)return s;
  UINT32 cfg[64];
  Stage1Begin("evidence-record",Completed);VerifyCheckpoint="evidence-config-read";
  s=Stage1ReadConfig(cfg);
  if(s!=EFI_SUCCESS && Stage1Working.ConfigValid)VerifyCheckpoint="evidence-config-identity";
  if(s==EFI_SUCCESS){CHECKPOINT("evidence-config-log");s=LogConfig("pilot-pci256",cfg);}
  if(s==EFI_SUCCESS)CHECKPOINT("evidence-log");
  if(s==EFI_SUCCESS)s=Full.Log(Full.Context,"pilot-graph-records",Current.Count);
  UINT8 digest[32];GuSha256(Current.Edges,Current.Count*sizeof(EDGE),digest);
  char label[84]="pilot-graph-sha256=";UINTN at=sizeof("pilot-graph-sha256=")-1;
  for(UINTN i=0;i<32;i++){label[at++]="0123456789abcdef"[digest[i]>>4];label[at++]="0123456789abcdef"[digest[i]&15];}label[at]=0;
  if(s==EFI_SUCCESS)s=Note(label,EFI_SUCCESS);
  if(s==EFI_SUCCESS)s=Note("pilot-keyboard-pointers-stable",PilotInputStable()?EFI_SUCCESS:EFI_NOT_READY);
  if(s==EFI_SUCCESS)s=Full.Log(Full.Context,"pilot-memory-decode-readable",(cfg[1]&2)?1:0);
  if(s==EFI_SUCCESS)s=Full.Log(Full.Context,"stage1-no-release-permit",EFI_ACCESS_DENIED);
  Stage1Remember(s);
  return s;
}

static VOID Hex(char *out,UINTN *n,UINT64 value,UINTN digits) {
  for(UINTN i=digits;i;i--)out[(*n)++]="0123456789abcdef"[(value>>((i-1)*4))&15];
}
static EFI_STATUS Observe(VOID) {
  EFI_STATUS s=Snapshot(Full.Table->BootServices,&Current);
  EFI_STATUS written=Full.Log(Full.Context,"pilot-observe-graph-status",s);
  if(s!=EFI_SUCCESS)return s;if(written!=EFI_SUCCESS)return written;
  s=Full.Log(Full.Context,"pilot-observe-count",Current.Count);
  UINTN rows=0;
  for(UINTN i=0;s==EFI_SUCCESS && i<Current.Count;i++) {
    const EDGE *e=&Current.Edges[i];PLAN *p=&PreparedPlan;
    (void)p;if(++rows>MAX_RECORDS)return EFI_BAD_BUFFER_SIZE;
    char row[160]="pilot-edge ";UINTN n=11;
    Hex(row,&n,(UINTN)e->Handle,16);row[n++]=' ';
    Hex(row,&n,(UINTN)e->Agent,16);row[n++]=' ';
    Hex(row,&n,(UINTN)e->Controller,16);row[n++]=' ';
    const UINT8 *g=(const VOID *)&e->Protocol;for(UINTN j=0;j<16;j++)Hex(row,&n,g[j],2);row[n++]=' ';
    Hex(row,&n,e->Attributes,8);row[n++]=' ';Hex(row,&n,e->Count,8);row[n]=0;
    s=Full.Log(Full.Context,row,EFI_SUCCESS);
  }
  if(s==EFI_SUCCESS)s=Full.Log(Full.Context,"pilot-observe-keyboard-pointers",PilotInputStable()?EFI_SUCCESS:EFI_NOT_READY);
  return s;
}
