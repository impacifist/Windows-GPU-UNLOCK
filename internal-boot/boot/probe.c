/* GPL-2.0-only. Read-only GA10x capture for the 3070 Ti headless port.
 * PCI config reads, BAR0 Mem.Read, PROM hash, loaded-image dumps only. */
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
 * Generic deep reads accept only hardware-evidenced GA10x identities. The RTX
 * 3080 10 GiB research target is pinned to device 2216 + subsystem 403f:1458
 * and is capture-only here: the write/profile envelope remains separately
 * gated by research/profile/proven.json and gpu-family.h.
 * named probes pin their own
 * PCI/subsystem and require the same 16 MiB, MEM1/BME0 resource envelope.
 * All allocation/protocol calls finish before the final identity/config recheck. */
static BOOLEAN ProbeEnvelope(CAPTURE *c,EFI_PCI_IO_PROTOCOL *io,const UINT32 *cfg) {
#ifdef GU_PROBE_GENERIC
  BOOLEAN identity=cfg[0]==0x220410deU || cfg[0]==0x220310deU ||
    (cfg[0]==0x221610deU && cfg[11]==0x403f1458U) ||
    cfg[0]==0x248710deU || cfg[0]==0x250410deU;
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
  }
  if(raw && c->Table->BootServices->FreePool(raw)!=EFI_SUCCESS)good=FALSE;
  return good;
}
static BOOLEAN ProbeUnchanged(CAPTURE *c,EFI_HANDLE handle,EFI_PCI_IO_PROTOCOL *io,const UINT32 *cfg,UINTN expectedBus) {
  EFI_PCI_IO_PROTOCOL *current=NULL;UINTN seg=0,bus=0,dev=0,fun=0;UINT32 now[64];
  if(c->Table->BootServices->HandleProtocol(handle,&PciGuidL,(VOID **)&current)!=EFI_SUCCESS || current!=io ||
     !io->GetLocation || io->GetLocation(io,&seg,&bus,&dev,&fun)!=EFI_SUCCESS || seg || bus!=expectedBus || dev || fun ||
     io->Pci.Read(io,EfiPciIoWidthUint32,0,64,now)!=EFI_SUCCESS)return FALSE;
  for(UINTN j=0;j<64;j++)if(now[j]!=cfg[j])return FALSE;
  return TRUE;
}
#endif
static VOID ProbeBar(CAPTURE *c,EFI_PCI_IO_PROTOCOL *io,UINT32 bar,EFI_FILE_PROTOCOL *root,
                     EFI_HANDLE handle,const UINT32 *cfg,UINTN expectedBus) {
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
  if(!ProbeUnchanged(c,handle,io,cfg,expectedBus)) {
    c->Table->BootServices->FreePool(page);Problem(c,"probe-identity-changed",EFI_NOT_READY);return;
  }
#else
  (void)handle;(void)cfg;(void)expectedBus;
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
#ifdef GU_PROBE_GENERIC
  EFI_HANDLE targetHandle=NULL;EFI_PCI_IO_PROTOCOL *targetIo=NULL;
  UINT32 targetId=0,targetClass=0;UINTN targetBus=0;
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
#ifdef GU_PROBE_GENERIC
    UINTN seg=0,bus=0,dev=0,fun=0;
    if(!io->GetLocation || io->GetLocation(io,&seg,&bus,&dev,&fun)!=EFI_SUCCESS ||
       seg || !bus || bus>255 || dev || fun){inventoryOk=FALSE;continue;}
#ifndef GU_PROBE_AUTO_BUS
    if(bus!=GU_PROBE_BUS){inventoryOk=FALSE;continue;}
#endif
    targetHandle=hs[i];targetIo=io;targetId=id;targetClass=cls;targetBus=bus;
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
    UINTN expectedBus=GU_PROBE_BUS;
#ifdef GU_PROBE_GENERIC
    /* Bind this snapshot to the unique device inventoried before external log,
     * resource and allocation callbacks. No re-selection after a mismatch. */
    if(hs[i]!=targetHandle || io!=targetIo || id!=targetId || cfg[0]!=targetId || cfg[2]!=targetClass ||
       locationStatus!=EFI_SUCCESS || seg || bus!=targetBus || dev || fun) {
      Problem(c,"probe-inventory-changed",EFI_NOT_READY);continue;
    }
    expectedBus=targetBus;
    targetValidated=TRUE;
#endif
    if(bar)ProbeBar(c,io,bar,root,hs[i],cfg,expectedBus);else Problem(c,"bar0",EFI_UNSUPPORTED);
  }
#ifdef GU_PROBE_GENERIC
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
