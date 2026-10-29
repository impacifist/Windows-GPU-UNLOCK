#if defined(GU_AD104_DECODE_READ_TEST) && !defined(GU_AD104_PARENT_TEST)
#error Decode read test requires parent test
#endif
#if defined(GU_AD104_PARENT_TEST) && !defined(GU_AD104_OWNER_CHILD_TEST)
#error Parent test requires owner child test
#endif
#if defined(GU_AD104_OWNER_CHILD_TEST) && !defined(GU_AD104_CONSUMERS_TEST)
#error Owner child test requires consumers test
#endif
#if defined(GU_AD104_CONSUMERS_TEST) && !defined(GU_AD104_GOP_TEST)
#error Consumers test requires GOP test
#endif
/* GPL-2.0-only. Read-only GA10x capture for the 3070 Ti headless port.
 * PCI config reads, BAR0 Mem.Read, PROM hash, loaded-image dumps only. */
#if defined(GU_AD104_FWSEC_AUDIT_TEST) && !defined(GU_AD104_PROM_FULL_TEST)
#error FWSEC RAM audit requires full PROM capture
#endif
#if defined(GU_AD104_PROM_FULL_TEST) && !defined(GU_AD104_PROM_HEADER_TEST)
#error Full PROM requires header and recovery tests
#endif
#if defined(GU_AD104_PROM_HEADER_TEST) && !defined(GU_AD104_DELAYED_READ_TEST)
#error PROM header requires delayed read recovery
#endif
#if defined(GU_AD104_DELAYED_READ_TEST) && !defined(GU_AD104_DECODE_OFF_TEST)
#error Delayed reads require decode off recovery
#endif
#if defined(GU_AD104_DECODE_OFF_TEST) && !defined(GU_AD104_DECODE_READ_TEST)
#error Decode off requires decode read
#endif
#include <Uefi.h>
#include <Protocol/PciIo.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleFileSystem.h>
#include <IndustryStandard/Acpi.h>
#include "../unlock/sha256.h"
#include "probe.h"
#include "../unlock/machine-profile.h"

#if defined(GU_PROBE_AD104_METADATA) && !defined(GU_PROBE_AD104_MSI)
#error AD104 metadata requires the guarded AD104 diagnostic
#endif
#if defined(GU_PROBE_AD104_FIRMROM) && !defined(GU_PROBE_AD104_METADATA)
#error Firmware ROM capture requires AD104 metadata-only mode
#endif
#if defined(GU_PROBE_AD104_RESOURCES) && !defined(GU_PROBE_AD104_METADATA)
#error Resource observation requires AD104 metadata-only mode
#endif
#if defined(GU_PROBE_AD104_RESOURCES) && defined(GU_PROBE_AD104_FIRMROM)
#error Resource observation is separate from firmware ROM capture
#endif
#if defined(GU_PROBE_AD104_PREFLIGHT) && !defined(GU_PROBE_AD104_RESOURCES)
#error AD104 preflight requires observation-only resource mode
#endif
#if defined(GU_AD104_EDID_TEST) && (!defined(GU_PROBE_AD104_PREFLIGHT) || !defined(GU_DIAGNOSTIC_MBR))
#error EDID test requires exact AD104 preflight and pinned MBR handoff
#endif
#if defined(GU_AD104_OUTPUT_OBSERVATION) && (!defined(GU_PROBE_AD104_PREFLIGHT) || defined(GU_AD104_EDID_TEST))
#error Output observation requires preflight without EDID mutation
#endif
#if defined(GU_AD104_TEXT_TEST) && (!defined(GU_AD104_PCD_CAPTURE) || !defined(GU_DIAGNOSTIC_MBR) || defined(GU_AD104_EDID_TEST))
#error Text test requires PCD capture and pinned MBR, without EDID test
#endif
#if defined(GU_AD104_GOP_TEST) && !defined(GU_AD104_TEXT_TEST)
#error GOP consumer test requires guarded text test
#endif
#if defined(GU_AD104_PCD_CAPTURE) && !defined(GU_AD104_OUTPUT_OBSERVATION)
#error PCD capture requires read-only output observation
#endif

#if defined(GU_PROBE_AD104_MSI) && (!defined(GU_PROBE_GENERIC) || !defined(GU_DIAGNOSTIC_ONLY) || !defined(GU_DIAGNOSTIC_PROBE) || !defined(GU_USB_MEDIA) || defined(GU_FULL_UNLOCK) || defined(GU_UEFI_PORT) || defined(GU_INTERNAL_BOOT) || defined(GU_HEADLESS_RESEARCH) || defined(GU_CONSOLE_PREPARE) || defined(GU_AUTOMATIC_BOOT) || defined(GU_EXPERIMENTAL_3090))
#error AD104 capture is a separate read-only USB diagnostic, never an unlock profile
#endif

#if defined(GU_PROBE_DUMP_PROM) && (!defined(GU_DIAGNOSTIC_ONLY) || !defined(GU_DIAGNOSTIC_PROBE) || !defined(GU_USB_MEDIA) || defined(GU_FULL_UNLOCK) || defined(GU_EXPERIMENTAL_3090))
#error GU_PROBE_DUMP_PROM is only for the observation-only diagnostic image
#endif
#if defined(GU_PROBE_GENERIC) || defined(GU_PROBE_DUMP_PROM) || defined(GU_PROFILE_H610M_3060) || defined(GU_PROFILE_PROBE_H610M_3060) || (defined(GU_DIAGNOSTIC_ONLY) && defined(GU_USB_MEDIA))
#define GU_CHECKED_PROBE 1
#endif
#if defined(GU_PROBE_DUMP_PROM) || defined(GU_PROFILE_H610M_3060) || defined(GU_PROFILE_PROBE_H610M_3060) || (defined(GU_DIAGNOSTIC_ONLY) && defined(GU_USB_MEDIA))
#define GU_PROBE_PRESERVE_CAPTURE 1
#endif
/* Auto discovery is restricted to the profile-less read-only probe. An explicit
 * bus remains a constraint, never a fallback to a different target. */
#if defined(GU_PROBE_AUTO_BUS) && (!defined(GU_PROBE_GENERIC) || !defined(GU_DIAGNOSTIC_ONLY) || !defined(GU_DIAGNOSTIC_PROBE) || defined(GU_PROBE_BUS) || defined(GU_FULL_UNLOCK) || defined(GU_EXPERIMENTAL_3090))
#error GU_PROBE_AUTO_BUS requires an unconstrained generic read-only diagnostic probe
#endif
#ifndef GU_PROBE_BUS
#define GU_PROBE_BUS 1U
#endif
#if GU_PROBE_BUS < 1U || GU_PROBE_BUS > 255U
#error GU_PROBE_BUS must be between 1 and 255
#endif
#if defined(GU_PROBE_BUS) && !defined(GU_DIAGNOSTIC_ONLY) && GU_PROBE_BUS != 1U
#error A non-default GU_PROBE_BUS is only for the read-only diagnostic probe
#endif

