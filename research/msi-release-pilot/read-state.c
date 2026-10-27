/* GPL-2.0-only. Stage 1 read/identity helpers extracted from local revision 9b10bfa.
 * No general release or decode restoration implementation is included.
 */
#include "../../internal-boot/boot/full-release.h"
static BOOLEAN OwnRecord(EFI_HANDLE Handle,const EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *Entry);
#define GU_SNAPSHOT_OWN_RECORD OwnRecord
#include "../../internal-boot/boot/prepare-console.c"
#include "../../internal-boot/unlock/checks.h"
#include "../../internal-boot/unlock/sha256.h"
#include <Protocol/EdidDiscovered.h>
#include <IndustryStandard/Acpi.h>

#define GU_HEADLESS_MAX_DISPLAY_CHILDREN 4

static EFI_GUID EdidGuid=EFI_EDID_DISCOVERED_PROTOCOL_GUID;
static const EFI_GUID MouseFile={0xc7a7030c,0xc3d8,0x45ee,{0xbe,0xd9,0x5d,0x9e,0x76,0x76,0x29,0x53}};
typedef struct {
  EFI_SYSTEM_TABLE *Table;
  EFI_HANDLE Image,EdidAgent,NvAgent;
  GU_PREPARE_LOG Log;
  VOID *Context;
  UINT32 Config[64],Cold[9];
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR Bar;
  UINT64 Attributes;
  BOOLEAN Captured,Released,Owned,Restored;
#if defined(GU_HEADLESS_RESEARCH) || defined(GU_RELEASE_PILOT)
  EFI_HANDLE Children[GU_HEADLESS_MAX_DISPLAY_CHILDREN];
  UINT8 ChildPaths[GU_HEADLESS_MAX_DISPLAY_CHILDREN][36];
  UINTN ChildCount;
  UINTN EdidDetached;
#endif
} FULL;
static FULL Full;
static BOOLEAN OwnRecord(EFI_HANDLE h,const EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *e) {
  return Full.Owned && h==PreparedPlan.RtxPci && e->AgentHandle==Full.Image && !e->ControllerHandle &&
    e->Attributes==EFI_OPEN_PROTOCOL_EXCLUSIVE && e->OpenCount==1;
}
static const UINT32 ColdOffsets[]={0,0x118128,0x118234,0x110100,0x1fa828,0x823804,0x82381c,0x823820,0x1183a4};
static EFI_STATUS Note(const char *event,EFI_STATUS s) {
  EFI_STATUS written=Full.Log(Full.Context,event,s);return s==EFI_SUCCESS?written:s;
}
static EFI_STATUS LogConfig(const char *label,const UINT32 *cfg) {
  for(UINTN row=0;row<16;row++) {
    char event[100];UINTN n=0;
    for(UINTN i=0;label[i] && n<30;i++)event[n++]=label[i];
    event[n++]='-';event[n++]="0123456789abcdef"[row];event[n++]='=';
    for(UINTN word=0;word<4;word++) {
      for(int digit=7;digit>=0;digit--)event[n++]="0123456789abcdef"[(cfg[row*4+word]>>(digit*4))&15];
      event[n++]=',';
    }
    event[n]=0;EFI_STATUS s=Note(event,EFI_SUCCESS);if(s!=EFI_SUCCESS)return s;
  }
  return EFI_SUCCESS;
}

