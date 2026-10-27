/* GPL-2.0-only. Actual EFI entry + actual pilot core, generated synthetic fixtures.
 * All UEFI/files/PCI effects are simulated host RAM. No payload linked. */
#define GU_PILOT_MODEL_SETUP 1
#include "test-model.h"
#define GU_PILOT_CORE_INCLUDED 1
#include "core.c"
#define GU_PILOT_HOST_TEST 1
#include "entry.c"
#include <setjmp.h>
static VOID SyntheticFill(UINT8 *data,UINTN count,UINTN seed){for(UINTN i=0;i<count;i++)data[i]=(UINT8)((i*37+(i>>8)*17+seed*53)&255);}
#include "stage0-replay.h"
static jmp_buf ExitPoint;
static EFI_STATUS HaltStatus;
static int KeyReads,Confirmed,Automatic;
static UINT8 *CorruptByte;
static VOID CorruptConsole(BOOLEAN graphics,BOOLEAN last) {
  Check(!CorruptByte,"one controlled host-memory corruption per run");
  CorruptByte=(graphics?GfxMemory:OutMemory)+0x2a0+(last?(graphics?GU_CONSOLE_GFX_BYTES:GU_CONSOLE_TEXT_BYTES)-1:0);
  *CorruptByte^=1;
}
static int PilotScenario,Writes,Flushes,ClosedMarker,FileOpens,ClockTicks,ConfirmTick,WaitTick,WaitMode;
static EFI_LOADED_IMAGE_PROTOCOL PilotLoaded;
static EFI_SIMPLE_FILE_SYSTEM_PROTOCOL PilotFs;
static EFI_FILE_PROTOCOL RootFile,ParentFile,RunFile,ReportFile,DoneFile;
static UINT8 UsbPath[]={3,5,6,0,0,0,127,255,4,0};
static char LastEvent[256],AttributeTrace[2048];
static UINTN AttributeTraceBytes,AttributeReads;
#include "test_stage1_hooks.h"
static EFI_STATUS EFIAPI PilotAttributes(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION op,UINT64 mask,UINT64 *result) {
  if(op==EfiPciIoAttributeOperationGet) {
    if(Stage1Testing && TestFault==F_POST_ATTRIBUTE_READ && TestDisconnects){*result=0xdeadbeef;return EFI_DEVICE_ERROR;}
    AttributeReads++;
    if(PilotScenario==32 || (PilotScenario==37 && AttributeReads==2))return EFI_DEVICE_ERROR;
    if(PilotScenario==38 && AttributeReads==2){*result=0;return EFI_SUCCESS;}
    if(PilotScenario==41 && AttributeReads==2){*result=0x200;return EFI_SUCCESS;}
  }
  return Attributes(io,op,mask,result);
}
VOID PilotHostHalt(EFI_STATUS s){HaltStatus=s;longjmp(ExitPoint,1);}
static BOOLEAN Has(const CHAR16 *name,const char *ascii){for(UINTN i=0;;i++){if(name[i]!=(UINT8)ascii[i])return FALSE;if(!name[i])return TRUE;}}
static EFI_STATUS EFIAPI PilotHandle(EFI_HANDLE h,EFI_GUID *g,VOID **out) {
  if(Stage1Testing && TestFault==F_TEXT_POINTER && Injected && h==FX_Rtx && GuGuidEqual(g,&TextGuid)){*out=&ReplacementText;return EFI_SUCCESS;}
  if(h==(VOID *)9999 && GuGuidEqual(g,&Loaded)){*out=&PilotLoaded;return EFI_SUCCESS;}
  if(h==PilotLoaded.DeviceHandle && GuGuidEqual(g,&Path)){*out=UsbPath;return EFI_SUCCESS;}
  if(h==PilotLoaded.DeviceHandle && GuGuidEqual(g,&Fs)){*out=&PilotFs;return EFI_SUCCESS;}
  return Handle(h,g,out);
}
static EFI_STATUS EFIAPI PilotWrite(EFI_FILE_PROTOCOL *file,UINTN *n,VOID *buffer) {
  Check(TestTpl==TPL_APPLICATION,"file writes only at application TPL");
  Writes++;Check(file==&ReportFile || file==&DoneFile,"writes only to USB report or close-proof file");
  if(file==&DoneFile){
    Check(strstr((char *)buffer,"No GPU release/decode permit.")!=NULL,"splitter close proof never grants GPU release");
    if(Stage1Testing && TestFault==F_DONE_WRITE)return EFI_DEVICE_ERROR;
    if(Stage1Testing && TestFault==F_DONE_SHORT)(*n)--;
    ClosedMarker++;return EFI_SUCCESS;
  }
  UINTN count=*n<sizeof(LastEvent)-1?*n:sizeof(LastEvent)-1;memcpy(LastEvent,buffer,count);LastEvent[count]=0;
  if(strstr(LastEvent,"stage1-failure-")) {
    if(!TestEvidenceWrites)TestEvidenceMmio=TestMmio;
    Check(TestMmio==TestEvidenceMmio,"failure evidence performs no further GPU reads");TestEvidenceWrites++;
    if(!TestEvidenceInjected && TestEvidenceTransport) {
      TestEvidenceInjected=TRUE;
      if(TestEvidenceTransport==1)return EFI_WRITE_PROTECTED;
      if(TestEvidenceTransport==2)(*n)--;
      if(TestEvidenceTransport==4)Check(PilotVerify(1)!=EFI_SUCCESS,"diagnostic callback cannot renew failed authority");
    }
  }
  Stage1Event();
  if(Stage1Testing)Stage1GraphRow();
  if(Stage1Testing && TestFault==F_LOG_WRITE && strstr(LastEvent,"pilot-text-intent"))return EFI_DEVICE_ERROR;
  if(Stage1Testing && TestFault==F_LOG_SHORT && strstr(LastEvent,"pilot-text-intent"))(*n)--;
  if(strstr(LastEvent,"capture-attributes-") || strstr(LastEvent,"verify-") || strstr(LastEvent,"stage0-")) {
    Check(AttributeTraceBytes+count<sizeof(AttributeTrace),"bounded attribute evidence trace");
    memcpy(AttributeTrace+AttributeTraceBytes,LastEvent,count+1);AttributeTraceBytes+=count;
    if(PilotScenario==36 && strstr(LastEvent,"capture-attributes-value"))return EFI_DEVICE_ERROR;
  }
  if(PilotScenario==39 && strstr(LastEvent,"capture-attributes-match"))return EFI_DEVICE_ERROR;
  if(PilotScenario==40 && strstr(LastEvent,"stage0-release-attribute-check"))return EFI_DEVICE_ERROR;
  if(PilotScenario==44 && strstr(LastEvent,"pilot-stage0-automatic"))MockConfig[1]^=1;
  if((PilotScenario==50 || PilotScenario==51) && strstr(LastEvent,"pilot-text-intent"))CorruptConsole(PilotScenario==51,FALSE);
  if((PilotScenario==52 || PilotScenario==53) && strstr(LastEvent,"pilot-wait-new-key"))CorruptConsole(PilotScenario==53,TRUE);
  const char *digest=strstr(LastEvent,"pilot-graph-sha256=");
  if(digest){for(UINTN i=0;i<64;i++)Check(digest[19+i] && strchr("0123456789abcdef",digest[19+i]),"complete lowercase SHA-256 evidence field");Check(digest[83]==' ',"SHA-256 field has exactly 64 digits");}
  if(strstr(LastEvent,"pilot-preflight")){WaitMode=1;ConfirmTick=ClockTicks;if(PilotScenario==18)Keys.WaitForKey=NULL;}
  if(strstr(LastEvent,"pilot-wait-new-key")){WaitMode=2;WaitTick=ClockTicks;}
  if(strstr(LastEvent,"pilot-confirmed"))Confirmed++;
  if(strstr(LastEvent,"pilot-stage0-automatic"))Automatic++;
  if(strstr(LastEvent,"pilot-confirmed") || strstr(LastEvent,"pilot-stage0-automatic")) {
    if(PilotScenario==5)return EFI_DEVICE_ERROR;
    if(PilotScenario==7)(*n)--;
  }
  if(strstr(LastEvent,"pilot-nvidia-intent")) {
    if(PilotScenario==20){FullScenario=34;RestoreArmed=TRUE;}
    if(PilotScenario==21){FullScenario=35;RestoreArmed=TRUE;}
    if(PilotScenario==19)Live.Edges[Live.Count++]=(EDGE){FX_Rtx,FX_Input,FX_Rtx,EdidGuid,16,1};
  }
  Check(TestReportBytes+*n<sizeof(TestReport),"bounded complete Stage 1 report");
  memcpy(TestReport+TestReportBytes,buffer,*n);TestReportBytes+=*n;TestReport[TestReportBytes]=0;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI PilotFlush(EFI_FILE_PROTOCOL *file) {
  (void)file;Flushes++;
  Check(TestTpl==TPL_APPLICATION,"file flushes only at application TPL");
  if(TestEvidenceTransport==3 && strstr(LastEvent,"stage1-failure-status"))return EFI_WRITE_PROTECTED;
  if(Stage1Testing && file==&ReportFile && strstr(LastEvent,"pilot-text-intent"))Stage1Inject(AT_FLUSH);
  if(Stage1Testing && TestFault==F_LOG_FLUSH && strstr(LastEvent,"pilot-text-intent"))return EFI_DEVICE_ERROR;
  if(Stage1Testing && file==&DoneFile){Stage1Inject(AT_DONE);if(TestFault==F_DONE_FLUSH)return EFI_DEVICE_ERROR;}
  return PilotScenario==6 && (strstr(LastEvent,"pilot-confirmed") || strstr(LastEvent,"pilot-stage0-automatic"))?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI PilotClose(EFI_FILE_PROTOCOL *file) {
  if(Stage1Testing && file==&ReportFile)Stage1Inject(AT_CLOSE);
  if(Stage1Testing && file==&RootFile)Stage1Inject(AT_ROOT);
  if(Stage1Testing && ((TestFault==F_DONE_CLOSE && file==&DoneFile) || (TestFault==F_PARENT_CLOSE && file==&ParentFile) || (TestFault==F_REPORT_CLOSE && file==&ReportFile)))return EFI_DEVICE_ERROR;
  if(PilotScenario==8 && file==&ReportFile)return EFI_DEVICE_ERROR;
  if(PilotScenario==11 && file==&RootFile)return EFI_DEVICE_ERROR;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI PilotOpen(EFI_FILE_PROTOCOL *file,EFI_FILE_PROTOCOL **out,CHAR16 *name,UINT64 mode,UINT64 attrs) {
  FileOpens++;*out=NULL;
  if(Stage1Testing && TestFault==F_OPEN)return EFI_DEVICE_ERROR;
  if(file==&RootFile){Check(Has(name,"\\GPU-PILOT") && attrs==EFI_FILE_DIRECTORY,"one USB-only log parent");*out=&ParentFile;return EFI_SUCCESS;}
  if(file==&ParentFile) {
    Check(name[0]=='S' && name[1]=='0'+GU_PILOT_STAGE,"stage-labelled unique output folder");
    if(!(mode&EFI_FILE_MODE_CREATE)){if(PilotScenario==10){*out=&RunFile;return EFI_SUCCESS;}return EFI_NOT_FOUND;}
    *out=&RunFile;return EFI_SUCCESS;
  }
  Check(file==&RunFile,"report opens use only per-run USB directory");
  if(Has(name,"REPORT.TXT"))*out=&ReportFile;
  else {Check(Has(name,"REPORT-CLOSED.OK"),"only expected close proof permitted");*out=&DoneFile;}
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI PilotVolume(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs,EFI_FILE_PROTOCOL **out){Check(fs==&PilotFs,"only loaded USB volume");*out=&RootFile;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI PilotStall(UINTN us){ClockTicks+=(int)(us/100000);
  if(Stage1Testing && TestFault==F_STALL)return EFI_DEVICE_ERROR;
  return EFI_SUCCESS;}
static EFI_STATUS EFIAPI PilotKey(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *in,EFI_INPUT_KEY *key) {
  KeyReads++;Check(in==&Keys,"only preserved keyboard called");*key=(EFI_INPUT_KEY){0};
  if(Stage1Testing && TestFault==F_KEY_ERROR)return EFI_DEVICE_ERROR;
  if(Stage1Testing && TestFault==F_NO_K && WaitMode==2)return EFI_NOT_READY;
  if(Stage1Testing && TestFault==F_LOWER_K && WaitMode==2 && ClockTicks>WaitTick+50){key->UnicodeChar='k';return EFI_SUCCESS;}
  if(Stage1Testing && TestFault==F_ESC_K && WaitMode==2 && ClockTicks>WaitTick+50){key->UnicodeChar=27;return EFI_SUCCESS;}
  if(PilotScenario==30){key->UnicodeChar='C';return EFI_SUCCESS;}
  if(PilotScenario==29 && WaitMode==2){key->UnicodeChar='K';PilotScenario=9;return EFI_SUCCESS;}
  if(PilotScenario==4){key->UnicodeChar='C';PilotScenario=3;return EFI_SUCCESS;}
  if(WaitMode==1 && ClockTicks>ConfirmTick) {
    if(PilotScenario==2){key->UnicodeChar=27;return EFI_SUCCESS;}
    if(PilotScenario==3)return EFI_NOT_READY;
    if(PilotScenario==31){key->UnicodeChar='c';return EFI_SUCCESS;}
    key->UnicodeChar='C';WaitMode=0;return EFI_SUCCESS;
  }
  if(WaitMode==2 && ClockTicks>WaitTick+50 && PilotScenario!=9){key->UnicodeChar='K';WaitMode=0;return EFI_SUCCESS;}
  return EFI_NOT_READY;
}
static EFI_STATUS EFIAPI PilotOutput(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out,CHAR16 *text){(void)text;Check(out==&Text && !OutputDisabled && !Calls,"no console output during or after release");return EFI_SUCCESS;}
static EFI_STATUS EFIAPI PilotWatch(UINTN timeout,UINT64 code,UINTN size,CHAR16 *data){Check(!timeout&&!code&&!size&&!data,"only disable watchdog; never arm reset");return EFI_SUCCESS;}
static EFI_STATUS TestPilot(int scenario) {
  Splitter=(__typeof__(Splitter)){0}; /* A separate simulated boot, not firmware re-arming. */
  Stage1Working=Stage1Failure=(STAGE1_DIAGNOSTIC){0};Stage1Reported=FALSE;
  Stage1ConfigBaselineValid=Stage1AttributeBaselineValid=FALSE;
  FullMode=FALSE;FullScenario=0;Phase=0;ConsoleFault=0;RestoreArmed=RestoreInjected=FALSE;
  Check(Run(0)==EFI_SUCCESS,"fixture initialisation performs no detach");
  Calls=0;EnableCalls=ClaimCalls=CloseCalls=0;ChildStops=ParentStops=0;
  memcpy(MockConfig,FixtureFullConfig,sizeof(MockConfig));MockAttributes=0x200;
  NvBinding=(EFI_DRIVER_BINDING_PROTOCOL){0};NvBinding.Version=GU_NVGOP_VERSION;
  NvBinding.DriverBindingHandle=FX_NvAgent;NvBinding.ImageHandle=FX_NvImage;NvBinding.Stop=(EFI_DRIVER_BINDING_STOP)(NvMemory+GU_NVGOP_STOP);
  NvImage=(EFI_LOADED_IMAGE_PROTOCOL){0};NvImage.ImageBase=NvMemory;NvImage.ImageSize=sizeof(NvMemory);NvImage.ImageCodeType=EfiBootServicesCode;NvImage.DeviceHandle=FX_RtxPci;
  memcpy(NvPath,(UINT8[]){4,4,28,0},4);memcpy(NvPath+4,L"nvgop-ga1xx",24);memcpy(NvPath+28,(UINT8[]){127,255,4,0},4);NvImage.FilePath=(VOID *)NvPath;
  FullMode=TRUE;RtxPci.Mem.Read=MemRead;RtxPci.GetBarAttributes=Bar;RtxPci.Attributes=PilotAttributes;
  EFI_BOOT_SERVICES *bs=LastTable->BootServices;bs->HandleProtocol=PilotHandle;bs->Stall=PilotStall;bs->SetWatchdogTimer=PilotWatch;
  Check(!bs->LoadImage && !bs->StartImage && !bs->ConnectController && !RtxPci.Mem.Write && !RtxPci.Pci.Write && !RtxPci.Map && !RtxPci.AllocateBuffer,"no payload, chainload, reconnect, raw write or DMA capabilities in mocks");
  Text.OutputString=PilotOutput;Keys.ReadKeyStroke=PilotKey;
  PilotLoaded.DeviceHandle=(VOID *)8888;PilotFs.OpenVolume=PilotVolume;
  RootFile=ParentFile=RunFile=ReportFile=DoneFile=(EFI_FILE_PROTOCOL){0};
  RootFile.Open=ParentFile.Open=RunFile.Open=PilotOpen;
  RootFile.Close=ParentFile.Close=RunFile.Close=ReportFile.Close=DoneFile.Close=PilotClose;
  ReportFile.Write=DoneFile.Write=PilotWrite;ReportFile.Flush=DoneFile.Flush=PilotFlush;
  PilotScenario=scenario;CorruptByte=NULL;KeyReads=Confirmed=Automatic=0;AttributeReads=0;AttributeTraceBytes=0;AttributeTrace[0]=0;Writes=Flushes=ClosedMarker=FileOpens=ClockTicks=WaitMode=0;LastEvent[0]=0;
  UsbPath[1]=scenario==1?1:5;
  if(scenario==33)MockAttributes=0; /* Missing MEMORY despite command bit 1. */
  if(scenario==34)MockAttributes|=EFI_PCI_IO_ATTRIBUTE_BUS_MASTER;
  if(scenario==35 || scenario==39 || scenario==40 || scenario==41)MockAttributes|=EFI_PCI_IO_ATTRIBUTE_IO;
  if(scenario==42)MockConfig[1]|=4; /* Actual hardware BME, never observation-only. */
  if(scenario==43)MockConfig[1]&=~2U; /* Unsafe to read GPU MMIO. */
  if(scenario==12)ConsoleFault=3;
  if(scenario==13)FullScenario=1;
  if(scenario==14)FullScenario=8;
  if(scenario==15)ConsoleFault=5;
  if(scenario==16)ConsoleFault=6;
  if(scenario==17)FullScenario=28;
  if(scenario==22)FullScenario=6;
  if(scenario==24)FullScenario=4;
  if(scenario==25)FullScenario=13;
  if(scenario==26)FullScenario=10;
  if(scenario==27)FullScenario=30;
  if(scenario==28)FullScenario=31;
  if(scenario==45) {
    memcpy(MockConfig,Stage0Config,sizeof(MockConfig));MockAttributes=Stage0Attributes;
    Check(Stage0ExpectedAttributes==0x200 && Stage0Attributes==0x300,"synthetic observation carries independent command/attribute values");
    UINTN relevant=0;
    for(UINTN i=0;i<Live.Count;i++) {
      EDGE *e=&Live.Edges[i];
      if(e->Handle!=FX_Rtx && e->Handle!=FX_RtxPci && e->Handle!=FX_Console && e->Handle!=FX_Input && e->Handle!=FX_Error)continue;
      relevant++;UINTN found=0;
      for(UINTN j=0;j<sizeof(Stage0RelevantEdges)/sizeof(*Stage0RelevantEdges);j++)if(SameEdge(e,&Stage0RelevantEdges[j]))found++;
      Check(found==1,"synthetic observation role edge matches replay without raw handles");
    }
    Check(relevant==sizeof(Stage0RelevantEdges)/sizeof(*Stage0RelevantEdges) && Stage0GraphRecords>=relevant,"all recorded relevant ownership edges covered; full graph not reconstructed");
  }
  if(scenario>=46 && scenario<=49)CorruptConsole((scenario&1)!=0,scenario>=48);
  if(Stage1Testing)Stage1Setup();
  TestTpl=TPL_APPLICATION;bs->RaiseTPL=Stage1Raise;bs->RestoreTPL=Stage1Restore;
  TestReportBytes=0;TestReport[0]=0;TestEvidenceWrites=0;TestEvidenceInjected=FALSE;
  if(!setjmp(ExitPoint)) {efi_main((VOID *)9999,LastTable);Check(FALSE,"EFI entry never returns to firmware boot sequencing");}
  if(CorruptByte){*CorruptByte^=1;CorruptByte=NULL;} /* Preserve synthetic array for following cases. */
  if(TestNvCorrupted){NvMemory[TestNvOffset]^=1;TestNvCorrupted=FALSE;}
  if(!(Stage1Testing && TestFault==F_FLAGS && Injected))Check(!Full.Captured && !Full.Released && !Full.Owned && !Full.Restored && !PreparedReady,"Stage 1 never grants general release, claim or decode credentials");
  Check(!Splitter.Ready && !Splitter.Confirmed && Splitter.Ended,"all entry outcomes revoke splitter authority before halt");
  Check(Calls<=GU_PILOT_STAGE && EnableCalls<=(GU_PILOT_STAGE==4?1:0),"compiled stage bound and decode isolation");
  if(HaltStatus!=EFI_SUCCESS)fprintf(stderr,"pilot fault=%d halt=%llx last=%s\n",scenario,(unsigned long long)HaltStatus,LastEvent);
  int before=Calls,enables=EnableCalls;
  Check(PilotAdvance(GU_PILOT_STAGE+1)==EFI_ACCESS_DENIED && Calls==before && EnableCalls==enables,"later stage unreachable through core API");
  if(HaltStatus!=EFI_SUCCESS && Faulted)Check(PilotAdvance(Completed+1)==EFI_ACCESS_DENIED && Calls==before && EnableCalls==enables,"failed core cannot retry a partial operation");
  return HaltStatus;
}
#include "test_stage1_cases.h"
#include "test_stage1_safety.h"
int main(int argc,char **argv) {
  Check(argc==1 || argc==2,"optional regression selection only; no capture directory");
  SyntheticFill(PromMemory,sizeof(PromMemory),1);
  SyntheticFill(NvMemory+0x280,GU_NVGOP_TEXT_BYTES,2);
  SyntheticFill(GfxMemory+0x2a0,GU_CONSOLE_GFX_BYTES,3);
  SyntheticFill(OutMemory+0x2a0,GU_CONSOLE_TEXT_BYTES,4);
  if(argc==2){Stage1SafetyCases(argv[1]);return 0;}
  Check(TestPilot(0)==EFI_SUCCESS && ClosedMarker==1,"actual entry/core successful run closes report");
  Check(Confirmed==1 && !Automatic && KeyReads>0,"later stages still require explicit confirmation");
  Check(Calls==(GU_PILOT_STAGE<3?GU_PILOT_STAGE:3),"only exact compiled disconnect sequence");
  Check(TestPilot(45)==EFI_SUCCESS && Calls==1 && ClosedMarker==1 && Confirmed==1 && !Full.Captured,"synthetic mismatch fixture completes splitter-only policy without release authority");
  Check(strstr(AttributeTrace,"capture-attributes-value 0000000000000300") && strstr(AttributeTrace,"capture-attributes-expected 0000000000000200") && strstr(AttributeTrace,"capture-attributes-match 8000000000000006"),"synthetic replay preserves strict eligibility classification");
  for(int corrupt=46;corrupt<=49;corrupt++)Check(TestPilot(corrupt)==EFI_CRC_ERROR && !Calls && !EnableCalls && !Confirmed && !ClosedMarker,"changed first/last console code byte blocks preflight in every pilot stage");
  for(int corrupt=50;corrupt<=51;corrupt++)Check(TestPilot(corrupt)==EFI_CRC_ERROR && !Calls && !EnableCalls && !ClosedMarker,"console code drift after confirmation blocks first disconnect");
  for(int corrupt=52;corrupt<=53;corrupt++)Check(TestPilot(corrupt)==EFI_CRC_ERROR && Calls==1 && !EnableCalls && !ClosedMarker,"console code drift after splitter stop blocks progression and success proof");
  Check(TestPilot(32)==EFI_DEVICE_ERROR && !Calls && !EnableCalls && !ClosedMarker,"attribute read error stops before confirmation/effects");
  Check(strstr(AttributeTrace,"capture-attributes-read 8000000000000007") && !strstr(AttributeTrace,"capture-attributes-value"),"failed read logs status without an untrusted value");
  for(int mismatch=33;mismatch<=35;mismatch++) {
    if(GU_PILOT_STAGE==1 && mismatch==35)Check(TestPilot(mismatch)==EFI_SUCCESS && Calls==1 && ClosedMarker==1 && !Full.Captured,"only Stage 1 accepts exact observed IO mismatch without release permit");
    else Check(TestPilot(mismatch)==EFI_NOT_READY && !Calls && !EnableCalls && !ClosedMarker,"missing decode, unexpected BME and extra IO remain rejected");
    Check(strstr(AttributeTrace,"capture-attributes-read 0000000000000000") && strstr(AttributeTrace,"capture-attributes-expected 0000000000000200") && strstr(AttributeTrace,"capture-attributes-match 8000000000000006"),"attribute mismatch has explicit evidence and failing checkpoint");
  }
  Check(TestPilot(39)==(GU_PILOT_STAGE<=1?EFI_DEVICE_ERROR:EFI_NOT_READY) && !Calls && !EnableCalls && !ClosedMarker,"classification write failure cannot become observation success");
  Check(TestPilot(42)!=EFI_SUCCESS && !Calls && !ClosedMarker,"raw PCI bus mastering remains blocked in every stage");
  Check(TestPilot(43)!=EFI_SUCCESS && !Calls && !ClosedMarker,"raw memory decode disabled remains blocked before GPU reads");
  Check(TestPilot(37)==EFI_DEVICE_ERROR && !Calls && !ClosedMarker && strstr(AttributeTrace,"verify-attributes-read 8000000000000007"),"baseline verification identifies repeated attribute read failure");
  Check(TestPilot(38)==EFI_NOT_READY && !Calls && !ClosedMarker && strstr(AttributeTrace,"verify-attributes-match 8000000000000006"),"baseline verification identifies attribute drift without accepting it");
  Check(TestPilot(36)==EFI_DEVICE_ERROR && !Calls && !EnableCalls && !ClosedMarker,"new evidence write failure blocks confirmation and effects");
  Check(TestPilot(30)!=EFI_SUCCESS && !Calls,"stuck key queue cannot become confirmation");
  Check(TestPilot(31)!=EFI_SUCCESS && !Calls,"lowercase key does not satisfy explicit uppercase confirmation");
  Check(TestPilot(27)!=EFI_SUCCESS && !Calls && !ClosedMarker,"failed config read prevents every release");
  Check(TestPilot(28)!=EFI_SUCCESS && !Calls && !ClosedMarker,"failed GPU read prevents every release");
  Check(TestPilot(18)!=EFI_SUCCESS && !Calls && !ClosedMarker,"input drift before confirmation halts before keyboard use");
  for(int i=1;i<=11;i++) {
    if(GU_PILOT_STAGE==0 && (i==2 || i==3 || i==4 || i==9))continue;
    Check(TestPilot(i)!=EFI_SUCCESS,"entry cancellation, timeout or transport failure halts");
    if(i<=8 || i==10)Check(!ClosedMarker,"failed report never gets close-proof marker");
    if(i<=7)Check(!Calls&&!EnableCalls,"confirmation/log failure prevents mutation");
  }
  Check(TestPilot(29)!=EFI_SUCCESS && Calls==1 && !ClosedMarker,"queued K is drained and cannot authorise another step");
  Check(TestPilot(15)!=EFI_SUCCESS && Calls==1,"false splitter success never reaches later stages");
  Stage1Cases();
  Stage1SafetyCases("all");
  printf("PASS actual pilot entry/core stage %d: %d assertions; simulated services only\n",GU_PILOT_STAGE,Checks);return 0;
}