VOID Text(CAPTURE *c,const char *s);
VOID Hex(CAPTURE *c,UINT64 v,UINTN digits);
VOID End(CAPTURE *c);
VOID Problem(CAPTURE *c,const char *where,EFI_STATUS s);

#ifdef GU_PROBE_GENERIC
/* Profile-less probe for a new machine: every NVIDIA display controller
 * (vendor 0x10de, class 0x03) is recorded. Read-only diagnostic builds only. */
#if !defined(GU_DIAGNOSTIC_ONLY) || !defined(GU_DIAGNOSTIC_PROBE) || defined(GU_FULL_UNLOCK) || defined(GU_EXPERIMENTAL_3090)
#error GU_PROBE_GENERIC is a read-only diagnostic build only
#endif
static BOOLEAN ProbeTarget(EFI_PCI_IO_PROTOCOL *io,UINT32 id) {
  UINT32 cls=0;
  return (id&0xffffU)==0x10deU && io->Pci.Read(io,EfiPciIoWidthUint32,8,1,&cls)==EFI_SUCCESS && (cls>>24)==3;
}
#else
static BOOLEAN ProbeTarget(EFI_PCI_IO_PROTOCOL *io,UINT32 id) {(void)io;return id==GU_BOARD_ID;}
#endif
static EFI_GUID PciGuidL=EFI_PCI_IO_PROTOCOL_GUID;
static EFI_GUID BindGuidL=EFI_DRIVER_BINDING_PROTOCOL_GUID;
static EFI_GUID ImgGuidL=EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID PathGuidL=EFI_DEVICE_PATH_PROTOCOL_GUID;
static EFI_GUID GopGuidL=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
static EFI_SYSTEM_TABLE *SysT;
static UINTN DumpIndex;
static CHAR16 ImgPrefix[]=L"\\RTX3090-UNLOCK\\IMG";

#ifdef GU_PROBE_PRESERVE_CAPTURE
/* No CREATE flag until absence was confirmed. Never replace an earlier capture.
 * Repeated boots require the operator to archive the complete capture set. */
static BOOLEAN NewDump(CAPTURE *c,EFI_FILE_PROTOCOL *root,CHAR16 *name) {
  EFI_FILE_PROTOCOL *prior=NULL;
  EFI_STATUS s=root->Open(root,&prior,name,EFI_FILE_MODE_READ,0);
  if(s==EFI_NOT_FOUND && !prior)return TRUE;
  if(s==EFI_SUCCESS && prior && prior!=root && prior->Close)prior->Close(prior);
  Problem(c,"dump-exists-or-unreadable",s==EFI_SUCCESS?EFI_ALREADY_STARTED:s);
  return FALSE;
}
#endif

static VOID PH(CAPTURE *c,const char *s,UINT64 v){Text(c,s);Hex(c,v,16);End(c);}

static VOID DumpPath(CAPTURE *c,const EFI_DEVICE_PATH_PROTOCOL *p,UINTN bound) {
  if(!p){Text(c,"img path absent");End(c);return;}
  UINTN at=0;
  for(UINTN nodes=0;nodes<64 && at+4<=bound;nodes++) {
    const UINT8 *b=(const VOID *)p;UINTN n=b[2]+((UINTN)b[3]<<8);
    if(n<4||n>bound-at){Problem(c,"img-path-len",EFI_COMPROMISED_DATA);return;}
    Text(c,"img-path-node raw=");for(UINTN j=0;j<n;j++)Hex(c,b[j],2);End(c);
    at+=n;p=(const VOID *)(b+n);
    if(b[0]==END_DEVICE_PATH_TYPE&&b[1]==END_ENTIRE_DEVICE_PATH_SUBTYPE)return;
  }
}

static VOID DumpImage(CAPTURE *c,EFI_FILE_PROTOCOL *root,EFI_HANDLE img) {
  EFI_LOADED_IMAGE_PROTOCOL *li=NULL;
  if(SysT->BootServices->HandleProtocol(img,&ImgGuidL,(VOID **)&li)!=EFI_SUCCESS||!li)return;
  PH(c,"img base=0x",(UINTN)li->ImageBase);
  PH(c,"img size=0x",li->ImageSize);
  PH(c,"img code-type=0x",li->ImageCodeType);
  DumpPath(c,li->FilePath,4096);
  if(!li->ImageBase||!li->ImageSize||li->ImageSize>4U*1024U*1024U)return;
  if(DumpIndex>=100){Problem(c,"img-count",EFI_BAD_BUFFER_SIZE);return;}
  CHAR16 name[40];UINTN i=DumpIndex++,k=0;
  Text(c,"img dump index=");Hex(c,i,4);End(c);
  while(ImgPrefix[k])name[k]=ImgPrefix[k],k++;
  name[k++]=(CHAR16)('0'+(i/10)%10);name[k++]=(CHAR16)('0'+i%10);
  name[k++]='.';name[k++]='B';name[k++]='I';name[k++]='N';name[k]=0;
#ifdef GU_PROBE_PRESERVE_CAPTURE
  if(!NewDump(c,root,name))return;
#endif
  EFI_FILE_PROTOCOL *f=NULL;
  EFI_STATUS s=root->Open(root,&f,name,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0);
  if(s!=EFI_SUCCESS||!f){Problem(c,"img-open",s);return;}
  if(!f->Write || !f->Close){Problem(c,"img-file-methods",EFI_UNSUPPORTED);return;}
  UINTN n=li->ImageSize;s=f->Write(f,&n,li->ImageBase);
  if(s!=EFI_SUCCESS||n!=li->ImageSize)Problem(c,"img-write",s==EFI_SUCCESS?EFI_END_OF_FILE:s);
  if(!f->Flush || f->Flush(f)!=EFI_SUCCESS)Problem(c,"img-flush",EFI_DEVICE_ERROR);
  if(f->Close(f)!=EFI_SUCCESS)Problem(c,"img-close",EFI_DEVICE_ERROR);
}

