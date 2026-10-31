/* GPL-2.0-only. Pinned EVGA consumer Stop sequence. No unlock. */
#include "evga-text-stop.h"
#ifdef GU_EVGA_PROBE_RESOURCES
#include "evga-resources.h"
#endif
#ifdef GU_EVGA_UNLOCK
#if !defined(GU_EVGA_PROBE_RESOURCES) || !defined(GU_EVGA_PROBE_PLATFORM) || !defined(GU_EVGA_PROBE_STATE) || !defined(GU_EVGA_PROBE_DECODE) || !defined(GU_EVGA_STOP_NVIDIA_PARENT) || !defined(GU_EVGA_STOP_NVIDIA_CHILDREN) || !defined(GU_EVGA_AUDIT_NVIDIA) || !defined(GU_EVGA_RELEASE_EDID_SIBLINGS) || !defined(GU_EVGA_RELEASE_EDID) || !defined(GU_EVGA_RELEASE_GRAPHICS) || !defined(GU_EVGA_RELEASE_TEXT) || !defined(GU_EVGA_STOP_ONLY) || !defined(GU_PROFILE_EVGA_Z790_3090) || !defined(GU_EXPERIMENTAL_3090) || !defined(GU_ENTRY_LATCHED_SS1_FULL) || !defined(GU_EVGA_TRANSACTION_DEVELOPMENT)
#error EVGA unlock requires the complete exact-profile release and admission sequence
#endif
#ifdef GU_EVGA_TEST_ONLY
#error EVGA test reset must never be linked into an unlock image
#endif
#include "../unlock/evga-transaction.h"
#endif
#include <Protocol/GraphicsOutput.h>
#include <Protocol/DevicePath.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/PciIo.h>
#include "../unlock/sha256.h"
#ifdef GU_EVGA_PROBE_PLATFORM
#include "../unlock/checks.h"
#include <IndustryStandard/Acpi.h>
#endif

