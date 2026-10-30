/* GPL-2.0-only. Replay actual graph in host RAM. No real firmware/GPU calls. */
#ifdef GU_FULL_REPLAY_TEST
#include "full-release.c"
#else
#include "prepare-console.c"
#endif
#include "console-flow.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef GU_SINGLE_RTX_CONSOLE
#include "build/r2/replay-fixture.h"
#else
#include "build/console/replay-fixture.h"
#endif

static GRAPH Live;
static int Scenario,Calls,Checks,OutputCalls,LogCalls;
static EFI_HANDLE CalledAgent[2],CalledController[2];
static EFI_SYSTEM_TABLE *LastTable;
static EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE AmdMode,RtxMode;
static EFI_GRAPHICS_OUTPUT_MODE_INFORMATION AmdInfo,RtxInfo;
static EFI_GRAPHICS_OUTPUT_PROTOCOL AmdGop,RtxGop;
static EFI_SIMPLE_TEXT_OUTPUT_MODE TextMode;
static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL Text;
static EFI_SIMPLE_TEXT_INPUT_PROTOCOL Keys;
static EFI_PCI_IO_PROTOCOL AmdPci,RtxPci;
static EFI_DRIVER_BINDING_PROTOCOL OutBinding,GfxBinding;
static EFI_LOADED_IMAGE_PROTOCOL OutImage,GfxImage;
static UINT8 OutFilePath[24],GfxFilePath[24];
static UINT8 TestAmdPath[36],TestRtxPath[GU_RTX_DISPLAY_BYTES];
static EFI_GUID Dummy={0x12345678,0,0,{0}};
#ifdef GU_FULL_REPLAY_TEST
static BOOLEAN FullMode;
#ifdef GU_HEADLESS_RESEARCH
static BOOLEAN PhaseForHeadlessTest(VOID);
#endif
static EFI_STATUS ExtraHandle(EFI_HANDLE h,EFI_GUID *g,VOID **out);
static EFI_STATUS ExtraRead(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH width,UINT32 at,UINTN n,VOID *data);
static EFI_STATUS ExtraDisconnect(EFI_HANDLE h,EFI_HANDLE agent,EFI_HANDLE child);
#endif