static VOID ProbeCaps(CAPTURE *c,EFI_PCI_IO_PROTOCOL *io) {
  UINT32 v=0;UINT8 off=0;
  if(io->Pci.Read(io,EfiPciIoWidthUint8,0x34,1,&off)!=EFI_SUCCESS){Problem(c,"cap-ptr",EFI_DEVICE_ERROR);return;}
  for(UINTN i=0;i<48 && off>=0x40 && !(off&3);i++) {
    UINT8 id=0,next=0;
    if(io->Pci.Read(io,EfiPciIoWidthUint8,off,1,&id)!=EFI_SUCCESS)break;
    if(io->Pci.Read(io,EfiPciIoWidthUint8,off+1,1,&next)!=EFI_SUCCESS)break;
    PH(c,"cap off=0x",off);PH(c,"cap id=0x",id);PH(c,"cap next=0x",next);
    if(io->Pci.Read(io,EfiPciIoWidthUint32,off,1,&v)==EFI_SUCCESS){PH(c,"cap dword0=0x",v);}
    if(io->Pci.Read(io,EfiPciIoWidthUint32,off+4,1,&v)==EFI_SUCCESS){PH(c,"cap dword1=0x",v);}
    if(io->Pci.Read(io,EfiPciIoWidthUint32,off+8,1,&v)==EFI_SUCCESS){PH(c,"cap dword2=0x",v);}
    if(id==0x10){
      if(io->Pci.Read(io,EfiPciIoWidthUint32,off+12,1,&v)==EFI_SUCCESS)PH(c,"pcie dword3=0x",v);
      if(io->Pci.Read(io,EfiPciIoWidthUint32,off+16,1,&v)==EFI_SUCCESS)PH(c,"pcie dword4=0x",v);
      if(io->Pci.Read(io,EfiPciIoWidthUint32,off+20,1,&v)==EFI_SUCCESS)PH(c,"pcie dword5=0x",v);
    }
    if(!next||next==off)break;off=next;
  }
}

/* Host-test fixture only: the same PROM bytes that were hashed, never parsed here. */
#if defined(GU_PROBE_GENERIC) || defined(GU_PROBE_DUMP_PROM)
static VOID DumpProm(CAPTURE *c,EFI_FILE_PROTOCOL *root,UINT8 *page) {
#ifdef GU_PROBE_PRESERVE_CAPTURE
  if(!NewDump(c,root,L"\\RTX3090-UNLOCK\\PROM.BIN"))return;
#endif
  EFI_FILE_PROTOCOL *f=NULL;
  EFI_STATUS s=root->Open(root,&f,L"\\RTX3090-UNLOCK\\PROM.BIN",EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0);
  if(s!=EFI_SUCCESS||!f){Problem(c,"prom-open",s);return;}
  if(!f->Write || !f->Close){Problem(c,"prom-file-methods",EFI_UNSUPPORTED);return;}
  UINTN n=0x100000;s=f->Write(f,&n,page);
  if(s!=EFI_SUCCESS||n!=0x100000)Problem(c,"prom-write",s==EFI_SUCCESS?EFI_END_OF_FILE:s);
  else{Text(c,"prom dump=PROM.BIN");End(c);}
  if(!f->Flush || f->Flush(f)!=EFI_SUCCESS)Problem(c,"prom-flush",EFI_DEVICE_ERROR);
  if(f->Close(f)!=EFI_SUCCESS)Problem(c,"prom-close",EFI_DEVICE_ERROR);
}
#endif

#ifdef GU_CHECKED_PROBE
/* Inventory never implies permission to read another GPU's register map.
 * Generic reads accept the reviewed GA102, GA104 and GA106 identities;
 * named probes pin their own
 * PCI/subsystem and require the same 16 MiB, MEM1/BME0 resource envelope.
 * All allocation/protocol calls finish before the final identity/config recheck. */
