/* GPL-2.0-only. Read-only GA10x capture for the 3070 Ti headless port.
 * PCI config reads, BAR0 Mem.Read, PROM hash, loaded-image dumps only. */
#include <Uefi.h>
#include <Protocol/PciIo.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleFileSystem.h>
#include "../unlock/sha256.h"
#include "probe.h"
#include "../unlock/machine-profile.h"

VOID Text(CAPTURE *c,const char *s);
VOID Hex(CAPTURE *c,UINT64 v,UINTN digits);
VOID End(CAPTURE *c);
VOID Problem(CAPTURE *c,const char *where,EFI_STATUS s);

#define GU_TARGET_VENDEV GU_BOARD_ID
static EFI_GUID PciGuidL=EFI_PCI_IO_PROTOCOL_GUID;
static EFI_GUID BindGuidL=EFI_DRIVER_BINDING_PROTOCOL_GUID;
static EFI_GUID ImgGuidL=EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID PathGuidL=EFI_DEVICE_PATH_PROTOCOL_GUID;
static EFI_GUID GopGuidL=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
static EFI_GUID SfsGuidL=EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_SYSTEM_TABLE *SysT;
static UINTN DumpIndex;
static CHAR16 DirName[]=L"RTX3090-UNLOCK";
static CHAR16 ImgPrefix[]=L"\\RTX3090-UNLOCK\\IMG";

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
  CHAR16 name[40];UINTN i=DumpIndex++,k=0;
  Text(c,"img dump index=");Hex(c,i,4);End(c);
  while(ImgPrefix[k])name[k]=ImgPrefix[k],k++;
  name[k++]=(CHAR16)('0'+(i/10)%10);name[k++]=(CHAR16)('0'+i%10);
  name[k++]='.';name[k++]='B';name[k++]='I';name[k++]='N';name[k]=0;
  EFI_FILE_PROTOCOL *f=NULL;
  EFI_STATUS s=root->Open(root,&f,name,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0);
  if(s!=EFI_SUCCESS||!f){Problem(c,"img-open",s);return;}
  UINTN n=li->ImageSize;s=f->Write(f,&n,li->ImageBase);
  if(s!=EFI_SUCCESS||n!=li->ImageSize)Problem(c,"img-write",s==EFI_SUCCESS?EFI_END_OF_FILE:s);
  f->Close(f);
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

static VOID ProbeBar(CAPTURE *c,EFI_PCI_IO_PROTOCOL *io,UINT32 bar) {
  static const UINT32 offs[]={
    0x000000,0x108154,0x108170,0x108180,0x108354,
    0x118128,0x118234,0x110100,0x1183a4,0x1fa828,
    0x8219c0,0x822804,0x823804,0x82381c,0x823820,0x824140
  };
  Text(c,"probe-bar0 base=0x");Hex(c,bar,16);End(c);
  for(UINTN i=0;i<sizeof(offs)/sizeof(offs[0]);i++) {
    UINT32 v=0xdeadbeef;
    EFI_STATUS s=io->Mem.Read(io,EfiPciIoWidthUint32,0,offs[i],1,&v);
    Text(c,"mmio off=0x");Hex(c,offs[i],8);Text(c," status=0x");Hex(c,s,16);Text(c," value=0x");Hex(c,v,8);End(c);
  }
  UINT8 digest[32];
  UINT8 *page=NULL;
  EFI_STATUS s=c->Table->BootServices->AllocatePool(EfiBootServicesData,0x100000,(VOID **)&page);
  if(s==EFI_SUCCESS && page) {
    for(UINTN i=0;i<0x100000/4 && s==EFI_SUCCESS;i++)
      s=io->Mem.Read(io,EfiPciIoWidthUint32,0,0x300000+i*4,1,((UINT32*)page)+i);
    if(s==EFI_SUCCESS) {
      GuSha256(page,0x100000,digest);
      Text(c,"prom sha256=");for(UINTN i=0;i<32;i++)Hex(c,digest[i],2);End(c);
    } else { Problem(c,"prom-read",s); }
    c->Table->BootServices->FreePool(page);
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
  EFI_FILE_PROTOCOL *root=NULL;
  {
    EFI_HANDLE *sh=NULL;UINTN sn=0;
    if(t->BootServices->LocateHandleBuffer(ByProtocol,&SfsGuidL,NULL,&sn,&sh)==EFI_SUCCESS&&sh) {
      EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;
      for(UINTN i=0;i<sn && !root;i++)
        if(t->BootServices->HandleProtocol(sh[i],&SfsGuidL,(VOID **)&fs)==EFI_SUCCESS&&fs) {
          EFI_FILE_PROTOCOL *r=NULL,*d=NULL;
          if(fs->OpenVolume(fs,&r)==EFI_SUCCESS&&r&&
             r->Open(r,&d,DirName,EFI_FILE_MODE_READ,0)==EFI_SUCCESS&&d){d->Close(d);root=r;}
          else if(r)r->Close(r);
        }
      t->BootServices->FreePool(sh);
    }
  }
  EFI_BOOT_SERVICES *bs=t->BootServices;EFI_HANDLE *hs=NULL;UINTN n=0,found=0;
  EFI_STATUS s=bs->LocateHandleBuffer(ByProtocol,&PciGuidL,NULL,&n,&hs);
  if(s!=EFI_SUCCESS||!hs){Problem(c,"pci-enum",s);return s;}
  Text(c,"probe-pci-handles=");Hex(c,n,8);End(c);
  for(UINTN i=0;i<n && c->Io==EFI_SUCCESS;i++) {
    EFI_PCI_IO_PROTOCOL *io=NULL;UINT32 id=0;
    if(bs->HandleProtocol(hs[i],&PciGuidL,(VOID **)&io)!=EFI_SUCCESS||!io)continue;
    if(io->Pci.Read(io,EfiPciIoWidthUint32,0,1,&id)!=EFI_SUCCESS)continue;
    if(id!=GU_TARGET_VENDEV)continue;
    found++;
    UINTN seg=0,bus=0,dev=0,fun=0;
    if(io->GetLocation(io,&seg,&bus,&dev,&fun)==EFI_SUCCESS) {
      Text(c,"probe-target seg=0x");Hex(c,seg,4);Text(c," bus=0x");Hex(c,bus,4);
      Text(c," dev=0x");Hex(c,dev,4);Text(c," fun=0x");Hex(c,fun,4);End(c);
    }
    UINT32 cfg[64];
    s=io->Pci.Read(io,EfiPciIoWidthUint32,0,64,cfg);
    PH(c,"cfg256 status=0x",s);
    if(s==EFI_SUCCESS){Text(c,"cfg dwords=");for(UINTN j=0;j<64;j++){Hex(c,cfg[j],8);Text(c," ");}End(c);}
    UINT32 sub=0;io->Pci.Read(io,EfiPciIoWidthUint32,0x2c,1,&sub);PH(c,"subsystem=0x",sub);
    ProbeCaps(c,io);
    UINT32 bar=cfg[0x10/4]&~0xfU;
    if(bar)ProbeBar(c,io,bar);else Problem(c,"bar0",EFI_UNSUPPORTED);
  }
  if(hs)bs->FreePool(hs);
  Text(c,"probe targets=");Hex(c,found,4);End(c);
  ProbeGops(c);
  if(root)ProbeDrivers(c,root);else Problem(c,"dump-volume",EFI_NOT_FOUND);
  return found?EFI_SUCCESS:EFI_NOT_FOUND;
}
