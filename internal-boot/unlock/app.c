/* Modified 2026-10-19/20 for Windows GPU UNLOCK; changes documented in CHANGES.md. */ #include "app.h"
#include "app_io.h"
#include "compat/compat.h"
#include "checks.h"
#include "sha256.h"
#include "ownership.h"
#include "vendor/unlock_610.h"
#include "vendor/gfw_eval.h"
#include <Protocol/GraphicsOutput.h>
#include <IndustryStandard/Acpi.h>
#include <Guid/GlobalVariable.h>
#ifdef GU_HEADLESS_RESEARCH
#include "headless-policy.h"
static BOOLEAN GuAppError(EFI_STATUS s){return s!=EFI_SUCCESS;}
#undef EFI_ERROR
#define EFI_ERROR(Status) GuAppError(Status)
#endif
#if defined(GU_AUTOMATIC_BOOT) && (!defined(GU_EXPERIMENTAL_3090) || !defined(GU_INTERNAL_BOOT) || !defined(GU_ENTRY_LATCHED_SS1_FULL))
#error Automatic boot requires experimental internal entryLatchedSs1Full
#endif
#ifdef GU_HEADLESS_RESEARCH
static EFI_GUID PciGuid=EFI_PCI_IO_PROTOCOL_GUID,GlobalGuid=EFI_GLOBAL_VARIABLE;
#else
static EFI_GUID PciGuid=EFI_PCI_IO_PROTOCOL_GUID,GopGuid=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID,
  PathGuid=EFI_DEVICE_PATH_PROTOCOL_GUID,GlobalGuid=EFI_GLOBAL_VARIABLE;