static BOOLEAN ProbeEnvelope(CAPTURE *c,EFI_PCI_IO_PROTOCOL *io,const UINT32 *cfg) {
#ifdef GU_PROBE_GENERIC
#ifdef GU_PROBE_AD104_MSI
  BOOLEAN identity=cfg[0]==0x278210deU && cfg[11]==0x51321462U;
#else
  BOOLEAN identity=cfg[0]==0x220410deU || cfg[0]==0x220310deU ||
    cfg[0]==0x248710deU || cfg[0]==0x250410deU || cfg[0]==0x248410deU;
#endif
#else
  BOOLEAN identity=cfg[0]==GU_BOARD_ID && cfg[11]==GU_BOARD_SUBSYSTEM;
#ifdef GU_BAR0_BASE
  if(cfg[4]!=(UINT32)GU_BAR0_BASE)identity=FALSE;
#endif
#endif
  if(!identity || cfg[2]!=0x030000a1U ||
     (cfg[1]&6U)!=2U || !cfg[4] || (cfg[4]&0xffffffU) || !io->GetBarAttributes || !io->Mem.Read)return FALSE;
  VOID *raw=NULL;
  EFI_STATUS s=io->GetBarAttributes(io,0,NULL,&raw);
  BOOLEAN good=FALSE;
  if(s==EFI_SUCCESS && raw) {
    const EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *d=raw;
    const UINT8 *end=(const UINT8 *)(d+1);
    good=d->Desc==ACPI_ADDRESS_SPACE_DESCRIPTOR && d->Len==sizeof(*d)-3 && !d->ResType &&
      d->AddrSpaceGranularity==32 && d->AddrRangeMin==cfg[4] && d->AddrLen==0x1000000 &&
      !d->AddrTranslationOffset && (d->AddrRangeMax==(UINT64)cfg[4]+0xffffff || d->AddrRangeMax==0xffffff) &&
      end[0]==ACPI_END_TAG_DESCRIPTOR;
    if(good) {
      UINT8 sum=0;for(UINTN j=0;j<sizeof(*d)+2;j++)sum=(UINT8)(sum+((const UINT8 *)d)[j]);
      if(end[1] && sum)good=FALSE;
    }
    if(good) {
      UINT8 observed[sizeof(*d)+2];
      for(UINTN j=0;j<sizeof(observed);j++)observed[j]=((const UINT8 *)d)[j];
      Text(c,"pci-bar0-resource raw=");
      for(UINTN j=0;j<sizeof(observed);j++)Hex(c,observed[j],2);
      End(c);
    }
  }
  if(raw && c->Table->BootServices->FreePool(raw)!=EFI_SUCCESS)good=FALSE;
  return good;
}
static BOOLEAN ProbeUnchanged(CAPTURE *c,EFI_HANDLE handle,EFI_PCI_IO_PROTOCOL *io,const UINT32 *cfg,const UINTN *location) {
  EFI_PCI_IO_PROTOCOL *current=NULL;UINTN seg=0,bus=0,dev=0,fun=0;UINT32 now[64];
  if(c->Table->BootServices->HandleProtocol(handle,&PciGuidL,(VOID **)&current)!=EFI_SUCCESS || current!=io ||
     !io->GetLocation || io->GetLocation(io,&seg,&bus,&dev,&fun)!=EFI_SUCCESS ||
     !location || seg!=location[0] || bus!=location[1] || dev!=location[2] || fun!=location[3] ||
     io->Pci.Read(io,EfiPciIoWidthUint32,0,64,now)!=EFI_SUCCESS)return FALSE;
  for(UINTN j=0;j<64;j++)if(now[j]!=cfg[j])return FALSE;
  return TRUE;
}
#endif
#ifdef GU_PROBE_AD104_FIRMROM
#include "firmware-rom.h"
#endif
#ifdef GU_PROBE_AD104_RESOURCES
#ifdef GU_PROBE_AD104_PREFLIGHT
#include "ad104-preflight.h"
#ifdef GU_AD104_OUTPUT_OBSERVATION
#include "output-observation.h"
#endif
static VOID ObservePreflight(CAPTURE *c,EFI_HANDLE gpu,EFI_PCI_IO_PROTOCOL *io,const UINT32 *cfg) {
  EFI_SYSTEM_TABLE *t=c->Table;PF_STATE first={0},second={0};UINTN loc[4];
  if(!t->ConIn || !t->ConIn->Reset || !t->ConIn->ReadKeyStroke || !t->ConIn->WaitForKey ||
     !io->GetLocation || io->GetLocation(io,&loc[0],&loc[1],&loc[2],&loc[3])!=EFI_SUCCESS ||
     loc[0] || loc[1]!=11 || loc[2] || loc[3] || (cfg[1]&0xffffU)!=7) {
    Problem(c,"ad104-preflight-input-or-location",EFI_NOT_READY);return;
  }
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL *input=t->ConIn;
  EFI_INPUT_RESET reset=input->Reset;EFI_INPUT_READ_KEY read=input->ReadKeyStroke;EFI_EVENT wait=input->WaitForKey;
  EFI_HANDLE keys=t->ConsoleInHandle,out=t->ConsoleOutHandle,error=t->StandardErrorHandle;
  VOID *text=t->ConOut,*err=t->StdErr;
  EFI_STATUS s=PfCollect(t,gpu,PfPins,&first);
  if(s==EFI_SUCCESS)s=PfCollect(t,gpu,PfPins,&second);
  if(s==EFI_SUCCESS && (!PfEqual(&first,&second,sizeof(first)) ||
     first.SharedFlag!=1 || first.PointerCounts[0]!=1 || first.PointerCounts[1]!=1 ||
     t->ConIn!=input || input->Reset!=reset || input->ReadKeyStroke!=read || input->WaitForKey!=wait ||
     t->ConsoleInHandle!=keys || t->ConsoleOutHandle!=out || t->StandardErrorHandle!=error ||
     t->ConOut!=text || t->StdErr!=err || !ProbeUnchanged(c,gpu,io,cfg,loc)))s=EFI_NOT_READY;
  Text(c,"ad104-runtime-preflight topology-status=0x");Hex(c,s,16);
  Text(c," release-authority=0 cold-state-known=0 input-execution-tested=0 resource-admission-separate=1");End(c);
  UINTN roles=0;for(UINTN r=0;r<4;r++)if(first.Agent[r])roles|=1U<<r;
  Text(c,"ad104-preflight-observations roles=0x");Hex(c,roles,1);
  Text(c," null-binding-present=");Hex(c,first.NullBinding?1:0,1);
  Text(c," matched-edges=0x");Hex(c,first.MatchedEdges,2);
  Text(c," first-rejection-step=0x");Hex(c,first.RejectionStep,1);
  Text(c," second-rejection-step=0x");Hex(c,second.RejectionStep,1);
  Text(c," shared-byte=0x");Hex(c,first.SharedFlag,2);
  Text(c," shared-count1=0x");Hex(c,first.PointerCounts[0],16);
  Text(c," shared-count2=0x");Hex(c,first.PointerCounts[1],16);End(c);
  if(s!=EFI_SUCCESS)Problem(c,"ad104-runtime-preflight-refused",s);
#ifdef GU_AD104_OUTPUT_OBSERVATION
  else ObserveOutput(c,&first,gpu,io,cfg,loc);
#endif
}
#endif
static VOID ObserveResources(CAPTURE *c,EFI_HANDLE handle,EFI_PCI_IO_PROTOCOL *io,const UINT32 *cfg) {
  UINTN loc[4];VOID *raw=NULL;UINT64 supports=0;
  if(!io->GetLocation || !io->GetBarAttributes ||
     io->GetLocation(io,&loc[0],&loc[1],&loc[2],&loc[3])!=EFI_SUCCESS ||
     loc[0]>65535 || loc[1]>255 || loc[2]>31 || loc[3]>7 ||
     !ProbeUnchanged(c,handle,io,cfg,loc)) {
    Problem(c,"ad104-resource-precheck",EFI_NOT_READY);return;
  }
  EFI_STATUS s=io->GetBarAttributes(io,0,&supports,&raw);
  Text(c,"ad104-bar0-query status=0x");Hex(c,s,16);End(c);
  if(s==EFI_SUCCESS && raw && ProbeUnchanged(c,handle,io,cfg,loc)) {
    /* GetBarAttributes supplies a pool-allocated ACPI descriptor list under
     * the firmware protocol contract. Accept exactly one QWORD descriptor. */
    EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *d=raw;
    if(d->Desc==ACPI_ADDRESS_SPACE_DESCRIPTOR && d->Len==sizeof(*d)-3) {
      const UINT8 *end=(const UINT8 *)(d+1);UINT8 sum=0;
      for(UINTN i=0;i<sizeof(*d)+2;i++)sum=(UINT8)(sum+((UINT8 *)raw)[i]);
      BOOLEAN valid=end[0]==ACPI_END_TAG_DESCRIPTOR && (!end[1] || !sum);
      BOOLEAN shape=valid && !d->ResType && d->AddrSpaceGranularity==32 &&
        d->AddrRangeMin==cfg[4] && d->AddrLen==0x1000000 && !d->AddrTranslationOffset &&
        (d->AddrRangeMax==(UINT64)cfg[4]+0xffffff || d->AddrRangeMax==0xffffff);
      Text(c,"ad104-bar0-resource descriptor-valid=");Hex(c,valid,1);
      Text(c," envelope-shape=");Hex(c,shape,1);
      Text(c," type=0x");Hex(c,d->ResType,2);
      Text(c," granularity=0x");Hex(c,d->AddrSpaceGranularity,16);
      Text(c," minimum=0x");Hex(c,d->AddrRangeMin,16);
      Text(c," maximum=0x");Hex(c,d->AddrRangeMax,16);
      Text(c," length=0x");Hex(c,d->AddrLen,16);
      Text(c," translation=0x");Hex(c,d->AddrTranslationOffset,16);
      Text(c," supports=0x");Hex(c,supports,16);
      Text(c," observation-only=1 unlock-eligibility=0");End(c);
    } else Problem(c,"ad104-resource-format",EFI_UNSUPPORTED);
  } else Problem(c,"ad104-resource-unavailable-or-changed",EFI_NOT_READY);
  if(raw && c->Table->BootServices->FreePool(raw)!=EFI_SUCCESS)Problem(c,"ad104-resource-free",EFI_DEVICE_ERROR);
  /* Get/Supported are query operations. Never call Set/Enable/Disable here. */
  if(!io->Attributes || !ProbeUnchanged(c,handle,io,cfg,loc)) {
    Problem(c,"ad104-attributes-precheck",EFI_NOT_READY);return;
  }
  UINT64 current=0,supported=0;
  EFI_STATUS got=io->Attributes(io,EfiPciIoAttributeOperationGet,0,&current);
  EFI_STATUS support=io->Attributes(io,EfiPciIoAttributeOperationSupported,0,&supported);
  if(!ProbeUnchanged(c,handle,io,cfg,loc)) {
    Problem(c,"ad104-attributes-changed",EFI_NOT_READY);return;
  }
  Text(c,"ad104-pci-attributes get-status=0x");Hex(c,got,16);
  Text(c," supported-status=0x");Hex(c,support,16);
  if(got==EFI_SUCCESS && support==EFI_SUCCESS) {
    Text(c," current=0x");Hex(c,current,16);
    Text(c," supported=0x");Hex(c,supported,16);
  }
  Text(c," query-only=1 unlock-eligibility=0");End(c);
}
#endif
#ifdef GU_PROBE_AD104_MSI
/* Read set reviewed against gpu-unlock-ada@4e7fefcc, AD104 sample.c.
 * No ownership changes, firmware execution, polling or write method exists.
 * Legal discovered BDFs are latched; Windows enumeration is not assumed. */
