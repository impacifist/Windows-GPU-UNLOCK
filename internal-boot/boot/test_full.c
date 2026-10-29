/* ASRock port changes: 2026-10-27; publication notice: 2026-10-28. GPL-2.0-only; see CHANGES.md. */
/* GPL-2.0-only. Actual ownership graph and ROM/driver fixtures, host RAM only. */
#define GU_FULL_REPLAY_TEST
#define main ConsoleSuiteMain
#include "test_console.c"
#undef main
#include "full-flow.h"

#if GU_NO_EDID_DRIVER
/* This firmware binds no EDID driver: only the NVIDIA GOP driver image exists to replay. The
 * loaded-image size comes from the profile, and nvgop itself installs the EdidDiscovered
 * interface on each display child, which EdidInterfaceMark stands in for. */
static UINT8 NvMemory[GU_NVGOP_IMAGE_SIZE],PromMemory[1048576],EdidInterfaceMark[8];
static EFI_DRIVER_BINDING_PROTOCOL NvBinding;
static EFI_LOADED_IMAGE_PROTOCOL NvImage;
static UINT8 NvPath[32];
#else
static UINT8 MouseMemory[GU_EDID_IMAGE_SIZE],NvMemory[GU_NVGOP_IMAGE_SIZE],PromMemory[1048576];
static EFI_DRIVER_BINDING_PROTOCOL MouseBinding,NvBinding;
static EFI_LOADED_IMAGE_PROTOCOL MouseImage,NvImage;
static UINT8 MousePath[24],NvPath[32];
#endif
static UINT32 InitialFullConfig[64],MockConfig[64];
static UINT64 MockAttributes;
static int FullScenario,EnableCalls,ClaimCalls,CloseCalls,Phase,ColdCalls;
#ifdef GU_HEADLESS_RESEARCH
static UINT8 SecondChildPath[GU_RTX_DISPLAY_BYTES];
static BOOLEAN PhaseForHeadlessTest(VOID) {return Phase>=4;}
#endif
static const UINT32 MockCold[]={GU_CHIP_ID,0x8b8f,0x3ff,0x10,0,0xffffff8f,0x03000062,6,GU_DEVICE_MEMORY_SIZE};

