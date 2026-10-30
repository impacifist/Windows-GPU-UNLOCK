/* SPDX-License-Identifier: GPL-2.0-only */
/* Full production prepare/release path. All EFI services and images are RAM mocks. */
#include "full-release.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !GU_CONSOLE_AGGREGATE || !defined(GU_HEADLESS_RESEARCH)
#error This fixture requires the aggregate headless backend
#endif
#if GU_CONSOLE_MULTI_CHILD
#define TEST_CHILDREN GU_CONSOLE_CHILD_COUNT
#else
#define TEST_CHILDREN 1U
#endif
#ifdef GU_AGGREGATE_CAPTURE_FIXTURE
#include "build/aggregate/replay-fixture.h"
#if TEST_CHILDREN != FX_EXTRA_DISPLAY_CHILD+1
#error Captured child count disagrees with the compiled profile
#endif
#define PCI ((UINTN)FX_RtxPci)
#define PRIMARY ((UINTN)FX_Rtx)
#define SIBLING ((UINTN)FX_EdidChild)
#define AGG ((UINTN)FX_Agg)
#define CONSOLE ((UINTN)FX_Console)
#define INPUT ((UINTN)FX_Input)
#define STDERR_HANDLE ((UINTN)FX_Error)
#define NV ((UINTN)FX_NvAgent)
#define EDID ((UINTN)FX_EdidAgent)
#define COMBO ((UINTN)FX_ComboAgent)
#define GFX ((UINTN)FX_GfxAgent)
#define TEXT_DRIVER ((UINTN)FX_OutAgent)
#define COMBO_IMAGE ((UINTN)FX_ComboImage)
#define GFX_IMAGE ((UINTN)FX_GfxImage)
#define OUT_IMAGE ((UINTN)FX_OutImage)
#define APP (FX_HANDLES+1U)
#define PROTECTED (FX_HANDLES+2U)
#define OBSERVER (FX_HANDLES+3U)
#define HANDLE_LIMIT (FX_HANDLES+3U)
#else
enum {PCI=1,PRIMARY,SIBLING,AGG,CONSOLE,INPUT,NV,EDID,COMBO,GFX,TEXT_DRIVER,
      NV_IMAGE,EDID_IMAGE,COMBO_IMAGE,GFX_IMAGE,OUT_IMAGE,PROTECTED,OBSERVER,APP};
#define HANDLE_LIMIT APP
#define STDERR_HANDLE CONSOLE
#endif
#define H(n) ((EFI_HANDLE)(UINTN)(n))
#define TEST_CONTROLLER_BYTES (GU_RTX_DISPLAY_BYTES-12U)
static GRAPH Live;
static EFI_BOOT_SERVICES Boot;
static EFI_SYSTEM_TABLE Table;
static EFI_PCI_IO_PROTOCOL Io;
static EFI_GRAPHICS_OUTPUT_PROTOCOL Physical,Aggregate;
static EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE PhysicalMode,AggregateMode;
static EFI_GRAPHICS_OUTPUT_MODE_INFORMATION Info;
static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL Text;
static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL ErrorText;
static EFI_SIMPLE_TEXT_OUTPUT_MODE TextMode;
static EFI_SIMPLE_TEXT_INPUT_PROTOCOL Input;
static EFI_DRIVER_BINDING_PROTOCOL Bindings[5];
static EFI_LOADED_IMAGE_PROTOCOL Images[5];
static UINT8 DriverPaths[5][32],DisplayPaths[2][GU_RTX_DISPLAY_BYTES],ControllerPath[TEST_CONTROLLER_BYTES+4];
static UINT8 FaultDisplayPath[GU_RTX_DISPLAY_BYTES+6];
static UINTN FaultControllerBytes;
static BOOLEAN ParentDrift,PathFault,SegmentDrift;
static UINT8 NvMemory[GU_NVGOP_IMAGE_SIZE],EdidMemory[GU_EDID_IMAGE_SIZE],
  ComboMemory[GU_COMBO_SIZE],GfxMemory[GU_CONSOLE_GFX_SIZE],OutMemory[GU_CONSOLE_TEXT_SIZE],PromMemory[1048576];
