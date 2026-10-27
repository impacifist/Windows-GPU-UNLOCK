/* GPL-2.0-only. Adversarial Stage 1 services, host RAM only. */
#pragma once
enum {AT_SETUP,AT_CONFIRM,AT_INTENT,AT_FLUSH,AT_RESTORE,AT_DISCONNECT,
      AT_AFTER,AT_KEY,AT_CLOSE,AT_DONE,AT_ROOT};
enum {F_NONE,F_CONFIG,F_ATTRIBUTES,F_ADD_EDGE,F_DROP_EDGE,F_DUP_EDGE,F_COUNT,
      F_EDGE_ATTRIBUTES,F_TEXT_POINTER,F_NO_TEXT,F_NO_GOP,F_INPUT,F_GOP_MODE,
      F_PATH,F_NV_HASH,F_CONSOLE_HASH,F_BINDING,F_FLAGS,F_PLAN,F_BASELINE,
      F_GRAPH_BASELINE,F_REENTER,F_STOP_ERROR,F_STOP_PARTIAL,F_STOP_PARENT,
      F_STOP_WARNING,F_LINGER,F_ONE_EDGE,F_REORDER,F_CONFIG_READ,F_PARTIAL_READ,
      F_PROM_READ,F_COLD_READ,F_BAR,F_SNAPSHOT,F_OVERFLOW,F_OPEN,F_LOG_WRITE,
      F_LOG_SHORT,F_LOG_FLUSH,F_REPORT_CLOSE,F_DONE_WRITE,F_DONE_SHORT,
      F_DONE_FLUSH,F_DONE_CLOSE,F_PARENT_CLOSE,F_KEY_ERROR,F_STALL,F_NO_K,
      F_LOWER_K,F_ESC_K,F_DUP_TARGET,F_SUBSYSTEM,F_REVISION,F_LOCATION,
      F_BAR_RESOURCE,F_PROM_HASH,F_COLD_VALUE,F_IMAGE_SIZE,F_GUID,F_STOP_RVA,
      F_BINDING_IMAGE,F_GOP_POINTER,F_ERROR_POINTER,F_INPUT_HANDLE,F_SNAPSHOT_BAD,
      F_MEM_OFF,F_BME,F_SUBSTITUTE,F_UNCONFIRMED,F_COMPLETED,F_REPREFLIGHT,F_WRONG_STAGE,
      F_ALLOC_MEM,F_ALLOC_BME,F_ALLOC_CONFIG,F_ALLOC_READ,F_ALLOC_PARTIAL,F_ALLOC_FAIL,
      F_ALLOC_NOOP,F_ALLOC_FREE_FAIL,F_FREE_FAIL,F_BAR_MEM,F_COLD_BME,
      F_POST_CONFIG_READ,F_POST_PARTIAL_READ,F_POST_ATTRIBUTE_READ,F_CONFIG_MULTI};