#endif
static EFI_SYSTEM_TABLE *Table;
static UINT32 InitialConfig[64];
static const char *Stage;
static GU_RELEASE_OWNER PendingOwner;
static EFI_STATUS Step(EFI_STATUS Status) {
  GuLog("CHECK stage=%s status=%#llx\n",Stage,(unsigned long long)Status);
#ifdef GU_AUTOMATIC_BOOT
  if(Status!=EFI_SUCCESS && !EFI_ERROR(Status))return EFI_DEVICE_ERROR;
#endif
  return !GuLogReady() && !EFI_ERROR(Status)?EFI_DEVICE_ERROR:Status;
}
static VOID ConfigLog(const UINT32 *Config) {
  for(UINTN i=0;i<64;i+=4)GuLog("config[%02x]=%08x %08x %08x %08x\n",
    (unsigned)(i*4),Config[i],Config[i+1],Config[i+2],Config[i+3]);
}
#ifndef GU_HEADLESS_RESEARCH
static VOID GopInfoLog(UINTN Index,const EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info) {
  GuLog("GOP[%u] format=%u size=%ux%u stride=%u\n",(unsigned)Index,
    (unsigned)Info->PixelFormat,Info->HorizontalResolution,Info->VerticalResolution,Info->PixelsPerScanLine);
}
static BOOLEAN ConsoleGopCheck(EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop,UINTN Index,
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Output) {
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode=Gop->Mode;
  if(mode->Info && mode->SizeOfInfo>=sizeof(*mode->Info)) {
    *Output=*mode->Info;GopInfoLog(Index,Output);return Output->PixelFormat==PixelBltOnly;
  }
  /* RUN02's console aggregator leaves SizeOfInfo zero. Never read an unsized
     Info pointer: QueryMode supplies its own sized, callee-allocated buffer.
     The caller restricts this fallback to a pathless, zero-framebuffer GOP. */
  if(mode->SizeOfInfo || !Gop->QueryMode)return FALSE;
  UINT32 number=mode->Mode;UINTN bytes=0;EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info=NULL;
  EFI_STATUS status=Gop->QueryMode(Gop,number,&bytes,&info);
  GuLog("GOP[%u] console QueryMode: status=%#llx mode=%u info_bytes=%zu\n",
    (unsigned)Index,(unsigned long long)status,number,(size_t)bytes);
  BOOLEAN valid=!EFI_ERROR(status) && info && bytes>=sizeof(*info);
  if(valid) {
    *Output=*info;GopInfoLog(Index,Output);
    valid=(info->PixelFormat==PixelBltOnly || info->PixelFormat==PixelRedGreenBlueReserved8BitPerColor ||
      info->PixelFormat==PixelBlueGreenRedReserved8BitPerColor) && Gop->Mode==mode && mode->Mode==number &&
      !mode->FrameBufferBase && !mode->FrameBufferSize;
  }
  if(info) {
    status=Gu.bs->FreePool(info);
    if(EFI_ERROR(status)){GuLog("GOP[%u] query FreePool failed: %#llx\n",(unsigned)Index,(unsigned long long)status);valid=FALSE;}
  }
  return valid;
}
static BOOLEAN AmdGopParent(EFI_DEVICE_PATH_PROTOCOL *Path) {
  EFI_DEVICE_PATH_PROTOCOL *remaining=Path;EFI_HANDLE parent=NULL;
  EFI_PCI_IO_PROTOCOL *pci=NULL;UINT32 id=0,subsystem=0;UINTN s,b,d,f;
  if(!Gu.bs->LocateDevicePath)return FALSE;
  EFI_STATUS status=Gu.bs->LocateDevicePath(&PciGuid,&remaining,&parent);
  if(EFI_ERROR(status) || !parent || (UINT8 *)remaining!=(UINT8 *)Path+24)return FALSE;
  status=Gu.bs->HandleProtocol(parent,&PciGuid,(VOID **)&pci);
  if(EFI_ERROR(status) || !pci || !pci->GetLocation || !pci->Pci.Read)return FALSE;
  status=pci->GetLocation(pci,&s,&b,&d,&f);
  if(EFI_ERROR(status) || s || b!=12 || d || f)return FALSE;
  status=pci->Pci.Read(pci,EfiPciIoWidthUint32,0,1,&id);
  if(!EFI_ERROR(status))status=pci->Pci.Read(pci,EfiPciIoWidthUint32,0x2c,1,&subsystem);
  GuLog("AMD GOP PCI parent: status=%#llx id=%#x subsystem=%#x\n",(unsigned long long)status,id,subsystem);
  return !EFI_ERROR(status) && id==GU_AMD_ID && subsystem==GU_AMD_SUBSYSTEM;
}
#endif
static BOOLEAN DisplayCheck(VOID) {
#ifdef GU_HEADLESS_RESEARCH
  return GuHeadlessCheck() && GuLogReady();
#else
  EFI_HANDLE *handles=NULL;UINTN count=0,intel=0;BOOLEAN valid=TRUE;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION physical={0},forwarded={0};BOOLEAN forwarding=FALSE;
  EFI_STATUS status=Gu.bs->LocateHandleBuffer(ByProtocol,&GopGuid,NULL,&count,&handles);
  GuLog("GOP enumeration: status=%#llx handles=%zu\n",(unsigned long long)status,(size_t)count);
  if(EFI_ERROR(status) || !handles || !count || count>16){if(handles)GuFree(handles);return FALSE;}
  for(UINTN i=0;i<count;i++) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop=NULL;EFI_DEVICE_PATH_PROTOCOL *path=NULL;
    status=Gu.bs->HandleProtocol(handles[i],&GopGuid,(VOID **)&gop);
    if(EFI_ERROR(status) || !gop || !gop->Mode){
      GuLog("GOP[%u] invalid protocol/mode: status=%#llx\n",(unsigned)i,
        (unsigned long long)status);valid=FALSE;break;
    }
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode=gop->Mode;
    status=Gu.bs->HandleProtocol(handles[i],&PathGuid,(VOID **)&path);
    int route=!EFI_ERROR(status) && path?GuDisplayPath(path,256):0;
    GuLog("GOP[%u] path_status=%#llx route=%d framebuffer=%#llx bytes=%zu mode=%u max_mode=%u info_bytes=%zu console=%u\n",
      (unsigned)i,(unsigned long long)status,route,(unsigned long long)mode->FrameBufferBase,
      (size_t)mode->FrameBufferSize,mode->Mode,mode->MaxMode,(size_t)mode->SizeOfInfo,(unsigned)(handles[i]==Table->ConsoleOutHandle));
    if((status==EFI_UNSUPPORTED || status==EFI_NOT_FOUND) && !path && !mode->FrameBufferBase &&
       !mode->FrameBufferSize) {
      EFI_GRAPHICS_OUTPUT_MODE_INFORMATION info;
      if(ConsoleGopCheck(gop,i,&info)) {
        if(info.PixelFormat==PixelBltOnly)continue;
        /* EDK2 ConSplitter forwards QueryMode to its sole physical GOP. Accept
           that RGB/BGR result only on the system console, then match it against
            the one validated AMD GOP after enumeration (handle order varies). */
        if(handles[i]==Table->ConsoleOutHandle && !forwarding) {
          forwarded=info;forwarding=TRUE;continue;
        }
      }
      valid=FALSE;break;
    }
    if(!mode->Info || mode->SizeOfInfo<sizeof(*mode->Info)){valid=FALSE;break;}
    GopInfoLog(i,mode->Info);
     if(EFI_ERROR(status) || !path || route!=1 || !AmdGopParent(path) || !mode->FrameBufferBase ||
       !mode->FrameBufferSize || mode->Info->PixelFormat>PixelBlueGreenRedReserved8BitPerColor ||
       !mode->Info->HorizontalResolution || !mode->Info->VerticalResolution ||
       mode->Info->PixelsPerScanLine<mode->Info->HorizontalResolution ||
       (UINT64)mode->Info->PixelsPerScanLine*mode->Info->VerticalResolution>mode->FrameBufferSize/4) {
      valid=FALSE;break;
    }
    physical=*mode->Info;intel++;
  }
  if(forwarding) {
    BOOLEAN match=intel==1 && forwarded.Version==physical.Version && forwarded.PixelFormat==physical.PixelFormat &&
      forwarded.HorizontalResolution==physical.HorizontalResolution && forwarded.VerticalResolution==physical.VerticalResolution &&
      forwarded.PixelsPerScanLine==physical.PixelsPerScanLine;
    GuLog("GOP console forwarded AMD match=%u\n",match);valid=valid && match;
  }
  GuFree(handles);GuLog("GOP result: valid=%u amd_physical=%zu\n",valid,(size_t)intel);
  return valid && intel==1 && GuLogReady();