#ifndef GU_PROFILE_LOCAL
static VOID LoadMapped(const char *path,UINT8 *memory,UINTN size) {
  FILE *f=fopen(path,"rb");Check(f!=NULL,"driver fixture exists");
  fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);UINT8 *data=malloc(n);
  Check(data && fread(data,1,n,f)==(UINTN)n,"read driver fixture");fclose(f);
  UINT32 pe=*(UINT32 *)(data+0x3c);UINT16 count=*(UINT16 *)(data+pe+6),optional=*(UINT16 *)(data+pe+20);
  memset(memory,0,size);
  for(UINTN i=0;i<count;i++) {
    UINT8 *sec=data+pe+24+optional+i*40;
    UINT32 va=*(UINT32 *)(sec+12),bytes=*(UINT32 *)(sec+16),at=*(UINT32 *)(sec+20);
    Check((UINT64)va+bytes<=size && (UINT64)at+bytes<=(UINT64)n,"bounded PE section");memcpy(memory+va,data+at,bytes);
  }
  free(data);
}
#endif
#if !GU_NO_EDID_DRIVER
/* Probe captures are loaded-image dumps; legacy fixtures may be PE files. */
static VOID LoadDriverFixture(const char *path,UINT8 *memory,UINTN size) {
  FILE *file=fopen(path,"rb");Check(file!=NULL,"captured driver fixture exists");
  Check(!fseek(file,0,SEEK_END),"driver fixture seek");long length=ftell(file);rewind(file);
  if(length>=0 && (UINTN)length==size) {
    Check(fread(memory,1,size,file)==size && fgetc(file)==EOF,"read exact captured loaded driver image");fclose(file);
  } else {
    fclose(file);
#ifdef GU_PROFILE_LOCAL
    Check(FALSE,"captured loaded driver image size mismatch");
#else
    LoadMapped(path,memory,size);
#endif
  }
}
#endif
static EFI_STATUS ExtraHandle(EFI_HANDLE h,EFI_GUID *g,VOID **out) {
#ifdef GU_HEADLESS_RESEARCH
  if(h==FX_EdidChild && GuGuidEqual(g,&PathGuid) && Phase<4){*out=SecondChildPath;return EFI_SUCCESS;}
#if GU_NO_EDID_DRIVER
  if((h==FX_Rtx || h==FX_EdidChild) && GuGuidEqual(g,&EdidGuid) && Phase<4){*out=EdidInterfaceMark;return EFI_SUCCESS;}
#else
  if((h==FX_Rtx || h==FX_EdidChild) && GuGuidEqual(g,&EdidGuid) && Phase<4){*out=&MouseBinding;return EFI_SUCCESS;}
#endif
#endif
#if GU_NO_EDID_DRIVER
  if(GuGuidEqual(g,&BindingGuid) && h==FX_NvAgent){*out=&NvBinding;return EFI_SUCCESS;}
  if(GuGuidEqual(g,&ImageGuid) && h==FX_NvImage){*out=&NvImage;return EFI_SUCCESS;}
#else
  if(GuGuidEqual(g,&BindingGuid) && (h==FX_NvAgent || h==FX_EdidAgent)){*out=h==FX_NvAgent?(VOID *)&NvBinding:(VOID *)&MouseBinding;return EFI_SUCCESS;}
  if(GuGuidEqual(g,&ImageGuid) && (h==FX_NvImage || h==FX_EdidImage)){*out=h==FX_NvImage?(VOID *)&NvImage:(VOID *)&MouseImage;return EFI_SUCCESS;}
#endif
  if(h==FX_Rtx && Phase>=4 && (GuGuidEqual(g,&GopGuid) || GuGuidEqual(g,&PathGuid))){*out=NULL;return EFI_UNSUPPORTED;}
  return EFI_NOT_FOUND;
}
static EFI_STATUS ExtraRead(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH width,UINT32 at,UINTN n,VOID *data) {
  Check(width==EfiPciIoWidthUint32 && at==0 && (n==16 || (io==&RtxPci && n==64)),"only bounded config reads");
  memcpy(data,io==&RtxPci?MockConfig:FixtureAmdConfig,n*4);
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI MemRead(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT8 bar,UINT64 offset,UINTN count,VOID *dst) {
  Check(io==&RtxPci && bar==0,"only exact target BAR0 read");
  if(w==EfiPciIoWidthUint8 && offset>=0x300000 && offset+count<=0x400000) {
    memcpy(dst,PromMemory+offset-0x300000,count);
    if(FullScenario==10 && Phase==4 && count)((UINT8 *)dst)[0]^=1;
    return EFI_SUCCESS;
  }
  Check(w==EfiPciIoWidthUint32 && count==1,"only pinned cold dword reads");
  for(UINTN i=0;i<9;i++)if(offset==ColdOffsets[i]) {
    *(UINT32 *)dst=MockCold[i];ColdCalls++;
    if(FullScenario==11 && Phase==4 && i==7)*(UINT32 *)dst^=1;
    /* Phase 3 follows the EDID stage; without an EDID driver the last cold read before the NV stop is phase 2. */
    if(FullScenario==19 && Phase==(GU_NO_EDID_DRIVER?2:3) && i==8)Live.Edges[Live.Count++]=(EDGE){FX_RtxPci,FX_NvAgent,FX_Input,PathGuid,8,1};
#ifdef GU_HEADLESS_RESEARCH
    if(FullScenario==32 && Phase==3 && i==8)RtxInfo.HorizontalResolution--;
#endif
    return EFI_SUCCESS;
  }
  abort();
}
static EFI_STATUS EFIAPI Bar(EFI_PCI_IO_PROTOCOL *io,UINT8 index,UINT64 *supports,VOID **resources) {
  (void)supports;Check(io==&RtxPci && index==0,"only BAR0 descriptor");
  UINT8 *raw=calloc(1,sizeof(EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR)+2);*resources=raw;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *d=(VOID *)raw;
  d->Desc=ACPI_ADDRESS_SPACE_DESCRIPTOR;d->Len=sizeof(*d)-3;d->AddrSpaceGranularity=32;
  d->AddrRangeMin=GU_BAR0_BASE;d->AddrRangeMax=0x00ffffff;d->AddrLen=0x1000000;raw[sizeof(*d)]=ACPI_END_TAG_DESCRIPTOR;
  if(FullScenario==12 && Phase==4)d->AddrRangeMin++;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Attributes(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION op,UINT64 mask,UINT64 *result) {
  Check(io==&RtxPci,"attributes only on exact target");
  if(op==EfiPciIoAttributeOperationGet) {
    *result=MockAttributes;
    if(FullScenario==6 && Phase==4)*result|=EFI_PCI_IO_ATTRIBUTE_BUS_MASTER;
    return EFI_SUCCESS;
  }
  if(op==EfiPciIoAttributeOperationSupported){*result=FullScenario==7?0:0x700;return EFI_SUCCESS;}
  Check(op==EfiPciIoAttributeOperationEnable && mask && !(mask&~(EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY)),"only missing IO/MEM enable; never BME/SET/DEVICE_ENABLE");
  Check(Full.Owned,"exclusive held for PCI write");EnableCalls++;
  MockAttributes|=mask;if(mask&EFI_PCI_IO_ATTRIBUTE_IO)MockConfig[1]|=1;if(mask&EFI_PCI_IO_ATTRIBUTE_MEMORY)MockConfig[1]|=2;
  if(FullScenario==8)return EFI_DEVICE_ERROR; /* partial effect followed by error */
  if(FullScenario==9)MockAttributes|=EFI_PCI_IO_ATTRIBUTE_BUS_MASTER;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Open(EFI_HANDLE h,EFI_GUID *g,VOID **out,EFI_HANDLE agent,EFI_HANDLE controller,UINT32 attrs) {
  ClaimCalls++;
  Check(h==FX_RtxPci && GuGuidEqual(g,&PciGuid) && agent==(VOID *)9999 && !controller && attrs==EFI_OPEN_PROTOCOL_EXCLUSIVE,"claim only exact controller");
  if(FullScenario==4)return EFI_ACCESS_DENIED;
  *out=&RtxPci;Live.Edges[Live.Count++]=(EDGE){h,agent,NULL,PciGuid,EFI_OPEN_PROTOCOL_EXCLUSIVE,1};return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Close(EFI_HANDLE h,EFI_GUID *g,EFI_HANDLE agent,EFI_HANDLE controller) {
  (void)controller;CloseCalls++;
  if(FullScenario==13)return EFI_DEVICE_ERROR;
  Drop(h,agent,g);return EFI_SUCCESS;
}
static EFI_STATUS ExtraDisconnect(EFI_HANDLE h,EFI_HANDLE agent,EFI_HANDLE child) {
  Check(child==NULL,"no arbitrary child selection");Calls++;
#ifdef GU_HEADLESS_RESEARCH
#if GU_NO_EDID_DRIVER
  {
    /* Calls 1 and 2 were the console drivers; with no EDID driver the next stop is NVIDIA's. */
    Check(Calls==3 && h==FX_RtxPci && agent==FX_NvAgent,"NV stop only exact GPU/binding; this firmware has no EDID stage");
    if(FullScenario==3)return EFI_DEVICE_ERROR;
    if(FullScenario==30){for(UINTN i=0;i<Live.Count;i++)if(Live.Edges[i].Handle==h && GuGuidEqual(&Live.Edges[i].Protocol,&PathGuid)){Live.Edges[i]=Live.Edges[--Live.Count];break;}return EFI_DEVICE_ERROR;}
    Drop(h,agent,&PciGuid);Drop(h,agent,&PathGuid);Phase=4;
    MockConfig[1]&=~3U;MockAttributes&=~0x700ULL;
    if(FullScenario==5)MockConfig[4]^=0x1000000;
    if(FullScenario==14)Live.Edges[0].Count++;
  }
#else
  if(agent==FX_EdidAgent) {
    Check(Calls==3 || (Full.ChildCount==2 && Calls==4),"EDID stops follow console detach in child order");
    Check(h==(Calls==3?FX_Rtx:FX_EdidChild) && agent==FX_EdidAgent,"only each reviewed EDID child stopped");
    if(FullScenario==1 || (FullScenario==2 && (UINTN)Calls==2+Full.ChildCount))return EFI_DEVICE_ERROR;
    if(FullScenario==29)return EFI_WARN_UNKNOWN_GLYPH;
    Drop(h,agent,&EdidGuid);Phase=3;
  }else {
    Check((UINTN)Calls==3+Full.ChildCount && h==FX_RtxPci && agent==FX_NvAgent,"NV stop only after all reviewed EDID stops");
    if(FullScenario==3)return EFI_DEVICE_ERROR;
    if(FullScenario==30){for(UINTN i=0;i<Live.Count;i++)if(Live.Edges[i].Handle==h && GuGuidEqual(&Live.Edges[i].Protocol,&PathGuid)){Live.Edges[i]=Live.Edges[--Live.Count];break;}return EFI_DEVICE_ERROR;}
    Drop(h,agent,&PciGuid);Drop(h,agent,&PathGuid);Phase=4;
    MockConfig[1]&=~3U;MockAttributes&=~0x700ULL;
    if(FullScenario==5)MockConfig[4]^=0x1000000;
    if(FullScenario==14)Live.Edges[0].Count++;
  }
#endif
#else
  if(Calls==3) {
    Check(h==FX_Rtx && agent==FX_EdidAgent,"EDID stop only RTX child/binding");
    if(FullScenario==1)return EFI_DEVICE_ERROR;
    Drop(h,agent,&EdidGuid);Phase=3;
    if(FullScenario==2)Drop(FX_Amd,FX_EdidAgent,&EdidGuid);
  } else {
    Check(Calls==4 && h==FX_RtxPci && agent==FX_NvAgent,"NV stop only exact GPU/binding");
    if(FullScenario==3)return EFI_DEVICE_ERROR;
    Drop(h,agent,&PciGuid);Drop(h,agent,&PathGuid);Phase=4;
    MockConfig[1]&=~3U;MockAttributes&=~0x700ULL;
    if(FullScenario==5)MockConfig[4]^=0x1000000;
    if(FullScenario==14)Live.Edges[0].Count++;
  }
#endif
  return EFI_SUCCESS;
}
static EFI_STATUS FullLog(VOID *ctx,const char *event,EFI_STATUS status) {
  (void)ctx;(void)status;
#ifdef GU_HEADLESS_RESEARCH
  if(status!=EFI_SUCCESS)fprintf(stderr,"full event=%s status=%llx scenario=%d\n",event,(unsigned long long)status,FullScenario);
  if(!strcmp(event,"headless-console-pre-release-verification")) {
    if(FullScenario==23)Live.Edges[Live.Count++]=(EDGE){FX_RtxPci,FX_NvAgent,FX_Input,PathGuid,8,1};
    if(FullScenario==24) {
      EFI_HANDLE child=FX_EdidChild?FX_EdidChild:FX_Rtx;
      Live.Edges[Live.Count++]=(EDGE){child,FX_OutAgent,child,TextGuid,16,1};
    }
    if(FullScenario==27)return EFI_WARN_UNKNOWN_GLYPH;
#if GU_NO_EDID_DRIVER
    /* An EDID driver that appears on a display child after the console was prepared is unreviewed. */
    if(FullScenario==33)Live.Edges[Live.Count++]=(EDGE){FX_Rtx,FX_OutAgent,FX_Rtx,EdidGuid,EFI_OPEN_PROTOCOL_BY_DRIVER,1};
    if(FullScenario==34) {
      EFI_HANDLE child=FX_EdidChild?FX_EdidChild:FX_Rtx;
      Live.Edges[Live.Count++]=(EDGE){child,FX_OutAgent,child,EdidGuid,EFI_OPEN_PROTOCOL_BY_DRIVER,1};
    }
#else
    if(FullScenario==28)Drop(FX_EdidChild?FX_EdidChild:FX_Rtx,FX_EdidAgent,&EdidGuid);
#endif
  }
  if(!strcmp(event,"detach-nvidia-gop-intent")) {
    if(FullScenario==25) {
      if(FX_EdidChild)SecondChildPath[sizeof(SecondChildPath)-8]^=1;
      else TestRtxPath[sizeof(TestRtxPath)-8]^=1;
    }
    if(FullScenario==26)NvMemory[0x280]^=1;
  }
#endif
  if(FullScenario==18 && !strcmp(event,"nvidia-detach-verified"))Live.Edges[Live.Count++]=(EDGE){FX_RtxPci,FX_Input,FX_RtxPci,Dummy,16,1};
  if(!strcmp(event,"pci-decode-restore-intent-io-mem-only")) {
    if(FullScenario==15)return EFI_DEVICE_ERROR;
    if(FullScenario==16)
#ifdef GU_HEADLESS_RESEARCH
      Keys.WaitForKey=NULL;
#else
      Drop(FX_Amd,FX_OutAgent,&TextGuid);
#endif
    if(FullScenario==20)Live.Edges[0].Count++;
  }
  return EFI_SUCCESS;
}
static EFI_STATUS RunFull(int scenario) {
  FullMode=FALSE;Check(Run(scenario==21?23:scenario==22?24:0)==EFI_SUCCESS,"hardware-validated Stage2A replay prerequisite");
  FullScenario=scenario;EnableCalls=ClaimCalls=CloseCalls=0;Phase=2;ColdCalls=0;
#ifdef GU_HEADLESS_RESEARCH
#ifdef FX_EDID_CHILD_PATH_BYTES
  const UINT8 recordedChildPath[]={FX_EDID_CHILD_PATH_BYTES};
  Check(sizeof(recordedChildPath)==sizeof(SecondChildPath),"recorded child path has compiled target length");
  memcpy(SecondChildPath,recordedChildPath,sizeof(recordedChildPath));
#else
  memcpy(SecondChildPath,TestRtxPath,sizeof(SecondChildPath));SecondChildPath[sizeof(SecondChildPath)-8]^=0x50;
#endif
#endif
  memcpy(MockConfig,InitialFullConfig,sizeof(MockConfig));
  MockAttributes=(MockConfig[1]&1U?EFI_PCI_IO_ATTRIBUTE_IO:0U)|(MockConfig[1]&2U?EFI_PCI_IO_ATTRIBUTE_MEMORY:0U);
#if GU_NO_EDID_DRIVER
  NvBinding=(EFI_DRIVER_BINDING_PROTOCOL){0};
  NvBinding.Version=GU_NVGOP_VERSION;NvBinding.DriverBindingHandle=NvBinding.ImageHandle=FX_NvAgent;NvBinding.Stop=(EFI_DRIVER_BINDING_STOP)(NvMemory+GU_NVGOP_STOP);
  NvImage=(EFI_LOADED_IMAGE_PROTOCOL){0};
  NvImage.ImageBase=NvMemory;NvImage.ImageSize=sizeof(NvMemory);NvImage.ImageCodeType=EfiBootServicesCode;NvImage.DeviceHandle=FX_RtxPci;
#else
  MouseBinding=(EFI_DRIVER_BINDING_PROTOCOL){0};NvBinding=MouseBinding;
  MouseBinding.Version=1;MouseBinding.DriverBindingHandle=MouseBinding.ImageHandle=FX_EdidAgent;MouseBinding.Stop=(EFI_DRIVER_BINDING_STOP)(MouseMemory+GU_EDID_STOP);
  NvBinding.Version=GU_NVGOP_VERSION;NvBinding.DriverBindingHandle=NvBinding.ImageHandle=FX_NvAgent;NvBinding.Stop=(EFI_DRIVER_BINDING_STOP)(NvMemory+GU_NVGOP_STOP);
  MouseImage=(EFI_LOADED_IMAGE_PROTOCOL){0};NvImage=MouseImage;
  MouseImage.ImageBase=MouseMemory;MouseImage.ImageSize=sizeof(MouseMemory);MouseImage.ImageCodeType=EfiRuntimeServicesCode;
  NvImage.ImageBase=NvMemory;NvImage.ImageSize=sizeof(NvMemory);NvImage.ImageCodeType=EfiBootServicesCode;NvImage.DeviceHandle=FX_RtxPci;
  memcpy(MousePath,(UINT8[]){4,6,20,0},4);memcpy(MousePath+4,&MouseFile,16);memcpy(MousePath+20,(UINT8[]){127,255,4,0},4);MouseImage.FilePath=(VOID *)MousePath;
#endif
  memcpy(NvPath,(UINT8[]){4,4,28,0},4);memcpy(NvPath+4,L"nvgop-ga1xx",24);memcpy(NvPath+28,(UINT8[]){127,255,4,0},4);NvImage.FilePath=(VOID *)NvPath;
  RtxPci.Mem.Read=MemRead;RtxPci.GetBarAttributes=Bar;RtxPci.Attributes=Attributes;
  LastTable->BootServices->OpenProtocol=Open;LastTable->BootServices->CloseProtocol=Close;
#if !GU_NO_EDID_DRIVER
  if(scenario==17)MouseMemory[0x1000]^=1;
#endif
  FullMode=TRUE;
  EFI_STATUS s=GuReleaseGop(LastTable,(VOID *)9999,FullLog,NULL);
#ifdef GU_HEADLESS_RESEARCH
  if(scenario==26)NvMemory[0x280]^=1;
#endif
#if !GU_NO_EDID_DRIVER
  if(scenario==17)MouseMemory[0x1000]^=1;
#endif
  return s;
}
static VOID DecodeTests(VOID) {
  UINT32 after[64];UINT64 mask=0;
  memcpy(after,InitialFullConfig,sizeof(after));after[1]&=~3U;
  UINT64 expected=(InitialFullConfig[1]&1U?EFI_PCI_IO_ATTRIBUTE_IO:0U)|(InitialFullConfig[1]&2U?EFI_PCI_IO_ATTRIBUTE_MEMORY:0U);
  Check(GuDecodeRestoreMask(InitialFullConfig,after,&mask) && mask==expected,"exact missing IO/MEM accepted");
  for(UINTN word=0;word<64;word++)for(UINTN bit=0;bit<32;bit++) {
    memcpy(after,InitialFullConfig,sizeof(after));after[word]^=1U<<bit;
    BOOLEAN ok=GuDecodeRestoreMask(InitialFullConfig,after,&mask);
    Check(ok==(word==1 && bit<2 && (InitialFullConfig[1]&(1U<<bit))!=0),"every other config bit rejected");
  }
}
#if !GU_NO_EDID_DRIVER /* the legacy suite replays the EDID stage; test_headless_full.c has the no-EDID suite */
#ifdef GU_HEADLESS_RESEARCH
int LegacyFullMain(int argc,char **argv) {
#else
int main(int argc,char **argv) {
#endif
  Check(argc==5,"need two driver PE files, PROM and 256-byte config fixture");
  LoadDriverFixture(argv[1],MouseMemory,sizeof(MouseMemory));LoadDriverFixture(argv[2],NvMemory,sizeof(NvMemory));
  FILE *f=fopen(argv[3],"rb");Check(f && fread(PromMemory,1,sizeof(PromMemory),f)==sizeof(PromMemory),"read pinned PROM fixture");fclose(f);
  f=fopen(argv[4],"rb");Check(f && fread(InitialFullConfig,1,sizeof(InitialFullConfig),f)==sizeof(InitialFullConfig),"read full config fixture");fclose(f);
  Check(GuHashMatches(PromMemory,sizeof(PromMemory),"8ea297373e9e5d3600ff41ad9fae88f2d14c98c017d100669f7333981da673b4"),"PROM source pin");
  DecodeTests();Check(RunFull(0)==EFI_SUCCESS && Calls==4 && EnableCalls==1 && ClaimCalls==1 && CloseCalls==1,"complete release/claim/decode restoration succeeds");
  Check(Bytes(MockConfig,InitialFullConfig,sizeof(MockConfig)) && MockAttributes==0x300,"config/cache restored exactly");
  Check(GuVerifyReleased(LastTable,TRUE)==EFI_SUCCESS,"fresh cold pre-unlock verification");
  Check(!strcmp(GuLastVerifyDiagnostic()->Stage,"verify-complete") && GuLastVerifyDiagnostic()->Status==EFI_SUCCESS && !GuLastVerifyDiagnostic()->ConfigDiff,"successful verification clears stale diagnosis");
  /* Reproduce FULL01: same Device Control, only old CED/URD status cleared. */
  Check(InitialFullConfig[32]==0x00092930 && InitialFullConfig[30]==0x0012b410,"recorded capability and status fixture match FULL01");
  MockConfig[32]=0x00002930;
  Check(GuVerifyReleased(LastTable,FALSE)==EFI_SUCCESS,"FULL01 observed post-unlock status clearing permits verification");
  Check(GuVerifyReleased(LastTable,TRUE)==EFI_NOT_READY,"same change before unlock remains blocked");
  MockConfig[32]=InitialFullConfig[32];
  for(UINTN word=0;word<64;word++)for(UINTN bit=0;bit<32;bit++) {
    UINT32 before=InitialFullConfig[word],after=before^(1U<<bit);
    BOOLEAN allowed=word==32 && (bit==16 || bit==19);
    Check(VerifyConfigWord(word,before,after,FALSE)==allowed,"every config bit change except old CED/URD clearing rejected after unlock");
    Check(!VerifyConfigWord(word,before,after,TRUE),"all config changes rejected before unlock");
  }
  for(UINT32 before=0;before<16;before++)for(UINT32 after=0;after<16;after++) {
    UINT32 old=0x2930|(before<<16),now=0x2930|(after<<16);
    BOOLEAN allowed=!(after & ~before) && !((before^after)&~9U);
    Check(VerifyConfigWord(32,old,now,FALSE)==allowed,"error flag combinations allow only clearing old CED/URD, never new error or fatal/nonfatal changes");
  }
  Full.Config[30]^=0x10000;
  Check(!VerifyConfigWord(32,0x92930,0x2930,FALSE),"capability profile drift cannot authorize status exception");
  Full.Config[30]^=0x10000;
  for(int i=1;i<=20;i++) {
    EFI_STATUS s=RunFull(i);Check(s!=EFI_SUCCESS,"full release negative case fails closed");
    if(i==8 || i==9 || i==10 || i==11 || i==12 || i==13)Check(EnableCalls==1,"error after PCI effect remains failure");
    else Check(!EnableCalls,"pre-write fault never enables PCI");
    if(i==18)Check(!ClaimCalls,"new owner before claim prevents EXCLUSIVE auto-disconnect");
    if(i==19)Check(Calls==3,"late cold-read new child blocks NVIDIA stop");
    if(i==16 || i==20)Check(!EnableCalls,"intent log state change blocks Enable");
  }
  /* Final pre-unlock checks remain live after logging/capture callbacks. */
  Check(RunFull(0)==EFI_SUCCESS,"prepare final graph mutation case");
  Drop(FX_Amd,FX_EdidAgent,&EdidGuid);
  Check(GuVerifyReleased(LastTable,TRUE)!=EFI_SUCCESS,"AMD EDID loss after release blocks unlock handoff");
  Check(!strcmp(GuLastVerifyDiagnostic()->Stage,"verify-driver-graph-match"),"late protected graph drift identifies its failing gate");
  Check(RunFull(0)==EFI_SUCCESS,"prepare precise PCI mismatch diagnosis");
  MockConfig[16]^=0x100;
  Check(GuVerifyReleased(LastTable,FALSE)==EFI_NOT_READY,"non-identity PCI drift remains blocked");
  const GU_VERIFY_DIAGNOSTIC *d=GuLastVerifyDiagnostic();
  Check(d->ConfigDiff && d->Offset==64 && d->Expected==InitialFullConfig[16] && d->Observed==MockConfig[16] && !strcmp(d->Stage,"verify-pci-config-diff"),"diagnosis preserves exact config byte offset and both DWORDs");
  Check(RunFull(0)==EFI_SUCCESS,"prepare late config mutation case");
  MockConfig[1]|=4;
  Check(GuVerifyReleased(LastTable,FALSE)!=EFI_SUCCESS,"post-unlock BME drift blocks Windows");
  Check(!strcmp(GuLastVerifyDiagnostic()->Stage,"verify-pci-read") && !GuLastVerifyDiagnostic()->ConfigDiff,"identity/BME guard failure has its own stage and clears prior mismatch");
  Check(RunFull(21)==EFI_SUCCESS && GuVerifyReleased(LastTable,FALSE)==EFI_SUCCESS,"FULL02 RTX endpoint succeeds through full driver release and final verifier");
  Check(RunFull(22)==EFI_SUCCESS && GuVerifyReleased(LastTable,FALSE)==EFI_SUCCESS,"discovered AMD endpoint survives full release and final verifier");
  TestAmdPath[28]^=1;
  Check(GuVerifyReleased(LastTable,FALSE)!=EFI_SUCCESS,"AMD endpoint change after full release blocks Windows");
  printf("PASS full-release host checks: %d; actual graph/code/PROM fixtures, no real firmware calls\n",Checks);
  return 0;
}
#endif
