/* GPL-2.0-only. Real generic probe, RAM-only EFI mocks. No firmware/device writes. */
#define main FixedProbeMain
#include "test_probe.c"
#undef main
#include <IndustryStandard/Acpi.h>
#ifndef GU_PROBE_BUS
#define GU_PROBE_BUS 1U /* must match probe.c; -DGU_PROBE_BUS=0x2bU exercises a non-default bus */
#endif
#ifndef GU_TEST_PROBE_BUS
#define GU_TEST_PROBE_BUS GU_PROBE_BUS
#endif

enum {G_OK,G_UNKNOWN,G_BME,G_MEM_OFF,G_BAD_BAR,G_BAD_RESOURCE,G_CHANGED,G_MULTI,
      G_SHORT_WRITE,G_FLUSH,G_CLOSE,G_OTHER_BDF,G_PROTOCOL_CHANGED,G_CFG_FAIL,G_BAD_CHECKSUM,
      G_EXISTING_PROM,G_EXISTING_IMG,G_DUMP_READ_ERROR,
      G_ATTR_FAIL,G_ATTR_MEM,G_ATTR_BME,G_ATTR_CONFIG,G_ATTR_BDF,G_ATTR_INTERFACE,
      G_BUS_ZERO,G_BUS_OVERFLOW,G_SEGMENT,G_DEVICE,G_FUNCTION,G_LOCATION_FAIL,
      G_INVENTORY_READ_FAIL,G_INVENTORY_CLASS_FAIL,G_INITIAL_INTERFACE,G_INITIAL_ID,
      G_INITIAL_CLASS,G_INITIAL_BUS,G_MIXED_MULTI,G_NO_DISPLAY,
      G_LOST_HANDLE,G_LOST_ID,G_LOST_VENDOR,G_LOST_CLASS,G_LOST_CLASS_READ,
      G_INITIAL_SEGMENT,G_ATTR_SEGMENT,G_MULTI_SEGMENT};
static int Mode,RootCloses,RootOpens,ForeignOpens,SfsSearches,Dumps,ExistingWriteAttempts;
static EFI_FILE_PROTOCOL DumpFile,ForeignRoot;
static EFI_HANDLE BindingHandle=(VOID *)50,ImageHandle=(VOID *)51;
static EFI_DRIVER_BINDING_PROTOCOL Binding;
static EFI_LOADED_IMAGE_PROTOCOL Image;
static UINT8 ImageBytes[512];
static CAPTURE *ActiveCapture;
static int Drifted,ReadsAfterDrift;
static BOOLEAN AttributeReturned;
static int AttributeConfigReads;
static EFI_PCI_IO_PROTOCOL ChangedPci;
static UINTN TestSegment,TestBus;
static UINT32 TestId;
static int LocationCalls;