static void Check(int condition,const char *name) {
  Checks++;if(!condition){fprintf(stderr,"FAIL %s scenario=%d calls=%d\n",name,Scenario,Calls);exit(1);}
}
static EFI_STATUS EFIAPI Allocate(EFI_MEMORY_TYPE type,UINTN n,VOID **p) {(void)type;*p=malloc(n);return *p?EFI_SUCCESS:EFI_OUT_OF_RESOURCES;}
static EFI_STATUS EFIAPI Free(VOID *p) {free(p);return EFI_SUCCESS;}
static EFI_TPL EFIAPI Raise(EFI_TPL x) {(void)x;return TPL_APPLICATION;}
static VOID EFIAPI Restore(EFI_TPL x) {(void)x;}
static EFI_STATUS EFIAPI Locate(EFI_LOCATE_SEARCH_TYPE search,EFI_GUID *g,VOID *key,UINTN *n,EFI_HANDLE **out) {
  (void)key;
  if(search==AllHandles) {
    *n=FX_HANDLES;*out=malloc(*n*sizeof(**out));
    for(UINTN i=0;i<*n;i++)(*out)[i]=(EFI_HANDLE)(i+1);
  } else {
    Check(GuGuidEqual(g,&GopGuid),"only GOP-specific enumeration");
#ifdef GU_SINGLE_RTX_CONSOLE
#ifdef GU_HEADLESS_RESEARCH
    if(FullMode && PhaseForHeadlessTest()) {*n=0;*out=NULL;return EFI_NOT_FOUND;}
#endif
    *n=Scenario==9?2:1;
    *out=malloc(*n*sizeof(**out));(*out)[0]=FX_Rtx;
    if(Scenario==29)(*out)[0]=FX_Amd;
    if(*n==2)(*out)[1]=FX_Rtx;
    return EFI_SUCCESS;
#else
    *n=Scenario==9?3:2;
#ifdef GU_FULL_REPLAY_TEST
    if(FullMode && Full.Released)*n=1;
#endif
    *out=malloc(*n*sizeof(**out));(*out)[0]=FX_Amd;if(*n>1)(*out)[1]=FX_Rtx;
    if(*n==3)(*out)[2]=FX_Rtx;
#endif
  }
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Protocols(EFI_HANDLE h,EFI_GUID ***out,UINTN *n) {
  *out=malloc(256*sizeof(**out));*n=1;(*out)[0]=&Dummy;
  for(UINTN i=0;i<Live.Count;i++)if(Live.Edges[i].Handle==h) {
    BOOLEAN found=FALSE;for(UINTN j=0;j<*n;j++)if(GuGuidEqual((*out)[j],&Live.Edges[i].Protocol))found=TRUE;
    if(!found)(*out)[(*n)++]=&Live.Edges[i].Protocol;
  }
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Opens(EFI_HANDLE h,EFI_GUID *g,EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **out,UINTN *n) {
  *out=malloc(512*sizeof(**out));*n=0;
  for(UINTN i=0;i<Live.Count;i++) {
    const EDGE *e=&Live.Edges[i];
    if(e->Handle==h && GuGuidEqual(g,&e->Protocol))(*out)[(*n)++]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){e->Agent,e->Controller,e->Attributes,e->Count};
  }
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI ReadKey(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *in,EFI_INPUT_KEY *key) {(void)in;(void)key;return EFI_NOT_READY;}
#ifdef GU_SINGLE_RTX_CONSOLE
static EFI_STATUS EFIAPI ResetInput(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *in,BOOLEAN extended) {(void)in;(void)extended;return EFI_SUCCESS;}
#endif
static EFI_STATUS EFIAPI Output(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out,CHAR16 *s) {
  (void)out;(void)s;OutputCalls++;
  if(Scenario==21 && OutputCalls==2)AmdMode.FrameBufferBase=0;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Handle(EFI_HANDLE h,EFI_GUID *g,VOID **out) {
  *out=NULL;
#ifdef GU_FULL_REPLAY_TEST
  if(FullMode) {
    EFI_STATUS extra=ExtraHandle(h,g,out);if(extra!=EFI_NOT_FOUND)return extra;
  }
#endif
  if(GuGuidEqual(g,&GopGuid)) {
    if(h==FX_Amd)*out=&AmdGop;
    if(h==FX_Rtx)*out=&RtxGop;
  } else if(GuGuidEqual(g,&PathGuid)) {
    if(h==FX_Amd)*out=TestAmdPath;
    if(h==FX_Rtx)*out=TestRtxPath;
  } else if(GuGuidEqual(g,&PciGuid)) {
    if(h==FX_AmdPci)*out=&AmdPci;
    if(h==FX_RtxPci)*out=&RtxPci;
  } else if(GuGuidEqual(g,&TextGuid)) {
    if(h==FX_Amd || h==FX_Console || (h==FX_Rtx && Calls<2))*out=&Text;
  } else if(GuGuidEqual(g,&InputGuid)) {
    if(h==FX_Input)*out=&Keys;
  } else if(GuGuidEqual(g,&BindingGuid)) {
    if(h==FX_OutAgent)*out=&OutBinding;
    if(h==FX_GfxAgent)*out=&GfxBinding;
  } else if(GuGuidEqual(g,&ImageGuid)) {
    if(h==FX_OutImage)*out=&OutImage;
    if(h==FX_GfxImage)*out=&GfxImage;
  }
  return *out?EFI_SUCCESS:EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI FindPci(EFI_GUID *g,EFI_DEVICE_PATH_PROTOCOL **path,EFI_HANDLE *h) {
  if(!GuGuidEqual(g,&PciGuid))abort();
  *h=(VOID *)*path==(VOID *)TestAmdPath?FX_AmdPci:FX_RtxPci;
  if(Scenario==27)*h=FX_AmdPci; /* Wrong GPU behind a matching-looking path. */
  UINTN consumed=(VOID *)*path==(VOID *)TestAmdPath?24:sizeof(TestRtxPath)-12;
  *path=(VOID *)((UINT8 *)*path+consumed);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Location(EFI_PCI_IO_PROTOCOL *io,UINTN *seg,UINTN *bus,UINTN *dev,UINTN *fun) {
  *seg=*dev=*fun=0;*bus=io==&AmdPci?12:GU_TARGET_BUS;
#ifdef GU_PROFILE_LOCAL
  if(io==&RtxPci)*seg=FX_PCI_SEGMENT;
#endif
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Read(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH width,UINT32 at,UINTN n,VOID *data) {
#ifdef GU_FULL_REPLAY_TEST
  if(FullMode)return ExtraRead(io,width,at,n,data);
#endif
  if(width!=EfiPciIoWidthUint32 || at!=0 || n!=16)abort();
  if(Scenario==13)return EFI_DEVICE_ERROR;
  memcpy(data,io==&AmdPci?FixtureAmdConfig:FixtureRtxConfig,64);
#ifdef GU_SINGLE_RTX_CONSOLE
  if(Scenario==30)((UINT32 *)data)[1]^=4;
#endif
  return EFI_SUCCESS;
}
static VOID Drop(EFI_HANDLE handle,EFI_HANDLE agent,EFI_GUID *g) {
  UINTN n=0;
  for(UINTN i=0;i<Live.Count;i++) {
    EDGE *e=&Live.Edges[i];
    if(e->Handle!=handle || e->Agent!=agent || !GuGuidEqual(&e->Protocol,g))Live.Edges[n++]=*e;
  }
  Live.Count=n;
}
static EFI_STATUS EFIAPI Disconnect(EFI_HANDLE controller,EFI_HANDLE agent,EFI_HANDLE child) {
#ifdef GU_FULL_REPLAY_TEST
  if(FullMode && Calls>=2)return ExtraDisconnect(controller,agent,child);
#endif
  Check(Calls<2,"maximum two explicit disconnections");
  CalledAgent[Calls]=agent;CalledController[Calls]=controller;Calls++;
  Check(controller==FX_Rtx && child==NULL,"only physical RTX child passed as controller");
  Check(agent==(Calls==1?FX_OutAgent:FX_GfxAgent),"exact binding order; never image/NV/EDID");
  if(Scenario==3 || (Scenario==7 && Calls==2))return EFI_DEVICE_ERROR;
  if(Scenario==19)return EFI_WARN_UNKNOWN_GLYPH;
  if(Scenario!=4)Drop(FX_Rtx,agent,Calls==1?&TextGuid:&GopGuid);
  if(Scenario==5)Drop(FX_Amd,FX_OutAgent,&TextGuid);
  if(Scenario==6)AmdMode.FrameBufferBase=0;
  if(Scenario==10)Live.Edges[0].Count++;
  if(Scenario==16)Live.Edges[Live.Count++]=(EDGE){FX_Input,FX_GfxAgent,FX_Input,Dummy,16,1};
  if(Scenario==20)GfxBinding.Version++;
  if(Scenario==25)TestRtxPath[sizeof(TestRtxPath)-8]^=1;
  if(Scenario==26)TestAmdPath[28]^=1;
  return EFI_SUCCESS;
}
static EFI_STATUS Log(VOID *context,const char *event,EFI_STATUS s) {
  (void)context;(void)s;LogCalls++;
  if(Scenario==2 && !strcmp(event,"detach-rtx-textout-intent"))Live.Edges[0].Count++;
  if(Scenario==11 && !strcmp(event,"detach-rtx-textout-intent"))return EFI_DEVICE_ERROR;
  if(Scenario==12 && !strcmp(event,"detach-rtx-textout-return"))return EFI_DEVICE_ERROR;
  if(Scenario==22 && !strcmp(event,"stage2a-success-no-unlock-no-gop-removal"))Drop(FX_Amd,FX_OutAgent,&TextGuid);
  return EFI_SUCCESS;
}
static EFI_STATUS Run(int scenario) {
  Scenario=scenario;Calls=0;OutputCalls=0;LogCalls=0;
  memcpy(TestAmdPath,AmdPath,sizeof(TestAmdPath));memcpy(TestRtxPath,RtxPath,sizeof(TestRtxPath));
#ifdef GU_PROFILE_LOCAL
  const UINT8 observedTarget[]={FX_PRIMARY_PATH_BYTES};
  Check(sizeof(observedTarget)==sizeof(TestRtxPath) && FX_CONTROLLER_BYTES==sizeof(TestRtxPath)-12,
        "captured LOCAL path dimensions match the compiled target");
  memcpy(TestRtxPath,observedTarget,sizeof(TestRtxPath));
#endif
  if(scenario==23)TestRtxPath[sizeof(TestRtxPath)-8]=0x40; /* FULL02 observed output endpoint. */
  if(scenario==24){TestRtxPath[sizeof(TestRtxPath)-8]=0x40;TestAmdPath[28]^=0x20;}
  if(scenario==28)TestRtxPath[17]^=1; /* Different PCI ancestry. */
  Live.Count=sizeof(FixtureEdges)/sizeof(FixtureEdges[0]);memcpy(Live.Edges,FixtureEdges,sizeof(FixtureEdges));
  if(scenario==1)Live.Edges[Live.Count++]=(EDGE){FX_Rtx,FX_Input,FX_Rtx,TextGuid,16,1};
  if(scenario==14)Live.Edges[Live.Count++]=(EDGE){FX_Rtx,FX_OutAgent,FX_Input,Dummy,8,1};
  if(scenario==15)Drop(FX_Amd,FX_OutAgent,&TextGuid);
  static EFI_BOOT_SERVICES bs;static EFI_SYSTEM_TABLE t;
  bs=(EFI_BOOT_SERVICES){0};t=(EFI_SYSTEM_TABLE){0};LastTable=&t;
  bs.AllocatePool=Allocate;bs.FreePool=Free;bs.RaiseTPL=Raise;bs.RestoreTPL=Restore;
  bs.LocateHandleBuffer=Locate;bs.ProtocolsPerHandle=Protocols;bs.OpenProtocolInformation=Opens;
  bs.HandleProtocol=Handle;bs.LocateDevicePath=FindPci;bs.DisconnectController=Disconnect;
  AmdInfo=(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION){0};AmdInfo.HorizontalResolution=1920;AmdInfo.VerticalResolution=1080;AmdInfo.PixelsPerScanLine=1920;
  RtxInfo=AmdInfo;
  AmdMode=(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE){0};AmdMode.MaxMode=9;AmdMode.Info=&AmdInfo;AmdMode.SizeOfInfo=sizeof(AmdInfo);AmdMode.FrameBufferBase=0xfce0000000;AmdMode.FrameBufferSize=1920*1080*4;
  RtxMode=AmdMode;RtxMode.FrameBufferBase=0xfcc0000000;AmdGop.Mode=&AmdMode;RtxGop.Mode=&RtxMode;
#ifdef GU_SINGLE_RTX_CONSOLE
  RtxMode.Info=&RtxInfo;
#endif
  TextMode=(EFI_SIMPLE_TEXT_OUTPUT_MODE){0};Text.Mode=&TextMode;Text.OutputString=Output;
  Keys=(EFI_SIMPLE_TEXT_INPUT_PROTOCOL){0};Keys.ReadKeyStroke=ReadKey;Keys.WaitForKey=(VOID *)1;
#ifdef GU_SINGLE_RTX_CONSOLE
  Keys.Reset=ResetInput;
  if(scenario==31)Keys.Reset=NULL;
#endif
  AmdPci=(EFI_PCI_IO_PROTOCOL){0};AmdPci.GetLocation=Location;AmdPci.Pci.Read=Read;RtxPci=AmdPci;
  OutBinding=(EFI_DRIVER_BINDING_PROTOCOL){0};GfxBinding=OutBinding;
  /* Fingerprints come from the selected machine profile (B650M: 0x7820/0x1494, 0x48a0/0xf04). */
  OutBinding.Version=GU_CONSOLE_TEXT_VERSION;GfxBinding.Version=GU_CONSOLE_GFX_VERSION;
  OutBinding.DriverBindingHandle=FX_OutAgent;GfxBinding.DriverBindingHandle=FX_GfxAgent;
  OutBinding.ImageHandle=FX_OutImage;GfxBinding.ImageHandle=FX_GfxImage;
  OutBinding.Stop=(EFI_DRIVER_BINDING_STOP)(UINTN)(0x100000+GU_CONSOLE_TEXT_STOP);GfxBinding.Stop=(EFI_DRIVER_BINDING_STOP)(UINTN)(0x200000+GU_CONSOLE_GFX_STOP);
  OutImage=(EFI_LOADED_IMAGE_PROTOCOL){0};GfxImage=OutImage;
  OutImage.ImageBase=(VOID *)0x100000;OutImage.ImageSize=GU_CONSOLE_TEXT_SIZE;OutImage.ImageCodeType=EfiBootServicesCode;
  GfxImage.ImageBase=(VOID *)0x200000;GfxImage.ImageSize=GU_CONSOLE_GFX_SIZE;GfxImage.ImageCodeType=EfiBootServicesCode;
  memcpy(OutFilePath,(UINT8[]){4,6,20,0},4);memcpy(OutFilePath+4,&OutFile,16);memcpy(OutFilePath+20,(UINT8[]){127,255,4,0},4);
  memcpy(GfxFilePath,(UINT8[]){4,6,20,0},4);memcpy(GfxFilePath+4,&GfxFile,16);memcpy(GfxFilePath+20,(UINT8[]){127,255,4,0},4);
  OutImage.FilePath=(VOID *)OutFilePath;GfxImage.FilePath=(VOID *)GfxFilePath;
  if(scenario==8)OutBinding.Version=0x11;
  t.BootServices=&bs;t.ConsoleOutHandle=FX_Console;t.StandardErrorHandle=FX_Error;t.ConsoleInHandle=FX_Input;
  t.ConOut=&Text;t.StdErr=&Text;t.ConIn=&Keys;
  return GuPrepareConsole(&t,Log,NULL);
}
typedef struct {
  EFI_STATUS prepare,choose,before,detach,after,record,ready,verify,windows;
  BOOLEAN skip;
  BOOLEAN realVerify;
  int lateChange;
  int captures,detaches,boots;
} FLOW_TEST;
static EFI_STATUS FlowPrepare(VOID *v) {return ((FLOW_TEST *)v)->prepare;}
static EFI_STATUS FlowChoose(VOID *v,BOOLEAN *skip) {FLOW_TEST *f=v;*skip=f->skip;return f->choose;}
static EFI_STATUS FlowCapture(VOID *v) {
  FLOW_TEST *f=v;f->captures++;
  if(f->captures==2 && f->lateChange==1)
#ifdef GU_SINGLE_RTX_CONSOLE
    RtxMode.FrameBufferBase=0;
#else
    AmdMode.FrameBufferBase=0;
#endif
  return f->captures==1?f->before:f->after;
}
static EFI_STATUS FlowDetach(VOID *v) {FLOW_TEST *f=v;f->detaches++;return f->detach;}
static EFI_STATUS FlowWindows(VOID *v) {FLOW_TEST *f=v;f->boots++;return f->windows;}
static EFI_STATUS FlowReady(VOID *v) {
  FLOW_TEST *f=v;
  if(f->lateChange==3)
#ifdef GU_SINGLE_RTX_CONSOLE
    RtxMode.FrameBufferBase=0;
#else
    Drop(FX_Amd,FX_OutAgent,&TextGuid);
#endif
  return f->ready;
}
static EFI_STATUS FlowVerify(VOID *v) {FLOW_TEST *f=v;return f->realVerify?GuVerifyPreparedConsole(LastTable):f->verify;}
static EFI_STATUS FlowRecord(VOID *v,const char *event,EFI_STATUS s) {
  (void)event;(void)s;FLOW_TEST *f=v;if(f->lateChange==2)GfxBinding.Version++;return f->record;
}
static EFI_STATUS FlowRun(FLOW_TEST *f) {
  GU_CONSOLE_FLOW_OPS o={f,FlowPrepare,FlowChoose,FlowCapture,FlowDetach,FlowReady,FlowVerify,FlowWindows,FlowRecord};return GuConsoleFlow(&o);
}
static VOID FlowTests(VOID) {
  FLOW_TEST f={0};
  Check(FlowRun(&f)==EFI_ABORTED && f.boots==1 && f.detaches==1 && f.captures==2,"successful EFI flow only then Windows");
  f=(FLOW_TEST){.skip=TRUE};
  Check(FlowRun(&f)==EFI_ABORTED && f.boots==1 && !f.detaches && !f.captures,"Esc skip has no mutation");
  EFI_STATUS bad[]={EFI_DEVICE_ERROR,EFI_ACCESS_DENIED,EFI_WARN_UNKNOWN_GLYPH};
  for(UINTN i=0;i<3;i++) {
    f=(FLOW_TEST){.prepare=bad[i]};Check(FlowRun(&f)==bad[i] && !f.boots && !f.detaches,"prepare failure never boots");
    f=(FLOW_TEST){.choose=bad[i]};Check(FlowRun(&f)==bad[i] && !f.boots && !f.detaches,"choice failure never boots");
    f=(FLOW_TEST){.before=bad[i]};Check(FlowRun(&f)==bad[i] && !f.boots && !f.detaches,"initial capture failure never mutates or boots");
    f=(FLOW_TEST){.detach=bad[i]};Check(FlowRun(&f)==bad[i] && !f.boots && f.detaches==1 && f.captures==2,"partial detach captures once more; never boots or retries");
    f=(FLOW_TEST){.after=bad[i]};Check(FlowRun(&f)==bad[i] && !f.boots && f.detaches==1,"post capture failure never boots");
    f=(FLOW_TEST){.record=bad[i]};Check(FlowRun(&f)==bad[i] && !f.boots && f.detaches==1,"final log failure never boots");
    f=(FLOW_TEST){.ready=bad[i]};Check(FlowRun(&f)==bad[i] && !f.boots,"file close failure never boots");
    f=(FLOW_TEST){.verify=bad[i]};Check(FlowRun(&f)==bad[i] && !f.boots,"AMD loss after final log/capture/file-close never boots");
    f=(FLOW_TEST){.windows=bad[i]};Check(FlowRun(&f)==bad[i] && f.boots==1,"returned Windows error preserved");
  }
  for(int late=1;late<=3;late++) {
    Check(Run(0)==EFI_SUCCESS,"prepare a real session for late callback mutation");
    f=(FLOW_TEST){.realVerify=TRUE,.lateChange=late};
    Check(FlowRun(&f)!=EFI_SUCCESS && !f.boots,"real final verifier blocks late capture/log/close mutation");
  }
}
int main(void) {
  Check(FX_OutAgent!=FX_OutImage && FX_GfxAgent!=FX_GfxImage,"real firmware image/binding separation");
  Check(Run(0)==EFI_SUCCESS && Calls==2 && OutputCalls==2,"real graph replay succeeds");
  Check(Live.Count+3==sizeof(FixtureEdges)/sizeof(FixtureEdges[0]),"exactly three RTX ownership edges removed");
  Check(Run(23)==EFI_SUCCESS && Calls==2 && PreparedPlan.RtxDisplayPath[sizeof(TestRtxPath)-8]==0x40,"FULL02 endpoint discovered and pinned");
  Check(Run(24)==EFI_SUCCESS && Calls==2,"both GPU display ADRs discovered this boot");
  for(int scenario=25;scenario<=28;scenario++) {
    Check(Run(scenario)!=EFI_SUCCESS,"late endpoint drift or wrong GPU/ancestry rejected");
    Check(Calls==(scenario<=26?1:0),"no further detach after route identity failure");
  }
  for(int scenario=1;scenario<=22;scenario++) {
    if(scenario==17 || scenario==18)continue;
    EFI_STATUS s=Run(scenario);
    Check(s!=EFI_SUCCESS,"negative case fails closed");
    int expected=0;
    if(scenario==3 || scenario==4 || scenario==5 || scenario==6 || scenario==10 || scenario==12 || scenario==16 || scenario==19 || scenario==20)expected=1;
    if(scenario==7 || scenario==21 || scenario==22)expected=2;
    Check(Calls==expected,"no extra disconnect after failure");
  }
  Check(GuPrepareConsole(NULL,Log,NULL)==EFI_INVALID_PARAMETER,"null platform rejected");
  FlowTests();
  printf("PASS console prepare checks: %d; replay %d handles / %zu active records; no real firmware access\n",Checks,FX_HANDLES,sizeof(FixtureEdges)/sizeof(FixtureEdges[0]));
  return 0;
}