static UINT32 Config[64];
static UINT64 Attributes;
static unsigned Calls,EdidCalls,NvCalls,Enables,Claims,Closes,Checks,Mode;
static BOOLEAN GpuGone;
static BOOLEAN ScenarioReached;
static const char *FirstError;
static EFI_GUID PrivateGuid={0x11223344,0,0,{0}};
static EFI_STATUS EFIAPI Handle(EFI_HANDLE h,EFI_GUID *guid,VOID **out);
static void ChangePathLength(BOOLEAN longer) {
  const UINT8 *source=DisplayPaths[TEST_CHILDREN==2?1:0];
  if(longer) {
    UINTN at=TEST_CONTROLLER_BYTES-6;
    memcpy(FaultDisplayPath,source,at);memcpy(FaultDisplayPath+at,(UINT8[]){1,1,6,0,0,4},6);
    memcpy(FaultDisplayPath+at+6,source+at,GU_RTX_DISPLAY_BYTES-at);FaultControllerBytes=TEST_CONTROLLER_BYTES+6;
  } else {
    memcpy(FaultDisplayPath,source,12);memcpy(FaultDisplayPath+12,source+18,GU_RTX_DISPLAY_BYTES-18);
    FaultControllerBytes=TEST_CONTROLLER_BYTES-6;
  }
  PathFault=TRUE;
}
static UINTN DriverHandle(UINTN i) {const UINTN handles[]={NV,EDID,COMBO,GFX,TEXT_DRIVER};return handles[i];}
static UINTN ImageHandle(UINTN i) {const UINTN handles[]={NV,EDID,COMBO_IMAGE,GFX_IMAGE,OUT_IMAGE};return handles[i];}
static void Check(BOOLEAN good,const char *why) {
  Checks++;if(!good){fprintf(stderr,"FAIL aggregate full: %s mode=%u calls=%u\n",why,Mode,Calls);exit(1);}
}
static void Edge(UINTN h,UINTN a,UINTN c,const EFI_GUID *guid,UINT32 flags) {
  Check(Live.Count<MAX_RECORDS,"bounded graph");
  Live.Edges[Live.Count++]=(EDGE){H(h),H(a),H(c),*guid,flags,1};
}
static void Drop(UINTN handle,const EFI_GUID *guid) {
  UINTN count=0;for(UINTN i=0;i<Live.Count;i++) {
    EDGE *e=&Live.Edges[i];if(e->Handle!=H(handle) || !GuGuidEqual(&e->Protocol,guid))Live.Edges[count++]=*e;
  }Live.Count=count;
}
static EFI_STATUS EFIAPI Allocate(EFI_MEMORY_TYPE type,UINTN bytes,VOID **out) {
  (void)type;*out=malloc(bytes);return *out?EFI_SUCCESS:EFI_OUT_OF_RESOURCES;
}
static EFI_STATUS EFIAPI Free(VOID *memory) {free(memory);return EFI_SUCCESS;}
static EFI_TPL EFIAPI Raise(EFI_TPL level) {(void)level;return TPL_APPLICATION;}
static VOID EFIAPI Restore(EFI_TPL level) {(void)level;}
static EFI_STATUS EFIAPI Locate(EFI_LOCATE_SEARCH_TYPE type,EFI_GUID *guid,VOID *key,UINTN *count,EFI_HANDLE **out) {
  (void)key;*count=0;*out=NULL;
  if(type==AllHandles) {
    *count=HANDLE_LIMIT;*out=malloc(HANDLE_LIMIT*sizeof(**out));for(UINTN i=0;i<HANDLE_LIMIT;i++)(*out)[i]=H(i+1);
  } else {
    Check(GuGuidEqual(guid,&GopGuid),"GOP-only enumeration");
    *out=malloc(3*sizeof(**out));
    for(UINTN h=1;h<=HANDLE_LIMIT;h++) {
      VOID *interface=NULL;
      if(Handle(H(h),&GopGuid,&interface)==EFI_SUCCESS && interface) {
        Check(*count<3,"bounded installed GOP inventory");(*out)[(*count)++]=H(h);
      }
    }
    if(!*count){free(*out);*out=NULL;return EFI_NOT_FOUND;}
  }return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Protocols(EFI_HANDLE h,EFI_GUID ***out,UINTN *count) {
  EFI_GUID *known[]={&PrivateGuid,&PciGuid,&PathGuid,&GopGuid,&TextGuid,&InputGuid,&BindingGuid,&ImageGuid,&EdidGuid};
  UINTN capacity=sizeof(known)/sizeof(known[0]);
#ifdef GU_AGGREGATE_CAPTURE_FIXTURE
  capacity+=sizeof(FixtureInstalledProtocols)/sizeof(FixtureInstalledProtocols[0]);
#endif
  *out=malloc(capacity*sizeof(**out));Check(*out!=NULL,"protocol inventory allocation");*count=0;
  for(UINTN i=0;i<sizeof(known)/sizeof(known[0]);i++) {
    VOID *interface=NULL;
    if(Handle(h,known[i],&interface)==EFI_SUCCESS && interface)(*out)[(*count)++]=known[i];
  }
#ifdef GU_AGGREGATE_CAPTURE_FIXTURE
  /* Capture-only protocols can have no active opener, or belong to unrelated
   * protected drivers. Preserve them independently of the mutable edge list. */
  for(UINTN i=0;i<sizeof(FixtureInstalledProtocols)/sizeof(FixtureInstalledProtocols[0]);i++) {
    const EFI_GUID *guid=&FixtureInstalledProtocols[i].Guid;
    if(FixtureInstalledProtocols[i].Handle!=h)continue;
    BOOLEAN found=FALSE;for(UINTN j=0;j<*count;j++)if(GuGuidEqual((*out)[j],guid))found=TRUE;
    VOID *interface=NULL;
    if(!found && Handle(h,(EFI_GUID *)guid,&interface)==EFI_SUCCESS && interface) {
      Check(*count<capacity,"bounded captured protocol inventory");(*out)[(*count)++]=(EFI_GUID *)guid;
    }
  }
#endif
  for(UINTN i=0;i<Live.Count;i++)if(Live.Edges[i].Handle==h) {
    VOID *interface=NULL;
    Check(Handle(h,&Live.Edges[i].Protocol,&interface)==EFI_SUCCESS && interface,"open records require installed protocol");
  }
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Opens(EFI_HANDLE h,EFI_GUID *guid,EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **out,UINTN *count) {
  *out=malloc(MAX_RECORDS*sizeof(**out));*count=0;
  for(UINTN i=0;i<Live.Count;i++) {
    EDGE *e=&Live.Edges[i];if(e->Handle==h && GuGuidEqual(guid,&e->Protocol))
      (*out)[(*count)++]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){e->Agent,e->Controller,e->Attributes,e->Count};
  }return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Handle(EFI_HANDLE h,EFI_GUID *guid,VOID **out) {
  *out=NULL;
  if(GuGuidEqual(guid,&PrivateGuid) && (UINTN)h>=1 && (UINTN)h<=HANDLE_LIMIT)*out=&PrivateGuid;
  if(GuGuidEqual(guid,&PciGuid) && h==H(PCI))*out=&Io;
  if(GuGuidEqual(guid,&PathGuid) && (!GpuGone || Mode==7)) {
    if(h==H(PRIMARY))*out=DisplayPaths[0];
    if(TEST_CHILDREN==2 && h==H(SIBLING))*out=DisplayPaths[1];
    if(PathFault && h==H(TEST_CHILDREN==2?SIBLING:PRIMARY))*out=FaultDisplayPath;
  }
  if(GuGuidEqual(guid,&PathGuid) && h==H(PCI))*out=ControllerPath;
  if(GuGuidEqual(guid,&PathGuid) && GpuGone && Mode==10 && h==H(AGG))*out=DisplayPaths[0];
  if(GuGuidEqual(guid,&GopGuid)) {
    if((!GpuGone || Mode==7) && h==H(PRIMARY))*out=&Physical;
    if((!GpuGone || (Mode>=6 && Mode<=11)) && h==H(AGG) && !(GpuGone && Mode==8))*out=&Aggregate;
    if(GpuGone && Mode==8 && h==H(PROTECTED))*out=&Aggregate;
  }
  if(GuGuidEqual(guid,&EdidGuid) && !GpuGone &&
     (h==H(PRIMARY) || (TEST_CHILDREN==2 && h==H(SIBLING))))*out=EdidMemory;
  if(GuGuidEqual(guid,&TextGuid) && (h==H(CONSOLE) || h==H(AGG)))*out=&Text;
  if(GuGuidEqual(guid,&TextGuid) && h==H(STDERR_HANDLE) && h!=H(CONSOLE) && h!=H(AGG))*out=&ErrorText;
  if(GuGuidEqual(guid,&InputGuid) && h==H(INPUT))*out=&Input;
  for(UINTN i=0;i<5;i++) {
    if(GuGuidEqual(guid,&BindingGuid) && h==H(DriverHandle(i)))*out=&Bindings[i];
    if(GuGuidEqual(guid,&ImageGuid) && h==H(ImageHandle(i)))*out=&Images[i];
  }
#ifdef GU_AGGREGATE_CAPTURE_FIXTURE
  BOOLEAN hidden=(GpuGone && (h==H(PRIMARY) || h==H(SIBLING)) &&
    (GuGuidEqual(guid,&GopGuid) || GuGuidEqual(guid,&PathGuid) || GuGuidEqual(guid,&EdidGuid))) ||
    (GpuGone && h==H(AGG) && GuGuidEqual(guid,&GopGuid) && (!(Mode>=6 && Mode<=11) || Mode==8));
  if(!*out && !hidden && !GuGuidEqual(guid,&GopGuid))for(UINTN i=0;i<sizeof(FixtureInstalledProtocols)/sizeof(FixtureInstalledProtocols[0]);i++)
    if(FixtureInstalledProtocols[i].Handle==h && GuGuidEqual(guid,&FixtureInstalledProtocols[i].Guid))*out=&PrivateGuid;
#endif
  return *out?EFI_SUCCESS:EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI FindPci(EFI_GUID *guid,EFI_DEVICE_PATH_PROTOCOL **path,EFI_HANDLE *handle) {
  Check(GuGuidEqual(guid,&PciGuid),"PCI-only lookup");
  BOOLEAN fault=(VOID *)*path==FaultDisplayPath;
  BOOLEAN known=(VOID *)*path==DisplayPaths[0] || (TEST_CHILDREN==2 && (VOID *)*path==DisplayPaths[1]) || fault;
  if(!known)return EFI_UNSUPPORTED;*handle=H(PCI);
  *path=(VOID *)((UINT8 *)*path+(fault?FaultControllerBytes:TEST_CONTROLLER_BYTES)-(ParentDrift?6:0));return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Location(EFI_PCI_IO_PROTOCOL *io,UINTN *seg,UINTN *bus,UINTN *dev,UINTN *fun) {
  Check(io==&Io,"exact PCI interface");
#ifdef GU_AGGREGATE_CAPTURE_FIXTURE
  *seg=FX_PCI_SEGMENT;*bus=FX_PCI_BUS;*dev=FX_PCI_DEVICE;*fun=FX_PCI_FUNCTION;
#else
  *seg=GU_TARGET_SEGMENT;*dev=*fun=0;*bus=GU_TARGET_BUS;
#endif
  if(SegmentDrift)*seg^=1U;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Read(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH width,UINT32 at,UINTN count,VOID *out) {
  Check(io==&Io && width==EfiPciIoWidthUint32 && at==0 && (count==16 || count==64),"bounded PCI snapshot");
  memcpy(out,Config,count*4);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI MemRead(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH width,UINT8 bar,UINT64 at,UINTN count,VOID *out) {
  Check(io==&Io && !bar,"exact BAR0 read");
  if(width==EfiPciIoWidthUint8 && at>=0x300000 && at<=0x400000 && count<=0x400000-at) {
    memcpy(out,PromMemory+(UINTN)at-0x300000,count);return EFI_SUCCESS;
  }
  Check(width==EfiPciIoWidthUint32 && count==1,"cold dword only");
  const UINT32 values[]={GU_CHIP_ID,0x8b8f,0x3ff,0x10,0,0xffffff8f,0x03000062,6,GU_DEVICE_MEMORY_SIZE};
#ifdef GU_AGGREGATE_CAPTURE_FIXTURE
  (void)values;
  for(UINTN i=0;i<9;i++)if(at==FixtureColdOffsets[i]){*(UINT32 *)out=FixtureCold[i];return EFI_SUCCESS;}
#else
  for(UINTN i=0;i<9;i++)if(at==ColdOffsets[i]){*(UINT32 *)out=values[i];return EFI_SUCCESS;}
#endif
  Check(FALSE,"unknown register read");return EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI Bar(EFI_PCI_IO_PROTOCOL *io,UINT8 index,UINT64 *supports,VOID **out) {
  (void)supports;Check(io==&Io && !index,"BAR0 resource only");
  UINT8 *bytes=calloc(1,sizeof(EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR)+2);*out=bytes;
#ifdef GU_AGGREGATE_CAPTURE_FIXTURE
  Check(sizeof(FixtureBar0Resource)==sizeof(EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR)+2,"complete resource observation");
  memcpy(bytes,FixtureBar0Resource,sizeof(FixtureBar0Resource));
#else
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *d=(VOID *)bytes;d->Desc=ACPI_ADDRESS_SPACE_DESCRIPTOR;
  d->Len=sizeof(*d)-3;d->AddrSpaceGranularity=32;d->AddrRangeMin=GU_BAR0_BASE;
  d->AddrRangeMax=0xffffff;d->AddrLen=0x1000000;bytes[sizeof(*d)]=ACPI_END_TAG_DESCRIPTOR;
#endif
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Attr(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION op,UINT64 mask,UINT64 *out) {
  Check(io==&Io,"exact attributes target");
  if(op==EfiPciIoAttributeOperationGet){*out=Attributes;return EFI_SUCCESS;}
  if(op==EfiPciIoAttributeOperationSupported){*out=0x700;return EFI_SUCCESS;}
  Check(op==EfiPciIoAttributeOperationEnable && mask && !(mask&~0x300ULL) && Full.Owned,"IO/MEM-only restore under claim");
  Enables++;Attributes|=mask;Config[1]|=(mask&0x100?1U:0U)|(mask&0x200?2U:0U);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Open(EFI_HANDLE h,EFI_GUID *guid,VOID **out,EFI_HANDLE agent,EFI_HANDLE controller,UINT32 attr) {
  Check(h==H(PCI) && GuGuidEqual(guid,&PciGuid) && agent==H(APP) && !controller && attr==EFI_OPEN_PROTOCOL_EXCLUSIVE,"exact exclusive claim");
  Claims++;*out=&Io;Edge(PCI,APP,0,guid,attr);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Close(EFI_HANDLE h,EFI_GUID *guid,EFI_HANDLE agent,EFI_HANDLE controller) {
  (void)controller;Check(h==H(PCI) && agent==H(APP),"exact exclusive close");Closes++;Drop(PCI,guid);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Disconnect(EFI_HANDLE h,EFI_HANDLE agent,EFI_HANDLE child) {
  Check(!child,"no arbitrary child request");Calls++;
  if(agent==H(EDID)) {
    Check(h==H(EdidCalls?SIBLING:PRIMARY) && EdidCalls<TEST_CHILDREN && !NvCalls,"EDID child order before NVIDIA");
    EdidCalls++;
    if(Mode==1 || Mode==2 || Mode==14)ScenarioReached=TRUE;
    if(Mode==1)return EFI_DEVICE_ERROR;if(Mode==2)return EFI_WARN_UNKNOWN_GLYPH;
    if(Mode==14)return EFI_SUCCESS; /* Successful Stop that leaves the required edge. */
    Drop((UINTN)h,&EdidGuid);return EFI_SUCCESS;
  }
  Check(agent==H(NV) && h==H(PCI) && EdidCalls==TEST_CHILDREN && !NvCalls,"NVIDIA only after all EDID; never console driver Stop");
  NvCalls++;if(Mode>=3)ScenarioReached=TRUE;
  if(Mode==3)return EFI_DEVICE_ERROR;
  UINTN keep=0;
  for(UINTN i=0;i<Live.Count;i++)if(!(Live.Edges[i].Handle==H(PCI) && Live.Edges[i].Agent==H(NV)))Live.Edges[keep++]=Live.Edges[i];
  Live.Count=keep;
  Drop(PRIMARY,&GopGuid);Drop(PRIMARY,&EdidGuid);
  if(Mode==4)return EFI_DEVICE_ERROR;
  if(Mode!=6)Drop(AGG,&GopGuid);
  Drop(AGG,&TextGuid);
  if(Mode==9)AggregateMode.FrameBufferBase=1;
  if(Mode==11)Edge(AGG,OBSERVER,AGG,&GopGuid,16);
  if(Mode==12)Edge(PCI,NV,PRIMARY,&PathGuid,8);
  if(Mode==13)Drop(PROTECTED,&PrivateGuid);
  GpuGone=TRUE;Config[1]&=~3U;Attributes&=~0x300ULL;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Reset(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *input,BOOLEAN extended) {(void)input;(void)extended;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Key(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *input,EFI_INPUT_KEY *key) {(void)input;(void)key;return EFI_NOT_READY;}
static EFI_STATUS EFIAPI Output(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *text,CHAR16 *message) {(void)text;(void)message;Check(FALSE,"headless output forbidden");return EFI_DEVICE_ERROR;}
static EFI_STATUS Log(VOID *context,const char *event,EFI_STATUS status) {
  (void)context;
  if(Mode==5 && !strcmp(event,"headless-console-pre-release-verification")) {
    ScenarioReached=TRUE;Edge(PROTECTED,COMBO,PROTECTED,&PrivateGuid,16);
  }
  if(status!=EFI_SUCCESS) {
    if(!FirstError)FirstError=event;
    fprintf(stderr,"aggregate event=%s status=%llx\n",event,(unsigned long long)status);
  }
  return EFI_SUCCESS;
}
static void Setup(void) {
  memset(&Boot,0,sizeof(Boot));memset(&Table,0,sizeof(Table));memset(&Io,0,sizeof(Io));Live.Count=0;
  Calls=EdidCalls=NvCalls=Enables=Claims=Closes=0;GpuGone=ScenarioReached=FALSE;
  ParentDrift=PathFault=SegmentDrift=FALSE;memset(FaultDisplayPath,0,sizeof(FaultDisplayPath));
  FirstError=NULL;memset(NvMemory,0,sizeof(NvMemory));memset(EdidMemory,0,sizeof(EdidMemory));
  Boot.AllocatePool=Allocate;Boot.FreePool=Free;Boot.RaiseTPL=Raise;Boot.RestoreTPL=Restore;
  Boot.LocateHandleBuffer=Locate;Boot.ProtocolsPerHandle=Protocols;Boot.OpenProtocolInformation=Opens;
  Boot.HandleProtocol=Handle;Boot.LocateDevicePath=FindPci;Boot.DisconnectController=Disconnect;
  Boot.OpenProtocol=Open;Boot.CloseProtocol=Close;Table.BootServices=&Boot;
  Table.ConsoleOutHandle=H(CONSOLE);Table.StandardErrorHandle=H(STDERR_HANDLE);Table.ConsoleInHandle=H(INPUT);
  TextMode=(EFI_SIMPLE_TEXT_OUTPUT_MODE){0};Text=(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL){0};Text.Mode=&TextMode;Text.OutputString=Output;
  ErrorText=Text;
  Input=(EFI_SIMPLE_TEXT_INPUT_PROTOCOL){Reset,Key,H(1)};Table.ConOut=&Text;
  Table.StdErr=(STDERR_HANDLE==CONSOLE || STDERR_HANDLE==AGG)?&Text:&ErrorText;Table.ConIn=&Input;
  Io.GetLocation=Location;Io.Pci.Read=Read;Io.Mem.Read=MemRead;Io.GetBarAttributes=Bar;Io.Attributes=Attr;
  Info=(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION){0};Info.HorizontalResolution=1920;Info.VerticalResolution=1080;Info.PixelsPerScanLine=1920;
  PhysicalMode=(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE){0};PhysicalMode.MaxMode=1;PhysicalMode.Info=&Info;PhysicalMode.SizeOfInfo=sizeof(Info);
  PhysicalMode.FrameBufferBase=0xfcc0000000ULL;PhysicalMode.FrameBufferSize=1920*1080*4;Physical.Mode=&PhysicalMode;
  AggregateMode=(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE){0};Aggregate.Mode=&AggregateMode;
  memcpy(DisplayPaths[0],RtxPath,sizeof(RtxPath));memcpy(DisplayPaths[1],RtxPath,sizeof(RtxPath));DisplayPaths[1][TEST_CONTROLLER_BYTES+4]^=0x30;
  memcpy(ControllerPath,RtxPath,TEST_CONTROLLER_BYTES);memcpy(ControllerPath+TEST_CONTROLLER_BYTES,(UINT8[]){127,255,4,0},4);
  memset(Config,0,sizeof(Config));Config[0]=GU_BOARD_ID;Config[1]=0x00100003;Config[2]=0x030000a1;
  Config[4]=GU_BAR0_BASE;Config[11]=GU_BOARD_SUBSYSTEM;Config[13]=0x40;
  Config[16]=0x00034801;Config[18]=0x00020010;Config[19]=1U<<28;Attributes=0x300;
#ifdef GU_AGGREGATE_CAPTURE_FIXTURE
  const UINT8 captured_path[]={FX_PRIMARY_PATH_BYTES};
  Check(sizeof(captured_path)==sizeof(RtxPath) && !memcmp(captured_path,RtxPath,sizeof(RtxPath)),"captured primary path matches profile");
  Check(FX_DISPLAY_PATH_BYTES==sizeof(RtxPath) && FX_CONTROLLER_BYTES==TEST_CONTROLLER_BYTES,"captured path lengths match profile and parent consumption");
  memcpy(DisplayPaths[0],captured_path,sizeof(DisplayPaths[0]));
#if TEST_CHILDREN==2
  const UINT8 sibling_path[]={FX_EDID_CHILD_PATH_BYTES};
  Check(sizeof(sibling_path)==sizeof(DisplayPaths[1]),"captured sibling length matches profile");
  memcpy(DisplayPaths[1],sibling_path,sizeof(DisplayPaths[1]));
#endif
  memcpy(ControllerPath,captured_path,TEST_CONTROLLER_BYTES);memcpy(Config,FixtureFullRtxConfig,sizeof(Config));
  Check(!memcmp(Config,FixtureRtxConfig,sizeof(FixtureRtxConfig)),"complete config matches graph PCI snapshot");
  Check(FixtureAmdConfig[0]!=Config[0],"unrelated adapter is not the target");
  Attributes=FX_PCI_ATTRIBUTES;
  PhysicalMode.FrameBufferBase=FX_RTX_FRAMEBUFFER;PhysicalMode.FrameBufferSize=FX_RTX_FRAMEBUFFER_BYTES;
  PhysicalMode.Mode=FX_RTX_MODE;PhysicalMode.MaxMode=FX_RTX_MAX_MODE;PhysicalMode.SizeOfInfo=FX_RTX_INFO_BYTES;
  Info.HorizontalResolution=FX_RTX_WIDTH;Info.VerticalResolution=FX_RTX_HEIGHT;Info.PixelsPerScanLine=FX_RTX_STRIDE;Info.PixelFormat=FX_RTX_PIXEL_FORMAT;
#endif
  VOID *memory[]={NvMemory,EdidMemory,ComboMemory,GfxMemory,OutMemory};
  UINTN sizes[]={sizeof(NvMemory),sizeof(EdidMemory),sizeof(ComboMemory),sizeof(GfxMemory),sizeof(OutMemory)};
  UINTN stops[]={GU_NVGOP_STOP,GU_EDID_STOP,GU_COMBO_STOP,GU_CONSOLE_GFX_STOP,GU_CONSOLE_TEXT_STOP};
  UINT32 versions[]={GU_NVGOP_VERSION,1,GU_COMBO_VERSION,GU_CONSOLE_GFX_VERSION,GU_CONSOLE_TEXT_VERSION};
  const EFI_GUID *files[]={NULL,&EdidFile,&ComboFile,&GfxFile,&OutFile};
  for(UINTN i=0;i<5;i++) {
    memset(&Bindings[i],0,sizeof(Bindings[i]));memset(&Images[i],0,sizeof(Images[i]));memset(DriverPaths[i],0,32);
    Bindings[i].DriverBindingHandle=H(DriverHandle(i));Bindings[i].ImageHandle=H(ImageHandle(i));Bindings[i].Version=versions[i];
    Bindings[i].Stop=(EFI_DRIVER_BINDING_STOP)((UINT8 *)memory[i]+stops[i]);
    Images[i].ImageBase=memory[i];Images[i].ImageSize=sizes[i];Images[i].ImageCodeType=i==1?EfiRuntimeServicesCode:EfiBootServicesCode;
    Images[i].DeviceHandle=H(PCI);UINT8 *path=DriverPaths[i];
    if(!i){memcpy(path,(UINT8[]){4,4,28,0},4);memcpy(path+4,L"nvgop-ga1xx",24);memcpy(path+28,(UINT8[]){127,255,4,0},4);}
    else {memcpy(path,(UINT8[]){4,6,20,0},4);memcpy(path+4,files[i],16);memcpy(path+20,(UINT8[]){127,255,4,0},4);}
    Images[i].FilePath=(VOID *)path;
  }
#ifdef GU_AGGREGATE_CAPTURE_FIXTURE
  const UINTN captured_sizes[]={FX_NV_IMAGE_SIZE,FX_EDID_IMAGE_SIZE,FX_COMBO_IMAGE_SIZE,FX_GFX_IMAGE_SIZE,FX_OUT_IMAGE_SIZE};
  const UINTN captured_stops[]={FX_NV_STOP,FX_EDID_STOP,FX_COMBO_STOP,FX_GFX_STOP,FX_OUT_STOP};
  const UINT32 captured_versions[]={FX_NV_VERSION,FX_EDID_VERSION,FX_COMBO_VERSION,FX_GFX_VERSION,FX_OUT_VERSION};
  const UINT32 captured_types[]={FX_NV_CODE_TYPE,FX_EDID_CODE_TYPE,FX_COMBO_CODE_TYPE,FX_GFX_CODE_TYPE,FX_OUT_CODE_TYPE};
  EFI_HANDLE captured_devices[]={FX_NV_DEVICE_HANDLE,FX_EDID_DEVICE_HANDLE,FX_COMBO_DEVICE_HANDLE,FX_GFX_DEVICE_HANDLE,FX_OUT_DEVICE_HANDLE};
  for(UINTN i=0;i<5;i++)Check(sizes[i]==captured_sizes[i] && stops[i]==captured_stops[i] &&
    versions[i]==captured_versions[i] && (UINT32)Images[i].ImageCodeType==captured_types[i],"capture driver metadata matches profile");
  for(UINTN i=0;i<5;i++)Images[i].DeviceHandle=captured_devices[i];
  Check(sizeof(FixtureEdges)<=sizeof(Live.Edges),"bounded captured graph");
  memcpy(Live.Edges,FixtureEdges,sizeof(FixtureEdges));Live.Count=sizeof(FixtureEdges)/sizeof(FixtureEdges[0]);
#else
  Edge(PCI,NV,PCI,&PciGuid,16);Edge(PCI,NV,PCI,&PrivateGuid,16);Edge(PCI,NV,PRIMARY,&PathGuid,8);
  if(TEST_CHILDREN==2){Edge(PCI,NV,SIBLING,&PathGuid,8);Edge(SIBLING,EDID,SIBLING,&EdidGuid,16);}
  Edge(PRIMARY,EDID,PRIMARY,&EdidGuid,16);Edge(PRIMARY,COMBO,PRIMARY,&GopGuid,16);Edge(PRIMARY,COMBO,AGG,&GopGuid,8);
  Edge(AGG,GFX,AGG,&GopGuid,16);Edge(AGG,TEXT_DRIVER,AGG,&TextGuid,16);Edge(AGG,TEXT_DRIVER,CONSOLE,&TextGuid,8);
#endif
  Edge(PROTECTED,OBSERVER,PROTECTED,&PrivateGuid,16);
}
static void LoadExact(const char *path,VOID *out,UINTN bytes) {
  FILE *file=fopen(path,"rb");Check(file!=NULL,"input image exists");
  Check(fread(out,1,bytes,file)==bytes && fgetc(file)==EOF,"input image size mismatch");
  Check(!fclose(file),"input image close");
}
int main(int argc,char **argv) {
#if defined(GU_PROFILE_LOCAL)
  Check(argc==4,"LOCAL replay requires captured EDID, NVIDIA and PROM images");
#else
  Check(argc==1 || argc==4,"optional EDID, NVIDIA and PROM images");
#endif
  for(Mode=0;Mode<24;Mode++) {
    Setup();
    if(argc==4) {
      LoadExact(argv[1],EdidMemory,sizeof(EdidMemory));LoadExact(argv[2],NvMemory,sizeof(NvMemory));
      LoadExact(argv[3],PromMemory,sizeof(PromMemory));
    }
    EFI_STATUS status=GuPrepareConsole(&Table,Log,NULL);
    Check(status==EFI_SUCCESS && !Calls,"aggregate prepare performs zero detach calls");
    if(Mode>=15) {
      ScenarioReached=TRUE;
      if(Mode==15)NvMemory[0x280]^=1;
      if(Mode==16)EdidMemory[0x1000]^=1;
      if(Mode==17)DisplayPaths[0][TEST_CONTROLLER_BYTES+4]^=1;
      if(Mode==18)Bindings[2].Stop=(EFI_DRIVER_BINDING_STOP)(ComboMemory+GU_COMBO_STOP+1);
      if(Mode==19)ParentDrift=TRUE;
      if(Mode==20 || Mode==21)ChangePathLength(Mode==21);
      if(Mode==22)DisplayPaths[0][16]^=1;
      if(Mode==23)SegmentDrift=TRUE;
    }
    status=GuReleaseGop(&Table,H(APP),Log,NULL);
    if(!Mode || Mode==6) {
      Check(status==EFI_SUCCESS && Calls==TEST_CHILDREN+1 && EdidCalls==TEST_CHILDREN && NvCalls==1,"complete production release");
      Check(Enables==1 && Claims==1 && Closes==1 && !Full.Owned && (Config[1]&7)==
#ifdef GU_AGGREGATE_CAPTURE_FIXTURE
        (FixtureFullRtxConfig[1]&7),
#else
        3,
#endif
        "IO/MEM restore, BME off and closed claim");
      Check(GuVerifyReleased(&Table,TRUE)==EFI_SUCCESS,"released state remains valid");
#ifdef GU_AGGREGATE_CAPTURE_FIXTURE
      Check(Attributes==FX_PCI_ATTRIBUTES,"all captured attribute bits are preserved");
#endif
    } else {
      Check(ScenarioReached && status!=EFI_SUCCESS && !Enables && !Claims && !Closes,
            "injected failure reached; no restore/claim on rejected release");
      unsigned expected_edid=(Mode==5 || Mode>=15)?0:((Mode==1 || Mode==2 || Mode==14)?1:TEST_CHILDREN);
      Check(EdidCalls==expected_edid && NvCalls==((Mode==1 || Mode==2 || Mode==5 || Mode==14 || Mode>=15)?0U:1U),
            "failure occurs at the intended detach stage");
      if(Mode==15 || Mode==16)Check(status==EFI_CRC_ERROR && FirstError &&
         !strcmp(FirstError,Mode==15?"nvidia-code-profile":"ami-runtime-code-profile"),"corrupt code rejected at exact fingerprint gate");
    }
  }
  printf("PASS aggregate full prepare/release: %u checks, %u display children, 24 scenarios; RAM-only\n",Checks,TEST_CHILDREN);
  return 0;
}