static VOID ProbeAd104Bar(CAPTURE *c,EFI_PCI_IO_PROTOCOL *io,EFI_FILE_PROTOCOL *root,
                         EFI_HANDLE handle,const UINT32 *cfg) {
  static const UINT32 offsets[]={0,0x118128,0x118234,0x110100,0x1fa824,0x1fa828,
    0x1fa7cc,0x1fa7c4,0x9a0148,0x823804,0x823814,0x823818,0x82381c,0x823820,
    0x1183a4,0x625f04,0x1fa7c8,0x1fa82c,0x1fa830,0x824148,0x8241e0,0x840100};
  UINTN location[4];UINT8 *page=NULL;UINT32 values[22],chip=0,signature=0;
  EFI_STATUS s;
#ifdef GU_PROBE_AD104_METADATA
  if(cfg[0]!=0x278210deU || cfg[11]!=0x51321462U || cfg[2]!=0x030000a1U) {
    Problem(c,"ad104-metadata-identity",EFI_UNSUPPORTED);return;
  }
  /* Metadata alone never dereferences RomImage. The optional FIRMROM extension
   * allocates and reads admitted RAM; neither path calls GPU MMIO methods. */
  Text(c,"ad104-metadata-only=1 rom-pointer-present=");Hex(c,io->RomImage?1:0,1);
  Text(c," rom-bytes=0x");Hex(c,io->RomSize,16);
  Text(c," command=0x");Hex(c,cfg[1]&0xffffU,4);
  Text(c," memory-enabled=");Hex(c,(cfg[1]>>1)&1U,1);
  Text(c," bus-master-enabled=");Hex(c,(cfg[1]>>2)&1U,1);End(c);
#ifdef GU_PROBE_AD104_FIRMROM
  FirmwareRom(c,handle,io,root,cfg);
#endif
#ifdef GU_PROBE_AD104_RESOURCES
  ObserveResources(c,handle,io,cfg);
#endif
#ifdef GU_PROBE_AD104_PREFLIGHT
  ObservePreflight(c,handle,io,cfg);
#endif
  Problem(c,"ad104-metadata-only-no-mmio",EFI_NOT_READY);return;
#endif
  Text(c,"ad104-admission command=0x");Hex(c,cfg[1]&0xffffU,4);
  Text(c," memory-enabled=");Hex(c,(cfg[1]>>1)&1U,1);
  Text(c," bus-master-enabled=");Hex(c,(cfg[1]>>2)&1U,1);End(c);
  if(!io->GetLocation || io->GetLocation(io,&location[0],&location[1],&location[2],&location[3])!=EFI_SUCCESS ||
     location[0]>65535 || location[1]>255 || location[2]>31 || location[3]>7 || !ProbeEnvelope(c,io,cfg)) {
    Problem(c,"ad104-envelope",EFI_UNSUPPORTED);return;
  }
  s=c->Table->BootServices->AllocatePool(EfiBootServicesData,0x100000,(VOID **)&page);
  if(s!=EFI_SUCCESS || !page){Problem(c,"ad104-alloc",EFI_OUT_OF_RESOURCES);return;}
  if(!ProbeUnchanged(c,handle,io,cfg,location)){s=EFI_NOT_READY;goto done;}
  s=io->Mem.Read(io,EfiPciIoWidthUint32,0,0,1,&values[0]);
  if(s!=EFI_SUCCESS || values[0]!=0x194000a1U){s=EFI_UNSUPPORTED;goto done;}
  for(UINTN i=1;i<22;i++) {
    s=io->Mem.Read(io,EfiPciIoWidthUint32,0,offsets[i],1,&values[i]);
    if(s!=EFI_SUCCESS || values[i]==MAX_UINT32 || (values[i]&0xffff0000U)==0xbadf0000U){s=EFI_DEVICE_ERROR;goto done;}
  }
  s=io->Mem.Read(io,EfiPciIoWidthUint32,0,0x300000,1,&signature);
  if(s!=EFI_SUCCESS || (signature&0xffffU)!=0xaa55U){s=EFI_COMPROMISED_DATA;goto done;}
  for(UINTN i=0;i<0x100000/4;i++) {
    s=io->Mem.Read(io,EfiPciIoWidthUint32,0,0x300000+i*4,1,((UINT32 *)page)+i);
    if(s!=EFI_SUCCESS)goto done;
  }
  if(((UINT32 *)page)[0]!=signature){s=EFI_NOT_READY;goto done;}
  s=io->Mem.Read(io,EfiPciIoWidthUint32,0,0,1,&chip);
  if(s!=EFI_SUCCESS || chip!=values[0] || !ProbeUnchanged(c,handle,io,cfg,location)) {s=EFI_NOT_READY;goto done;}
  /* Only complete, stable observations are published as a usable capture. */
  Text(c,"ad104-read-only-capture=1 unlock-eligibility=0");End(c);
  for(UINTN i=0;i<22;i++) {
    Text(c,"mmio off=0x");Hex(c,offsets[i],8);Text(c," status=0x");Hex(c,EFI_SUCCESS,16);
    Text(c," value=0x");Hex(c,values[i],8);End(c);
  }
  UINT8 digest[32];GuSha256(page,0x100000,digest);
  Text(c,"prom bytes=0x00100000 sha256=");for(UINTN i=0;i<32;i++)Hex(c,digest[i],2);End(c);
  if(root)DumpProm(c,root,page);else s=EFI_NOT_FOUND;
done:
  if(s!=EFI_SUCCESS)Problem(c,"ad104-capture-incomplete",s);
  if(c->Table->BootServices->FreePool(page)!=EFI_SUCCESS)Problem(c,"ad104-free",EFI_DEVICE_ERROR);
}
#endif
static VOID ProbeBar(CAPTURE *c,EFI_PCI_IO_PROTOCOL *io,UINT32 bar,EFI_FILE_PROTOCOL *root,
                     EFI_HANDLE handle,const UINT32 *cfg,UINTN expectedSegment,UINTN expectedBus) {
#ifdef GU_PROBE_AD104_MSI
  (void)expectedSegment;(void)expectedBus;
  (void)bar;ProbeAd104Bar(c,io,root,handle,cfg);
#else
  static const UINT32 offs[]={
    0x000000,0x108154,0x108170,0x108180,0x108354,
    0x118128,0x118234,0x110100,0x1183a4,0x1fa828,
    0x8219c0,0x822804,0x823804,0x82381c,0x823820,0x824140
  };
  UINT8 *page=NULL;EFI_STATUS s;
#ifdef GU_CHECKED_PROBE
  if(!ProbeEnvelope(c,io,cfg)){Problem(c,"probe-unreviewed-envelope",EFI_UNSUPPORTED);return;}
  s=c->Table->BootServices->AllocatePool(EfiBootServicesData,0x100000,(VOID **)&page);
  if(s!=EFI_SUCCESS || !page){Problem(c,"prom-alloc",s==EFI_SUCCESS?EFI_OUT_OF_RESOURCES:s);return;}
  /* The release's CapturePermit requires the PciIo attribute cache to match the raw IO/MEM/BME
   * command bits. Record it here, read-only (Get only), so a local profile can be refused before
   * any release instead of stopping mid-release. A protocol call, so it precedes the recheck. */
  UINT64 pciAttributes=0;
  EFI_STATUS attributesStatus=io->Attributes?io->Attributes(io,EfiPciIoAttributeOperationGet,0,&pciAttributes):EFI_UNSUPPORTED;
  UINTN expectedLocation[]={expectedSegment,expectedBus,0,0};
  if(!ProbeUnchanged(c,handle,io,cfg,expectedLocation)) {
    c->Table->BootServices->FreePool(page);Problem(c,"probe-identity-changed",EFI_NOT_READY);return;
  }
#else
  (void)handle;(void)cfg;(void)expectedSegment;(void)expectedBus;
#endif
#ifdef GU_CHECKED_PROBE
  UINT32 values[sizeof(offs)/sizeof(offs[0])];
  EFI_STATUS statuses[sizeof(offs)/sizeof(offs[0])];
  /* No logging, file I/O, allocation or other firmware-service call between
   * the final raw-state check and this bounded MMIO batch. Log afterwards. */
  for(UINTN i=0;i<sizeof(offs)/sizeof(offs[0]);i++) {
    values[i]=0xdeadbeef;
    statuses[i]=io->Mem.Read(io,EfiPciIoWidthUint32,0,offs[i],1,&values[i]);
  }
  s=EFI_SUCCESS;
  for(UINTN i=0;i<0x100000/4 && s==EFI_SUCCESS;i++)
    s=io->Mem.Read(io,EfiPciIoWidthUint32,0,0x300000+i*4,1,((UINT32*)page)+i);
  EFI_STATUS promStatus=s;
#endif
  Text(c,"probe-bar0 base=0x");Hex(c,bar,16);End(c);
#ifdef GU_CHECKED_PROBE
  Text(c,"pci-attributes status=0x");Hex(c,attributesStatus,16);Text(c," value=0x");Hex(c,pciAttributes,16);End(c);
#endif
  for(UINTN i=0;i<sizeof(offs)/sizeof(offs[0]);i++) {
    UINT32 v=0xdeadbeef;
#ifdef GU_CHECKED_PROBE
    EFI_STATUS s=statuses[i];v=values[i];
#else
    EFI_STATUS s=io->Mem.Read(io,EfiPciIoWidthUint32,0,offs[i],1,&v);
#endif
    Text(c,"mmio off=0x");Hex(c,offs[i],8);Text(c," status=0x");Hex(c,s,16);Text(c," value=0x");Hex(c,v,8);End(c);
  }
  UINT8 digest[32];
#ifndef GU_CHECKED_PROBE
  s=c->Table->BootServices->AllocatePool(EfiBootServicesData,0x100000,(VOID **)&page);
#else
  s=EFI_SUCCESS;
#endif
  if(s==EFI_SUCCESS && page) {
#ifndef GU_CHECKED_PROBE
    for(UINTN i=0;i<0x100000/4 && s==EFI_SUCCESS;i++)
      s=io->Mem.Read(io,EfiPciIoWidthUint32,0,0x300000+i*4,1,((UINT32*)page)+i);
#else
    s=promStatus;
#endif
    if(s==EFI_SUCCESS) {
      GuSha256(page,0x100000,digest);
      Text(c,"prom sha256=");for(UINTN i=0;i<32;i++)Hex(c,digest[i],2);End(c);
#if defined(GU_PROBE_GENERIC) || defined(GU_PROBE_DUMP_PROM)
      if(root)DumpProm(c,root,page);else Problem(c,"prom-dump-volume",EFI_NOT_FOUND);
#else
      (void)root; /* Reviewed fixed-profile diagnostics remain hash-only. */
#endif
    } else { Problem(c,"prom-read",s); }
    if(c->Table->BootServices->FreePool(page)!=EFI_SUCCESS)Problem(c,"prom-free",EFI_DEVICE_ERROR);
  } else Problem(c,"prom-alloc",s==EFI_SUCCESS?EFI_OUT_OF_RESOURCES:s);
#endif
}

