/* GPL-2.0-only. Exact-platform console detach experiment, 2026-10-21.
 * All handles are discovered afresh. Only DisconnectController on the RTX
 * GOP child and two validated DriverBindingHandles is permitted. The NVIDIA
 * GPU owner and shared AMI runtime EDID/input driver are never stopped.
 * This stage does NOT remove RTX GOP and must NEVER execute the unlock app.
 */
#include "prepare-console.h"
#include "../unlock/sha256.h"
#ifdef GU_RELEASE_PILOT
#include "../../research/msi-release-pilot/profile.h"
#else
#include "../unlock/machine-profile.h"
#define GU_TARGET_BUS 1U
#endif
#include "media.h"
#include <Protocol/GraphicsOutput.h>
#include <Protocol/PciIo.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleTextOut.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/EdidDiscovered.h>
#ifndef GU_CONSOLE_MULTI_CHILD
#define GU_CONSOLE_MULTI_CHILD 0
#endif
#ifndef GU_CONSOLE_CHILD_COUNT
#define GU_CONSOLE_CHILD_COUNT 1
#endif
#if GU_CONSOLE_MULTI_CHILD && !GU_CONSOLE_AGGREGATE
#error Multi-child console support requires the aggregate console profile
#endif
#if GU_CONSOLE_MULTI_CHILD && (GU_CONSOLE_CHILD_COUNT < 2 || GU_CONSOLE_CHILD_COUNT > 4)
#error GU_CONSOLE_CHILD_COUNT must be 2..4 for a multi-child aggregate console
#endif
#define GU_AGGREGATE_MAX_CHILDREN 4
#if defined(GU_HEADLESS_RESEARCH) && (!defined(GU_SINGLE_RTX_CONSOLE) || !defined(GU_FULL_UNLOCK) || !defined(GU_AUTOMATIC_BOOT) || defined(GU_DIAGNOSTIC_ONLY) || defined(GU_CONSOLE_PREPARE))
#error Headless unlock requires an isolated automatic single RTX full build
#endif
#if defined(GU_SINGLE_RTX_CONSOLE) && !defined(GU_HEADLESS_RESEARCH) && !defined(GU_RELEASE_PILOT) && (!defined(GU_DIAGNOSTIC_ONLY) || !defined(GU_CONSOLE_PREPARE) || defined(GU_FULL_UNLOCK) || defined(GU_AUTOMATIC_BOOT))
#error Single RTX console research must remain a separate non-unlock build
#endif