static BOOLEAN Stage1Testing,Injected,TestNvCorrupted;
static int TestFault,TestAt,TestWord,TestRaw=2,TestDisconnects,TestMmio;
static UINTN TestNvOffset;
static UINT64 TestAttributes=0x300;
static UINTN TestGraphRows,TestGraphExpected,TestGraphSets;
static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL ReplacementText;
static int TestAllocationTarget=1,TestAllocations,TestPromFrees,TestMmioAtAllocation;
static BOOLEAN TestAllocationInjected;
static VOID *TestPromPool;
static EFI_TPL TestTpl;
static char TestReport[131072];static UINTN TestReportBytes;
static int TestEvidenceTransport,TestEvidenceWrites,TestEvidenceMmio;
static BOOLEAN TestEvidenceInjected;
static EFI_TPL EFIAPI Stage1Raise(EFI_TPL tpl){EFI_TPL old=TestTpl;(void)Raise(tpl);TestTpl=tpl;return old;}
static EFI_STATUS EFIAPI Stage1Allocate(EFI_MEMORY_TYPE type,UINTN n,VOID **out) {
  if(n==1048576 && ++TestAllocations==TestAllocationTarget && TestFault==F_ALLOC_FAIL){*out=NULL;return EFI_OUT_OF_RESOURCES;}
  EFI_STATUS s=Allocate(type,n,out);
  if(s==EFI_SUCCESS && n==1048576) {
    Check(!TestPromPool,"no overlapping PROM pool");TestPromPool=*out;
    if(TestAllocations==TestAllocationTarget && TestFault>=F_ALLOC_MEM && TestFault<=F_FREE_FAIL) {
      TestAllocationInjected=TRUE;TestMmioAtAllocation=TestMmio;
      if(TestFault==F_ALLOC_MEM || TestFault==F_ALLOC_FREE_FAIL)MockConfig[1]&=~2U;
      if(TestFault==F_ALLOC_BME)MockConfig[1]|=4;
      if(TestFault==F_ALLOC_CONFIG)MockConfig[30]^=0x10;
    }
  }
  return s;
}
static EFI_STATUS EFIAPI Stage1Free(VOID *p) {
  BOOLEAN prom=p==TestPromPool;
  if(prom){TestPromPool=NULL;TestPromFrees++;}
  EFI_STATUS s=Free(p);
  return prom && TestAllocationInjected && (TestFault==F_ALLOC_FREE_FAIL || TestFault==F_FREE_FAIL)?EFI_WRITE_PROTECTED:s;
}
static EDGE *Stage1TextEdge(VOID) {
  for(UINTN i=0;i<Live.Count;i++)if(Live.Edges[i].Handle==FX_Rtx && GuGuidEqual(&Live.Edges[i].Protocol,&TextGuid))return &Live.Edges[i];
  return NULL;
}
static VOID Stage1Inject(int point) {
  if(!Stage1Testing || Injected || point!=TestAt)return;
  Injected=TRUE;EDGE *e=Stage1TextEdge();
  switch(TestFault) {
  case F_CONFIG:MockConfig[TestWord]^=TestWord==1?1U:0x10U;break;
  case F_CONFIG_MULTI:MockConfig[29]^=0x4000;MockConfig[30]^=0x10;MockConfig[63]^=0x20;break;
  case F_ATTRIBUTES:MockAttributes^=1ULL<<TestWord;break;
  case F_ADD_EDGE:Live.Edges[Live.Count++]=(EDGE){FX_Rtx,FX_Input,FX_Rtx,EdidGuid,16,1};break;
  case F_DROP_EDGE:if(Live.Count)Live.Count--;break;
  case F_DUP_EDGE:if(e)Live.Edges[Live.Count++]=*e;break;
  case F_COUNT:if(e)e->Count++;break;
  case F_EDGE_ATTRIBUTES:if(e)e->Attributes=EFI_OPEN_PROTOCOL_EXCLUSIVE;break;
  case F_NO_TEXT:PhysicalTextInstalled=FALSE;break;
  case F_NO_GOP:PhysicalGopInstalled=FALSE;break;
  case F_INPUT:Keys.WaitForKey=NULL;break;
  case F_GOP_MODE:RtxMode.FrameBufferBase^=0x1000;break;
  case F_PATH:TestRtxPath[17]^=1;break;
  case F_NV_HASH:TestNvOffset=0x280+(TestWord?GU_NVGOP_TEXT_BYTES-1:0);NvMemory[TestNvOffset]^=1;TestNvCorrupted=TRUE;break;
  case F_CONSOLE_HASH:CorruptConsole(TestWord&1,TestWord&2);break;
  case F_BINDING:OutBinding.Version++;break;
  case F_FLAGS:
    Check(!Full.Captured && !Full.Released && !Full.Owned && !Full.Restored && !PreparedReady,"no general credentials before test forgery");
    if(TestWord==0)Full.Captured=TRUE;
    if(TestWord==1)Full.Released=TRUE;
    if(TestWord==2)Full.Owned=TRUE;
    if(TestWord==3)Full.Restored=TRUE;
    if(TestWord==4)PreparedReady=TRUE;
    break;
  case F_PLAN:PreparedPlan.OutAgent=FX_GfxAgent;break;
  case F_BASELINE:Full.Config[TestWord]^=1;break;
  case F_GRAPH_BASELINE:if(Initial.Count)Initial.Edges[0].Count++;break;
  case F_REORDER:for(UINTN i=0;i<Live.Count/2;i++){EDGE tmp=Live.Edges[i];Live.Edges[i]=Live.Edges[Live.Count-1-i];Live.Edges[Live.Count-1-i]=tmp;}break;
  case F_SUBSYSTEM:MockConfig[11]^=1;break;
  case F_REVISION:MockConfig[2]^=1;break;
  case F_IMAGE_SIZE:OutImage.ImageSize--;break;
  case F_GUID:OutFilePath[4]^=1;break;
  case F_STOP_RVA:OutBinding.Stop=(EFI_DRIVER_BINDING_STOP)(OutMemory+GU_CONSOLE_TEXT_STOP+1);break;
  case F_BINDING_IMAGE:Check(OutBinding.ImageHandle!=FX_OutAgent,"binding fault must change image identity");OutBinding.ImageHandle=FX_OutAgent;break;
  case F_GOP_POINTER:RtxGop.Mode=&AmdMode;break;
  case F_ERROR_POINTER:LastTable->StdErr=&ReplacementText;break;
  case F_INPUT_HANDLE:LastTable->ConsoleInHandle=FX_RtxPci;break;
  case F_MEM_OFF:MockConfig[1]&=~2U;break;
  case F_BME:MockConfig[1]|=4;break;
  case F_SUBSTITUTE:if(Live.Count)Live.Edges[0].Agent=FX_Input;break;
  case F_UNCONFIRMED:Check(PilotAdvance(1)==EFI_ACCESS_DENIED,"unconfirmed direct core call cannot use authority");break;
  case F_COMPLETED:Completed=3;break;
  case F_REPREFLIGHT:Check(PilotPreflight(NULL,NULL,NULL,NULL)==EFI_ACCESS_DENIED,"reentrant preflight denied");break;
  case F_WRONG_STAGE:Check(PilotAdvance(TestWord)==EFI_ACCESS_DENIED,"wrong stage cannot use splitter authority");break;
  default:break;
  }
}
static VOID Stage1Event(VOID) {
  if(!Stage1Testing)return;
  if(strstr(LastEvent,"pilot-confirmed"))Stage1Inject(AT_CONFIRM);
  if(strstr(LastEvent,"pilot-text-intent"))Stage1Inject(AT_INTENT);
  if(strstr(LastEvent,"pilot-fresh-key"))Stage1Inject(AT_KEY);
}
static VOID Stage1GraphRow(VOID) {
  /* Reconstruct each private evidence tuple and compare all fields. */
  if(strstr(LastEvent,"pilot-observe-count")) {
    if(TestGraphSets)Check(TestGraphRows==TestGraphExpected,"previous full active graph log is complete");
    TestGraphSets++;TestGraphRows=0;TestGraphExpected=Current.Count;
  }
  if(strstr(LastEvent,"pilot-edge ")) {
    unsigned long long handle,agent,controller;unsigned attributes,count;char guid[33];
    Check(sscanf(LastEvent,"stage=1 pilot-edge %llx %llx %llx %32s %x %x",&handle,&agent,&controller,guid,&attributes,&count)==6,"parse complete private graph tuple");
    EDGE e={(EFI_HANDLE)(UINTN)handle,(EFI_HANDLE)(UINTN)agent,(EFI_HANDLE)(UINTN)controller,{0},attributes,count};
    for(UINTN i=0;i<16;i++){unsigned byte;char pair[3]={guid[i*2],guid[i*2+1],0};Check(sscanf(pair,"%x",&byte)==1,"parse graph GUID byte");((UINT8 *)&e.Protocol)[i]=(UINT8)byte;}
    Check(TestGraphRows<Current.Count && SameEdge(&e,&Current.Edges[TestGraphRows]),"every logged active graph field matches observed tuple");TestGraphRows++;
  }
}
static VOID EFIAPI Stage1Restore(EFI_TPL tpl){TestTpl=tpl;Restore(tpl);Stage1Inject(AT_RESTORE);}
static EFI_STATUS EFIAPI Stage1Read(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH width,UINT32 at,UINTN n,VOID *data) {
  if(n==64 && ((TestAllocationInjected && TestFault==F_ALLOC_READ) || (TestDisconnects && TestFault==F_POST_CONFIG_READ)))return EFI_DEVICE_ERROR;
  if(n==64 && ((TestAllocationInjected && TestFault==F_ALLOC_PARTIAL) || (TestDisconnects && TestFault==F_POST_PARTIAL_READ))) {
    ((UINT32 *)data)[0]=0xdeadbeef;return EFI_DEVICE_ERROR;
  }
  if(TestFault==F_CONFIG_READ)return EFI_DEVICE_ERROR;
  if(TestFault==F_PARTIAL_READ){if(n)((UINT32 *)data)[0]=GU_BOARD_ID;return EFI_DEVICE_ERROR;}
  return ExtraRead(io,width,at,n,data);
}
static EFI_STATUS EFIAPI Stage1Memory(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH width,UINT8 bar,UINT64 offset,UINTN n,VOID *data) {
  TestMmio++;Check((MockConfig[1]&7U)==2U,"Stage 1 raw safe decoding before every simulated MMIO read");
  if(TestFault==F_PROM_READ && offset>=0x300000)return EFI_DEVICE_ERROR;
  if(TestFault==F_COLD_READ && offset<0x300000)return EFI_DEVICE_ERROR;
  EFI_STATUS s=MemRead(io,width,bar,offset,n,data);
  if(s==EFI_SUCCESS && TestDisconnects && TestFault==F_COLD_BME && offset==ColdOffsets[8])MockConfig[1]|=4;
  if(s==EFI_SUCCESS && TestFault==F_PROM_HASH && offset==0x300000)((UINT8 *)data)[0]^=1;
  if(s==EFI_SUCCESS && TestFault==F_COLD_VALUE && offset==0)*(UINT32 *)data^=1;
  return s;
}
static EFI_STATUS EFIAPI Stage1Bar(EFI_PCI_IO_PROTOCOL *io,UINT8 index,UINT64 *supported,VOID **out) {
  if(TestFault==F_BAR){*out=NULL;return EFI_DEVICE_ERROR;}
  EFI_STATUS s=Bar(io,index,supported,out);
  if(s==EFI_SUCCESS && TestDisconnects && TestFault==F_BAR_MEM)MockConfig[1]&=~2U;
  if(s==EFI_SUCCESS && TestFault==F_BAR_RESOURCE)((EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *)*out)->AddrLen--;
  return s;
}
static EFI_STATUS EFIAPI Stage1Locate(EFI_LOCATE_SEARCH_TYPE search,EFI_GUID *g,VOID *key,UINTN *n,EFI_HANDLE **out) {
  if(search==AllHandles && TestFault==F_SNAPSHOT){*n=0;*out=NULL;return EFI_DEVICE_ERROR;}
  EFI_STATUS s=Locate(search,g,key,n,out);
  if(search==AllHandles && TestFault==F_OVERFLOW)*n=MAX_HANDLES+1;
  if(search==ByProtocol && TestFault==F_DUP_TARGET){free(*out);*n=2;*out=malloc(2*sizeof(**out));(*out)[0]=(*out)[1]=FX_Rtx;}
  return s;
}
static EFI_STATUS EFIAPI Stage1Protocols(EFI_HANDLE h,EFI_GUID ***out,UINTN *n) {
  EFI_STATUS s=Protocols(h,out,n);if(TestFault==F_SNAPSHOT_BAD && *n)(*out)[0]=NULL;return s;
}
static EFI_STATUS EFIAPI Stage1Location(EFI_PCI_IO_PROTOCOL *io,UINTN *seg,UINTN *bus,UINTN *dev,UINTN *fun) {
  EFI_STATUS s=Location(io,seg,bus,dev,fun);if(TestFault==F_LOCATION)(*bus)++;return s;
}
static EFI_STATUS EFIAPI Stage1Disconnect(EFI_HANDLE controller,EFI_HANDLE agent,EFI_HANDLE child) {
  TestDisconnects++;
  Check(TestDisconnects==1 && controller==FX_Rtx && agent==FX_OutAgent && !child,"Stage 1 exact single splitter tuple; no graphics/NVIDIA release");
  Check(Splitter.Attempted && !Splitter.Confirmed && Faulted,"authority consumed and attempt latched before vendor call");
  Stage1Inject(AT_DISCONNECT);
  if(TestFault==F_REENTER){Check(PilotAdvance(1)==EFI_ACCESS_DENIED,"reentrant attempt denied");return EFI_ACCESS_DENIED;}
  if(TestFault==F_STOP_ERROR)ConsoleFault=1;
  if(TestFault==F_STOP_PARTIAL)ConsoleFault=8;
  if(TestFault==F_LINGER)ConsoleFault=5;
  if(TestFault==F_ONE_EDGE){ConsoleFault=8;(void)Disconnect(controller,agent,child);return EFI_SUCCESS;}
  EFI_STATUS s=Disconnect(controller,agent,child);
  Stage1Inject(AT_AFTER);
  if(TestFault==F_STOP_PARENT)return EFI_DEVICE_ERROR;
  if(TestFault==F_STOP_WARNING)return EFI_WARN_UNKNOWN_GLYPH;
  return s;
}
static VOID Stage1Setup(VOID) {
  Injected=TestNvCorrupted=FALSE;TestDisconnects=TestMmio=0;
  TestGraphRows=TestGraphExpected=TestGraphSets=0;
  TestAllocations=TestPromFrees=TestMmioAtAllocation=0;TestPromPool=NULL;TestAllocationInjected=FALSE;
  MockConfig[1]=(MockConfig[1]&~7U)|(UINT32)TestRaw;MockAttributes=TestAttributes;
  ReplacementText=Text;
  EFI_BOOT_SERVICES *bs=LastTable->BootServices;
  bs->DisconnectController=Stage1Disconnect;bs->RestoreTPL=Stage1Restore;
  bs->AllocatePool=Stage1Allocate;bs->FreePool=Stage1Free;
  bs->LocateHandleBuffer=Stage1Locate;bs->ProtocolsPerHandle=Stage1Protocols;
  RtxPci.Pci.Read=Stage1Read;RtxPci.Mem.Read=Stage1Memory;RtxPci.GetBarAttributes=Stage1Bar;RtxPci.GetLocation=Stage1Location;
  Stage1Inject(AT_SETUP);
}