static VOID ProbeDrivers(CAPTURE *c,EFI_FILE_PROTOCOL *root) {
  EFI_HANDLE *hs=NULL;UINTN n=0;
  if(SysT->BootServices->LocateHandleBuffer(ByProtocol,&BindGuidL,NULL,&n,&hs)!=EFI_SUCCESS||!hs)return;
  Text(c,"driver-bindings=");Hex(c,n,8);End(c);
  for(UINTN i=0;i<n && c->Io==EFI_SUCCESS;i++) {
    EFI_DRIVER_BINDING_PROTOCOL *d=NULL;
    if(SysT->BootServices->HandleProtocol(hs[i],&BindGuidL,(VOID **)&d)!=EFI_SUCCESS||!d)continue;
    Text(c,"binding handle=0x");Hex(c,(UINTN)hs[i],12);Text(c," image=0x");Hex(c,(UINTN)d->ImageHandle,12);
    Text(c," version=0x");Hex(c,d->Version,8);Text(c," stop=0x");Hex(c,(UINTN)d->Stop,12);End(c);
    if(d->ImageHandle)DumpImage(c,root,d->ImageHandle);
  }
  SysT->BootServices->FreePool(hs);
}

static VOID ProbeGops(CAPTURE *c) {
  EFI_HANDLE *hs=NULL;UINTN n=0;
  EFI_STATUS s=SysT->BootServices->LocateHandleBuffer(ByProtocol,&GopGuidL,NULL,&n,&hs);
  if(s==EFI_NOT_FOUND){Text(c,"gops=0");End(c);return;}
  if(s!=EFI_SUCCESS||!hs){Problem(c,"gop-enum",s);return;}
  Text(c,"gops=");Hex(c,n,4);End(c);
  for(UINTN i=0;i<n;i++) {
    EFI_DEVICE_PATH_PROTOCOL *dp=NULL;
    EFI_STATUS ps=SysT->BootServices->HandleProtocol(hs[i],&PathGuidL,(VOID **)&dp);
    Text(c,"gop handle=0x");Hex(c,(UINTN)hs[i],12);
    Text(c," path-status=0x");Hex(c,ps,16);Text(c," has-path=");Hex(c,dp?1:0,1);End(c);
    DumpPath(c,dp,4096);
  }
  SysT->BootServices->FreePool(hs);
  Text(c,"console-out=0x");Hex(c,(UINTN)SysT->ConsoleOutHandle,12);
  Text(c," stderr=0x");Hex(c,(UINTN)SysT->StandardErrorHandle,12);
  Text(c," conin=0x");Hex(c,(UINTN)SysT->ConsoleInHandle,12);End(c);
}