#endif
}
static EFI_STATUS ReadConfig(UINT32 *Config) {
  for(UINTN i=0;i<64;i++)if(EFI_ERROR(GuConfigRead(EfiPciIoWidthUint32,(UINT32)i*4,&Config[i]))) {
    GuLog("PCI config read failed: offset=%#x valid_bytes=%u; snapshot incomplete\n",(unsigned)(i*4),(unsigned)(i*4));
    return EFI_DEVICE_ERROR;
  }
  return EFI_SUCCESS;
}
static EFI_STATUS ConfigCheck(BOOLEAN Save) {
  UINT32 config[64];GU_CAPS caps;UINTN segment,bus,device,function;
  EFI_STATUS status=Gu.pci->GetLocation(Gu.pci,&segment,&bus,&device,&function);
  GuLog("target location: status=%#llx\n",(unsigned long long)status);
  if(EFI_ERROR(status))return status;
  status=ReadConfig(config);if(EFI_ERROR(status))return status;
  BOOLEAN checked=GuCheckConfig(config,segment,bus,device,function,&caps);
  BOOLEAN valid=checked && config[2]==0x030000a1 && config[0xc8/4]==0x00050011 &&
    config[0x10/4]==GU_BAR0_BASE && !(config[(caps.pcie+8)/4]&0x00200000U);
  BOOLEAN same=Save || !memcmp(config,InitialConfig,sizeof(config));
  if(Save || !valid || !same)ConfigLog(config);
  GuLog("target BDF=%zu:%zu:%zu.%zu checks=%u pm=%#x pcie=%#x msi=%#x msix=%#x\n",
    (size_t)segment,(size_t)bus,(size_t)device,(size_t)function,checked,
    (unsigned)caps.pm,(unsigned)caps.pcie,(unsigned)caps.msi,(unsigned)caps.msix);
  if(!valid)return EFI_UNSUPPORTED;
  if(!same)return EFI_NOT_READY;
  if(Save)memcpy(InitialConfig,config,sizeof(config));
  Gu.pm_cap=caps.pm;Gu.pcie_cap=caps.pcie;Gu.pcie_version=caps.pcie_version;Gu.msi_cap=caps.msi;Gu.msix_cap=caps.msix;
  return EFI_SUCCESS;
}
static EFI_STATUS RestoreDecode(VOID) {
  UINT32 current[64],normalized[64],after[64];UINTN segment,bus,device,function;
  if(GU_PREFLIGHT_ONLY || !Gu.profile_verified || !Gu.pci || !Gu.owned || !Gu.armed || !Gu.display_ok || Gu.started || Gu.allocations ||
    Gu.dma_count || Gu.fault || Gu.retained)return EFI_ACCESS_DENIED;
  EFI_STATUS status=Gu.pci->GetLocation(Gu.pci,&segment,&bus,&device,&function);
  if(EFI_ERROR(status))return status;
  status=ReadConfig(current);if(EFI_ERROR(status))return status;
  GuLogText("config before decode restore (256 valid bytes):\n");ConfigLog(current);
  if(!GuTargetConfig(current,segment,bus,device,function))return EFI_UNSUPPORTED;
  UINT32 missing=InitialConfig[1]&~current[1]&3U;
  memcpy(normalized,current,sizeof(normalized));normalized[1]|=missing;
  /* Restore only cleared, previously enabled IO/MEM bits. Every other bit,
     including BME, status, BAR, ROM and PM/capability state must be unchanged. */
  if(memcmp(normalized,InitialConfig,sizeof(normalized))) {
    GuLogText("decode restore refused: config differs beyond cleared original IO/MEM bits\n");
    return EFI_UNSUPPORTED;
  }
  if(!missing){GuLogText("decode restore not needed\n");return EFI_SUCCESS;}
  if(!Gu.pci->Attributes)return EFI_UNSUPPORTED;
  UINT64 needed=(missing&1?EFI_PCI_IO_ATTRIBUTE_IO:0)|(missing&2?EFI_PCI_IO_ATTRIBUTE_MEMORY:0);
  UINT64 supported=0,attributes=0;
  status=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationSupported,0,&supported);
  GuLog("decode supported attributes: status=%#llx mask=%#llx\n",(unsigned long long)status,(unsigned long long)supported);
  if(EFI_ERROR(status))return status;
  if((supported&needed)!=needed)return EFI_UNSUPPORTED;
  status=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationGet,0,&attributes);
  GuLog("decode current attributes: status=%#llx mask=%#llx\n",(unsigned long long)status,(unsigned long long)attributes);
  if(EFI_ERROR(status))return status;
  GuLog("decode restore intent: saved_command=%#x current_command=%#x missing=%#x attributes=%#llx\n",
    InitialConfig[1]&0xffff,current[1]&0xffff,missing,(unsigned long long)needed);
  if(!GuLogReady() || Gu.fault)return EFI_DEVICE_ERROR;
  /* Never use EFI_PCI_DEVICE_ENABLE: it would also enable bus mastering. */
  EFI_STATUS enabled=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationEnable,needed,NULL);
  GuLog("decode enable status=%#llx\n",(unsigned long long)enabled);
  if(!GuLogReady() || Gu.fault)return EFI_ERROR(enabled)?enabled:EFI_DEVICE_ERROR;
  /* Even an error may follow a partial hardware effect. Capture it if readable,
     but never turn an API error into success merely because readback matches. */
  status=ReadConfig(after);
  BOOLEAN matched=FALSE;
  if(!EFI_ERROR(status)) {
    GuLogText("config after decode restore (256 valid bytes):\n");ConfigLog(after);
    matched=!memcmp(after,InitialConfig,sizeof(after));GuLog("decode readback matched=%u\n",matched);
  }
  if(EFI_ERROR(enabled))return enabled;
  if(EFI_ERROR(status))return status;
  if(!GuLogReady() || Gu.fault)return EFI_DEVICE_ERROR;
  return matched?EFI_SUCCESS:EFI_COMPROMISED_DATA;
}
static EFI_STATUS FindTarget(VOID) {
  EFI_HANDLE *handles=NULL;UINTN count=0,found=0;
  EFI_STATUS status=Gu.bs->LocateHandleBuffer(ByProtocol,&PciGuid,NULL,&count,&handles);
  GuLog("PCI enumeration: status=%#llx handles=%zu\n",(unsigned long long)status,(size_t)count);
  if(EFI_ERROR(status) || !handles || !count || count>256){if(handles)GuFree(handles);return EFI_NOT_FOUND;}
  for(UINTN i=0;i<count;i++) {
    EFI_PCI_IO_PROTOCOL *pci=NULL;UINT32 id=0;
    status=Gu.bs->HandleProtocol(handles[i],&PciGuid,(VOID **)&pci);
    if(!EFI_ERROR(status) && pci)status=pci->Pci.Read(pci,EfiPciIoWidthUint32,0,1,&id);
    if(EFI_ERROR(status) || !pci){GuLog("PCI scan failed: index=%zu status=%#llx\n",(size_t)i,(unsigned long long)status);status=EFI_DEVICE_ERROR;break;}
    if(id==GU_BOARD_ID){found++;Gu.handle=handles[i];Gu.pci=pci;}
  }
  GuFree(handles);GuLog("PCI target matches=%zu status=%#llx\n",(size_t)found,(unsigned long long)status);
  if(EFI_ERROR(status) || found!=1)return EFI_UNSUPPORTED;
  return ConfigCheck(TRUE);
}
static EFI_STATUS BarCheck(VOID) {
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *r=NULL;
  EFI_STATUS status=Gu.pci->GetBarAttributes(Gu.pci,0,NULL,(VOID **)&r);
  GuLog("bar0 query: status=%#llx\n",(unsigned long long)status);
  if(!EFI_ERROR(status)) {
    if(r && r->Desc==ACPI_ADDRESS_SPACE_DESCRIPTOR && r->Len==sizeof(*r)-3)
      GuLog("bar0 descriptor: type=%u granularity=%llu min=%#llx max=%#llx length=%#llx translation=%#llx\n",
        (unsigned)r->ResType,(unsigned long long)r->AddrSpaceGranularity,(unsigned long long)r->AddrRangeMin,
        (unsigned long long)r->AddrRangeMax,(unsigned long long)r->AddrLen,(unsigned long long)r->AddrTranslationOffset);
    if(!r || r->Desc!=ACPI_ADDRESS_SPACE_DESCRIPTOR || r->Len!=sizeof(*r)-3 || r->ResType!=0 ||
      r->AddrRangeMin!=GU_BAR0_BASE || r->AddrLen!=0x1000000 || r->AddrTranslationOffset ||
      /* UEFI describes an inclusive end; pinned EDK2 PciBusDxe instead returns
         PciBar.Alignment. Both representations describe this exact 16 MiB BAR. */
      (r->AddrRangeMax!=GU_BAR0_END && r->AddrRangeMax!=0x00ffffff) ||
      r->AddrSpaceGranularity!=32 || ((UINT8 *)(r+1))[0]!=ACPI_END_TAG_DESCRIPTOR)status=EFI_UNSUPPORTED;
    else {
      UINT8 *raw=(UINT8 *)r,sum=0;
      for(UINTN i=0;i<sizeof(*r)+2;i++)sum=(UINT8)(sum+raw[i]);
      if(raw[sizeof(*r)+1] && sum)status=EFI_COMPROMISED_DATA;
      else {Gu.bar_base=r->AddrRangeMin;Gu.bar_size=r->AddrLen;}
    }
  }
  if(r)GuFree(r);return status;
}
static EFI_STATUS ColdCheck(VOID) {
  Gu.profile_verified=FALSE;
  UINT32 chip=GuRead32(0);
  struct gpu_unlock_gfw_sample s={.config_ok=true,.plm=GuRead32(GU_GFW_PLM_OFFSET),
    .boot=GuRead32(GU_GFW_BOOT_OFFSET),.cpuctl=GuRead32(GU_GFW_CPUCTL_OFFSET)};
  UINT32 hi=GuRead32(0x1fa828),plm=GuRead32(GU_FEAT_PLM_OFFSET),ss0=GuRead32(GU_FEAT_SS0_OFFSET),ss1=GuRead32(GU_FEAT_SS1_OFFSET);
  GuLog("cold state: chip=%#x plm=%#x gfw=%#x/%#x/%#x WPR2_HI=%#x SS0=%#x SS1=%#x\n",
    chip,plm,s.plm,s.boot,s.cpuctl,hi,ss0,ss1);
  UINT32 memory=GuRead32(0x1183a4);
#if !defined(GU_EXPERIMENTAL_3090)
  GuLog("DIAGNOSTIC PREFLIGHT ONLY: chip baseline=UNKNOWN SS0 baseline=UNKNOWN memory=%#x expected=%#x; cannot arm; no GPU writes/reset/DMA/driver release\n",memory,GU_DEVICE_MEMORY_SIZE);
  return Gu.fault?EFI_DEVICE_ERROR:EFI_NOT_READY;
#else
#ifdef GU_ENTRY_LATCHED
  UINT32 entry0=0,entry1=0;BOOLEAN latched=GuGetEntrySelectors(&entry0,&entry1);
  GuLog("cold expected: profile=%s chip=%#x memory=%#x entry_latched=%u SS0=%#x SS1=%#x SS0target=0x88888888 SS1policy=%s entry=%#x target=0x8\n",
    GU_STOCK_PROFILE_NAME,GU_CHIP_ID,GU_DEVICE_MEMORY_SIZE,latched,entry0,entry1,
#ifdef GU_ENTRY_LATCHED_SS1_PRESERVE
    "preserve",
#else
    "target",
#endif
    entry1);
#else
  GuLog("cold expected: profile=%s chip=%#x memory=%#x SS0=%#x SS1=%#x\n",
    GU_STOCK_PROFILE_NAME,GU_CHIP_ID,GU_DEVICE_MEMORY_SIZE,GU_STOCK_PROFILE_SS0,6U);
#endif
  GuLog("cold observed: profile=%s chip=%#x memory=%#x SS0=%#x SS1=%#x\n",
    GU_STOCK_PROFILE_NAME,chip,memory,ss0,ss1);
  if(Gu.fault)return EFI_DEVICE_ERROR;
  if(chip!=GU_CHIP_ID || memory!=GU_DEVICE_MEMORY_SIZE ||
     gpu_unlock_gfw_assess(&s) || s.plm!=0x8b8f || s.boot!=0x3ff || s.cpuctl!=0x10 ||
     hi!=0 || plm!=0xffffff8f || !Gu.prom_verified)
    return EFI_NOT_READY;
#ifdef GU_ENTRY_LATCHED
  if(!GuLatchEntrySelectors(ss0,ss1))return EFI_NOT_READY;
  GuLog("entry selectors: latched_once=1 SS0=%#x SS1=%#x SS0target=0x88888888 SS1policy=%s SS1target=0x8\n",ss0,ss1,
#ifdef GU_ENTRY_LATCHED_SS1_PRESERVE
    "preserve",
#else
    "full",
#endif
    ss1);
  /* Logging is external I/O: a failed log never authorizes this profile. */
  if(!GuLogReady() || Gu.fault)return EFI_DEVICE_ERROR;
#else
  if(ss0!=GU_STOCK_PROFILE_SS0 || ss1!=6)return EFI_NOT_READY;
#endif
  Gu.profile_verified=TRUE;
#ifdef GU_ENTRY_LATCHED
  GuLog("EXPERIMENTAL exact-card profile %s verified; SS0target=0x88888888 SS1policy=%s entry=%#x target=0x8; hardware untested.\n",GU_STOCK_PROFILE_NAME,
#ifdef GU_ENTRY_LATCHED_SS1_PRESERVE
    "preserve",
#else
    "full",
#endif
    ss1);
#else
  GuLog("EXPERIMENTAL exact-card profile %s verified; SS1 target 8 effect remains hardware untested.\n",GU_STOCK_PROFILE_NAME);
#endif
  return EFI_SUCCESS;
#endif
}
static EFI_STATUS PromCheck(VOID) {
  Gu.prom_verified=FALSE;
  Gu.profile_verified=FALSE;
  VOID *prom=GuAlloc(1048576);if(!prom)return EFI_OUT_OF_RESOURCES;
  GuCopyFromIo(prom,(VOID *)(UINTN)(Gu.bar_base+0x300000),1048576);
  BOOLEAN good=!Gu.fault && GuHashMatches(prom,1048576,GU_PROM_SHA256);
  GuFree(prom);Gu.prom_verified=good;GuLog("live PROM hash matched=%u\n",(unsigned)good);return good?EFI_SUCCESS:EFI_CRC_ERROR;
}
static EFI_STATUS Arm(VOID) {
#if !defined(GU_EXPERIMENTAL_3090)
  return EFI_UNSUPPORTED;
#else
  if(!Gu.profile_verified || Gu.fault || !Gu.display_ok)return EFI_ACCESS_DENIED;
#ifdef GU_AUTOMATIC_BOOT
#if !defined(GU_INTERNAL_BOOT) || !defined(GU_ENTRY_LATCHED_SS1_FULL)
#error Automatic boot requires the exact internal full profile
#endif
  if(!Gu.owned || !Gu.prom_verified || PendingOwner.agent)return EFI_ACCESS_DENIED;
  if(!Table->ConIn || !Table->ConIn->Reset || !Table->ConIn->ReadKeyStroke || !Gu.bs->Stall)return EFI_UNSUPPORTED;
  EFI_STATUS status=Table->ConIn->Reset(Table->ConIn,FALSE);if(status!=EFI_SUCCESS)return status;
  GuLogText("AUTOMATIC INTERNAL UNLOCK: Esc within 3 seconds cancels. No U required. One guarded attempt only.\n");
  if(!GuLogReady())return EFI_DEVICE_ERROR;
  for(UINTN i=0;i<30;i++) {
#ifdef GU_HEADLESS_RESEARCH
    if(!GuHeadlessCheck())return EFI_NOT_READY;
#endif
    EFI_INPUT_KEY key;status=Table->ConIn->ReadKeyStroke(Table->ConIn,&key);
    if(status==EFI_SUCCESS) {
      if(key.ScanCode==0x17 || key.UnicodeChar==27)return EFI_ABORTED;
    }else if(status!=EFI_NOT_READY)return status;
    status=Gu.bs->Stall(100000);if(status!=EFI_SUCCESS)return status;
    Gu.ticks+=100000;
  }
  return GuLogReady() && Gu.profile_verified && Gu.owned && Gu.prom_verified && Gu.display_ok && !Gu.fault && !PendingOwner.agent?EFI_SUCCESS:EFI_ACCESS_DENIED;
#else
  if(!Table->ConIn || !Table->ConIn->Reset || !Table->ConIn->ReadKeyStroke)return EFI_UNSUPPORTED;
  EFI_STATUS status=Table->ConIn->Reset(Table->ConIn,FALSE);if(EFI_ERROR(status))return status;
  GuLog("EXPERIMENTAL MANUAL UNLOCK: profile=%s exact RTX3090 24GB / AMD display / verified PROM and assets.\n",GU_STOCK_PROFILE_NAME);
  if(PendingOwner.agent)GuLogText("U will release the RTX firmware driver, restore verified access settings if needed, then recheck and attempt once.\n");
  GuLogText("Press U to attempt once; Esc or 30-second timeout cancels.\n");
  if(!GuLogReady())return EFI_DEVICE_ERROR;
  for(UINTN i=0;i<300;i++) {
    EFI_INPUT_KEY key;status=Table->ConIn->ReadKeyStroke(Table->ConIn,&key);
    if(!EFI_ERROR(status)) {
      if(key.ScanCode==0x17 || key.UnicodeChar==27)return EFI_ABORTED;
      if(key.UnicodeChar=='u' || key.UnicodeChar=='U')return Gu.profile_verified && !Gu.fault?EFI_SUCCESS:EFI_ACCESS_DENIED;
    } else if(status!=EFI_NOT_READY)return status;
    GuStall(100000);
  }
  return EFI_TIMEOUT;
#endif
#endif
}
static VOID Halt(VOID) {
  Gu.retained=TRUE;
  GuLogText("STOP: cleanup cannot be proven. Resources remain reserved.\nPOWER OFF using the physical power button; do not continue to Windows.\n");
  /* No ResetSystem, ExitBootServices, retry, protocol close or memory release. */
  for(;;)Gu.bs->Stall(1000000);
}
#ifdef GU_HEADLESS_RESEARCH
/* Last file log can execute firmware callbacks. Revalidate without logging. */
static BOOLEAN HeadlessFinalHardware(VOID) {
  UINT32 cfg[64],ss0=0,ss1=0;
  if(ReadConfig(cfg)!=EFI_SUCCESS || memcmp(cfg,InitialConfig,sizeof(cfg)) ||
     !Gu.profile_verified || !Gu.prom_verified || !Gu.owned || !GuGetEntrySelectors(&ss0,&ss1))return FALSE;
  struct gpu_unlock_gfw_sample sample={.config_ok=true,.plm=GuRead32(GU_GFW_PLM_OFFSET),
    .boot=GuRead32(GU_GFW_BOOT_OFFSET),.cpuctl=GuRead32(GU_GFW_CPUCTL_OFFSET)};
  return !gpu_unlock_gfw_assess(&sample) && sample.plm==0x8b8f && sample.boot==0x3ff && sample.cpuctl==0x10 &&
    GuRead32(0)==GU_CHIP_ID && GuRead32(0x1fa828)==0 && GuRead32(GU_FEAT_PLM_OFFSET)==0xffffff8f &&
    GuRead32(0x1183a4)==GU_DEVICE_MEMORY_SIZE && GuRead32(GU_FEAT_SS0_OFFSET)==ss0 && GuRead32(GU_FEAT_SS1_OFFSET)==ss1 && !Gu.fault;
}
#endif
static VOID LogContainment(VOID) {
  /* Called only after any TPL_NOTIFY containment section has ended. */
  GuLog("BME containment: attribute_status=%#llx hardware_status=%#llx command=%#x cached=%#llx pending=%u\n",
    (unsigned long long)Gu.master_attribute_status,(unsigned long long)Gu.master_hardware_status,
    Gu.master_last_command,(unsigned long long)Gu.master_last_attributes,Gu.master_attributes_pending);
}
static VOID ReleaseHalt(EFI_STATUS Status,BOOLEAN Attempted) {
  /* A failed claim may follow interface removal/replacement during logging.
     Resolve, validate and clear BME without a callback window or stale access. */
  EFI_TPL old=Gu.bs->RaiseTPL(TPL_NOTIFY);
  EFI_STATUS refreshed=GuRefreshTargetPci();
  EFI_STATUS contained=EFI_ERROR(refreshed)?refreshed:GuClearMaster();
  Gu.bs->RestoreTPL(old);
  LogContainment();
  GuLog("FINAL status=%#llx stage=%s attempted=%u driver_release=1 clean=0 containment=%#llx pci_available=%u\n",
    (unsigned long long)Status,Stage,Attempted,(unsigned long long)contained,(unsigned)(Gu.pci!=NULL));
  Halt();
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE Image,EFI_SYSTEM_TABLE *St) {
  if(!St || !St->BootServices || !St->RuntimeServices)return EFI_INVALID_PARAMETER;
  Table=St;GuInit(St->BootServices,NULL);Gu.image=Image;Gu.display_check=DisplayCheck;
#ifdef GU_HEADLESS_RESEARCH
  GuHeadlessReset(St);
#endif
  memset(&PendingOwner,0,sizeof(PendingOwner));
  Stage="usb-log";EFI_STATUS status=GuIoOpen(Image,St);BOOLEAN attempted=FALSE,release_attempted=FALSE;
  if(EFI_ERROR(status))goto finish;
#if defined(GU_AUTOMATIC_BOOT)
  GuLog("BUILD MODE: AUTOMATIC INTERNAL UNLOCK; profile=%s; Esc cancellation; all hardware gates retained.\n",GU_STOCK_PROFILE_NAME);
#elif defined(GU_EXPERIMENTAL_3090)
  GuLog("BUILD MODE: EXPERIMENTAL MANUAL UNLOCK; compile-time profile=%s. The shared I/O preflight banner above is superseded: exact-card checks and manual U can authorize one stock transaction.\n",GU_STOCK_PROFILE_NAME);
#endif
  UINT8 secure=0xff;UINTN bytes=1;
  Stage="secure-boot";
  status=St->RuntimeServices->GetVariable(L"SecureBoot",&GlobalGuid,NULL,&bytes,&secure);
  if(EFI_ERROR(status) || bytes!=1 || secure){status=EFI_SECURITY_VIOLATION;goto finish;}
  status=Step(status);if(EFI_ERROR(status))goto finish;
  Stage="watchdog";status=Step(Gu.bs->SetWatchdogTimer(0,0,0,NULL));if(EFI_ERROR(status))goto finish;
  Stage="target";status=Step(FindTarget());if(EFI_ERROR(status))goto finish;
  Stage="display";Gu.display_ok=DisplayCheck();status=Step(Gu.display_ok?EFI_SUCCESS:EFI_UNSUPPORTED);if(EFI_ERROR(status))goto finish;
  /* Read-only validation precedes ownership, asset loading and manual input. */
  Stage="bar0";status=Step(BarCheck());if(EFI_ERROR(status))goto finish;
  Stage="prom";status=Step(PromCheck());if(EFI_ERROR(status))goto finish;
  Stage="diagnostic-cold-state";status=Step(ColdCheck());
#if !defined(GU_EXPERIMENTAL_3090)
  goto finish;
#else
  if(EFI_ERROR(status))goto finish;
#endif
  Stage="ownership";status=GuClaimController();
#ifndef GU_AUTOMATIC_BOOT
  if(status==EFI_ACCESS_DENIED) {
    status=GuFindReleaseOwner(St,&PendingOwner,TRUE);
    if(!EFI_ERROR(status))GuLogText("Exclusive ownership deferred until manual U and explicit driver release.\n");
  }
#endif
  status=Step(status);if(EFI_ERROR(status))goto finish;
  Stage="config-recheck";status=Step(ConfigCheck(FALSE));if(EFI_ERROR(status))goto finish;
  Stage="bar0";status=Step(BarCheck());if(EFI_ERROR(status))goto finish;
  Stage="prom";status=Step(PromCheck());if(EFI_ERROR(status))goto finish;
  Stage="cold-state";status=Step(ColdCheck());if(EFI_ERROR(status))goto finish;
  Stage="assets";status=Step(GuLoadAssets());if(EFI_ERROR(status))goto finish;
  Stage="arming";status=Step(Arm());if(EFI_ERROR(status))goto finish;
  Stage="armed-config";status=Step(ConfigCheck(FALSE));if(EFI_ERROR(status))goto finish;
  Stage="armed-display";Gu.display_ok=DisplayCheck();status=Step(Gu.display_ok?EFI_SUCCESS:EFI_UNSUPPORTED);if(EFI_ERROR(status))goto finish;
  Stage="armed-cold-state";status=Step(ColdCheck());if(EFI_ERROR(status))goto finish;
  if(PendingOwner.agent) {
    GU_RELEASE_OWNER current;
    Stage="owner-recheck";
    GuLogText("Manual U confirmed: release RTX firmware driver once, then revalidate.\n");
    if(!GuLogReady()){status=EFI_DEVICE_ERROR;goto finish;}
    /* Durable intent precedes the last owner snapshot. No file IO between a
       successful final comparison and DisconnectController. */
    status=GuFindReleaseOwner(St,&current,FALSE);
    if(!EFI_ERROR(status) && !GuSameReleaseOwner(&PendingOwner,&current))status=EFI_ABORTED;
    if(EFI_ERROR(status)){status=Step(status);goto finish;}
    EFI_PCI_IO_PROTOCOL *prior=Gu.pci;
    Stage="driver-release";release_attempted=TRUE;Gu.armed=TRUE;Gu.pci=NULL;
    EFI_STATUS released=Gu.bs->DisconnectController(Gu.handle,current.agent,NULL);
    EFI_TPL refresh_tpl=Gu.bs->RaiseTPL(TPL_NOTIFY);
    EFI_STATUS refreshed=GuRefreshTargetPci();
    Gu.bs->RestoreTPL(refresh_tpl);
    GuLog("driver release: status=%#llx refresh=%#llx same_pci=%u\n",
      (unsigned long long)released,(unsigned long long)refreshed,(unsigned)(Gu.pci==prior));
    status=Step(EFI_ERROR(released)?released:EFI_ERROR(refreshed)?refreshed:Gu.pci!=prior?EFI_COMPROMISED_DATA:EFI_SUCCESS);
    if(EFI_ERROR(status))goto finish;
    Stage="ownership-after-release";status=Step(GuClaimController());if(EFI_ERROR(status))goto finish;
    Stage="display-after-release";Gu.display_ok=DisplayCheck();status=Step(Gu.display_ok?EFI_SUCCESS:EFI_UNSUPPORTED);if(EFI_ERROR(status))goto finish;
    Stage="decode-after-release";status=Step(RestoreDecode());if(EFI_ERROR(status))goto finish;
    Stage="config-after-release";status=Step(ConfigCheck(FALSE));if(EFI_ERROR(status))goto finish;
    Stage="bar0-after-release";status=Step(BarCheck());if(EFI_ERROR(status))goto finish;
    Stage="prom-after-release";status=Step(PromCheck());if(EFI_ERROR(status))goto finish;
    Stage="cold-after-release";status=Step(ColdCheck());if(EFI_ERROR(status))goto finish;
  }
  Stage="final-prom";status=Step(PromCheck());if(EFI_ERROR(status))goto finish;
  Stage="final-cold";status=Step(ColdCheck());if(EFI_ERROR(status))goto finish;
  if(!Gu.profile_verified || Gu.fault){status=EFI_ACCESS_DENIED;goto finish;}
  GuLogText("ARMED: one transaction is starting. Do not reset or remove the USB.\n");
  if(!GuLogReady()){status=EFI_DEVICE_ERROR;goto finish;}
#ifdef GU_HEADLESS_RESEARCH
  /* File callbacks may change devices; no log/output between this gate and core. */
  if(!HeadlessFinalHardware() || !GuHeadlessCheck()){status=EFI_NOT_READY;goto finish;}
#endif
  Stage="transaction";Gu.armed=TRUE;attempted=TRUE;
  UINT64 mask=0xffffffffULL;
  struct pci_dev device={.dev={.dma_mask=&mask,.coherent_dma_mask=0xffffffffULL},.pm_cap=Gu.pm_cap,.refs=1};
  struct gpu_unlock_610_shared shared={0};
  int result=gpu_unlock_610_load_shared(&device,&shared);
  if(!result)result=gpu_unlock_610_unlock(&device,&shared);
  /* Also covers failures before GuStarted, including BME-enable readback. */
  EFI_STATUS contained=GuClearMaster();
  LogContainment();
  GuLog("transaction result=%d containment=%#llx started=%u quiesced=%u fault=%u retained=%u maps=%u FLR=%u\n",
    result,(unsigned long long)contained,Gu.started,Gu.quiesced,Gu.fault,Gu.retained,Gu.dma_count,Gu.flrs);
  if(EFI_ERROR(contained) || !GuSafeToReturn() || Gu.allocations || (Gu.started && result))Halt();
  gpu_unlock_610_release_shared(&shared);Gu.armed=FALSE;
  status=result?EFI_DEVICE_ERROR:EFI_SUCCESS;
 finish:
  if(release_attempted && (EFI_ERROR(status) || Gu.fault))ReleaseHalt(status,attempted);
  if(attempted && !GuSafeToReturn())Halt();
  Gu.armed=FALSE;
  if(GuLogReady())GuLog("FINAL status=%#llx stage=%s attempted=%u driver_release=%u clean=%u\n",(unsigned long long)status,Stage,attempted,release_attempted,GuSafeToReturn());
  else GuConsole("Preflight or logging failed; no new attempt will start.\n");
  if(!GuLogReady()){status=EFI_DEVICE_ERROR;if(release_attempted)ReleaseHalt(status,attempted);if(Gu.started)Halt();}
  if(attempted && !GuSafeToReturn())Halt();
  EFI_STATUS closed=GuIoClose();
  if(EFI_ERROR(closed)){if(release_attempted)ReleaseHalt(closed,attempted);if(Gu.started)Halt();status=closed;}
  if(Gu.owned) {
    closed=Gu.bs->CloseProtocol(Gu.handle,&PciGuid,Image,NULL);
    if(EFI_ERROR(closed)){if(release_attempted)ReleaseHalt(closed,attempted);if(Gu.started)Halt();status=closed;}else Gu.owned=FALSE;
  }
  GuReleaseAssets();
  if(!EFI_ERROR(status))GuConsole("UEFI transaction completed and cleaned up. Returning to firmware; Windows validation is pending.\n");
  else GuConsole("Attempt cancelled or rejected. Returning without another attempt.\n");
  return status;
}