#define MAX_HANDLES 1024
#define MAX_RECORDS 4096
typedef struct {
  EFI_HANDLE Handle,Agent,Controller;
  EFI_GUID Protocol;
  UINT32 Attributes,Count;
} EDGE;
typedef struct {
  UINTN Count;
  EDGE Edges[MAX_RECORDS];
} GRAPH;
typedef struct {
  EFI_HANDLE Amd,Rtx,AmdPci,RtxPci,Console,Input,Error,OutAgent,GfxAgent;
#if GU_CONSOLE_AGGREGATE
  EFI_HANDLE NvAgent,NvChildAgent,EdidAgent,ComboAgent,Agg;
  EFI_HANDLE AggCandidate,AggDriver;
#if GU_CONSOLE_MULTI_CHILD
  EFI_HANDLE Children[GU_AGGREGATE_MAX_CHILDREN];
  UINT8 ChildPaths[GU_AGGREGATE_MAX_CHILDREN][36];
  UINTN ChildCount;
#endif
#endif
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *AmdText,*ConsoleText,*ErrorText;
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL *Keys;
  EFI_PCI_IO_PROTOCOL *AmdIo,*RtxIo;
  UINT32 AmdConfig[16],RtxConfig[16];
  UINT8 AmdDisplayPath[36],RtxDisplayPath[36];
#ifdef GU_SINGLE_RTX_CONSOLE
  EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode,ModeCopy;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info,InfoCopy;
  UINTN KeyReset,KeyRead;
  EFI_EVENT KeyWait;
#endif
} PLAN;
static PLAN PreparedPlan;
static const GRAPH *PreparedGraph;
static BOOLEAN PreparedReady;
static EFI_GUID PciGuid=EFI_PCI_IO_PROTOCOL_GUID,PathGuid=EFI_DEVICE_PATH_PROTOCOL_GUID;
static EFI_GUID GopGuid=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID,TextGuid=EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID;
static EFI_GUID InputGuid=EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID;
static EFI_GUID BindingGuid=EFI_DRIVER_BINDING_PROTOCOL_GUID,ImageGuid=EFI_LOADED_IMAGE_PROTOCOL_GUID;
static const EFI_GUID OutFile={0x628a497d,0x2bf6,0x4264,{0x87,0x41,0x06,0x9d,0xbd,0x33,0x99,0xd6}};
static const EFI_GUID GfxFile={0x43e7abdd,0xe352,0x4cfb,{0xa2,0x30,0x4c,0xdc,0x1d,0x35,0x0e,0x5c}};
#if GU_CONSOLE_AGGREGATE
static EFI_GUID EdidFoundGuid=EFI_EDID_DISCOVERED_PROTOCOL_GUID;
static const EFI_GUID EdidFile={0xc7a7030c,0xc3d8,0x45ee,{0xbe,0xd9,0x5d,0x9e,0x76,0x76,0x29,0x53}};
static const EFI_GUID ComboFile={0x55987e62,0x1df6,0x41d1,{0x90,0x62,0x33,0x4f,0xca,0xc3,0xe5,0x4d}};
#endif
static const UINT8 AmdPath[]={2,1,12,0,0xd0,0x41,3,10,0,0,0,0,1,1,6,0,1,8,1,1,6,0,0,0,2,3,8,0,0,3,1,128,127,255,4,0};
static const UINT8 RtxPath[]={GU_RTX_PATH_BYTES};
static BOOLEAN Bytes(const VOID *a,const VOID *b,UINTN n) {
  const UINT8 *x=a,*y=b;for(UINTN i=0;i<n;i++)if(x[i]!=y[i])return FALSE;return TRUE;
}
static BOOLEAN Passive(UINT32 a) {return a==1 || a==2 || a==4;}
static EFI_STATUS Snapshot(EFI_BOOT_SERVICES *bs,GRAPH *out) {
  EFI_HANDLE *handles=NULL;UINTN count=0;out->Count=0;
  EFI_STATUS s=bs->LocateHandleBuffer(AllHandles,NULL,NULL,&count,&handles);
  if(s==EFI_SUCCESS && (!handles || !count || count>MAX_HANDLES))s=EFI_BAD_BUFFER_SIZE;
  for(UINTN i=0;s==EFI_SUCCESS && i<count;i++) {
    EFI_GUID **protocols=NULL;UINTN pc=0;
    s=bs->ProtocolsPerHandle(handles[i],&protocols,&pc);
    if(s==EFI_SUCCESS && (!protocols || !pc || pc>256))s=EFI_BAD_BUFFER_SIZE;
    for(UINTN j=0;s==EFI_SUCCESS && j<pc;j++) {
      EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *entries=NULL;UINTN ec=0;
      if(!protocols[j]){s=EFI_COMPROMISED_DATA;break;}
      s=bs->OpenProtocolInformation(handles[i],protocols[j],&entries,&ec);
      if(s==EFI_SUCCESS && (ec>512 || (ec && !entries)))s=EFI_BAD_BUFFER_SIZE;
      for(UINTN k=0;s==EFI_SUCCESS && k<ec;k++) {
        EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *e=&entries[k];
        if(Passive(e->Attributes))continue;
        BOOLEAN controllerOk=e->ControllerHandle!=NULL;
#ifdef GU_SNAPSHOT_OWN_RECORD
        if(!controllerOk)controllerOk=GU_SNAPSHOT_OWN_RECORD(handles[i],e);
#endif
        if(out->Count==MAX_RECORDS || !e->AgentHandle || !controllerOk || !e->OpenCount){s=EFI_COMPROMISED_DATA;break;}
        out->Edges[out->Count++]=(EDGE){handles[i],e->AgentHandle,e->ControllerHandle,*protocols[j],e->Attributes,e->OpenCount};
      }
      if(entries && bs->FreePool(entries)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
    }
    if(protocols && bs->FreePool(protocols)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  }
  if(handles && bs->FreePool(handles)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  return s;
}
static EFI_STATUS Interface(EFI_BOOT_SERVICES *bs,EFI_HANDLE h,EFI_GUID *guid,VOID **out) {
  *out=NULL;EFI_STATUS s=bs->HandleProtocol(h,guid,out);
  return s==EFI_SUCCESS && !*out?EFI_COMPROMISED_DATA:s;
}
static BOOLEAN PathMatches(const EFI_DEVICE_PATH_PROTOCOL *p,const UINT8 *expected) {
  UINTN bytes=0;return GuInspectPath(p,NULL,&bytes,NULL,NULL) && bytes==sizeof(AmdPath) && Bytes(p,expected,bytes);
}
/* Keep the exact platform PCI ancestry and single ACPI ADR node shape.
 * The four-byte ADR is a display endpoint, not the GPU identity: discover
 * it this boot, then pin the entire path for every later check. */
static BOOLEAN DisplayRoute(const EFI_DEVICE_PATH_PROTOCOL *p,const UINT8 *platform) {
  UINTN bytes=0;
  return GuInspectPath(p,NULL,&bytes,NULL,NULL) && bytes==36 &&
    Bytes(p,platform,28) && Bytes((const UINT8 *)p+32,platform+32,4);
}
static EFI_STATUS PciFor(EFI_BOOT_SERVICES *bs,EFI_DEVICE_PATH_PROTOCOL *path,UINTN bus,UINT32 id,UINT32 subsystem,EFI_HANDLE *handle,EFI_PCI_IO_PROTOCOL **io,UINT32 *config) {
  EFI_DEVICE_PATH_PROTOCOL *rest=path;*handle=NULL;
  EFI_STATUS s=bs->LocateDevicePath(&PciGuid,&rest,handle);
  if(s!=EFI_SUCCESS || !*handle || (UINT8 *)rest!=(UINT8 *)path+24)return EFI_UNSUPPORTED;
  s=Interface(bs,*handle,&PciGuid,(VOID **)io);if(s!=EFI_SUCCESS)return s;
  UINTN seg=0,b=0,d=0,f=0;
  if(!(*io)->GetLocation || !(*io)->Pci.Read)return EFI_UNSUPPORTED;
  s=(*io)->GetLocation(*io,&seg,&b,&d,&f);
  if(s!=EFI_SUCCESS || seg || b!=bus || d || f)return EFI_UNSUPPORTED;
  s=(*io)->Pci.Read(*io,EfiPciIoWidthUint32,0,16,config);
  if(s!=EFI_SUCCESS || config[0]!=id || config[11]!=subsystem || (config[2]>>24)!=3)return EFI_UNSUPPORTED;
  return EFI_SUCCESS;
}
static EFI_STATUS Driver(EFI_BOOT_SERVICES *bs,EFI_HANDLE agent,const EFI_GUID *file,UINTN size,UINTN stop,UINT32 version,UINT8 codetype);
#if GU_CONSOLE_AGGREGATE
/* The only pathless zero-FB GOP this topology accepts is the AMI combo
 * splitter aggregate. Its single BY_DRIVER opener must be the fingerprinted
 * GfxFile image; unknown virtual GOPs are refused. A detached survivor may
 * legitimately have no BY_DRIVER opener left (agent==NULL). */
static EFI_STATUS AggGopAgent(EFI_BOOT_SERVICES *bs,EFI_HANDLE h,EFI_HANDLE *agent) {
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *entries=NULL;UINTN n=0,active=0;
  *agent=NULL;
  EFI_STATUS s=bs->OpenProtocolInformation(h,&GopGuid,&entries,&n);
  if(s!=EFI_SUCCESS)return s;
  for(UINTN i=0;i<n;i++) {
    const EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *e=&entries[i];
    if(Passive(e->Attributes))continue;
    active++;
    if(e->Attributes==EFI_OPEN_PROTOCOL_BY_DRIVER && e->ControllerHandle==h) {
      if(*agent){s=EFI_ACCESS_DENIED;break;}
      *agent=e->AgentHandle;
    } else if(!(e->Attributes==EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER && e->ControllerHandle)){s=EFI_ACCESS_DENIED;break;}
    if(!e->AgentHandle || !e->OpenCount){s=EFI_ACCESS_DENIED;break;}
  }
  if(s==EFI_SUCCESS && active>2)s=EFI_ACCESS_DENIED;
  if(entries && bs->FreePool(entries)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  return s;
}
#endif
static EFI_STATUS Displays(EFI_SYSTEM_TABLE *t,PLAN *p,BOOLEAN first) {
  EFI_BOOT_SERVICES *bs=t->BootServices;EFI_HANDLE *handles=NULL,amd=NULL,rtx=NULL;UINTN count=0;
  EFI_STATUS s=bs->LocateHandleBuffer(ByProtocol,&GopGuid,NULL,&count,&handles);
  if(s==EFI_SUCCESS && (!handles ||
#ifdef GU_SINGLE_RTX_CONSOLE
     count<1 || count>2
#else
     count<2 || count>3
#endif
     ))s=EFI_UNSUPPORTED;
  for(UINTN i=0;s==EFI_SUCCESS && i<count;i++) {
    EFI_DEVICE_PATH_PROTOCOL *path=NULL;EFI_GRAPHICS_OUTPUT_PROTOCOL *gop=NULL;
    s=Interface(bs,handles[i],&GopGuid,(VOID **)&gop);if(s!=EFI_SUCCESS)break;
    if(!gop->Mode){s=EFI_UNSUPPORTED;break;}
    EFI_STATUS ps=Interface(bs,handles[i],&PathGuid,(VOID **)&path);
    /* A new pathless virtual GOP may appear after one physical console leaves. */
#if GU_CONSOLE_AGGREGATE
    if((ps==EFI_UNSUPPORTED || ps==EFI_NOT_FOUND) && !path && !gop->Mode->FrameBufferBase && !gop->Mode->FrameBufferSize) {
      EFI_HANDLE agent=NULL;
      if(first){if(p->AggCandidate){s=EFI_UNSUPPORTED;break;}}
      else if(handles[i]!=p->Agg){s=EFI_UNSUPPORTED;break;}
      s=AggGopAgent(bs,handles[i],&agent);
      if(s!=EFI_SUCCESS)break;
      if(agent){s=Driver(bs,agent,&GfxFile,GU_CONSOLE_GFX_SIZE,GU_CONSOLE_GFX_STOP,GU_CONSOLE_GFX_VERSION,EfiBootServicesCode);if(s!=EFI_SUCCESS)break;}
      if(first){p->AggCandidate=handles[i];p->AggDriver=agent;}
      continue;
    }
#else
    if((ps==EFI_UNSUPPORTED || ps==EFI_NOT_FOUND) && !path && handles[i]==t->ConsoleOutHandle && !gop->Mode->FrameBufferBase && !gop->Mode->FrameBufferSize)continue;
#endif
    if(ps!=EFI_SUCCESS || !path){s=EFI_UNSUPPORTED;break;}
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *m=gop->Mode;
    if(!m->Info || m->SizeOfInfo<sizeof(*m->Info) || !m->FrameBufferBase || !m->FrameBufferSize || m->Mode>=m->MaxMode ||
       m->Info->PixelFormat>PixelBlueGreenRedReserved8BitPerColor || !m->Info->HorizontalResolution || !m->Info->VerticalResolution ||
       m->Info->PixelsPerScanLine<m->Info->HorizontalResolution || (UINT64)m->Info->PixelsPerScanLine*m->Info->VerticalResolution>m->FrameBufferSize/4){s=EFI_UNSUPPORTED;break;}
    if(DisplayRoute(path,AmdPath) && !amd) {
#ifdef GU_SINGLE_RTX_CONSOLE
      s=EFI_UNSUPPORTED;break; /* Sole physical GOP must be this RTX. */
#else
      amd=handles[i];UINT32 cfg[16];EFI_HANDLE h=NULL;EFI_PCI_IO_PROTOCOL *io=NULL;
      s=PciFor(bs,path,12,0x164e1002,0x88771043,&h,&io,cfg);
      if(s!=EFI_SUCCESS)break;
      if(first){p->AmdPci=h;p->AmdIo=io;for(UINTN j=0;j<16;j++)p->AmdConfig[j]=cfg[j];for(UINTN j=0;j<36;j++)p->AmdDisplayPath[j]=((UINT8 *)path)[j];}
      else if(p->AmdPci!=h || p->AmdIo!=io || !Bytes(cfg,p->AmdConfig,sizeof(cfg)) || !PathMatches(path,p->AmdDisplayPath))s=EFI_NOT_READY;
#endif
    } else if(DisplayRoute(path,RtxPath) && !rtx) {
      rtx=handles[i];UINT32 cfg[16];EFI_HANDLE h=NULL;EFI_PCI_IO_PROTOCOL *io=NULL;
      s=PciFor(bs,path,GU_TARGET_BUS,GU_BOARD_ID,GU_BOARD_SUBSYSTEM,&h,&io,cfg);
      if(s!=EFI_SUCCESS)break;
      if(first){p->RtxPci=h;p->RtxIo=io;for(UINTN j=0;j<16;j++)p->RtxConfig[j]=cfg[j];for(UINTN j=0;j<36;j++)p->RtxDisplayPath[j]=((UINT8 *)path)[j];}
      else if(p->RtxPci!=h || p->RtxIo!=io || !Bytes(cfg,p->RtxConfig,sizeof(cfg)) || !PathMatches(path,p->RtxDisplayPath))s=EFI_NOT_READY;
#ifdef GU_SINGLE_RTX_CONSOLE
      if(first){p->Gop=gop;p->Mode=m;p->ModeCopy=*m;p->Info=m->Info;p->InfoCopy=*m->Info;}
      else if(p->Gop!=gop || p->Mode!=m || p->Info!=m->Info || !Bytes(&p->ModeCopy,m,sizeof(*m)) || !Bytes(&p->InfoCopy,m->Info,sizeof(*m->Info)))s=EFI_NOT_READY;
#endif
    } else s=EFI_UNSUPPORTED;
  }
  if(handles && bs->FreePool(handles)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
#ifdef GU_SINGLE_RTX_CONSOLE
  if(s!=EFI_SUCCESS || !rtx || amd || rtx==p->Console || rtx==p->Input || rtx==p->Error)return s==EFI_SUCCESS?EFI_UNSUPPORTED:s;
  if(first)p->Rtx=rtx;else if(p->Rtx!=rtx)s=EFI_NOT_READY;
  return s;
#else
  if(s!=EFI_SUCCESS || !amd || !rtx || amd==rtx)return s==EFI_SUCCESS?EFI_UNSUPPORTED:s;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *text=NULL;
  s=Interface(bs,amd,&TextGuid,(VOID **)&text);
  if(s!=EFI_SUCCESS || !text || !text->OutputString || !text->Mode || text->Mode->Mode<0)return EFI_UNSUPPORTED;
  if(first){p->Amd=amd;p->Rtx=rtx;p->AmdText=text;}
  else if(p->Amd!=amd || p->Rtx!=rtx || p->AmdText!=text)s=EFI_NOT_READY;
  return s;
#endif
}
static BOOLEAN SameEdge(const EDGE *a,const EDGE *b) {
  return a->Handle==b->Handle && a->Agent==b->Agent && a->Controller==b->Controller && a->Attributes==b->Attributes && a->Count==b->Count && GuGuidEqual(&a->Protocol,&b->Protocol);
}
static BOOLEAN Removed(const PLAN *p,const EDGE *e,UINTN stage) {
  if(e->Handle!=p->Rtx)return FALSE;
  if(stage>=1 && e->Agent==p->OutAgent && GuGuidEqual(&e->Protocol,&TextGuid))return TRUE;
  return stage>=2 && e->Agent==p->GfxAgent && GuGuidEqual(&e->Protocol,&GopGuid);
}
static BOOLEAN ExpectedGraph(const PLAN *p,const GRAPH *original,const GRAPH *current,UINTN stage) {
  UINTN wanted=0;
  for(UINTN i=0;i<original->Count;i++) {
    if(Removed(p,&original->Edges[i],stage))continue;
    wanted++;UINTN found=0;
    for(UINTN j=0;j<current->Count;j++)if(SameEdge(&original->Edges[i],&current->Edges[j]))found++;
    if(found!=1)return FALSE;
  }
  return current->Count==wanted;
}
static EFI_STATUS Driver(EFI_BOOT_SERVICES *bs,EFI_HANDLE agent,const EFI_GUID *file,UINTN size,UINTN stop,UINT32 version,UINT8 codetype) {
  EFI_DRIVER_BINDING_PROTOCOL *d=NULL;EFI_LOADED_IMAGE_PROTOCOL *im=NULL;
  EFI_STATUS s=Interface(bs,agent,&BindingGuid,(VOID **)&d);
  if(s!=EFI_SUCCESS || !d || !d->Stop || d->DriverBindingHandle!=agent || d->Version!=version || !d->ImageHandle)return EFI_UNSUPPORTED;
  s=Interface(bs,d->ImageHandle,&ImageGuid,(VOID **)&im);
  if(s!=EFI_SUCCESS || !im || !im->ImageBase || im->ImageCodeType!=codetype || im->ImageSize!=size ||
     (UINTN)d->Stop!=(UINTN)im->ImageBase+stop || !im->FilePath)return EFI_UNSUPPORTED;
  UINTN n=0;
  if(!GuInspectPath(im->FilePath,NULL,&n,NULL,NULL) || n!=24 || im->FilePath->Type!=MEDIA_DEVICE_PATH || im->FilePath->SubType!=MEDIA_PIWG_FW_FILE_DP ||
     im->FilePath->Length[0]!=20 || im->FilePath->Length[1] || !GuGuidEqual((UINT8 *)im->FilePath+4,file))return EFI_UNSUPPORTED;
#ifdef GU_RELEASE_PILOT
  BOOLEAN text=GuGuidEqual(file,&OutFile),gfx=GuGuidEqual(file,&GfxFile);
  if(!text && !gfx)return EFI_UNSUPPORTED;
  UINTN bytes=text?GU_CONSOLE_TEXT_BYTES:GU_CONSOLE_GFX_BYTES;
  const char *hash=text?GU_CONSOLE_TEXT_SHA256:GU_CONSOLE_GFX_SHA256;
  if(0x2a0U+bytes>im->ImageSize)return EFI_UNSUPPORTED;
  if(!GuHashMatches((UINT8 *)im->ImageBase+0x2a0,bytes,hash))return EFI_CRC_ERROR;
#endif
  return EFI_SUCCESS;
}
#if GU_CONSOLE_AGGREGATE
/* GA104 headless topology: nvgop owns RtxPci (PciIo + one private protocol,
 * each BY_DRIVER) and produced exactly one GOP child. The console path is the
 * AMI GOP combo splitter (C1 GOP -> aggregate) plus the EDID/mouse runtime
 * driver. The aggregate carries GOP (GfxAgent) and SimpleTextOut (OutAgent,
 * BY_CHILD for the virtual console). Any other live consumer is refused. */
static EFI_STATUS Consumers(EFI_SYSTEM_TABLE *t,PLAN *p,const GRAPH *g) {
  EFI_HANDLE nv=NULL,childGopAgent=NULL,conOutAgent=NULL;
  (void)t;
  UINTN owners=0,children=0,edid=0,combo=0,gfx=0,out=0,conChild=0;
  for(UINTN i=0;i<g->Count;i++) {
    const EDGE *e=&g->Edges[i];
    if(e->Handle==p->RtxPci) {
      if(e->Count!=1 || !e->Agent)return EFI_ACCESS_DENIED;
      if(GuGuidEqual(&e->Protocol,&PathGuid) && e->Attributes==EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER) {
#if GU_CONSOLE_MULTI_CHILD
        if(!e->Controller || children>=GU_AGGREGATE_MAX_CHILDREN)return EFI_ACCESS_DENIED;
        for(UINTN prior=0;prior<children;prior++)if(p->Children[prior]==e->Controller)return EFI_ACCESS_DENIED;
        p->Children[children++]=e->Controller;
        if(!p->NvChildAgent)p->NvChildAgent=e->Agent;
        else if(p->NvChildAgent!=e->Agent)return EFI_ACCESS_DENIED;
#else
        if(e->Controller!=p->Rtx)return EFI_ACCESS_DENIED;
        children++;p->NvChildAgent=e->Agent;
#endif
      }
      else if(e->Attributes==EFI_OPEN_PROTOCOL_BY_DRIVER && e->Controller==p->RtxPci) {
        if(!nv)nv=e->Agent;else if(e->Agent!=nv)return EFI_ACCESS_DENIED;
        if(GuGuidEqual(&e->Protocol,&PciGuid)){p->NvAgent=e->Agent;owners++;}
      } else return EFI_ACCESS_DENIED;
    }
  }
#if GU_CONSOLE_MULTI_CHILD
  if(owners!=1 || children!=GU_CONSOLE_CHILD_COUNT || !nv || p->NvAgent!=nv || p->NvChildAgent!=nv)return EFI_ACCESS_DENIED;
  UINTN primary=children;
  for(UINTN c=0;c<children;c++)if(p->Children[c]==p->Rtx)primary=c;
  if(primary==children)return EFI_ACCESS_DENIED;
  EFI_HANDLE first=p->Children[0];p->Children[0]=p->Children[primary];p->Children[primary]=first;
  p->ChildCount=children;
#else
  if(owners!=1 || children!=1 || !nv || p->NvAgent!=nv || p->NvChildAgent!=nv)return EFI_ACCESS_DENIED;
#endif
  for(UINTN i=0;i<g->Count;i++) {
    const EDGE *e=&g->Edges[i];
    if(e->Handle!=p->Rtx)continue;
    if(e->Count!=1 || !e->Agent)return EFI_ACCESS_DENIED;
    if(GuGuidEqual(&e->Protocol,&EdidFoundGuid) && e->Attributes==EFI_OPEN_PROTOCOL_BY_DRIVER && e->Controller==p->Rtx){p->EdidAgent=e->Agent;edid++;}
    else if(GuGuidEqual(&e->Protocol,&GopGuid) && e->Attributes==EFI_OPEN_PROTOCOL_BY_DRIVER && e->Controller==p->Rtx){combo++;childGopAgent=e->Agent;}
    else if(GuGuidEqual(&e->Protocol,&GopGuid) && e->Attributes==EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER && e->Controller){p->Agg=e->Controller;p->ComboAgent=e->Agent;}
    else return EFI_ACCESS_DENIED;
  }
  if(edid!=1 || combo!=1 || !p->Agg || !p->ComboAgent || childGopAgent!=p->ComboAgent ||
     p->Agg!=p->AggCandidate || !p->AggDriver ||
     p->Agg==p->Rtx || p->Agg==p->RtxPci || p->EdidAgent==nv || p->ComboAgent==nv || p->EdidAgent==p->ComboAgent)return EFI_ACCESS_DENIED;
#if GU_CONSOLE_MULTI_CHILD
  /* Primary child (c==0) is p->Rtx, already validated above as the console GOP
   * child. Each additional child must be the same GPU (path/PCI), carry EDID
   * from the same EdidAgent, and have no GOP or TextOut of its own. */
  for(UINTN c=0;c<p->ChildCount;c++) {
    EFI_HANDLE child=p->Children[c];EFI_DEVICE_PATH_PROTOCOL *path=NULL;
    VOID *foundEdid=NULL,*other=NULL;UINT32 config[16];EFI_HANDLE pci=NULL;EFI_PCI_IO_PROTOCOL *io=NULL;
    if(child==p->RtxPci || child==p->Console || child==p->Input || child==p->Error || child==p->Agg)return EFI_ACCESS_DENIED;
    EFI_STATUS s=Interface(t->BootServices,child,&PathGuid,(VOID **)&path);
    if(s!=EFI_SUCCESS || !DisplayRoute(path,RtxPath))return EFI_UNSUPPORTED;
    s=PciFor(t->BootServices,path,1,GU_BOARD_ID,GU_BOARD_SUBSYSTEM,&pci,&io,config);
    if(s!=EFI_SUCCESS || pci!=p->RtxPci || io!=p->RtxIo || !Bytes(config,p->RtxConfig,sizeof(config)))return EFI_NOT_READY;
    for(UINTN prior=0;prior<c;prior++)if(Bytes(p->ChildPaths[prior],path,36))return EFI_UNSUPPORTED;
    for(UINTN j=0;j<36;j++)p->ChildPaths[c][j]=((UINT8 *)path)[j];
    if(c==0 && !Bytes(path,p->RtxDisplayPath,36))return EFI_NOT_READY;
    if(Interface(t->BootServices,child,&EdidFoundGuid,&foundEdid)!=EFI_SUCCESS || !foundEdid)return EFI_UNSUPPORTED;
    if(c) {
      s=Interface(t->BootServices,child,&GopGuid,&other);
      if(!((s==EFI_NOT_FOUND || s==EFI_UNSUPPORTED) && !other))return EFI_UNSUPPORTED;
      s=Interface(t->BootServices,child,&TextGuid,&other);
      if(!((s==EFI_NOT_FOUND || s==EFI_UNSUPPORTED) && !other))return EFI_UNSUPPORTED;
      UINTN edges=0;
      for(UINTN i=0;i<g->Count;i++) {
        const EDGE *e=&g->Edges[i];if(e->Handle!=child)continue;
        if(!GuGuidEqual(&e->Protocol,&EdidFoundGuid) || e->Attributes!=EFI_OPEN_PROTOCOL_BY_DRIVER ||
           e->Controller!=child || e->Agent!=p->EdidAgent || e->Count!=1)return EFI_ACCESS_DENIED;
        edges++;
      }
      if(edges!=1)return EFI_ACCESS_DENIED;
    }
  }
#endif
  for(UINTN i=0;i<g->Count;i++) {
    const EDGE *e=&g->Edges[i];
    if(e->Handle!=p->Agg)continue;
    if(e->Count!=1 || !e->Agent)return EFI_ACCESS_DENIED;
    if(GuGuidEqual(&e->Protocol,&GopGuid) && e->Attributes==EFI_OPEN_PROTOCOL_BY_DRIVER && e->Controller==p->Agg){p->GfxAgent=e->Agent;gfx++;}
    else if(GuGuidEqual(&e->Protocol,&TextGuid) && e->Attributes==EFI_OPEN_PROTOCOL_BY_DRIVER && e->Controller==p->Agg){conOutAgent=e->Agent;out++;}
    else if(GuGuidEqual(&e->Protocol,&TextGuid) && e->Attributes==EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER && e->Controller==p->Console)conChild++;
    else return EFI_ACCESS_DENIED;
  }
  if(gfx!=1 || out!=1 || conChild!=1 || !conOutAgent)return EFI_ACCESS_DENIED;
  p->OutAgent=conOutAgent;
  if(p->OutAgent==p->GfxAgent || p->OutAgent==nv || p->GfxAgent==nv || p->GfxAgent!=p->AggDriver)return EFI_ACCESS_DENIED;
  for(UINTN i=0;i<g->Count;i++) {
    const EDGE *e=&g->Edges[i];
    if((e->Agent==nv || e->Agent==p->ComboAgent) && e->Handle!=p->RtxPci && e->Handle!=p->Rtx && e->Handle!=p->Agg)return EFI_ACCESS_DENIED;
  }
  EFI_STATUS s=Driver(t->BootServices,p->EdidAgent,&EdidFile,GU_EDID_IMAGE_SIZE,GU_EDID_STOP,1,EfiRuntimeServicesCode);
  if(s==EFI_SUCCESS)s=Driver(t->BootServices,p->ComboAgent,&ComboFile,GU_COMBO_SIZE,GU_COMBO_STOP,GU_COMBO_VERSION,EfiBootServicesCode);
  if(s==EFI_SUCCESS)s=Driver(t->BootServices,p->GfxAgent,&GfxFile,GU_CONSOLE_GFX_SIZE,GU_CONSOLE_GFX_STOP,GU_CONSOLE_GFX_VERSION,EfiBootServicesCode);
  if(s==EFI_SUCCESS)s=Driver(t->BootServices,p->OutAgent,&OutFile,GU_CONSOLE_TEXT_SIZE,GU_CONSOLE_TEXT_STOP,GU_CONSOLE_TEXT_VERSION,EfiBootServicesCode);
  return s;
}
#else
static EFI_STATUS Consumers(EFI_SYSTEM_TABLE *t,PLAN *p,const GRAPH *g) {
  UINTN out=0,child=0,gfx=0;
  for(UINTN i=0;i<g->Count;i++) {
    const EDGE *e=&g->Edges[i];
    if(e->Handle!=p->Rtx)continue;
    if(GuGuidEqual(&e->Protocol,&TextGuid)) {
      if(e->Count!=1)return EFI_ACCESS_DENIED;
      if(e->Attributes==EFI_OPEN_PROTOCOL_BY_DRIVER && e->Controller==p->Rtx){p->OutAgent=e->Agent;out++;}
      else if(e->Attributes==EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER && e->Controller==t->ConsoleOutHandle)child++;
      else return EFI_ACCESS_DENIED;
    } else if(GuGuidEqual(&e->Protocol,&GopGuid)) {
      if(e->Count!=1 || e->Attributes!=EFI_OPEN_PROTOCOL_BY_DRIVER || e->Controller!=p->Rtx)return EFI_ACCESS_DENIED;
      p->GfxAgent=e->Agent;gfx++;
    }
  }
  if(out!=1 || child!=1 || gfx!=1 || p->OutAgent==p->GfxAgent)return EFI_ACCESS_DENIED;
  /* Each target binding may own other controllers, but its only RTX child
   * relationships must be exactly the reviewed TextOut/GOP edges above. */
  for(UINTN i=0;i<g->Count;i++) {
    const EDGE *e=&g->Edges[i];
    if(e->Handle==p->Rtx && e->Agent==p->OutAgent && !GuGuidEqual(&e->Protocol,&TextGuid))return EFI_ACCESS_DENIED;
    if(e->Handle==p->Rtx && e->Agent==p->GfxAgent && !GuGuidEqual(&e->Protocol,&GopGuid))return EFI_ACCESS_DENIED;
    if(e->Handle==p->Rtx && GuGuidEqual(&e->Protocol,&TextGuid) && e->Agent!=p->OutAgent)return EFI_ACCESS_DENIED;
  }
#ifndef GU_SINGLE_RTX_CONSOLE
  UINTN amdOut=0,amdChild=0,amdGfx=0;
  for(UINTN i=0;i<g->Count;i++) {
    const EDGE *e=&g->Edges[i];
    if(e->Handle!=p->Amd)continue;
    if(GuGuidEqual(&e->Protocol,&TextGuid)) {
      if(e->Agent!=p->OutAgent || e->Count!=1)return EFI_ACCESS_DENIED;
      if(e->Attributes==EFI_OPEN_PROTOCOL_BY_DRIVER && e->Controller==p->Amd)amdOut++;
      else if(e->Attributes==EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER && e->Controller==t->ConsoleOutHandle)amdChild++;
      else return EFI_ACCESS_DENIED;
    } else if(GuGuidEqual(&e->Protocol,&GopGuid)) {
      if(e->Agent!=p->GfxAgent || e->Count!=1 || e->Attributes!=EFI_OPEN_PROTOCOL_BY_DRIVER || e->Controller!=p->Amd)return EFI_ACCESS_DENIED;
      amdGfx++;
    }
  }
  if(amdOut!=1 || amdChild!=1 || amdGfx!=1)return EFI_ACCESS_DENIED;
#endif
  EFI_STATUS s=Driver(t->BootServices,p->OutAgent,&OutFile,GU_CONSOLE_TEXT_SIZE,GU_CONSOLE_TEXT_STOP,GU_CONSOLE_TEXT_VERSION,EfiBootServicesCode);
  if(s==EFI_SUCCESS)s=Driver(t->BootServices,p->GfxAgent,&GfxFile,GU_CONSOLE_GFX_SIZE,GU_CONSOLE_GFX_STOP,GU_CONSOLE_GFX_VERSION,EfiBootServicesCode);
  return s;
}
#endif
static EFI_STATUS Alive(EFI_SYSTEM_TABLE *t,PLAN *p) {
#ifdef GU_SINGLE_RTX_CONSOLE
  if(!t->ConIn || (UINTN)t->ConIn->Reset!=p->KeyReset || (UINTN)t->ConIn->ReadKeyStroke!=p->KeyRead || t->ConIn->WaitForKey!=p->KeyWait)return EFI_NOT_READY;
#endif
  if(t->ConsoleOutHandle!=p->Console || t->ConsoleInHandle!=p->Input || t->StandardErrorHandle!=p->Error ||
     t->ConOut!=p->ConsoleText || t->StdErr!=p->ErrorText || t->ConIn!=p->Keys || !t->ConIn || !t->ConIn->ReadKeyStroke || !t->ConIn->WaitForKey)return EFI_NOT_READY;
  VOID *out=NULL,*in=NULL,*err=NULL;
  EFI_STATUS s=Interface(t->BootServices,p->Console,&TextGuid,&out);
  if(s==EFI_SUCCESS)s=Interface(t->BootServices,p->Error,&TextGuid,&err);
  if(s==EFI_SUCCESS)s=Interface(t->BootServices,p->Input,&InputGuid,&in);
  if(s!=EFI_SUCCESS || out!=p->ConsoleText || err!=p->ErrorText || in!=p->Keys)return EFI_NOT_READY;
  return Displays(t,p,FALSE);
}
EFI_STATUS GuPrepareConsole(EFI_SYSTEM_TABLE *t,GU_PREPARE_LOG log,VOID *context) {
  PreparedReady=FALSE;
  if(!t || !t->BootServices || !log || !t->ConOut || !t->StdErr || !t->ConIn || !t->ConsoleOutHandle || !t->ConsoleInHandle || !t->StandardErrorHandle)return EFI_INVALID_PARAMETER;
  EFI_BOOT_SERVICES *bs=t->BootServices;
  if(!bs->DisconnectController || !bs->RaiseTPL || !bs->RestoreTPL)return EFI_UNSUPPORTED;
#ifdef GU_SINGLE_RTX_CONSOLE
  if(!t->ConIn->Reset || !t->ConIn->ReadKeyStroke || !t->ConIn->WaitForKey)return EFI_UNSUPPORTED;
#endif
#if GU_CONSOLE_AGGREGATE
  static GRAPH original;PLAN p={0};
#else
  static GRAPH original,current;PLAN p={0};
#endif
  p.Console=t->ConsoleOutHandle;p.Input=t->ConsoleInHandle;p.Error=t->StandardErrorHandle;
  p.ConsoleText=t->ConOut;p.ErrorText=t->StdErr;p.Keys=t->ConIn;
#ifdef GU_SINGLE_RTX_CONSOLE
  p.KeyReset=(UINTN)t->ConIn->Reset;p.KeyRead=(UINTN)t->ConIn->ReadKeyStroke;p.KeyWait=t->ConIn->WaitForKey;
#endif
  EFI_STATUS s=Displays(t,&p,TRUE);
  if(s==EFI_SUCCESS)s=Snapshot(bs,&original);
  if(s==EFI_SUCCESS)s=Consumers(t,&p,&original);
  if(s==EFI_SUCCESS)s=Alive(t,&p);
  EFI_STATUS recorded=log(context,"console-prepare-preflight",s);
  if(s!=EFI_SUCCESS || recorded!=EFI_SUCCESS)return s==EFI_SUCCESS?recorded:s;
#if !GU_CONSOLE_AGGREGATE
  for(UINTN stage=1;stage<=2;stage++) {
    /* Flush intent before the final live recheck so filesystem notifications
     * cannot make the graph snapshot older than our log operation. */
    s=log(context,stage==1?"detach-rtx-textout-intent":"detach-rtx-graphics-console-intent",EFI_SUCCESS);
    if(s!=EFI_SUCCESS)return s;
    s=Alive(t,&p);
    EFI_TPL old=bs->RaiseTPL(TPL_NOTIFY);
    if(s==EFI_SUCCESS)s=Snapshot(bs,&current);
    if(s==EFI_SUCCESS && !ExpectedGraph(&p,&original,&current,stage-1))s=EFI_NOT_READY;
    if(s==EFI_SUCCESS)s=Driver(bs,stage==1?p.OutAgent:p.GfxAgent,stage==1?&OutFile:&GfxFile,stage==1?GU_CONSOLE_TEXT_SIZE:GU_CONSOLE_GFX_SIZE,stage==1?GU_CONSOLE_TEXT_STOP:GU_CONSOLE_GFX_STOP,GU_CONSOLE_TEXT_VERSION,EfiBootServicesCode);
    bs->RestoreTPL(old);
    if(s!=EFI_SUCCESS)return s;
    /* Flush intent BEFORE mutation. Never pass a virtual console, AMD, PCI
     * parent, NULL driver, image handle, or the AMI runtime driver here. */
    s=bs->DisconnectController(p.Rtx,stage==1?p.OutAgent:p.GfxAgent,NULL);
    recorded=log(context,stage==1?"detach-rtx-textout-return":"detach-rtx-graphics-console-return",s);
    if(s!=EFI_SUCCESS || recorded!=EFI_SUCCESS)return s==EFI_SUCCESS?recorded:s;
    s=Snapshot(bs,&current);
    if(s==EFI_SUCCESS && !ExpectedGraph(&p,&original,&current,stage))s=EFI_NOT_READY;
    if(s==EFI_SUCCESS)s=Alive(t,&p);
    if(s==EFI_SUCCESS) {
      EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *rtxText=NULL;
      EFI_STATUS absent=Interface(bs,p.Rtx,&TextGuid,(VOID **)&rtxText);
      if(stage==1 && (absent!=EFI_SUCCESS || !rtxText))s=EFI_NOT_READY;
      if(stage==2 && !((absent==EFI_UNSUPPORTED || absent==EFI_NOT_FOUND) && !rtxText))s=EFI_NOT_READY;
    }
    recorded=log(context,stage==1?"detach-textout-verified":"detach-graphics-console-verified",s);
    if(s!=EFI_SUCCESS || recorded!=EFI_SUCCESS)return s==EFI_SUCCESS?recorded:s;
#ifndef GU_SINGLE_RTX_CONSOLE
    /* Active output test uses the already preserved AMD-backed console. */
    s=t->ConOut->OutputString(t->ConOut,L"RTX console consumer detached; AMD console retained.\r\n");
    if(s!=EFI_SUCCESS)return s;
#endif
  }
#endif
  s=log(context,"stage2a-success-no-unlock-no-gop-removal",EFI_SUCCESS);
  if(s!=EFI_SUCCESS)return s;
  PreparedPlan=p;PreparedGraph=&original;PreparedReady=TRUE;
  return GuVerifyPreparedConsole(t);
}
EFI_STATUS GuVerifyPreparedConsole(EFI_SYSTEM_TABLE *t) {
  if(!PreparedReady || !t || !t->BootServices || !PreparedGraph)return EFI_NOT_READY;
  PLAN *p=&PreparedPlan;static GRAPH final;
  EFI_STATUS s=Alive(t,p);
#if GU_CONSOLE_AGGREGATE
  if(s==EFI_SUCCESS)s=Driver(t->BootServices,p->EdidAgent,&EdidFile,GU_EDID_IMAGE_SIZE,GU_EDID_STOP,1,EfiRuntimeServicesCode);
  if(s==EFI_SUCCESS)s=Driver(t->BootServices,p->ComboAgent,&ComboFile,GU_COMBO_SIZE,GU_COMBO_STOP,GU_COMBO_VERSION,EfiBootServicesCode);
  if(s==EFI_SUCCESS)s=Driver(t->BootServices,p->GfxAgent,&GfxFile,GU_CONSOLE_GFX_SIZE,GU_CONSOLE_GFX_STOP,GU_CONSOLE_GFX_VERSION,EfiBootServicesCode);
  if(s==EFI_SUCCESS)s=Driver(t->BootServices,p->OutAgent,&OutFile,GU_CONSOLE_TEXT_SIZE,GU_CONSOLE_TEXT_STOP,GU_CONSOLE_TEXT_VERSION,EfiBootServicesCode);
#else
  if(s==EFI_SUCCESS)s=Driver(t->BootServices,p->OutAgent,&OutFile,GU_CONSOLE_TEXT_SIZE,GU_CONSOLE_TEXT_STOP,GU_CONSOLE_TEXT_VERSION,EfiBootServicesCode);
  if(s==EFI_SUCCESS)s=Driver(t->BootServices,p->GfxAgent,&GfxFile,GU_CONSOLE_GFX_SIZE,GU_CONSOLE_GFX_STOP,GU_CONSOLE_GFX_VERSION,EfiBootServicesCode);
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *text=NULL;
  if(s==EFI_SUCCESS) {
    EFI_STATUS absent=Interface(t->BootServices,p->Rtx,&TextGuid,(VOID **)&text);
    if(!((absent==EFI_UNSUPPORTED || absent==EFI_NOT_FOUND) && !text))s=EFI_NOT_READY;
  }
#endif
  if(s==EFI_SUCCESS)s=Snapshot(t->BootServices,&final);
  if(s==EFI_SUCCESS && !ExpectedGraph(p,PreparedGraph,&final,GU_CONSOLE_AGGREGATE?0:2))s=EFI_NOT_READY;
  if(s!=EFI_SUCCESS)PreparedReady=FALSE;
  return s;
}