static EFI_GUID Gop=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID,Text=EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID;
static EFI_GUID Path=EFI_DEVICE_PATH_PROTOCOL_GUID,Binding=EFI_DRIVER_BINDING_PROTOCOL_GUID;
static EFI_GUID Loaded=EFI_LOADED_IMAGE_PROTOCOL_GUID,Pci=EFI_PCI_IO_PROTOCOL_GUID;
typedef struct {EFI_HANDLE Physical,Virtual,Console,Gfx,VirtualText,TextAgent;EFI_DRIVER_BINDING_PROTOCOL *Driver;} PLAN;
static PLAN Plan;
#ifdef GU_EVGA_AUDIT_NVIDIA
static EFI_HANDLE NvidiaParent,NvidiaAgent,NvidiaStoredChild;
static UINTN NvidiaChildClass,NvidiaFlag;
static BOOLEAN NvidiaSaved;
#ifdef GU_EVGA_STOP_NVIDIA_CHILDREN
static EFI_DRIVER_BINDING_PROTOCOL *NvidiaDriver;
static BOOLEAN NvidiaChildrenStopped;
#ifdef GU_EVGA_STOP_NVIDIA_PARENT
static BOOLEAN NvidiaParentStopped;
#endif
#endif
#endif
#ifdef GU_EVGA_RELEASE_EDID
#include <Protocol/EdidDiscovered.h>
static EFI_GUID Edid=EFI_EDID_DISCOVERED_PROTOCOL_GUID;
static EFI_HANDLE EdidAgent,EdidHandles[3];
static EFI_DRIVER_BINDING_PROTOCOL *EdidDriver;
static BOOLEAN EdidReleased;
#ifdef GU_EVGA_RELEASE_EDID_SIBLINGS
static EFI_HANDLE EdidSiblingReleased[2];
#endif
#endif
#ifdef GU_EVGA_RELEASE_TEXT
static EFI_DRIVER_BINDING_PROTOCOL *VirtualDriver;
static BOOLEAN TextReleased;
#ifdef GU_EVGA_RELEASE_GRAPHICS
static EFI_DRIVER_BINDING_PROTOCOL *GraphicsDriver;
static BOOLEAN GraphicsReleased;
#undef ATTEMPT_PATH
#ifdef GU_EVGA_RELEASE_EDID
#ifdef GU_EVGA_RELEASE_EDID_SIBLINGS
#ifdef GU_EVGA_AUDIT_NVIDIA
#ifdef GU_EVGA_STOP_NVIDIA_CHILDREN
#ifdef GU_EVGA_STOP_NVIDIA_PARENT
#ifdef GU_EVGA_UNLOCK
#ifndef GU_EVGA_ATTEMPT_SLOT
#define GU_EVGA_ATTEMPT_SLOT 4
#endif
#if GU_EVGA_ATTEMPT_SLOT < 4 || GU_EVGA_ATTEMPT_SLOT > 99
#error Invalid EVGA attempt slot
#endif
#if GU_EVGA_ATTEMPT_SLOT < 10
static CHAR16 AttemptPath[]=L"\\RTX3090-UNLOCK\\EVGA-UNLOCK0.ARMED";
#else
static CHAR16 AttemptPath[]=L"\\RTX3090-UNLOCK\\EVGA-UNLOCK00.ARMED";
#endif
#define ATTEMPT_PATH AttemptPath
#elif defined(GU_EVGA_PROBE_RESOURCES)
#define ATTEMPT_PATH L"\\RTX3090-UNLOCK\\EVGA-RESOURCES-PROBE.ARMED"
#elif defined(GU_EVGA_PROBE_PLATFORM)
#define ATTEMPT_PATH L"\\RTX3090-UNLOCK\\EVGA-PLATFORM-PROBE.ARMED"
#elif defined(GU_EVGA_PROBE_STATE)
#define ATTEMPT_PATH L"\\RTX3090-UNLOCK\\EVGA-STATE-PROBE.ARMED"
#elif defined(GU_EVGA_PROBE_DECODE)
#define ATTEMPT_PATH L"\\RTX3090-UNLOCK\\EVGA-DECODE-PROBE.ARMED"
#else
#define ATTEMPT_PATH L"\\RTX3090-UNLOCK\\EVGA-NVIDIA-PARENT.ARMED"
#endif
#else
#define ATTEMPT_PATH L"\\RTX3090-UNLOCK\\EVGA-NVIDIA-CHILDREN.ARMED"
#endif
#else
#define ATTEMPT_PATH L"\\RTX3090-UNLOCK\\EVGA-NVIDIA-AUDIT.ARMED"
#endif
#else
#define ATTEMPT_PATH L"\\RTX3090-UNLOCK\\EVGA-EDID-SIBLINGS.ARMED"
#endif
#else
#define ATTEMPT_PATH L"\\RTX3090-UNLOCK\\EVGA-EDID-RELEASE.ARMED"
#endif
#else
#define ATTEMPT_PATH L"\\RTX3090-UNLOCK\\EVGA-GFX-RELEASE.ARMED"
#endif
#else
#define ATTEMPT_PATH L"\\RTX3090-UNLOCK\\EVGA-TEXT-RELEASE.ARMED"
#endif
#else
#define ATTEMPT_PATH L"\\RTX3090-UNLOCK\\EVGA-TEXT-STOP.ARMED"
#endif
#ifdef GU_SHARED_ESP_QUAL
#ifdef GU_EVGA_UNLOCK
#error QUAL01 must never contain an unlock transaction
#endif
#undef ATTEMPT_PATH
#define ATTEMPT_PATH L"\\RTX3090-UNLOCK\\QUAL01-GPU.ARMED"
#endif
static EFI_STATUS Checked(GU_EVGA_RECORD record,const char *event,EFI_STATUS status) {
  if(record){EFI_STATUS io=record(NULL,event,status);if(io!=EFI_SUCCESS)return io;}
  return status;
}
static EFI_STATUS Value(GU_EVGA_RECORD record,const char *label,UINT64 value) {
  if(!record)return EFI_SUCCESS;
  char event[100];UINTN n=0;
  while(*label && n<70)event[n++]=*label++;
  event[n++]='=';event[n++]='0';event[n++]='x';
  for(int i=15;i>=0;i--)event[n++]="0123456789abcdef"[(value>>(i*4))&15];
  event[n]=0;return record(NULL,event,EFI_SUCCESS);
}
#ifdef GU_EVGA_UNLOCK
static BOOLEAN EvgaAllowExclusive;
#endif
static EFI_STATUS Edges(EFI_BOOT_SERVICES *b,EFI_HANDLE source,EFI_GUID *protocol,
                       EFI_HANDLE child,BOOLEAN pair,EFI_HANDLE *owner,BOOLEAN absent) {
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *e=NULL;UINTN n=0,drivers=0,children=0;EFI_HANDLE agent=NULL;
  EFI_STATUS s=b->OpenProtocolInformation(source,protocol,&e,&n);
  if(s!=EFI_SUCCESS || n>64 || (n && !e))return EFI_COMPROMISED_DATA;
#ifdef GU_EVGA_UNLOCK
  UINTN own=0;BOOLEAN expected=absent && EvgaAllowExclusive && source==NvidiaParent && protocol==&Pci;
#endif
  for(UINTN i=0;i<n;i++) {
    if(e[i].Attributes==1 || e[i].Attributes==2 || e[i].Attributes==4)continue;
    if(absent){
#ifdef GU_EVGA_UNLOCK
      if(expected && e[i].Attributes==EFI_OPEN_PROTOCOL_EXCLUSIVE && e[i].AgentHandle==GuEvgaResourcesImage() &&
         !e[i].ControllerHandle && e[i].OpenCount==1 && !own){own++;continue;}
#endif
      s=EFI_ACCESS_DENIED;break;
    }
    if(e[i].OpenCount!=1){s=EFI_ACCESS_DENIED;break;}
    if(e[i].Attributes==EFI_OPEN_PROTOCOL_BY_DRIVER && e[i].ControllerHandle==source) {
      drivers++;if(agent && agent!=e[i].AgentHandle){s=EFI_ACCESS_DENIED;break;}agent=e[i].AgentHandle;
    } else if(pair && e[i].Attributes==EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER && e[i].ControllerHandle==child) {
      children++;if(agent && agent!=e[i].AgentHandle){s=EFI_ACCESS_DENIED;break;}agent=e[i].AgentHandle;
    } else {s=EFI_ACCESS_DENIED;break;}
  }
  if(s==EFI_SUCCESS && !absent && (drivers!=1 || children!=(pair?1U:0U) || !agent))s=EFI_ACCESS_DENIED;
#ifdef GU_EVGA_UNLOCK
  if(s==EFI_SUCCESS && expected && own!=1)s=EFI_ACCESS_DENIED;
#endif
  if(e && b->FreePool(e)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  if(s==EFI_SUCCESS && !absent) {
    if(*owner && *owner!=agent)return EFI_ACCESS_DENIED;
    *owner=agent;
  }
  return s;
}
static EFI_STATUS Code(EFI_BOOT_SERVICES *b,EFI_HANDLE agent,UINTN bytes,UINTN codeRva,UINTN codeBytes,
                       UINTN stop,UINT32 version,const char *hash,EFI_DRIVER_BINDING_PROTOCOL **out,GU_EVGA_RECORD record) {
  EFI_DRIVER_BINDING_PROTOCOL *d=NULL;EFI_LOADED_IMAGE_PROTOCOL *image=NULL;
  EFI_STATUS s=b->HandleProtocol(agent,&Binding,(VOID **)&d);
  if(s==EFI_SUCCESS && d){s=Value(record,"evga-observed-binding-version",d->Version);if(s!=EFI_SUCCESS)return s;}
  if(s!=EFI_SUCCESS || !d || d->DriverBindingHandle!=agent || !d->ImageHandle || !d->Stop || d->Version!=version)return Checked(record,"evga-code-binding",EFI_UNSUPPORTED);
  s=b->HandleProtocol(d->ImageHandle,&Loaded,(VOID **)&image);
  if(s==EFI_SUCCESS && image){s=Value(record,"evga-observed-image-bytes",image->ImageSize);if(s!=EFI_SUCCESS)return s;}
  if(s!=EFI_SUCCESS || !image || !image->ImageBase || image->ImageSize!=bytes || bytes<=codeRva || codeBytes>bytes-codeRva)return Checked(record,"evga-code-loaded-image",EFI_UNSUPPORTED);
  UINTN base=(UINTN)image->ImageBase;
  if((UINTN)d->Stop>=base){s=Value(record,"evga-observed-stop-rva",(UINTN)d->Stop-base);if(s!=EFI_SUCCESS)return s;}
  if(base>~(UINTN)0-bytes || (UINTN)d->Stop!=base+stop || stop<codeRva || stop-codeRva>=codeBytes)return Checked(record,"evga-code-stop-address",EFI_UNSUPPORTED);
  if(!GuHashMatches((UINT8 *)image->ImageBase+codeRva,codeBytes,hash))return Checked(record,"evga-code-section-hash",EFI_CRC_ERROR);
  if(out)*out=d;return EFI_SUCCESS;
}
static EFI_STATUS Inventory(EFI_BOOT_SERVICES *b,GU_EVGA_RECORD record) {
  EFI_HANDLE *handles=NULL;UINTN count=0,displays=0,matches=0;
  EFI_STATUS s=b->LocateHandleBuffer(ByProtocol,&Pci,NULL,&count,&handles);
  if(s!=EFI_SUCCESS || !handles || !count || count>1024)return EFI_UNSUPPORTED;
  for(UINTN i=0;i<count;i++) {
    EFI_PCI_IO_PROTOCOL *pci=NULL;UINT32 cfg[16];
    s=b->HandleProtocol(handles[i],&Pci,(VOID **)&pci);
    if(s!=EFI_SUCCESS || !pci){s=EFI_UNSUPPORTED;break;}
    s=pci->Pci.Read(pci,EfiPciIoWidthUint32,0,16,cfg);if(s!=EFI_SUCCESS)break;
    if((cfg[2]>>24)==3){displays++;if(cfg[0]==0x220410deU && cfg[11]==0x33873842U){matches++;
#ifdef GU_EVGA_AUDIT_NVIDIA
      NvidiaParent=handles[i];
#endif
    }}
  }
  if(b->FreePool(handles)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  EFI_STATUS io=Value(record,"evga-observed-display-count",displays);if(io!=EFI_SUCCESS)return io;
  io=Value(record,"evga-observed-matching-card-count",matches);if(io!=EFI_SUCCESS)return io;
  return s==EFI_SUCCESS && displays==1 && matches==1?EFI_SUCCESS:EFI_UNSUPPORTED;
}
/* Enumerate membership: this firmware returns a pointer for an unlisted GOP. */
static EFI_STATUS ConsoleProtocols(EFI_SYSTEM_TABLE *t,GU_EVGA_RECORD record) {
  EFI_GUID **list=NULL;UINTN n=0,gops=0,texts=0;
  EFI_STATUS s=t->BootServices->ProtocolsPerHandle(t->ConsoleOutHandle,&list,&n);
  EFI_STATUS io=Checked(record,"evga-console-protocol-list",s);if(io!=s)return io;
  if(s!=EFI_SUCCESS || !list || n>256)return EFI_UNSUPPORTED;
  EFI_GUID expectedGop=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID,expectedText=EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID;
  for(UINTN i=0;i<n;i++) {
    if(!list[i]){s=EFI_COMPROMISED_DATA;break;}
    BOOLEAN g=TRUE,x=TRUE;
    for(UINTN j=0;j<sizeof(EFI_GUID);j++) {
      if(((UINT8 *)list[i])[j]!=((UINT8 *)&expectedGop)[j])g=FALSE;
      if(((UINT8 *)list[i])[j]!=((UINT8 *)&expectedText)[j])x=FALSE;
    }
    gops+=g;texts+=x;
  }
  if(t->BootServices->FreePool(list)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
  if(s!=EFI_SUCCESS)return s;
  io=Value(record,"evga-console-listed-gops",gops);if(io!=EFI_SUCCESS)return io;
  io=Value(record,"evga-console-listed-texts",texts);if(io!=EFI_SUCCESS)return io;
  return gops==0 && texts==1?EFI_SUCCESS:EFI_UNSUPPORTED;
}
#ifdef GU_EVGA_RELEASE_TEXT
static EFI_STATUS VirtualInterfaces(EFI_SYSTEM_TABLE *t,BOOLEAN released) {
  EFI_GUID **list=NULL;UINTN n=0,texts=0,ugas=0,extras=0;
  EFI_GUID uga={0xd3b36f2c,0xd551,0x11d4,{0x9a,0x46,0,0x90,0x27,0x3f,0xc1,0x4d}};
  EFI_GUID extra={0xe1e4a857,0xc970,0x4075,{0xa4,0xda,0xe9,0xc4,0x1b,0x69,0xad,0xfc}};
  EFI_STATUS s=t->BootServices->ProtocolsPerHandle(Plan.Virtual,&list,&n);
  if(s!=EFI_SUCCESS || !list || n>256)return EFI_UNSUPPORTED;
  for(UINTN i=0;i<n;i++) {
    if(!list[i]){s=EFI_COMPROMISED_DATA;break;}
    BOOLEAN x=TRUE,u=TRUE,e=TRUE;
    for(UINTN j=0;j<sizeof(EFI_GUID);j++) {
      UINT8 v=((UINT8 *)list[i])[j];
      if(v!=((UINT8 *)&Text)[j])x=FALSE;
      if(v!=((UINT8 *)&uga)[j])u=FALSE;
      if(v!=((UINT8 *)&extra)[j])e=FALSE;
    }
    texts+=x;ugas+=u;extras+=e;
  }
  if(t->BootServices->FreePool(list)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
  if(s!=EFI_SUCCESS)return s;
  return ugas==0 && texts==(released?0U:1U) && extras==(released?0U:1U)?EFI_SUCCESS:EFI_ACCESS_DENIED;
}
#endif
#ifdef GU_EVGA_RELEASE_EDID
static EFI_STATUS EdidState(EFI_SYSTEM_TABLE *t,GU_EVGA_RECORD record) {
  EFI_BOOT_SERVICES *b=t->BootServices;EFI_HANDLE *handles=NULL;UINTN n=0;
  EFI_STATUS s=b->LocateHandleBuffer(ByProtocol,&Edid,NULL,&n,&handles);
  if(s!=EFI_SUCCESS || !handles || n!=3){if(handles)b->FreePool(handles);return EFI_UNSUPPORTED;}
  UINTN physical=0;
#ifdef GU_EVGA_RELEASE_EDID_SIBLINGS
  UINTN pathBits=0;
#endif
  for(UINTN i=0;i<n;i++) {
    for(UINTN j=0;j<i;j++)if(handles[j]==handles[i])s=EFI_COMPROMISED_DATA;
    if(s!=EFI_SUCCESS)break;
    if(EdidHandles[0]) {
      BOOLEAN found=FALSE;for(UINTN j=0;j<3;j++)if(EdidHandles[j]==handles[i])found=TRUE;
      if(!found){s=EFI_ACCESS_DENIED;break;}
    }
    if(handles[i]==Plan.Physical)physical++;
#ifdef GU_EVGA_RELEASE_EDID_SIBLINGS
    /* Exact captured PCI parent plus three distinct ACPI ADR children. */
    static const UINT8 expected[36]={0x02,0x01,0x0c,0x00,0xd0,0x41,0x03,0x0a,0,0,0,0,0x01,0x01,0x06,0,0,1,0x01,0x01,0x06,0,0,0,0x02,0x03,0x08,0,0x20,0x03,0x01,0x80,0x7f,0xff,4,0};
    UINT8 *path=NULL;s=b->HandleProtocol(handles[i],&Path,(VOID **)&path);
    if(s!=EFI_SUCCESS || !path){s=EFI_UNSUPPORTED;break;}
    for(UINTN off=0;off<36;) {
      for(UINTN j=0;j<4;j++)if(path[off+j]!=expected[off+j])s=EFI_ACCESS_DENIED;
      if(s!=EFI_SUCCESS)break;
      UINTN length=expected[off+2];
      for(UINTN j=4;j<length;j++)if(off+j!=28 && path[off+j]!=expected[off+j])s=EFI_ACCESS_DENIED;
      if(s!=EFI_SUCCESS)break;off+=length;
    }
    if(s!=EFI_SUCCESS)break;
    UINTN bit=path[28]==0x20?1:(path[28]==0x40?2:(path[28]==0x70?4:0));
    if(!bit || (pathBits&bit) || ((handles[i]==Plan.Physical)!=(bit==1)))s=EFI_ACCESS_DENIED;
    if(s!=EFI_SUCCESS)break;pathBits|=bit;
#endif
    BOOLEAN absent=EdidReleased && handles[i]==Plan.Physical;
#ifdef GU_EVGA_RELEASE_EDID_SIBLINGS
    for(UINTN j=0;j<2;j++)if(EdidSiblingReleased[j]==handles[i])absent=TRUE;
#endif
    s=Edges(b,handles[i],&Edid,NULL,FALSE,&EdidAgent,absent);
    if(s!=EFI_SUCCESS)break;
  }
#ifdef GU_EVGA_RELEASE_EDID_SIBLINGS
  if(s==EFI_SUCCESS && pathBits!=7)s=EFI_ACCESS_DENIED;
#endif
  if(s==EFI_SUCCESS && physical!=1)s=EFI_ACCESS_DENIED;
  if(s==EFI_SUCCESS && !EdidHandles[0])for(UINTN i=0;i<3;i++)EdidHandles[i]=handles[i];
  if(b->FreePool(handles)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  s=Checked(record,"evga-edid-edges",s);if(s!=EFI_SUCCESS)return s;
  return Checked(record,"evga-edid-code",Code(b,EdidAgent,0x9000,0x1000,0x37da,0x14d4,1,"a015d52f55fa6441414458e2b699ba2edcb53f03b001ac6367e45bcd908c418e",&EdidDriver,record));
}
#endif
static EFI_STATUS Graph(EFI_SYSTEM_TABLE *t,BOOLEAN after,GU_EVGA_RECORD record) {
  EFI_BOOT_SERVICES *b=t->BootServices;EFI_HANDLE *handles=NULL;UINTN count=0;
  EFI_STATUS s=b->LocateHandleBuffer(ByProtocol,&Gop,NULL,&count,&handles);
  if(s!=EFI_SUCCESS || !handles || count!=2){if(handles)b->FreePool(handles);return Checked(record,"evga-gop-inventory",EFI_UNSUPPORTED);}
  EFI_HANDLE physical=NULL,virtual=NULL;
  for(UINTN i=0;i<count;i++) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *g=NULL;VOID *path=NULL;
    s=b->HandleProtocol(handles[i],&Gop,(VOID **)&g);
    if(s!=EFI_SUCCESS || !g || !g->Mode){s=EFI_UNSUPPORTED;break;}
    EFI_STATUS ps=b->HandleProtocol(handles[i],&Path,&path);
    if(ps==EFI_SUCCESS && path && g->Mode->FrameBufferBase && g->Mode->FrameBufferSize && !physical)physical=handles[i];
    else if(ps==EFI_UNSUPPORTED && !g->Mode->FrameBufferBase && !g->Mode->FrameBufferSize && !virtual)virtual=handles[i];
    else {s=EFI_UNSUPPORTED;break;}
  }
  if(b->FreePool(handles)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  if(s!=EFI_SUCCESS || !physical || !virtual || !t->ConsoleOutHandle || t->StandardErrorHandle!=t->ConsoleOutHandle)return Checked(record,"evga-gop-shape",EFI_UNSUPPORTED);
  s=Checked(record,"evga-console-membership",ConsoleProtocols(t,record));if(s!=EFI_SUCCESS)return s;
  VOID *text=NULL;EFI_STATUS io;
#ifdef GU_EVGA_RELEASE_TEXT
  if(!TextReleased) {
#endif
  s=b->HandleProtocol(virtual,&Text,&text);
  io=Checked(record,"evga-virtual-text-interface",s);if(io!=s)return io;
  if(s!=EFI_SUCCESS || !text)return Checked(record,"evga-virtual-text-interface-valid",EFI_UNSUPPORTED);
#ifdef GU_EVGA_RELEASE_TEXT
  }
#endif
  text=NULL;s=b->HandleProtocol(t->ConsoleOutHandle,&Text,&text);
  io=Checked(record,"evga-system-text-interface",s);if(io!=s)return io;
  if(s!=EFI_SUCCESS || !text)return Checked(record,"evga-system-text-interface-valid",EFI_UNSUPPORTED);
  if(Plan.Physical && (Plan.Physical!=physical || Plan.Virtual!=virtual || Plan.Console!=t->ConsoleOutHandle))return EFI_ACCESS_DENIED;
  Plan.Physical=physical;Plan.Virtual=virtual;Plan.Console=t->ConsoleOutHandle;
  s=Checked(record,"evga-gfx-edges",Edges(b,physical,&Gop,virtual,TRUE,&Plan.Gfx,
#ifdef GU_EVGA_RELEASE_GRAPHICS
    GraphicsReleased
#else
    FALSE
#endif
  ));if(s!=EFI_SUCCESS)return s;
  s=Checked(record,"evga-virtual-text-edges",Edges(b,virtual,&Gop,NULL,FALSE,&Plan.VirtualText,
#ifdef GU_EVGA_RELEASE_TEXT
    TextReleased
#else
    FALSE
#endif
  ));if(s!=EFI_SUCCESS)return s;
#ifdef GU_EVGA_RELEASE_TEXT
  if(TextReleased) {
    s=Checked(record,"evga-virtual-interfaces-removed",VirtualInterfaces(t,TRUE));if(s!=EFI_SUCCESS)return s;
  } else {
#endif
  s=Checked(record,"evga-text-splitter-edges",Edges(b,virtual,&Text,Plan.Console,TRUE,&Plan.TextAgent,after));if(s!=EFI_SUCCESS)return s;
#ifdef GU_EVGA_RELEASE_TEXT
  }
#endif
  s=Checked(record,"evga-gfx-code",Code(b,Plan.Gfx,0x2180,0x2a0,0x1990,0x88c,17,"3f4c4ea26023ddf00990b59385d0fcf3139acb583c70406828777cdceab8cecb",
#ifdef GU_EVGA_RELEASE_GRAPHICS
    &GraphicsDriver,
#else
    NULL,
#endif
    record));if(s!=EFI_SUCCESS)return s;
  s=Checked(record,"evga-virtual-text-code",Code(b,Plan.VirtualText,0x3bc0,0x2a0,0x1e00,0xb2c,16,"e5e68f0856c6e932da9bf3c680bac955dbf180eb083da78b64007388d89e24a7",
#ifdef GU_EVGA_RELEASE_TEXT
    &VirtualDriver,
#else
    NULL,
#endif
    record));if(s!=EFI_SUCCESS)return s;
  s=Checked(record,"evga-text-splitter-code",Code(b,Plan.TextAgent,0x6ae0,0x2a0,0x5620,0x104c,16,"e2b072199726ae784f749057c3c5453ed8db0bb3fbe1a73ccca7e6f302325291",&Plan.Driver,record));
  if(s!=EFI_SUCCESS)return s;
#ifdef GU_EVGA_RELEASE_EDID
  return EdidState(t,record);
#else
  return EFI_SUCCESS;
#endif
}
#ifdef GU_EVGA_AUDIT_NVIDIA
/* CPU protocol data only. Never call NVIDIA Stop or any private method. */
static EFI_STATUS NvidiaAudit(EFI_SYSTEM_TABLE *t,GU_EVGA_RECORD record) {
  EFI_BOOT_SERVICES *b=t->BootServices;
  if(!NvidiaParent)return EFI_UNSUPPORTED;
  EFI_STATUS s=Checked(record,"evga-nvidia-pci-owner",Edges(b,NvidiaParent,&Pci,NULL,FALSE,&NvidiaAgent,FALSE));if(s!=EFI_SUCCESS)return s;
  s=Checked(record,"evga-nvidia-code",Code(b,NvidiaAgent,0x2b700,0x280,0x23d82,0x698,0x60009,"d045cee2113e461c7c678ec21542b1d0f18356765c0fc6a0bf2dcf824088e011",
#ifdef GU_EVGA_STOP_NVIDIA_CHILDREN
    &NvidiaDriver,
#else
    NULL,
#endif
    record));if(s!=EFI_SUCCESS)return s;
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *e=NULL;UINTN n=0,bits=0;
  s=b->OpenProtocolInformation(NvidiaParent,&Path,&e,&n);
  if(s!=EFI_SUCCESS || n>64 || (n&&!e))return EFI_COMPROMISED_DATA;
  for(UINTN i=0;i<n;i++) {
    if(e[i].Attributes==1 || e[i].Attributes==2 || e[i].Attributes==4)continue;
    UINTN bit=0;for(UINTN j=0;j<3;j++)if(e[i].ControllerHandle==EdidHandles[j])bit=(UINTN)1<<j;
    if(e[i].Attributes!=8 || e[i].OpenCount!=1 || e[i].AgentHandle!=NvidiaAgent || !bit || (bits&bit)){s=EFI_ACCESS_DENIED;break;}
    bits|=bit;
  }
  if(e && b->FreePool(e)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  if(s==EFI_SUCCESS && bits!=7)s=EFI_ACCESS_DENIED;
  s=Checked(record,"evga-nvidia-three-children",s);if(s!=EFI_SUCCESS)return s;
  EFI_GUID privateGuid={0xfe47349a,0x7f0d,0x4641,{0x82,0x2b,0x34,0xba,0xa2,0x8e,0xcd,0xd0}};
  UINT8 *context=NULL;s=b->HandleProtocol(NvidiaParent,&privateGuid,(VOID **)&context);
  s=Checked(record,"evga-nvidia-private-interface",s);if(s!=EFI_SUCCESS || !context)return EFI_UNSUPPORTED;
  if((UINTN)context>~(UINTN)0-0x99)return EFI_COMPROMISED_DATA;
  EFI_HANDLE child=*(EFI_HANDLE *)(context+0x88);UINTN flag=context[0x98];
  UINTN kind=child?4:0;
  if(child==Plan.Physical)kind=1;
  else for(UINTN j=0;j<3;j++)if(child==EdidHandles[j])kind=2;
  s=Value(record,"evga-nvidia-stored-child-class",kind);if(s!=EFI_SUCCESS)return s;
  s=Value(record,"evga-nvidia-parent-flag98",flag);if(s!=EFI_SUCCESS)return s;
  /* Classes:0 null,1 physical GOP,2 EDID-only sibling,4 unknown. */
  if(kind==4 || flag>1)return EFI_UNSUPPORTED;
#ifdef GU_EVGA_STOP_NVIDIA_CHILDREN
  if(kind!=1 || flag!=0)return EFI_UNSUPPORTED;
#endif
  if(NvidiaSaved && (child!=NvidiaStoredChild || kind!=NvidiaChildClass || flag!=NvidiaFlag))return EFI_ACCESS_DENIED;
  NvidiaStoredChild=child;NvidiaChildClass=kind;NvidiaFlag=flag;NvidiaSaved=TRUE;
  return Checked(record,"evga-nvidia-audit",EFI_SUCCESS);
}
#endif
#ifdef GU_EVGA_STOP_NVIDIA_CHILDREN
static EFI_STATUS NvidiaPost(EFI_SYSTEM_TABLE *t,GU_EVGA_RECORD record) {
  EFI_BOOT_SERVICES *b=t->BootServices;EFI_HANDLE *handles=NULL;UINTN n=0;
  EFI_STATUS s=b->LocateHandleBuffer(ByProtocol,&Gop,NULL,&n,&handles);
  BOOLEAN onlyVirtual=s==EFI_SUCCESS && handles && n==1 && handles[0]==Plan.Virtual;
  if(handles && b->FreePool(handles)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
  if(!onlyVirtual)return Checked(record,"evga-nvidia-only-virtual-gop",EFI_ACCESS_DENIED);
  EFI_GRAPHICS_OUTPUT_PROTOCOL *virtualGop=NULL;
  s=b->HandleProtocol(Plan.Virtual,&Gop,(VOID **)&virtualGop);
  if(s!=EFI_SUCCESS || !virtualGop || !virtualGop->Mode || virtualGop->Mode->FrameBufferBase || virtualGop->Mode->FrameBufferSize)return EFI_UNSUPPORTED;
  handles=NULL;n=0;s=b->LocateHandleBuffer(ByProtocol,&Edid,NULL,&n,&handles);
  BOOLEAN noEdid=s==EFI_NOT_FOUND && n==0 && !handles;
  if(handles && b->FreePool(handles)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
  if(!noEdid)return Checked(record,"evga-nvidia-edid-absent",EFI_ACCESS_DENIED);
  handles=NULL;n=0;s=b->LocateHandleBuffer(AllHandles,NULL,NULL,&n,&handles);
  if(s!=EFI_SUCCESS || !handles || !n || n>4096){if(handles)b->FreePool(handles);return EFI_UNSUPPORTED;}
  for(UINTN i=0;i<n;i++)for(UINTN j=0;j<3;j++)if(handles[i]==EdidHandles[j])s=EFI_ACCESS_DENIED;
  if(b->FreePool(handles)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
  s=Checked(record,"evga-nvidia-child-handles-absent",s);if(s!=EFI_SUCCESS)return s;
  s=ConsoleProtocols(t,record);if(s!=EFI_SUCCESS)return s;
  if(t->ConsoleOutHandle!=Plan.Console || t->StandardErrorHandle!=Plan.Console)return EFI_ACCESS_DENIED;
  VOID *text=NULL;s=b->HandleProtocol(Plan.Console,&Text,&text);if(s!=EFI_SUCCESS || !text)return EFI_UNSUPPORTED;
  s=VirtualInterfaces(t,TRUE);if(s!=EFI_SUCCESS)return s;
  s=Edges(b,Plan.Virtual,&Gop,NULL,FALSE,&Plan.VirtualText,TRUE);if(s!=EFI_SUCCESS)return s;
  s=Edges(b,NvidiaParent,&Path,NULL,FALSE,&NvidiaAgent,TRUE);if(s!=EFI_SUCCESS)return s;
  s=Edges(b,NvidiaParent,&Pci,NULL,FALSE,&NvidiaAgent,
#ifdef GU_EVGA_STOP_NVIDIA_PARENT
    NvidiaParentStopped
#else
    FALSE
#endif
  );if(s!=EFI_SUCCESS)return s;
  s=Code(b,NvidiaAgent,0x2b700,0x280,0x23d82,0x698,0x60009,"d045cee2113e461c7c678ec21542b1d0f18356765c0fc6a0bf2dcf824088e011",&NvidiaDriver,record);if(s!=EFI_SUCCESS)return s;
  EFI_GUID privateGuid={0xfe47349a,0x7f0d,0x4641,{0x82,0x2b,0x34,0xba,0xa2,0x8e,0xcd,0xd0}};
#ifdef GU_EVGA_STOP_NVIDIA_PARENT
  if(NvidiaParentStopped) {
    EFI_GUID **protocols=NULL;UINTN count=0;s=b->ProtocolsPerHandle(NvidiaParent,&protocols,&count);
    if(s!=EFI_SUCCESS || !protocols || count>256)return EFI_UNSUPPORTED;
    for(UINTN i=0;i<count;i++) {
      if(!protocols[i]){s=EFI_COMPROMISED_DATA;break;}
      BOOLEAN equal=TRUE;for(UINTN j=0;j<sizeof(EFI_GUID);j++)if(((UINT8 *)protocols[i])[j]!=((UINT8 *)&privateGuid)[j])equal=FALSE;
      if(equal){s=EFI_ACCESS_DENIED;break;}
    }
    if(b->FreePool(protocols)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
    if(s!=EFI_SUCCESS)return s;
    EFI_PCI_IO_PROTOCOL *pci=NULL;UINT32 config[16];
    s=b->HandleProtocol(NvidiaParent,&Pci,(VOID **)&pci);if(s!=EFI_SUCCESS || !pci)return EFI_UNSUPPORTED;
    s=pci->Pci.Read(pci,EfiPciIoWidthUint32,0,16,config);if(s!=EFI_SUCCESS)return s;
    if(config[0]!=0x220410deU || config[11]!=0x33873842U || (config[2]>>24)!=3)return EFI_UNSUPPORTED;
    s=Value(record,"evga-nvidia-post-parent-command",config[1]&0xffff);if(s!=EFI_SUCCESS)return s;
    return Checked(record,"evga-nvidia-parent-post-graph",EFI_SUCCESS);
  }
#endif
  UINT8 *context=NULL;s=b->HandleProtocol(NvidiaParent,&privateGuid,(VOID **)&context);
  if(s!=EFI_SUCCESS || !context || (UINTN)context>~(UINTN)0-0x99)return EFI_UNSUPPORTED;
  if(*(EFI_HANDLE *)(context+0x88)!=NULL || context[0x98]!=0)return EFI_ACCESS_DENIED;
  return Checked(record,"evga-nvidia-children-post-graph",EFI_SUCCESS);
}
#ifdef GU_EVGA_UNLOCK
static EFI_SYSTEM_TABLE *EvgaGraphTable;
/* The graph callback makes no file or console calls and holds TPL_NOTIFY. */
static BOOLEAN EvgaTransactionGraph(EFI_HANDLE parent,EFI_HANDLE image,BOOLEAN owned) {
 EFI_BOOT_SERVICES *b=EvgaGraphTable?EvgaGraphTable->BootServices:NULL;
 if(!b || !b->RaiseTPL || !b->RestoreTPL || parent!=NvidiaParent || image!=GuEvgaResourcesImage())return FALSE;
 EFI_TPL old=b->RaiseTPL(TPL_NOTIFY);EvgaAllowExclusive=owned;
 /* Graph() expects the pre-release physical GOP; only NvidiaPost() models
    the one-virtual-GOP state after the NVIDIA children have been removed. */
 EFI_STATUS s=NvidiaPost(EvgaGraphTable,NULL);
 /* All parent protocols must be passive, except the exact own PCI claim. */
 EFI_GUID **protocols=NULL;UINTN count=0,own=0;
 if(s==EFI_SUCCESS)s=b->ProtocolsPerHandle(parent,&protocols,&count);
 if(s==EFI_SUCCESS && (!protocols || !count || count>256))s=EFI_COMPROMISED_DATA;
 for(UINTN i=0;s==EFI_SUCCESS && i<count;i++) {
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *entries=NULL;UINTN n=0;
  if(!protocols[i]){s=EFI_COMPROMISED_DATA;break;}
  s=b->OpenProtocolInformation(parent,protocols[i],&entries,&n);
  if(s==EFI_SUCCESS && (n>64 || (n && !entries)))s=EFI_COMPROMISED_DATA;
  for(UINTN j=0;s==EFI_SUCCESS && j<n;j++) {
   EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *e=&entries[j];
   if(e->Attributes==1 || e->Attributes==2 || e->Attributes==4)continue;
   BOOLEAN same=TRUE;for(UINTN k=0;k<sizeof(EFI_GUID);k++)if(((UINT8 *)protocols[i])[k]!=((UINT8 *)&Pci)[k])same=FALSE;
   if(owned && same && e->Attributes==EFI_OPEN_PROTOCOL_EXCLUSIVE && e->AgentHandle==image &&
      !e->ControllerHandle && e->OpenCount==1 && !own){own++;continue;}
   s=EFI_ACCESS_DENIED;
  }
  if(entries && b->FreePool(entries)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
 }
 if(protocols && b->FreePool(protocols)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
 if(s==EFI_SUCCESS && own!=(owned?1U:0U))s=EFI_ACCESS_DENIED;
 EvgaAllowExclusive=FALSE;b->RestoreTPL(old);return s==EFI_SUCCESS;
}
#endif
#ifdef GU_EVGA_PROBE_DECODE
/* One bounded admission experiment, not an unlock path. Error holds; no blind restoration. */
static EFI_STATUS DecodeConfig(EFI_PCI_IO_PROTOCOL *pci,UINT16 expected,GU_EVGA_RECORD record,const char *event) {
  UINT32 config[16];EFI_STATUS s=pci->Pci.Read(pci,EfiPciIoWidthUint32,0,16,config);
  if(s==EFI_SUCCESS && (config[0]!=0x220410deU || config[11]!=0x33873842U || (config[2]>>24)!=3 || (config[1]&0xffff)!=expected))s=EFI_ACCESS_DENIED;
  return Checked(record,event,s);
}
#ifdef GU_EVGA_PROBE_STATE
static UINT32 StateProm[0x100000/4];
static EFI_STATUS StateProbe(EFI_PCI_IO_PROTOCOL *pci,GU_EVGA_RECORD record) {
  static const UINT32 offsets[]={0x118128,0x118234,0x110100,0x1fa828,0x823804,0x82381c,0x823820,0x1183a4};
  static const char *first[]={"evga-state-plm1","evga-state-boot1","evga-state-cpuctl1","evga-state-wpr1","evga-state-feature-plm1","evga-state-ss01","evga-state-ss11","evga-state-memory1"};
  static const char *second[]={"evga-state-plm2","evga-state-boot2","evga-state-cpuctl2","evga-state-wpr2","evga-state-feature-plm2","evga-state-ss02","evga-state-ss12","evga-state-memory2"};
  UINT32 baseline[8],value;EFI_STATUS s;
  for(UINTN pass=0;pass<2;pass++)for(UINTN i=0;i<8;i++) {
    s=Value(record,"evga-state-read-before",offsets[i]);if(s!=EFI_SUCCESS)return s;
    s=pci->Mem.Read(pci,EfiPciIoWidthUint32,0,offsets[i],1,&value);if(s!=EFI_SUCCESS)return Checked(record,"evga-state-read",s);
    s=Value(record,pass?second[i]:first[i],value);if(s!=EFI_SUCCESS)return s;
    if(value==0xffffffffU || (value>>16)==0xbadfU)return EFI_UNSUPPORTED;
    if(!pass)baseline[i]=value;else if(value!=baseline[i])return EFI_ACCESS_DENIED;
  }
  s=record(NULL,"evga-state-prom-before",EFI_SUCCESS);if(s!=EFI_SUCCESS)return s;
  /* Fixed1MiB window, one forward DWORD pass; no GPU write or firmware execution. */
  for(UINTN i=0;i<0x100000/4;i++) {
    s=pci->Mem.Read(pci,EfiPciIoWidthUint32,0,0x300000+i*4,1,&StateProm[i]);if(s!=EFI_SUCCESS)return Checked(record,"evga-state-prom-read",s);
  }
  if(!GuHashMatches(StateProm,sizeof(StateProm),"f1c7c6b205d9a5c0626af4f82e28cc377aa16cde8c25c61c77b4b5160b26e71f"))return Checked(record,"evga-state-prom-hash",EFI_COMPROMISED_DATA);
  return Checked(record,"evga-state-prom-hash",EFI_SUCCESS);
}
#endif
#ifdef GU_EVGA_PROBE_PLATFORM
static EFI_STATUS PlatformProbe(EFI_PCI_IO_PROTOCOL *pci,EFI_SYSTEM_TABLE *t,GU_EVGA_RECORD record) {
  UINT32 config[64];UINTN segment,bus,device,function;GU_CAPS caps;EFI_STATUS s;
  if(!pci->GetLocation || !pci->GetBarAttributes)return EFI_UNSUPPORTED;
  s=pci->GetLocation(pci,&segment,&bus,&device,&function);if(s!=EFI_SUCCESS)return s;
  s=pci->Pci.Read(pci,EfiPciIoWidthUint32,0,64,config);if(s!=EFI_SUCCESS)return s;
  if(!GuCheckConfigIdentity(config,segment,bus,device,function,1,0x220410deU,0x33873842U,&caps))return Checked(record,"evga-runtime-config",EFI_UNSUPPORTED);
  s=Checked(record,"evga-runtime-config",EFI_SUCCESS);if(s!=EFI_SUCCESS)return s;
  for(UINTN i=0;i<64;i++){s=Value(record,"evga-runtime-config-offset",i*4);if(s!=EFI_SUCCESS)return s;s=Value(record,"evga-runtime-config-dword",config[i]);if(s!=EFI_SUCCESS)return s;}
  if(config[2]!=0x030000a1U || config[0x10/4]!=0x84000000U || config[0xc8/4]!=0x00050011U || (config[(caps.pcie+8)/4]&0x00200000U))return Checked(record,"evga-runtime-config-extra",EFI_UNSUPPORTED);
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *r=NULL;s=pci->GetBarAttributes(pci,0,NULL,(VOID **)&r);
  if(s!=EFI_SUCCESS || !r){if(r)t->BootServices->FreePool(r);return EFI_UNSUPPORTED;}
  BOOLEAN valid=r->Desc==ACPI_ADDRESS_SPACE_DESCRIPTOR && r->Len==sizeof(*r)-3 && r->ResType==0 && r->AddrRangeMin==0x84000000ULL && r->AddrLen==0x1000000ULL && !r->AddrTranslationOffset && (r->AddrRangeMax==0x84ffffffULL || r->AddrRangeMax==0xffffffULL) && (r->AddrSpaceGranularity==32 || r->AddrSpaceGranularity==64);
  s=Value(record,"evga-runtime-bar-min",r->AddrRangeMin);if(s==EFI_SUCCESS)s=Value(record,"evga-runtime-bar-max",r->AddrRangeMax);if(s==EFI_SUCCESS)s=Value(record,"evga-runtime-bar-length",r->AddrLen);
  EFI_STATUS freed=t->BootServices->FreePool(r);if(s!=EFI_SUCCESS)return s;if(freed!=EFI_SUCCESS)return freed;
  return Checked(record,"evga-runtime-bar",valid?EFI_SUCCESS:EFI_UNSUPPORTED);
}
#endif
static EFI_STATUS DecodeProbe(EFI_SYSTEM_TABLE *t,GU_EVGA_RECORD record) {
  EFI_PCI_IO_PROTOCOL *pci=NULL;UINT64 saved=0,now=0;UINT32 chip=0;EFI_STATUS s;
  s=t->BootServices->HandleProtocol(NvidiaParent,&Pci,(VOID **)&pci);if(s!=EFI_SUCCESS || !pci)return EFI_UNSUPPORTED;
  if(!pci->Attributes || !pci->Mem.Read)return EFI_UNSUPPORTED;
  s=DecodeConfig(pci,0,record,"evga-decode-command-before");if(s!=EFI_SUCCESS)return s;
  s=pci->Attributes(pci,EfiPciIoAttributeOperationGet,0,&saved);if(s!=EFI_SUCCESS)return s;
  if(saved & (EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY|EFI_PCI_IO_ATTRIBUTE_BUS_MASTER|EFI_PCI_IO_ATTRIBUTE_VGA_MEMORY|EFI_PCI_IO_ATTRIBUTE_VGA_IO))return EFI_ACCESS_DENIED;
  s=record(NULL,"evga-memory-enable-before",EFI_SUCCESS);if(s!=EFI_SUCCESS)return s;
  s=pci->Attributes(pci,EfiPciIoAttributeOperationEnable,EFI_PCI_IO_ATTRIBUTE_MEMORY,NULL);
  s=Checked(record,"evga-memory-enable-return",s);if(s!=EFI_SUCCESS)return s;
  s=DecodeConfig(pci,2,record,"evga-decode-command-enabled");if(s!=EFI_SUCCESS)return s;
  for(UINTN i=0;i<2;i++) {
    s=record(NULL,i?"evga-chip-second-before":"evga-chip-first-before",EFI_SUCCESS);if(s!=EFI_SUCCESS)return s;
    s=pci->Mem.Read(pci,EfiPciIoWidthUint32,0,0,1,&chip);if(s!=EFI_SUCCESS)return Checked(record,"evga-chip-read",s);
    s=Value(record,i?"evga-chip-second":"evga-chip-first",chip);if(s!=EFI_SUCCESS)return s;
    if(chip!=0xb72000a1U)return Checked(record,"evga-chip-identity",EFI_UNSUPPORTED);
  }
#ifdef GU_EVGA_PROBE_PLATFORM
  s=PlatformProbe(pci,t,record);if(s!=EFI_SUCCESS)return s;
#if defined(GU_EVGA_PROBE_RESOURCES) && !defined(GU_EVGA_UNLOCK)
  s=GuEvgaResourcesClaim(t,NvidiaParent,pci,record);if(s!=EFI_SUCCESS)return s;
  s=NvidiaPost(t,record);if(s!=EFI_SUCCESS)return s;
#endif
#endif
#ifdef GU_EVGA_PROBE_STATE
  s=StateProbe(pci,record);if(s!=EFI_SUCCESS)return s;
#endif
#ifdef GU_EVGA_UNLOCK
  EvgaGraphTable=t;s=record(NULL,"evga-unlock-transaction-before",EFI_SUCCESS);if(s!=EFI_SUCCESS)return s;
  s=GuEvgaTransaction(t,GuEvgaResourcesImage(),NvidiaParent,pci,EvgaTransactionGraph,GuEvgaResourcesLogReady);
  EFI_STATUS logged=record(NULL,"evga-unlock-transaction-return",s);if(s!=EFI_SUCCESS)return s;if(logged!=EFI_SUCCESS)return logged;
  if(!Gu.owned || !GuSafeToReturn() || Gu.allocations || Gu.dma_count || !GuEvgaResourcesLogReady() ||
     !EvgaTransactionGraph(NvidiaParent,GuEvgaResourcesImage(),TRUE))return EFI_ACCESS_DENIED;
#endif
  s=record(NULL,"evga-memory-disable-before",EFI_SUCCESS);if(s!=EFI_SUCCESS)return s;
  s=pci->Attributes(pci,EfiPciIoAttributeOperationDisable,EFI_PCI_IO_ATTRIBUTE_MEMORY,NULL);
  s=Checked(record,"evga-memory-disable-return",s);if(s!=EFI_SUCCESS)return s;
  for(UINTN i=0;i<2;i++) {
    s=DecodeConfig(pci,0,record,"evga-decode-command-restored");if(s!=EFI_SUCCESS)return s;
    s=pci->Attributes(pci,EfiPciIoAttributeOperationGet,0,&now);if(s!=EFI_SUCCESS || now!=saved)return EFI_ACCESS_DENIED;
  }
#ifdef GU_EVGA_UNLOCK
  if(!Gu.owned || !GuSafeToReturn() || Gu.allocations || Gu.dma_count || !GuEvgaResourcesLogReady())return EFI_ACCESS_DENIED;
  s=record(NULL,"evga-unlock-close-before",EFI_SUCCESS);if(s!=EFI_SUCCESS)return s;
  s=t->BootServices->CloseProtocol(NvidiaParent,&Pci,GuEvgaResourcesImage(),NULL);
  if(s==EFI_SUCCESS)Gu.owned=FALSE;
  EFI_STATUS closedLog=record(NULL,"evga-unlock-close-return",s);if(s!=EFI_SUCCESS)return s;if(closedLog!=EFI_SUCCESS)return closedLog;
  if(!EvgaTransactionGraph(NvidiaParent,GuEvgaResourcesImage(),FALSE))return EFI_ACCESS_DENIED;
  s=GuEvgaResourcesRelease(t);if(s!=EFI_SUCCESS)return s;
  s=record(NULL,"evga-unlock-assets-released",EFI_SUCCESS);if(s!=EFI_SUCCESS)return s;
#endif
  return NvidiaPost(t,record);
}
#endif
static EFI_STATUS NvidiaChildrenStop(EFI_SYSTEM_TABLE *t,GU_EVGA_RECORD record) {
  EFI_STATUS s=NvidiaAudit(t,record);if(s!=EFI_SUCCESS)return s;
  EFI_HANDLE children[3];UINTN n=1;children[0]=Plan.Physical;
  for(UINTN i=0;i<3;i++)if(EdidHandles[i]!=Plan.Physical){if(n>=3)return EFI_COMPROMISED_DATA;children[n++]=EdidHandles[i];}
  if(n!=3 || !NvidiaDriver)return EFI_COMPROMISED_DATA;
  s=record(NULL,"evga-nvidia-children-stop-before",EFI_SUCCESS);if(s!=EFI_SUCCESS)return s;
  s=NvidiaDriver->Stop(NvidiaDriver,NvidiaParent,3,children);
  EFI_STATUS logged=record(NULL,"evga-nvidia-children-stop-return",s);if(logged!=EFI_SUCCESS)return logged;if(s!=EFI_SUCCESS)return s;
  NvidiaChildrenStopped=TRUE;s=NvidiaPost(t,record);if(s!=EFI_SUCCESS)return s;
#ifdef GU_EVGA_STOP_NVIDIA_PARENT
  s=record(NULL,"evga-nvidia-parent-stop-before",EFI_SUCCESS);if(s!=EFI_SUCCESS)return s;
  s=NvidiaDriver->Stop(NvidiaDriver,NvidiaParent,0,NULL);
  logged=record(NULL,"evga-nvidia-parent-stop-return",s);if(logged!=EFI_SUCCESS)return logged;if(s!=EFI_SUCCESS)return s;
  NvidiaParentStopped=TRUE;s=NvidiaPost(t,record);if(s!=EFI_SUCCESS)return s;
#ifdef GU_EVGA_PROBE_DECODE
  return DecodeProbe(t,record);
#else
  return s;
#endif
#else
  return EFI_SUCCESS;
#endif
}
#endif
EFI_STATUS GuEvgaTextStopVerify(EFI_SYSTEM_TABLE *t) {
  if(!t || !t->BootServices || !Plan.Driver || !Plan.TextAgent)return EFI_INVALID_PARAMETER;
#ifdef GU_EVGA_UNLOCK
  if(Gu.owned || Gu.armed || Gu.retained || Gu.fault || Gu.allocations || Gu.dma_count ||
     Gu.master_attributes_pending || !GuSafeToReturn() || !GuEvgaResourcesLogReady())return EFI_ACCESS_DENIED;
#endif
#ifdef GU_EVGA_STOP_NVIDIA_CHILDREN
  if(NvidiaChildrenStopped){
#ifdef GU_EVGA_PROBE_DECODE
    EFI_PCI_IO_PROTOCOL *pci=NULL;EFI_STATUS s=t->BootServices->HandleProtocol(NvidiaParent,&Pci,(VOID **)&pci);
    if(s!=EFI_SUCCESS || !pci)return EFI_UNSUPPORTED;
    s=DecodeConfig(pci,0,NULL,"unused");if(s!=EFI_SUCCESS)return s;
#endif
    return NvidiaPost(t,NULL);
  }
#endif
  EFI_STATUS s=Graph(t,TRUE,NULL); /* Files are closed: no logging on the final check. */
#ifdef GU_EVGA_AUDIT_NVIDIA
  if(s==EFI_SUCCESS)s=NvidiaAudit(t,NULL);
#endif
  return s;
}
EFI_STATUS GuEvgaTextStop(EFI_SYSTEM_TABLE *t,EFI_FILE_PROTOCOL *root,GU_EVGA_RECORD record,BOOLEAN *started) {
  if(!t || !t->BootServices || !t->ConIn || !root || !record || !started)return EFI_INVALID_PARAMETER;
  *started=FALSE;EFI_STATUS s=Checked(record,"evga-pci-inventory",Inventory(t->BootServices,record));if(s!=EFI_SUCCESS)return s;
  s=Checked(record,"evga-initial-graph",Graph(t,FALSE,record));if(s!=EFI_SUCCESS)return s;
#ifdef GU_EVGA_AUDIT_NVIDIA
  s=NvidiaAudit(t,record);if(s!=EFI_SUCCESS)return s;
#endif
  if(!t->ConOut || !t->ConOut->OutputString)return EFI_UNSUPPORTED;
  s=t->ConOut->OutputString(t->ConOut,L"Live guards passed. NOW press fresh Shift+G once (60 seconds); Esc cancels.\r\n");
  if(s!=EFI_SUCCESS)return s;
  /* Wait for a fresh uppercase G, not a key left over from the boot menu. */
  EFI_INPUT_KEY key;
  for(UINTN i=0;i<64;i++) {
    s=t->ConIn->ReadKeyStroke(t->ConIn,&key);if(s==EFI_NOT_READY)break;
    if(s!=EFI_SUCCESS)return s;if(i==63)return EFI_NOT_READY;
  }
  for(UINTN i=0;i<600;i++) {
    s=t->ConIn->ReadKeyStroke(t->ConIn,&key);
    if(s==EFI_SUCCESS && key.UnicodeChar=='G')break;
    if(s==EFI_SUCCESS && key.ScanCode==SCAN_ESC)return EFI_ABORTED;
    if(s!=EFI_SUCCESS && s!=EFI_NOT_READY)return s;
    if(i==599)return EFI_TIMEOUT;
    s=t->BootServices->Stall(100000);if(s!=EFI_SUCCESS)return s;
  }
  /* Durable one-shot marker, never overwritten or automatically removed. */
#ifdef GU_EVGA_UNLOCK
  UINTN attemptPathLength=0;while(AttemptPath[attemptPathLength])attemptPathLength++;
  AttemptPath[attemptPathLength-7]=(CHAR16)('0'+GU_EVGA_ATTEMPT_SLOT%10);
#if GU_EVGA_ATTEMPT_SLOT >= 10
  AttemptPath[attemptPathLength-8]=(CHAR16)('0'+GU_EVGA_ATTEMPT_SLOT/10);
#endif
#endif
  EFI_FILE_PROTOCOL *marker=NULL;
  s=root->Open(root,&marker,ATTEMPT_PATH,EFI_FILE_MODE_READ,0);
  if(s==EFI_SUCCESS){if(marker)marker->Close(marker);return EFI_ALREADY_STARTED;}
  if(s!=EFI_NOT_FOUND)return s;
  s=root->Open(root,&marker,ATTEMPT_PATH,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0);
  if(s!=EFI_SUCCESS || !marker)return EFI_DEVICE_ERROR;
#if defined(GU_EVGA_UNLOCK)
  char tag[]="EVGA release and one GPU unlock transaction armed; no retry\r\n";
#elif defined(GU_EVGA_STOP_NVIDIA_PARENT)
  char tag[]="Consumer and NVIDIA child/parent Stops armed; no retry\r\n";
#elif defined(GU_EVGA_STOP_NVIDIA_CHILDREN)
  char tag[]="Six consumer Stops plus one NVIDIA three-child Stop; no retry\r\n";
#elif defined(GU_EVGA_AUDIT_NVIDIA)
  char tag[]="Six consumer Stops plus NVIDIA CPU audit armed; no retry\r\n";
#elif defined(GU_EVGA_RELEASE_EDID_SIBLINGS)
  char tag[]="IMG13/16/17 and three EDID Stops armed; no retry\r\n";
#elif defined(GU_EVGA_RELEASE_EDID)
  char tag[]="IMG13/16/17 then physical EDID Stop armed; no retry\r\n";
#elif defined(GU_EVGA_RELEASE_GRAPHICS)
  char tag[]="IMG13/16/17 Stop armed; preserve evidence; no retry\r\n";
#elif defined(GU_EVGA_RELEASE_TEXT)
  char tag[]="IMG13 then IMG16 Stop armed; preserve evidence; no retry\r\n";
#else
  char tag[]="one IMG13 child Stop attempted; preserve evidence; no retry\r\n";
#endif
  UINTN n=sizeof(tag)-1;
  s=marker->Write(marker,&n,tag);if(s==EFI_SUCCESS && n!=sizeof(tag)-1)s=EFI_DEVICE_ERROR;
  if(s==EFI_SUCCESS)s=marker->Flush(marker);
  EFI_STATUS closed=marker->Close(marker);if(s==EFI_SUCCESS)s=closed;if(s!=EFI_SUCCESS)return s;
#ifdef GU_EVGA_UNLOCK
  char slotEvent[]="evga-unlock-slot-00";
  slotEvent[17]=(char)('0'+GU_EVGA_ATTEMPT_SLOT/10);
  slotEvent[18]=(char)('0'+GU_EVGA_ATTEMPT_SLOT%10);
  s=record(NULL,slotEvent,EFI_SUCCESS);if(s!=EFI_SUCCESS)return s;
#endif
  s=record(NULL,"evga-text-stop-before",EFI_SUCCESS);if(s!=EFI_SUCCESS)return s;
  s=Graph(t,FALSE,record);if(s!=EFI_SUCCESS)return s;
  /* Suppress all console calls from this point. No subsequent retry or rollback. */
  *started=TRUE;
  EFI_HANDLE child=Plan.Console;
  s=Plan.Driver->Stop(Plan.Driver,Plan.Virtual,1,&child);
  EFI_STATUS logged=record(NULL,"evga-text-stop-return",s);if(logged!=EFI_SUCCESS)return logged;
  if(s!=EFI_SUCCESS)return s;
  s=Graph(t,TRUE,record);
  logged=record(NULL,"evga-text-stop-post-graph",s);if(logged!=EFI_SUCCESS)return logged;if(s!=EFI_SUCCESS)return s;
#ifdef GU_EVGA_RELEASE_TEXT
  s=Checked(record,"evga-virtual-text-pre-stop",VirtualInterfaces(t,FALSE));if(s!=EFI_SUCCESS)return s;
  s=record(NULL,"evga-virtual-text-stop-before",EFI_SUCCESS);if(s!=EFI_SUCCESS)return s;
  /* No console/interface calls through the object IMG16 is about to free. */
  s=VirtualDriver->Stop(VirtualDriver,Plan.Virtual,0,NULL);
  logged=record(NULL,"evga-virtual-text-stop-return",s);if(logged!=EFI_SUCCESS)return logged;if(s!=EFI_SUCCESS)return s;
  TextReleased=TRUE;s=Graph(t,TRUE,record);
  s=Checked(record,"evga-virtual-text-post-graph",s);if(s!=EFI_SUCCESS)return s;
#ifdef GU_EVGA_RELEASE_GRAPHICS
  s=record(NULL,"evga-graphics-stop-before",EFI_SUCCESS);if(s!=EFI_SUCCESS)return s;
  EFI_HANDLE aggregate=Plan.Virtual;
  s=GraphicsDriver->Stop(GraphicsDriver,Plan.Physical,1,&aggregate);
  logged=record(NULL,"evga-graphics-stop-return",s);if(logged!=EFI_SUCCESS)return logged;if(s!=EFI_SUCCESS)return s;
  GraphicsReleased=TRUE;s=Graph(t,TRUE,record);
  s=Checked(record,"evga-graphics-post-graph",s);if(s!=EFI_SUCCESS)return s;
#ifdef GU_EVGA_RELEASE_EDID
  s=record(NULL,"evga-edid-stop-before",EFI_SUCCESS);if(s!=EFI_SUCCESS)return s;
  s=EdidDriver->Stop(EdidDriver,Plan.Physical,0,NULL);
  logged=record(NULL,"evga-edid-stop-return",s);if(logged!=EFI_SUCCESS)return logged;if(s!=EFI_SUCCESS)return s;
  EdidReleased=TRUE;s=Graph(t,TRUE,record);
  s=Checked(record,"evga-edid-post-graph",s);if(s!=EFI_SUCCESS)return s;
#ifdef GU_EVGA_RELEASE_EDID_SIBLINGS
  UINTN done=0;
  for(UINTN i=0;i<3;i++) {
    if(EdidHandles[i]==Plan.Physical)continue;
    if(done>=2)return EFI_COMPROMISED_DATA;
    s=record(NULL,done?"evga-edid-sibling2-before":"evga-edid-sibling1-before",EFI_SUCCESS);if(s!=EFI_SUCCESS)return s;
    s=EdidDriver->Stop(EdidDriver,EdidHandles[i],0,NULL);
    logged=record(NULL,done?"evga-edid-sibling2-return":"evga-edid-sibling1-return",s);if(logged!=EFI_SUCCESS)return logged;if(s!=EFI_SUCCESS)return s;
    EdidSiblingReleased[done]=EdidHandles[i];
    s=Graph(t,TRUE,record);
    s=Checked(record,done?"evga-edid-sibling2-post-graph":"evga-edid-sibling1-post-graph",s);if(s!=EFI_SUCCESS)return s;
    done++;
  }
  if(done!=2)return EFI_COMPROMISED_DATA;
#ifdef GU_EVGA_STOP_NVIDIA_CHILDREN
  return NvidiaChildrenStop(t,record);
#endif
#ifdef GU_EVGA_AUDIT_NVIDIA
  s=NvidiaAudit(t,record);if(s!=EFI_SUCCESS)return s;
#endif
#endif
  return EFI_SUCCESS;
#else
  return EFI_SUCCESS;
#endif
#else
  return EFI_SUCCESS;
#endif
#else
  return EFI_SUCCESS;
#endif
}