static EFI_STATUS CheckBar(BOOLEAN first) {
  EFI_PCI_IO_PROTOCOL *io=PreparedPlan.RtxIo;VOID *raw=NULL;
  if(!io->GetBarAttributes)return EFI_UNSUPPORTED;
  EFI_STATUS s=io->GetBarAttributes(io,0,NULL,&raw);
  if(s==EFI_SUCCESS) {
    EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *d=raw;
    if(!d || d->Desc!=ACPI_ADDRESS_SPACE_DESCRIPTOR || d->Len!=sizeof(*d)-3 || d->ResType || d->AddrSpaceGranularity!=32 ||
       d->AddrRangeMin!=GU_BAR0_BASE || d->AddrLen!=0x1000000 || d->AddrTranslationOffset ||
       (d->AddrRangeMax!=GU_BAR0_END && d->AddrRangeMax!=0x00ffffff) || ((UINT8 *)(d+1))[0]!=ACPI_END_TAG_DESCRIPTOR)s=EFI_UNSUPPORTED;
    if(s==EFI_SUCCESS) {
      UINT8 sum=0;for(UINTN i=0;i<sizeof(*d)+2;i++)sum=(UINT8)(sum+((UINT8 *)d)[i]);
      if(((UINT8 *)d)[sizeof(*d)+1] && sum)s=EFI_CRC_ERROR;
      else if(first)Full.Bar=*d;
      else if(!Bytes(d,&Full.Bar,sizeof(*d)))s=EFI_NOT_READY;
    }
  }
  if(raw && Full.Table->BootServices->FreePool(raw)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  return s;
}
static EFI_STATUS Cold(UINT32 *values) {
  EFI_PCI_IO_PROTOCOL *io=PreparedPlan.RtxIo;
  if(!io->Mem.Read)return EFI_UNSUPPORTED;
  for(UINTN i=0;i<9;i++) {
    EFI_STATUS s=io->Mem.Read(io,EfiPciIoWidthUint32,0,ColdOffsets[i],1,&values[i]);
    if(s!=EFI_SUCCESS)return s;
  }
  if(values[0]!=GU_CHIP_ID || values[1]!=0x8b8f || values[2]!=0x3ff || values[3]!=0x10 || values[4] ||
     values[5]!=0xffffff8f || values[6]==0xffffffff || values[6]==0x88888888 || (values[6]&0xffff0000)==0xbadf0000 ||
     values[7]>15 || values[8]!=GU_DEVICE_MEMORY_SIZE)return EFI_NOT_READY;
  return EFI_SUCCESS;
}
static EFI_STATUS Profile(EFI_HANDLE agent,BOOLEAN mouse) {
#if GU_NO_SEPARATE_EDID
  if(mouse)return EFI_UNSUPPORTED;
#endif
  EFI_BOOT_SERVICES *bs=Full.Table->BootServices;EFI_DRIVER_BINDING_PROTOCOL *d=NULL;EFI_LOADED_IMAGE_PROTOCOL *im=NULL;
  EFI_STATUS s=Interface(bs,agent,&BindingGuid,(VOID **)&d);
  if(s!=EFI_SUCCESS || !d || !d->Stop || d->DriverBindingHandle!=agent || d->ImageHandle!=agent || d->Version!=(mouse?1:GU_NVGOP_VERSION))return EFI_UNSUPPORTED;
  s=Interface(bs,d->ImageHandle,&ImageGuid,(VOID **)&im);
  UINTN size=mouse?GU_EDID_IMAGE_SIZE:GU_NVGOP_IMAGE_SIZE,offset=mouse?GU_EDID_STOP:GU_NVGOP_STOP;
  if(s!=EFI_SUCCESS || !im || !im->ImageBase || im->ImageSize!=size || im->ImageCodeType!=(mouse?EfiRuntimeServicesCode:EfiBootServicesCode) ||
     (UINTN)d->Stop!=(UINTN)im->ImageBase+offset || !im->FilePath)return EFI_UNSUPPORTED;
  UINTN n=0;
  if(!GuInspectPath(im->FilePath,NULL,&n,NULL,NULL))return EFI_UNSUPPORTED;
  if(mouse) {
    if(n!=24 || im->FilePath->Type!=MEDIA_DEVICE_PATH || im->FilePath->SubType!=MEDIA_PIWG_FW_FILE_DP ||
       im->FilePath->Length[0]!=20 || im->FilePath->Length[1] || !GuGuidEqual((UINT8 *)im->FilePath+4,&MouseFile))return EFI_UNSUPPORTED;
  } else {
    const CHAR16 name[]=L"nvgop-ga1xx";
    if(im->DeviceHandle!=PreparedPlan.RtxPci || n!=4+sizeof(name)+4 || im->FilePath->Type!=MEDIA_DEVICE_PATH || im->FilePath->SubType!=MEDIA_FILEPATH_DP ||
       im->FilePath->Length[0]!=4+sizeof(name) || im->FilePath->Length[1] || !Bytes((UINT8 *)im->FilePath+4,name,sizeof(name)))return EFI_UNSUPPORTED;
  }
  /* Both original PE relocation tables contain zero relocations in .text.
   * Therefore hash the loaded code range directly; never hash relocated data. */
  UINTN rva=mouse?0x1000:0x280,bytes=mouse?GU_EDID_TEXT_BYTES:GU_NVGOP_TEXT_BYTES;
  const char *hash=mouse?GU_EDID_TEXT_SHA256:GU_NVGOP_TEXT_SHA256;
  return GuHashMatches((UINT8 *)im->ImageBase+rva,bytes,hash)?EFI_SUCCESS:EFI_CRC_ERROR;
}
static EFI_STATUS FullConsumers(const GRAPH *g) {
  EFI_BOOT_SERVICES *bs=Full.Table->BootServices;
  UINTN owners=0,children=0;
  for(UINTN i=0;i<g->Count;i++) {
    const EDGE *e=&g->Edges[i];
    /* Snapshot excludes passive GET_PROTOCOL observations. Every remaining
     * physical-child consumer is absent from this prospective filtered view.
     * Filtering the view does not disconnect any real consumer. */
    if(e->Handle==PreparedPlan.Rtx)return EFI_ACCESS_DENIED;
    if(e->Handle!=PreparedPlan.RtxPci)continue;
    if(e->Count!=1 || !e->Agent || e->Agent==Full.Image)return EFI_ACCESS_DENIED;
    if(GuGuidEqual(&e->Protocol,&PciGuid) && e->Attributes==EFI_OPEN_PROTOCOL_BY_DRIVER && e->Controller==PreparedPlan.RtxPci) {
      Full.NvAgent=e->Agent;owners++;
    } else if(GuGuidEqual(&e->Protocol,&PathGuid) && e->Attributes==EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER && e->Controller==PreparedPlan.Rtx)children++;
    else return EFI_ACCESS_DENIED;
  }
  if(owners!=1 || children!=1)return EFI_ACCESS_DENIED;
  for(UINTN i=0;i<g->Count;i++)if(g->Edges[i].Handle==PreparedPlan.RtxPci && g->Edges[i].Agent!=Full.NvAgent)return EFI_ACCESS_DENIED;
  EFI_DEVICE_PATH_PROTOCOL *path=NULL;EFI_HANDLE pci=NULL;EFI_PCI_IO_PROTOCOL *io=NULL;UINT32 cfg[16];
  EFI_STATUS s=Interface(bs,PreparedPlan.Rtx,&PathGuid,(VOID **)&path);
  if(s!=EFI_SUCCESS || !DisplayRoute(path,RtxPath))return EFI_UNSUPPORTED;
  s=PciFor(bs,path,GU_TARGET_BUS,GU_BOARD_ID,GU_BOARD_SUBSYSTEM,&pci,&io,cfg);
  if(s!=EFI_SUCCESS || pci!=PreparedPlan.RtxPci || io!=PreparedPlan.RtxIo || !Bytes(cfg,PreparedPlan.RtxConfig,sizeof(cfg)))return EFI_NOT_READY;
  Full.ChildCount=1;Full.Children[0]=PreparedPlan.Rtx;
  for(UINTN i=0;i<36;i++)Full.ChildPaths[0][i]=((UINT8 *)path)[i];
  return EFI_SUCCESS;
}