static EFI_STATUS EFIAPI GenericRead(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 at,UINTN n,VOID *data) {
  if(w==EfiPciIoWidthUint32 && at==8 && n==1){
    *(UINT32 *)data=(Mode==G_NO_DISPLAY || (Mode==G_LOST_CLASS && S.IdReads>1))?0x020000a1:0x030000a1;
    return (Mode==G_INVENTORY_CLASS_FAIL || (Mode==G_LOST_CLASS_READ && S.IdReads>1))?EFI_DEVICE_ERROR:EFI_SUCCESS;
  }
  EFI_STATUS s=PciRead(io,w,at,n,data);
  if(s==EFI_SUCCESS && w==EfiPciIoWidthUint32 && at==0) {
    UINT32 *v=data;
    if(n==1) {
      v[0]=TestId;
      if(Mode==G_LOST_ID && S.IdReads>1)return EFI_DEVICE_ERROR;
      if(Mode==G_LOST_VENDOR && S.IdReads>1)v[0]=0x12348086;
      if(Mode==G_INVENTORY_READ_FAIL)return EFI_DEVICE_ERROR;
      if(Mode==G_MIXED_MULTI && S.IdReads==2)v[0]=0x999910de;
    }
    if(Mode==G_UNKNOWN && n==64)v[0]=0x999910de;
    if(n==64){
      v[0]=TestId;
      if(Mode==G_UNKNOWN || Mode==G_INITIAL_ID)v[0]=0x999910de;
      v[1]=(Mode==G_BME)?6U:((Mode==G_MEM_OFF)?0U:2U);
      v[2]=0x030000a1;v[4]=GU_BAR0_BASE;
      if(Mode==G_INITIAL_CLASS)v[2]^=1U;
      if(Mode==G_BAD_BAR)v[4]|=4U;
      if(Mode==G_CHANGED && S.CfgReads>1)v[11]^=1U;
      if(Mode==G_CFG_FAIL && S.CfgReads>1)return EFI_DEVICE_ERROR;
      if(AttributeReturned) {
        if(Mode==G_ATTR_MEM)v[1]&=~2U;
        if(Mode==G_ATTR_BME)v[1]|=4U;
        if(Mode==G_ATTR_CONFIG)v[63]^=1U;
      }
      if(ActiveCapture && S.CfgReads>1){memset(ActiveCapture->Buffer,'x',sizeof(ActiveCapture->Buffer)-1);ActiveCapture->Used=sizeof(ActiveCapture->Buffer)-1;}
    }
  }
  return s;
}
static EFI_STATUS EFIAPI DriftLog(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *data) {
  if(S.CfgReads>1 && S.PromReads<(int)PROM_DWORDS)Drifted=1;
  return LogWrite(f,n,data);
}
static EFI_STATUS EFIAPI GuardedMem(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT8 bar,UINT64 at,UINTN n,VOID *data) {
  if(Drifted)ReadsAfterDrift++;
  return MemRead(io,w,bar,at,n,data);
}
static EFI_STATUS EFIAPI GenericLocation(EFI_PCI_IO_PROTOCOL *io,UINTN *seg,UINTN *bus,UINTN *dev,UINTN *fun) {
  EFI_STATUS s=Location(io,seg,bus,dev,fun);
  LocationCalls++;
  *seg=TestSegment;
  if((Mode==G_INITIAL_SEGMENT && LocationCalls>1) || (Mode==G_ATTR_SEGMENT && AttributeReturned) ||
     (Mode==G_MULTI_SEGMENT && io==&ChangedPci))*seg=TestSegment^1U;
  *bus=((Mode==G_OTHER_BDF && S.PromAllocs) || (Mode==G_ATTR_BDF && AttributeReturned) ||
        (Mode==G_INITIAL_BUS && LocationCalls>1))?TestBus+1U:TestBus;
  if(Mode==G_BUS_ZERO)*bus=0;
  if(Mode==G_BUS_OVERFLOW)*bus=256;
  if(Mode==G_SEGMENT)*seg=0x10000U;
  if(Mode==G_DEVICE)*dev=1;
  if(Mode==G_FUNCTION)*fun=1;
  if(Mode==G_LOCATION_FAIL)return EFI_DEVICE_ERROR; /* Plausible partial BDF is not valid. */
  return s;
}
static EFI_STATUS EFIAPI Bar(EFI_PCI_IO_PROTOCOL *io,UINT8 index,UINT64 *supports,VOID **out) {
  (void)io;(void)index;(void)supports;
  UINT8 *p=calloc(1,sizeof(EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR)+2);if(!p)return EFI_OUT_OF_RESOURCES;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *d=(VOID *)p;
  d->Desc=ACPI_ADDRESS_SPACE_DESCRIPTOR;d->Len=sizeof(*d)-3;d->AddrSpaceGranularity=32;
  d->AddrRangeMin=GU_BAR0_BASE;d->AddrRangeMax=(UINT64)GU_BAR0_BASE+0xffffff;d->AddrLen=0x1000000;
  p[sizeof(*d)]=ACPI_END_TAG_DESCRIPTOR;
  if(Mode==G_BAD_RESOURCE)d->AddrLen=0x800000;
  if(Mode==G_BAD_CHECKSUM){UINT8 sum=0;for(UINTN i=0;i<sizeof(*d)+1;i++)sum=(UINT8)(sum+p[i]);p[sizeof(*d)+1]=(UINT8)(1-sum);if(!p[sizeof(*d)+1])p[sizeof(*d)+1]=2;}
  *out=p;return EFI_SUCCESS;
}
static int AttributeGets,AttributeOthers;
static EFI_STATUS EFIAPI Attr(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION op,UINT64 mask,UINT64 *result) {
  (void)io;
  if(op!=EfiPciIoAttributeOperationGet || mask || !result){AttributeOthers++;return EFI_UNSUPPORTED;}
  AttributeGets++;AttributeConfigReads=S.CfgReads;AttributeReturned=TRUE;
  *result=0x300; /* Even a failed read may leave a plausible output value. */
  return Mode==G_ATTR_FAIL?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI GenericLocate(EFI_LOCATE_SEARCH_TYPE search,EFI_GUID *g,VOID *key,UINTN *n,EFI_HANDLE **out) {
  if(g && !memcmp(g,&WantSfs,sizeof(*g))){SfsSearches++;return EFI_DEVICE_ERROR;}
  if(g && !memcmp(g,&WantBind,sizeof(*g))) {
    *out=malloc(sizeof(EFI_HANDLE));if(!*out)return EFI_OUT_OF_RESOURCES;
    (*out)[0]=BindingHandle;*n=1;return EFI_SUCCESS;
  }
  if(g && !memcmp(g,&WantPci,sizeof(*g)) && (Mode==G_MULTI || Mode==G_MIXED_MULTI || Mode==G_MULTI_SEGMENT)) {
    *out=malloc(2*sizeof(EFI_HANDLE));if(!*out)return EFI_OUT_OF_RESOURCES;
    (*out)[0]=PciHandle;(*out)[1]=(VOID *)4;*n=2;return EFI_SUCCESS;
  }
  return Locate(search,g,key,n,out);
}
static EFI_STATUS EFIAPI GenericHandle(EFI_HANDLE h,EFI_GUID *g,VOID **out) {
  EFI_GUID wantImage=EFI_LOADED_IMAGE_PROTOCOL_GUID;
  if(h==BindingHandle && !memcmp(g,&WantBind,sizeof(*g))){*out=&Binding;return EFI_SUCCESS;}
  if(h==ImageHandle && !memcmp(g,&wantImage,sizeof(*g))){*out=&Image;return EFI_SUCCESS;}
  if(Mode==G_LOST_HANDLE && S.IdReads && h==PciHandle && !memcmp(g,&WantPci,sizeof(*g))) {
    *out=NULL;return EFI_NOT_FOUND;
  }
  if(h==(VOID *)4 && !memcmp(g,&WantPci,sizeof(*g))){*out=Mode==G_MULTI_SEGMENT?&ChangedPci:&Pci;return EFI_SUCCESS;}
  if(Mode==G_PROTOCOL_CHANGED && h==PciHandle && S.PromAllocs){*out=NULL;return EFI_NOT_FOUND;}
  if(Mode==G_ATTR_INTERFACE && AttributeReturned && h==PciHandle && !memcmp(g,&WantPci,sizeof(*g))) {
    *out=&ChangedPci;return EFI_SUCCESS;
  }
  if(Mode==G_INITIAL_INTERFACE && S.IdReads && h==PciHandle && !memcmp(g,&WantPci,sizeof(*g))) {
    *out=&ChangedPci;return EFI_SUCCESS;
  }
  return Handle(h,g,out);
}
static EFI_STATUS EFIAPI DumpWrite(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *data) {
  (void)f;(void)data;Dumps++;if(Mode==G_SHORT_WRITE)(*n)--;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI DumpFlush(EFI_FILE_PROTOCOL *f){(void)f;return Mode==G_FLUSH?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI DumpClose(EFI_FILE_PROTOCOL *f){(void)f;return Mode==G_CLOSE?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI BorrowedClose(EFI_FILE_PROTOCOL *f){(void)f;RootCloses++;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI DumpOpen(EFI_FILE_PROTOCOL *f,EFI_FILE_PROTOCOL **out,CHAR16 *name,UINT64 mode,UINT64 attrs) {
  (void)attrs;
  const CHAR16 prom[]=L"\\RTX3090-UNLOCK\\PROM.BIN";
  UINTN at=0;while(prom[at] && prom[at]==name[at])at++;
  BOOLEAN isProm=prom[at]==name[at];
  BOOLEAN exists=(Mode==G_EXISTING_PROM && isProm) || (Mode==G_EXISTING_IMG && !isProm);
  if(!(mode&(EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE))) {
    *out=NULL;
    if(Mode==G_DUMP_READ_ERROR)return EFI_DEVICE_ERROR;
    if(exists){*out=&DumpFile;return EFI_SUCCESS;}
    return EFI_NOT_FOUND;
  }
  if(exists)ExistingWriteAttempts++;
  if(f==&RootFile)RootOpens++;else ForeignOpens++;
  *out=&DumpFile;return EFI_SUCCESS;
}
static void GenericSetup(int mode) {
  Setup(SC_OK);Mode=mode;RootCloses=RootOpens=ForeignOpens=SfsSearches=Dumps=0;
  ActiveCapture=NULL;Drifted=ReadsAfterDrift=0;
  ExistingWriteAttempts=0;
  Pci.Pci.Read=GenericRead;Pci.GetBarAttributes=Bar;Pci.GetLocation=GenericLocation;
  Pci.Attributes=Attr;AttributeGets=AttributeOthers=0;
  AttributeReturned=FALSE;AttributeConfigReads=0;
  TestSegment=0;TestBus=GU_TEST_PROBE_BUS;TestId=GU_BOARD_ID;LocationCalls=0;ChangedPci=Pci;
  Bs.LocateHandleBuffer=GenericLocate;Bs.HandleProtocol=GenericHandle;
  memset(&Binding,0,sizeof(Binding));Binding.ImageHandle=ImageHandle;
  memset(&Image,0,sizeof(Image));Image.ImageBase=ImageBytes;Image.ImageSize=sizeof(ImageBytes);
  DumpFile=LogFile;DumpFile.Write=DumpWrite;DumpFile.Flush=DumpFlush;DumpFile.Close=DumpClose;
  RootFile.Open=DumpOpen;RootFile.Close=BorrowedClose;ForeignRoot=RootFile;
}
static EFI_STATUS GenericRun(int mode,BOOLEAN withRoot) {
  GenericSetup(mode);
  EFI_STATUS s=GuCaptureGraphOnVolume(&Table,&LogFile,withRoot?&RootFile:NULL);
  Check(SfsSearches==0 && ForeignOpens==0,"never discovers or writes another volume");
  Check(RootCloses==0,"borrowed root is never closed");
  return s;
}
#ifdef GU_PROBE_PARENT_FIXTURE
#define main GenericProbeMain
#endif
int main(void) {
#ifdef GU_PROBE_GENERIC
  GenericSetup(G_OK);TestId=0x999910deU;
  Check(GuCaptureGraphOnVolume(&Table,&LogFile,&RootFile)==EFI_NOT_READY,"consistent unknown NVIDIA identity is inventory only");
  Check(S.RegReads==0 && S.PromReads==0 && !AttributeOthers,"auto discovery never enables unknown GPU register-map access");
  Check(!Logged("capture-complete=1"),"unknown deep-read identity cannot claim complete capture");
#endif
  Check(GenericRun(G_OK,TRUE)==EFI_SUCCESS,"reviewed generic envelope succeeds");
  Check(Logged("pci-bar0-resource raw=8a2b00000000"),"validated BAR descriptor bytes are recorded before the final target recheck");
  Check(S.RegReads==16 && S.PromReads==PROM_DWORDS,"reviewed generic capture reads bounded windows");
#if defined(GU_PROBE_GENERIC) || defined(GU_PROBE_DUMP_PROM)
  Check(RootOpens==2 && Dumps==2,"PROM and image use validated launch root");
#else
  Check(RootOpens==1 && Dumps==1,"hash-only profile writes images to validated launch root");
#endif
  Check(Logged("capture-complete=1"),"complete generic capture recorded");
  Check(AttributeGets==1 && !AttributeOthers,"PciIo attributes are read once, Get only; never set, enabled or disabled");
  Check(Logged("pci-attributes status=0x0000000000000000 value=0x0000000000000300"),"PciIo attribute cache recorded for the generator");
  Check(S.CfgReads>AttributeConfigReads,"raw configuration is rechecked after Attributes returns");
  const int attributeDrifts[]={G_ATTR_MEM,G_ATTR_BME,G_ATTR_CONFIG,G_ATTR_BDF,G_ATTR_INTERFACE,G_ATTR_SEGMENT};
  for(UINTN i=0;i<sizeof(attributeDrifts)/sizeof(attributeDrifts[0]);i++) {
    Check(GenericRun(attributeDrifts[i],TRUE)==EFI_NOT_READY,"Attributes callback drift fails closed");
    Check(AttributeGets==1 && !AttributeOthers,"drift follows exactly one Get and no attribute writes");
    Check(S.RegReads==0 && S.PromReads==0,"Attributes callback drift prevents all MMIO and PROM reads");
    Check(!Logged("capture-complete=1"),"drifted capture is never complete");
  }
  Check(GenericRun(G_ATTR_FAIL,TRUE)==EFI_SUCCESS,"failed attribute observation does not fabricate an overall probe failure");
  Check(Logged("pci-attributes status=0x8000000000000007 value=0x0000000000000300"),
        "failed Get retains its status despite a plausible partial result; generator must reject it");
  GenericSetup(G_OK);Pci.Attributes=NULL;
  Check(GuCaptureGraphOnVolume(&Table,&LogFile,&RootFile)==EFI_SUCCESS && Logged("pci-attributes status=0x8000000000000003"),
        "a missing Attributes method is recorded as unsupported, not invented");
  const int refuse[]={G_UNKNOWN,G_BME,G_MEM_OFF,G_BAD_BAR,G_BAD_RESOURCE,G_CHANGED,G_MULTI,G_OTHER_BDF,G_PROTOCOL_CHANGED,G_CFG_FAIL,G_BAD_CHECKSUM};
  for(UINTN i=0;i<sizeof(refuse)/sizeof(refuse[0]);i++) {
    Check(GenericRun(refuse[i],TRUE)==EFI_NOT_READY,"unreviewed or changed state fails closed");
    Check(S.RegReads==0 && S.PromReads==0,"refused generic target performs no MMIO or PROM reads");
    Check(!AttributeOthers,"a refused target never has its attributes changed");
    Check(!Logged("capture-complete=1"),"refusal cannot claim complete capture");
  }
#ifdef GU_PROBE_GENERIC
  const int inventoryRefuse[]={G_BUS_ZERO,G_BUS_OVERFLOW,G_SEGMENT,G_DEVICE,G_FUNCTION,G_LOCATION_FAIL,
    G_INVENTORY_READ_FAIL,G_INVENTORY_CLASS_FAIL,G_INITIAL_INTERFACE,G_INITIAL_ID,G_INITIAL_CLASS,G_INITIAL_BUS,
    G_MIXED_MULTI,G_NO_DISPLAY,G_LOST_HANDLE,G_LOST_ID,G_LOST_VENDOR,G_LOST_CLASS,G_LOST_CLASS_READ,
    G_INITIAL_SEGMENT,G_MULTI_SEGMENT};
  for(UINTN i=0;i<sizeof(inventoryRefuse)/sizeof(inventoryRefuse[0]);i++) {
    Check(GenericRun(inventoryRefuse[i],TRUE)==EFI_NOT_READY,"invalid or changed initial inventory fails closed");
    Check(!S.RegReads && !S.PromReads,"bad inventory permits no deep reads");
    Check(!Logged("capture-complete=1"),"bad inventory cannot claim a complete capture");
  }
  const UINT32 reviewedIds[]={0x220310deU,0x248710deU,0x250410deU,0x248410deU,0x24c910deU,0x220a10deU};
  for(UINTN i=0;i<sizeof(reviewedIds)/sizeof(reviewedIds[0]);i++) {
    GenericSetup(G_OK);TestId=reviewedIds[i];
    Check(GuCaptureGraphOnVolume(&Table,&LogFile,&RootFile)==EFI_SUCCESS,"reviewed GA10x capture identity succeeds");
    Check(S.RegReads==16 && S.PromReads==PROM_DWORDS,"each reviewed identity retains bounded deep reads");
  }
  const UINTN segments[]={1,0xffffU};
  for(UINTN i=0;i<sizeof(segments)/sizeof(segments[0]);i++) {
    GenericSetup(G_OK);TestSegment=segments[i];
    Check(GuCaptureGraphOnVolume(&Table,&LogFile,&RootFile)==EFI_SUCCESS,"generic discovery accepts a unique 16-bit segment target");
    Check(S.RegReads==16 && S.PromReads==PROM_DWORDS && !AttributeOthers,"nonzero segment retains bounded observation-only access");
    char observed[64];snprintf(observed,sizeof(observed),"probe-target seg=0x%04x",(unsigned)segments[i]);
    Check(Logged(observed),"captured segment is recorded without truncation");
    GenericSetup(G_ATTR_SEGMENT);TestSegment=segments[i];
    Check(GuCaptureGraphOnVolume(&Table,&LogFile,&RootFile)==EFI_NOT_READY && !S.RegReads && !S.PromReads,
          "nonzero segment drift after attribute callback permits no deep reads");
  }
#ifdef GU_PROBE_AUTO_BUS
  const UINTN buses[]={1,0x2b,255};
  for(UINTN i=0;i<sizeof(buses)/sizeof(buses[0]);i++) {
    GenericSetup(G_OK);TestBus=buses[i];
    Check(GuCaptureGraphOnVolume(&Table,&LogFile,&RootFile)==EFI_SUCCESS,"auto mode discovers a unique nonzero bus");
    Check(S.RegReads==16 && S.PromReads==PROM_DWORDS,"auto discovery retains bounded reads");
  }
#else
  GenericSetup(G_OK);TestBus=GU_PROBE_BUS==255?1:GU_PROBE_BUS+1;
  Check(GuCaptureGraphOnVolume(&Table,&LogFile,&RootFile)==EFI_NOT_READY,"explicit bus mismatch has no auto fallback");
  Check(!S.RegReads && !S.PromReads,"explicit mismatch permits no deep reads");
#endif
#endif
#ifndef GU_PROBE_GENERIC
  GenericSetup(G_OK);TestSegment=1;
  Check(GuCaptureGraphOnVolume(&Table,&LogFile,&RootFile)==EFI_NOT_READY && !S.RegReads && !S.PromReads,
        "named profile observation remains pinned to segment zero");
#endif
  for(int m=G_SHORT_WRITE;m<=G_CLOSE;m++) {
    Check(GenericRun(m,TRUE)==EFI_NOT_READY,"dump write/flush/close failure invalidates capture");
    Check(!Logged("capture-complete=1"),"failed dump never claims complete capture");
  }
  Check(GenericRun(G_OK,FALSE)==EFI_NOT_READY,"missing root never uses a prior root or another filesystem");
  Check(RootOpens==0 && Dumps==0,"capture without root creates no dump");
  Check(GuCaptureGraph(&Table,&LogFile)==EFI_NOT_READY,"legacy graph API cannot inherit the last root");
  Check(RootOpens==0,"legacy API uses no stale root");
#if defined(GU_PROBE_DUMP_PROM) || defined(GU_PROFILE_H610M_3060) || defined(GU_PROFILE_PROBE_H610M_3060)
  Check(GenericRun(G_EXISTING_IMG,TRUE)==EFI_NOT_READY,"existing image capture invalidates new capture");
  Check(ExistingWriteAttempts==0,"existing image capture is never opened for write");
  Check(GenericRun(G_DUMP_READ_ERROR,TRUE)==EFI_NOT_READY,"unreadable capture file fails closed");
  Check(RootOpens==0 && ExistingWriteAttempts==0,"file presence error permits no new dump write");
#ifdef GU_PROBE_DUMP_PROM
  Check(GenericRun(G_EXISTING_PROM,TRUE)==EFI_NOT_READY,"existing PROM invalidates new capture");
  Check(ExistingWriteAttempts==0,"existing PROM is never opened for write");
#endif
#endif
  GenericSetup(G_OK);
  static CAPTURE capture;memset(&capture,0,sizeof(capture));
  capture.Table=&Table;capture.File=&LogFile;capture.DumpRoot=&RootFile;capture.Complete=TRUE;capture.Io=EFI_SUCCESS;
  ActiveCapture=&capture;LogFile.Write=DriftLog;Pci.Mem.Read=GuardedMem;
  Check(GuProbeGpu(&Table,&capture)==EFI_SUCCESS && capture.Complete,"near-full log buffer capture succeeds");
  Check(S.PromReads==PROM_DWORDS && ReadsAfterDrift==0 && !Drifted,"log callbacks cannot intervene between raw-state check and MMIO batch");
  printf("PASS generic probe checks: %d; RAM-only mock files, no hardware access\n",Checks);
  return 0;
}