EFI_STATUS GuProbeGpu(EFI_SYSTEM_TABLE *t,CAPTURE *c) {
  SysT=t;
  DumpIndex=0;
  EFI_FILE_PROTOCOL *root=c->DumpRoot;
  if(root && !root->Open){Problem(c,"dump-volume",EFI_INVALID_PARAMETER);root=NULL;}
  EFI_BOOT_SERVICES *bs=t->BootServices;EFI_HANDLE *hs=NULL;UINTN n=0,found=0,cfgFailed=0;
  EFI_STATUS cfgError=EFI_SUCCESS;
  EFI_STATUS s=bs->LocateHandleBuffer(ByProtocol,&PciGuidL,NULL,&n,&hs);
  if(s!=EFI_SUCCESS||!hs){Problem(c,"pci-enum",s);return s;}
  if(n>1024){bs->FreePool(hs);Problem(c,"pci-count",EFI_BAD_BUFFER_SIZE);return EFI_BAD_BUFFER_SIZE;}
#ifdef GU_CHECKED_PROBE
  UINTN candidates=0;BOOLEAN inventoryOk=TRUE;
#if defined(GU_PROBE_GENERIC) && !defined(GU_PROBE_AD104_MSI)
  EFI_HANDLE targetHandle=NULL;EFI_PCI_IO_PROTOCOL *targetIo=NULL;
  UINT32 targetId=0,targetClass=0;UINTN targetSegment=0,targetBus=0;
  BOOLEAN targetValidated=FALSE;
#endif
  /* Count all display targets before any MMIO/PROM access. Multiple devices
   * cannot share a single PROM.BIN or be used to generate one local profile. */
  for(UINTN i=0;i<n;i++) {
    EFI_PCI_IO_PROTOCOL *io=NULL;UINT32 id=0,cls=0;
    if(bs->HandleProtocol(hs[i],&PciGuidL,(VOID **)&io)!=EFI_SUCCESS || !io || !io->Pci.Read ||
       io->Pci.Read(io,EfiPciIoWidthUint32,0,1,&id)!=EFI_SUCCESS){inventoryOk=FALSE;continue;}
    if((id&0xffffU)!=0x10deU)continue;
    if(io->Pci.Read(io,EfiPciIoWidthUint32,8,1,&cls)!=EFI_SUCCESS){inventoryOk=FALSE;continue;}
    if((cls>>24)!=3)continue;
    candidates++;
#if defined(GU_PROBE_GENERIC) && !defined(GU_PROBE_AD104_MSI)
    UINTN seg=0,bus=0,dev=0,fun=0;
    if(!io->GetLocation || io->GetLocation(io,&seg,&bus,&dev,&fun)!=EFI_SUCCESS ||
       seg>0xffffU || !bus || bus>255 || dev || fun){inventoryOk=FALSE;continue;}
#ifndef GU_PROBE_AUTO_BUS
    if(bus!=GU_PROBE_BUS){inventoryOk=FALSE;continue;}
#endif
    targetHandle=hs[i];targetIo=io;targetId=id;targetClass=cls;targetSegment=seg;targetBus=bus;
#endif
  }
  if(!inventoryOk || candidates!=1)Problem(c,"probe-inventory",EFI_UNSUPPORTED);
#endif
  Text(c,"probe-pci-handles=");Hex(c,n,8);End(c);
  for(UINTN i=0;i<n && c->Io==EFI_SUCCESS;i++) {
    EFI_PCI_IO_PROTOCOL *io=NULL;UINT32 id=0;
    if(bs->HandleProtocol(hs[i],&PciGuidL,(VOID **)&io)!=EFI_SUCCESS||!io)continue;
    if(!io->Pci.Read || !io->GetLocation){Problem(c,"pci-methods",EFI_UNSUPPORTED);continue;}
    if(io->Pci.Read(io,EfiPciIoWidthUint32,0,1,&id)!=EFI_SUCCESS)continue;
    if(!ProbeTarget(io,id))continue;
    found++;
    UINTN seg=0,bus=0,dev=0,fun=0;
    EFI_STATUS locationStatus=io->GetLocation(io,&seg,&bus,&dev,&fun);
    if(locationStatus==EFI_SUCCESS) {
      Text(c,"probe-target seg=0x");Hex(c,seg,4);Text(c," bus=0x");Hex(c,bus,4);
      Text(c," dev=0x");Hex(c,dev,4);Text(c," fun=0x");Hex(c,fun,4);End(c);
    }
    UINT32 cfg[64];
    s=io->Pci.Read(io,EfiPciIoWidthUint32,0,64,cfg);
    PH(c,"cfg256 status=0x",s);
    if(s!=EFI_SUCCESS) {
      /* The BAR base, subsystem id, capability walk and PROM hash all derive
       * from this read. Firmware may return an error after writing part of the
       * buffer, so cfg holds no trustworthy value: record the failure and skip
       * every dependent capture for this device. */
      Problem(c,"cfg256",s);cfgFailed++;
      if(cfgError==EFI_SUCCESS)cfgError=s;
      continue;
    }
    Text(c,"cfg dwords=");for(UINTN j=0;j<64;j++){Hex(c,cfg[j],8);Text(c," ");}End(c);
    UINT32 sub=0;io->Pci.Read(io,EfiPciIoWidthUint32,0x2c,1,&sub);PH(c,"subsystem=0x",sub);
    ProbeCaps(c,io);
    UINT32 bar=cfg[0x10/4]&~0xfU;
#ifdef GU_CHECKED_PROBE
    if(!inventoryOk || candidates!=1)continue;
#endif
    UINTN expectedSegment=0,expectedBus=GU_PROBE_BUS;
#if defined(GU_PROBE_GENERIC) && !defined(GU_PROBE_AD104_MSI)
    /* Bind this snapshot to the unique device inventoried before external log,
     * resource and allocation callbacks. No re-selection after a mismatch. */
    if(hs[i]!=targetHandle || io!=targetIo || id!=targetId || cfg[0]!=targetId || cfg[2]!=targetClass ||
       locationStatus!=EFI_SUCCESS || seg!=targetSegment || bus!=targetBus || dev || fun) {
      Problem(c,"probe-inventory-changed",EFI_NOT_READY);continue;
    }
    expectedBus=targetBus;
    expectedSegment=targetSegment;
    targetValidated=TRUE;
#endif
    if(bar)ProbeBar(c,io,bar,root,hs[i],cfg,expectedSegment,expectedBus);else Problem(c,"bar0",EFI_UNSUPPORTED);
  }
#if defined(GU_PROBE_GENERIC) && !defined(GU_PROBE_AD104_MSI)
  /* A selected target disappearing or turning non-display during the second
   * pass must not become a successful inventory-only capture. */
  if(inventoryOk && candidates==1 && !targetValidated)Problem(c,"probe-inventory-target-lost",EFI_NOT_READY);
#endif
  if(hs)bs->FreePool(hs);
  Text(c,"probe targets=");Hex(c,found,4);Text(c," cfg-incomplete=");Hex(c,cfgFailed,4);End(c);
  ProbeGops(c);
  if(root)ProbeDrivers(c,root);else Problem(c,"dump-volume",EFI_NOT_FOUND);
  /* Report a target that could not be captured rather than a clean success. */
  return found?cfgError:EFI_NOT_FOUND;
}
#if defined(GU_AD104_EDID_TEST) || defined(GU_AD104_TEXT_TEST)
#ifdef GU_AD104_TEXT_TEST
BOOLEAN GuTextOutputDisabled;
#endif
#include "ad104-edid.h"
#ifdef GU_AD104_PROM_FULL_TEST
EFI_STATUS GuEdidExperimentOnVolume(EFI_SYSTEM_TABLE *t,EFI_FILE_PROTOCOL *log,EFI_FILE_PROTOCOL *root,EFI_FILE_PROTOCOL *witness) {
  if(!root || !witness || root==log || root==witness || log==witness)return EFI_INVALID_PARAMETER;
#else
EFI_STATUS GuEdidExperiment(EFI_SYSTEM_TABLE *t,EFI_FILE_PROTOCOL *log) {
#endif
  if(!t || !t->BootServices || !log || !log->Write || !log->Flush)return EFI_INVALID_PARAMETER;
  static CAPTURE c;
  c.Table=t;c.File=log;
#ifdef GU_AD104_PROM_FULL_TEST
  c.DumpRoot=root;c.Witness=witness;
#else
  c.DumpRoot=NULL;
#endif
  c.Io=EFI_SUCCESS;c.Complete=FALSE;c.Used=0;c.Total=0;
  EFI_HANDLE *handles=NULL,gpu=NULL;EFI_PCI_IO_PROTOCOL *target=NULL;UINTN count=0,found=0;
  EFI_STATUS s=t->BootServices->LocateHandleBuffer(ByProtocol,&PciGuidL,NULL,&count,&handles);
  if(s!=EFI_SUCCESS || !handles || !count || count>1024) {
    if(handles)t->BootServices->FreePool(handles);return EFI_NOT_READY;
  }
  BOOLEAN inventory=TRUE;
  for(UINTN i=0;i<count;i++) {
    EFI_PCI_IO_PROTOCOL *io=NULL;UINT32 id=0,cls=0;
    if(t->BootServices->HandleProtocol(handles[i],&PciGuidL,(VOID **)&io)!=EFI_SUCCESS ||
       !io || !io->Pci.Read || io->Pci.Read(io,EfiPciIoWidthUint32,0,1,&id)!=EFI_SUCCESS){inventory=FALSE;continue;}
    if((id&0xffffU)!=0x10deU)continue;
    if(io->Pci.Read(io,EfiPciIoWidthUint32,8,1,&cls)!=EFI_SUCCESS){inventory=FALSE;continue;}
    if((cls>>24)==3){found++;gpu=handles[i];target=io;}
  }
  if(t->BootServices->FreePool(handles)!=EFI_SUCCESS)inventory=FALSE;
  if(!inventory || found!=1)return EFI_UNSUPPORTED;
#ifdef GU_AD104_TEXT_TEST
  if(!PcdProducerCaptured)return EFI_NOT_READY;
#endif
  s=EdRun(&c,gpu,target,PfPins);
  EdStatus(&c,"experiment-result",s);
  EFI_STATUS logged=EdFlush(&c);
  return logged!=EFI_SUCCESS?logged:s;
}
#ifdef GU_AD104_PROM_FULL_TEST
EFI_STATUS GuEdidExperiment(EFI_SYSTEM_TABLE *t,EFI_FILE_PROTOCOL *log) {
  (void)t;(void)log;return EFI_INVALID_PARAMETER;
}
#endif

#endif
