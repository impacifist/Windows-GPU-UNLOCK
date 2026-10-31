/* GPL-2.0-only. Real collector and EFIAPI signatures; RAM-backed services only. */
#define main adapter_fixture_main
#include "test_efi_adapter.c"
#undef main
#include "efi-capture.h"

static GU_DS_CAPTURE Capture;
static GU_DS_CAPTURE_TARGET Target;
static BOOLEAN CaptureReentry,FreezeTarget,Frozen;
static unsigned ReplaceAt;
static EFI_PCI_IO_PROTOCOL ReplacementIo;
static void Hook(void) {
  if(CaptureReentry) {
    unsigned calls=M->Calls;
    Check(GuDsEfiCapture(&Capture,&M->Boot,&Target)==EFI_ALREADY_STARTED,"same capture context cannot reenter or reset");
    Check(M->Calls==calls,"reentry invokes no service");
    GU_DS_CAPTURE fresh={0};
    Check(GuDsEfiCapture(&fresh,&M->Boot,&Target)==EFI_ALREADY_STARTED && !fresh.Used,
      "a fresh context cannot reenter the per-image capture workspace");
  }
  if(FreezeTarget && !Frozen){Frozen=TRUE;Target.PciId^=1;Target.Path[8]^=1;Target.Bus^=1;}
}
static EFI_STATUS EFIAPI CaptureHandle(EFI_HANDLE h,EFI_GUID *g,VOID **p){
  Hook();EFI_STATUS s=Handle(h,g,p);
  if(ReplaceAt && M->Calls>=ReplaceAt && !memcmp(g,&PciGuid,sizeof(*g)) && *p==&M->Devices[0].Io)*p=&ReplacementIo;
  return s;
}
static EFI_STATUS EFIAPI CaptureLocate(EFI_GUID *g,EFI_DEVICE_PATH_PROTOCOL **p,EFI_HANDLE *h){
  Hook();BOOLEAN known=FALSE;
  for(unsigned i=0;i<M->Count;i++)if(!memcmp(*p,M->Devices[i].Path,M->Devices[i].Bytes))known=TRUE;
  if(M->LocateMode==5 || !known){Tick();return EFI_NOT_FOUND;}return Locate(g,p,h);
}
static EFI_STATUS EFIAPI CaptureLocation(EFI_PCI_IO_PROTOCOL *p,UINTN *s,UINTN *b,UINTN *d,UINTN *f){Hook();return Location(p,s,b,d,f);}
static EFI_STATUS EFIAPI CaptureRead(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 o,UINTN n,VOID *v){Hook();return Read(p,w,o,n,v);}
static EFI_STATUS EFIAPI CaptureAttributes(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION o,UINT64 mask,UINT64 *v){
  if(Capture.Busy)Check(o==EfiPciIoAttributeOperationGet || o==EfiPciIoAttributeOperationSupported,"collector never enables/disables attributes");
  Hook();return Attributes(p,o,mask,v);
}
static void CaptureSetup(MODEL *m,unsigned parents,UINTN segment) {
  Setup(m,parents,segment);memset(&Capture,0,sizeof(Capture));memset(&Target,0,sizeof(Target));
  CaptureReentry=FreezeTarget=Frozen=FALSE;ReplaceAt=0;
  m->Boot.HandleProtocol=CaptureHandle;m->Boot.LocateDevicePath=CaptureLocate;
  for(unsigned i=0;i<m->Count;i++) {
    m->Devices[i].Io.GetLocation=CaptureLocation;m->Devices[i].Io.Pci.Read=CaptureRead;m->Devices[i].Io.Attributes=CaptureAttributes;
  }
  Target.Handle=m->Bindings[0].Handle;Target.PathBytes=m->Bindings[0].PathBytes;
  memcpy(Target.Path,m->Bindings[0].Path,Target.PathBytes);
  Target.PciId=m->Expected.Gpu.Config[0];Target.Subsystem=m->Expected.Gpu.Config[11];Target.Bar0=m->Expected.Gpu.Config[4];
  Target.Segment=segment;Target.Bus=m->Expected.Gpu.Bus;
  ReplacementIo=m->Devices[0].Io;
}
static EFI_STATUS Collect(MODEL *m){return GuDsEfiCapture(&Capture,&m->Boot,&Target);}
static void ReadOnly(MODEL *m) {
  Check(!m->Writes && !m->OwnerQueries && !m->Frees && !m->PoolLive && !m->Logs,"collector has no device write, allocation, ownership or log calls");
}
static void CaptureRefuse(MODEL *m) {
  Check(Collect(m)!=EFI_SUCCESS,"invalid or changed capture is refused");
  GU_DS_SNAPSHOT empty={0};GU_DS_EFI_BINDING binding={0};
  Check(!Capture.Ready && !Capture.Count && !Capture.Busy && Capture.Used,"failed capture is spent with no published count");
  Check(!memcmp(&Capture.Snapshot,&empty,sizeof(empty)),"failed capture publishes no partial snapshot");
  for(unsigned i=0;i<16;i++)Check(!memcmp(&Capture.Bindings[i],&binding,sizeof(binding)),"failed capture publishes no partial binding");
  unsigned calls=m->Calls;Check(Collect(m)==EFI_ALREADY_STARTED && m->Calls==calls,"failed capture cannot be retried");ReadOnly(m);
}
int main(void) {
  MODEL m;
  for(unsigned parents=1;parents<=15;parents++)for(unsigned segment=0;segment<3;segment++) {
    CaptureSetup(&m,parents,segment==2?0xffff:segment);CaptureReentry=TRUE;
    if(segment==2) {
      for(unsigned i=0;i<m.Count;i++) {
        m.Devices[i].Path[6]=m.Bindings[i].Path[6]=8;
        for(unsigned j=8;j<12;j++)m.Devices[i].Path[j]=m.Bindings[i].Path[j]=0xff;
      }
      Target.Path[6]=8;for(unsigned j=8;j<12;j++)Target.Path[j]=0xff;
    }
    Check(Collect(&m)==EFI_SUCCESS && Capture.Ready && Capture.Count==m.Count,"bounded chain captured across segments");
    Check(GuDecodeSnapshotMatches(&Capture.Snapshot,&m.Expected,FALSE),"current snapshot matches independent model state");
    for(unsigned i=0;i<m.Count;i++)Check(!memcmp(&Capture.Bindings[i],&m.Bindings[i],sizeof(m.Bindings[i])),"discovered interface and copied parent prefix match actual route");
    ReadOnly(&m);
  }
  CaptureSetup(&m,2,0);Check(Collect(&m)==EFI_SUCCESS,"capture callback baseline");unsigned calls=m.Calls;
  Check(Collect(&m)==EFI_ALREADY_STARTED && m.Calls==calls && Capture.Ready && Capture.Count==m.Count &&
    GuDecodeSnapshotMatches(&Capture.Snapshot,&m.Expected,FALSE),"reuse preserves old data but cannot report a new successful capture");
  for(unsigned at=1;at<=calls;at++) {
    for(unsigned warning=0;warning<2;warning++) {
      CaptureSetup(&m,2,0);m.FaultAt=at;m.FaultStatus=warning?EFI_WARN_UNKNOWN_GLYPH:EFI_DEVICE_ERROR;CaptureRefuse(&m);
    }
    CaptureSetup(&m,2,0);m.DropAt=at;CaptureRefuse(&m);
  }
  for(unsigned bad=0;bad<10;bad++) {
    CaptureSetup(&m,2,0);
    switch(bad) {
      case 0:Target.PciId^=1;break;case 1:Target.Subsystem^=1;break;case 2:Target.Bar0^=0x1000000;break;
      case 3:Target.Segment^=1;break;case 4:Target.Bus^=1;break;case 5:Target.Handle=m.Bindings[1].Handle;break;
      case 6:m.Devices[1].State.Config[6]^=0x100;break;case 7:m.Devices[1].State.Segment^=1;break;
      case 8:m.Devices[0].State.Config[1]|=4;break;
      case 9:Target.PciId=m.Devices[0].State.Config[0]=0x24c910de;break;
    }
    CaptureRefuse(&m);
  }
  for(unsigned mode=1;mode<=5;mode++){CaptureSetup(&m,2,0);m.LocateMode=mode;CaptureRefuse(&m);}
  CaptureSetup(&m,2,0);ReplaceAt=calls/2+1;CaptureRefuse(&m);
  CaptureSetup(&m,2,0);m.Alias=1;CaptureRefuse(&m);
  CaptureSetup(&m,2,0);m.Partial=1;
  for(unsigned i=0;i<m.Count;i++)m.Devices[i].State.Config[63]=0xa5a5a5a5;
  CaptureRefuse(&m);
  for(unsigned mutation=1;mutation<=10;mutation++) {
    CaptureSetup(&m,2,0);m.Mutation=mutation;m.MutateAt=calls/2+1;CaptureRefuse(&m);
  }
  for(unsigned mutation=6;mutation<=9;mutation++) {
    if(mutation==7)continue;
    CaptureSetup(&m,2,0);m.Mutation=mutation;m.MutationDevice=1;m.MutateAt=calls;CaptureRefuse(&m);
  }
  CaptureSetup(&m,2,0);FreezeTarget=TRUE;
  Check(Collect(&m)==EFI_SUCCESS && Frozen,"caller target edits cannot change private capture policy");ReadOnly(&m);
  CaptureSetup(&m,2,0);GU_DS_CAPTURE saved=Capture;
  Check(GuDsEfiCapture(&Capture,&m.Boot,(VOID *)&Capture)==EFI_INVALID_PARAMETER && !memcmp(&saved,&Capture,sizeof(saved)),"aliased target refuses without zeroing input/context");
  EFI_BOOT_SERVICES boot=m.Boot;
  Check(GuDsEfiCapture((VOID *)&m.Boot,&m.Boot,&Target)==EFI_INVALID_PARAMETER && !memcmp(&boot,&m.Boot,sizeof(boot)),"aliased Boot refuses without writing service storage");
  Check(GuDsEfiCapture(&Capture,&m.Boot,(VOID *)&m.Boot)==EFI_INVALID_PARAMETER && !memcmp(&boot,&m.Boot,sizeof(boot)),"target/Boot overlap refuses before reinterpretation");
  for(unsigned bad=0;bad<7;bad++) {
    CaptureSetup(&m,2,0);
    switch(bad) {
      case 0:Target.Path[6]=9;break;case 1:Target.PathBytes=113;break;
      case 2:Target.Path[Target.PathBytes-5]=1;break;case 3:Target.Path[Target.PathBytes-2]=5;break;
      case 4:Target.Segment=0x10000;break;case 5:Target.Bus=0;break;case 6:m.Boot.LocateDevicePath=NULL;break;
    }
    CaptureRefuse(&m);Check(!m.Calls,"malformed target refuses before callbacks");
  }
  CaptureSetup(&m,2,0);
  memmove(Target.Path+12,Target.Path+Target.PathBytes-10,6);
  memcpy(Target.Path+18,"\x7f\xff\x04\x00",4);Target.PathBytes=22;
  CaptureRefuse(&m);Check(!m.Calls,"one-node root-attached GPU remains outside parent-bridge capture scope");
  CaptureSetup(&m,2,0);Target.PathBytes=12+17*6+4;
  CaptureRefuse(&m);Check(!m.Calls,"seventeen-node path is rejected before accessing beyond the fixed path buffer");
  CaptureSetup(&m,2,0);Check(Collect(&m)==EFI_SUCCESS,"composition capture");
  Check(GuDsEfiInitialize(&m.Adapter,&m.Boot,(EFI_HANDLE)&m.OwnerTag,Capture.Bindings,Capture.Count,&Capture.Snapshot,Log,&m)==EFI_SUCCESS,"capture binds to existing owned adapter");
  Check(Run(&m)==GU_DS_FIXED && m.Writes==1,"captured baseline composes with owned adapter in RAM only");
  /* Shared MSI snapshot admission is observation only, never a write grant. */
  CaptureSetup(&m,1,0);
  /* Populate the exact synthetic route while keeping the collector hooks. */
  for(unsigned i=0;i<2;i++) {
    DEVICE *d=&m.Devices[i];GU_DS_DEVICE *v=&d->State;
    d->Path[8]=0;d->Path[16]=1;d->Path[17]=3;
    v->Bus=i?0:0x2d;v->Device=i?3:0;v->Function=i?1:0;
    if(i)v->Config[6]=0x002d2d00;
    else {v->Config[0]=0x220810de;v->Config[11]=0x88031043;v->Config[4]=0xfa000000;}
  }
  Target.Bus=0x2d;Target.PciId=0x220810de;Target.Subsystem=0x88031043;Target.Bar0=0xfa000000;
  memcpy(Target.Path,m.Devices[0].Path,Target.PathBytes);
  Check(Collect(&m)==EFI_SUCCESS && Capture.Ready,"exact MSI baseline collects read-only");ReadOnly(&m);
  m.OwnerMode=1;
  Check(GuDsEfiInitialize(&m.Adapter,&m.Boot,(EFI_HANDLE)&m.OwnerTag,Capture.Bindings,Capture.Count,&Capture.Snapshot,Log,&m)==EFI_SUCCESS,
    "MSI captured baseline binds independently of claim observation");
  Refuse(&m);Check(!m.Writes,"MSI captured baseline cannot replace the required GPU claim");
  printf("PASS %u capture/composition assertions; no EFI execution or hardware access\n",Checks);return 0;
}
