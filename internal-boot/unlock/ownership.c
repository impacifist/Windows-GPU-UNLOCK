#include "ownership.h"
#include "runtime.h"
#include <Protocol/LoadedImage.h>
#ifdef GU_HEADLESS_RESEARCH
static BOOLEAN GuOwnerError(EFI_STATUS s){return s!=EFI_SUCCESS;}
#undef EFI_ERROR
#define EFI_ERROR(Status) GuOwnerError(Status)
#endif
static EFI_GUID PciGuid=EFI_PCI_IO_PROTOCOL_GUID;
#ifndef GU_OWNERSHIP_CLAIM_ONLY
static EFI_GUID BindingGuid=EFI_DRIVER_BINDING_PROTOCOL_GUID,LoadedGuid=EFI_LOADED_IMAGE_PROTOCOL_GUID;
#endif
typedef struct {EFI_GUID protocol;EFI_OPEN_PROTOCOL_INFORMATION_ENTRY entry;} OWNER_RECORD;
#define MAX_RECORDS 32
static BOOLEAN Passive(UINT32 A) {
  return A==EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL || A==EFI_OPEN_PROTOCOL_GET_PROTOCOL || A==EFI_OPEN_PROTOCOL_TEST_PROTOCOL;
}
/* Caller holds TPL_NOTIFY through the snapshot and, for Claim, acquisition. */
static EFI_STATUS Snapshot(OWNER_RECORD *Records,UINTN *Count) {
  EFI_GUID **protocols=NULL;UINTN protocol_count=0;BOOLEAN pci=FALSE;*Count=0;
  if(!Gu.bs->ProtocolsPerHandle)return EFI_UNSUPPORTED;
  EFI_STATUS status=Gu.bs->ProtocolsPerHandle(Gu.handle,&protocols,&protocol_count);
  if(!EFI_ERROR(status) && (!protocols || !protocol_count || protocol_count>32))status=EFI_COMPROMISED_DATA;
  for(UINTN i=0;!EFI_ERROR(status) && i<protocol_count;i++) {
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *entries=NULL;UINTN count=0;
    if(!protocols[i]){status=EFI_COMPROMISED_DATA;break;}
    if(!memcmp(protocols[i],&PciGuid,sizeof(PciGuid)))pci=TRUE;
    status=Gu.bs->OpenProtocolInformation(Gu.handle,protocols[i],&entries,&count);
    if(!EFI_ERROR(status) && (count>MAX_RECORDS-*Count || (count && !entries)))status=EFI_COMPROMISED_DATA;
    if(!EFI_ERROR(status))for(UINTN j=0;j<count;j++) {
      Records[*Count].protocol=*protocols[i];Records[*Count].entry=entries[j];(*Count)++;
    }
    if(entries && EFI_ERROR(Gu.bs->FreePool(entries)))status=EFI_DEVICE_ERROR;
  }
  if(protocols && EFI_ERROR(Gu.bs->FreePool(protocols)))status=EFI_DEVICE_ERROR;
  return !EFI_ERROR(status) && !pci?EFI_NOT_FOUND:status;
}
static VOID LogRecords(const OWNER_RECORD *Records,UINTN Count,EFI_STATUS Status) {
  GuLog("ownership: records=%zu exclusive=%u status=%#llx\n",(size_t)Count,Gu.owned,(unsigned long long)Status);
  for(UINTN i=0;i<Count;i++) {
    const EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *e=&Records[i].entry;
    GuLog("owner[%u] protocol=%08x attributes=%#x count=%u agent=%#llx controller=%#llx\n",
      (unsigned)i,Records[i].protocol.Data1,e->Attributes,e->OpenCount,
      (unsigned long long)(UINTN)e->AgentHandle,(unsigned long long)(UINTN)e->ControllerHandle);
  }
}
EFI_STATUS GuClaimController(VOID) {
  OWNER_RECORD records[MAX_RECORDS];UINTN count=0;
  EFI_TPL old=Gu.bs->RaiseTPL(TPL_NOTIFY);
  EFI_STATUS status=Snapshot(records,&count);
  if(!EFI_ERROR(status))for(UINTN i=0;i<count;i++)if(!Passive(records[i].entry.Attributes))status=EFI_ACCESS_DENIED;
  if(!EFI_ERROR(status)) {
    EFI_PCI_IO_PROTOCOL *pci=NULL;
    status=Gu.bs->OpenProtocol(Gu.handle,&PciGuid,(VOID **)&pci,Gu.image,NULL,EFI_OPEN_PROTOCOL_EXCLUSIVE);
    if(!EFI_ERROR(status)){Gu.owned=TRUE;if(pci!=Gu.pci)status=EFI_COMPROMISED_DATA;}
  }
  Gu.bs->RestoreTPL(old);LogRecords(records,count,status);return status;
}
#ifndef GU_OWNERSHIP_CLAIM_ONLY
static BOOLEAN ConsoleHandle(EFI_SYSTEM_TABLE *Table,EFI_HANDLE Handle) {
  return Handle && (Handle==Table->ConsoleOutHandle || Handle==Table->ConsoleInHandle || Handle==Table->StandardErrorHandle);
}
EFI_STATUS GuFindReleaseOwner(EFI_SYSTEM_TABLE *Table,GU_RELEASE_OWNER *Owner,BOOLEAN Log) {
  OWNER_RECORD records[MAX_RECORDS];UINTN count=0,drivers=0;memset(Owner,0,sizeof(*Owner));
  if(!Gu.bs->DisconnectController || Gu.owned)return EFI_UNSUPPORTED;
  EFI_TPL old=Gu.bs->RaiseTPL(TPL_NOTIFY);
  EFI_STATUS status=Snapshot(records,&count);
  Gu.bs->RestoreTPL(old);if(Log)LogRecords(records,count,status);
  if(EFI_ERROR(status))return status;
  for(UINTN i=0;i<count;i++) {
    const EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *e=&records[i].entry;
    if(!memcmp(&records[i].protocol,&PciGuid,sizeof(PciGuid)) && e->Attributes==EFI_OPEN_PROTOCOL_BY_DRIVER) {
      if(!e->AgentHandle || e->ControllerHandle!=Gu.handle || e->OpenCount!=1)return EFI_ACCESS_DENIED;
      Owner->agent=e->AgentHandle;drivers++;
    }
  }
  if(drivers!=1 || Owner->agent==Gu.image || Owner->agent==Gu.handle || ConsoleHandle(Table,Owner->agent))return EFI_ACCESS_DENIED;
  for(UINTN i=0;i<count;i++) {
    const EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *e=&records[i].entry;
    if(Passive(e->Attributes))continue;
    /* This release handles a single device driver only. DisconnectController
       gathers children across every protocol, and physical console children
       need not equal the System Table's virtual console handles. Refuse all
       child relationships instead of assuming a route alone proves isolation. */
    if(e->Attributes!=EFI_OPEN_PROTOCOL_BY_DRIVER || e->AgentHandle!=Owner->agent ||
       e->ControllerHandle!=Gu.handle || e->OpenCount!=1) {
      if(Log)GuLog("release refused: active child, different owner or unsupported open at record %u\n",(unsigned)i);
      return EFI_ACCESS_DENIED;
    }
  }
  EFI_DRIVER_BINDING_PROTOCOL *binding=NULL;EFI_LOADED_IMAGE_PROTOCOL *image=NULL;
  status=Gu.bs->HandleProtocol(Owner->agent,&BindingGuid,(VOID **)&binding);
  if(EFI_ERROR(status) || !binding || binding->DriverBindingHandle!=Owner->agent || !binding->Stop ||
    !binding->ImageHandle || binding->ImageHandle==Gu.image)return EFI_UNSUPPORTED;
  status=Gu.bs->HandleProtocol(binding->ImageHandle,&LoadedGuid,(VOID **)&image);
  if(EFI_ERROR(status) || !image)return EFI_UNSUPPORTED;
  if(Log)GuLog("release candidate: agent=%#llx image=%#llx source=%#llx base=%#llx bytes=%llu version=%#x code_type=%u children=0\n",
    (unsigned long long)(UINTN)Owner->agent,(unsigned long long)(UINTN)binding->ImageHandle,
    (unsigned long long)(UINTN)image->DeviceHandle,(unsigned long long)(UINTN)image->ImageBase,
    (unsigned long long)image->ImageSize,binding->Version,(unsigned)image->ImageCodeType);
  if(!image->ImageBase || !image->ImageSize || image->ImageSize>64U*1024U*1024U || image->ImageCodeType!=EfiBootServicesCode)return EFI_UNSUPPORTED;
  Owner->image=binding->ImageHandle;Owner->base=image->ImageBase;Owner->size=image->ImageSize;
  Owner->version=binding->Version;Owner->stop=binding->Stop;return EFI_SUCCESS;
}
BOOLEAN GuSameReleaseOwner(const GU_RELEASE_OWNER *A,const GU_RELEASE_OWNER *B) {
  if(A->agent!=B->agent || A->image!=B->image || A->base!=B->base || A->size!=B->size ||
    A->version!=B->version || A->stop!=B->stop)return FALSE;
  return TRUE;
}
static BOOLEAN RefreshBusy,RefreshReentered;
static EFI_STATUS RefreshError(EFI_STATUS s) {return RETURN_ERROR(s)?s:EFI_DEVICE_ERROR;}
static BOOLEAN RefreshStable(EFI_BOOT_SERVICES *bs,EFI_HANDLE handle,EFI_HANDLE_PROTOCOL resolve,
  EFI_PCI_IO_PROTOCOL *pci,EFI_PCI_IO_PROTOCOL_GET_LOCATION location,
  EFI_PCI_IO_PROTOCOL_CONFIG read,EFI_PCI_IO_PROTOCOL_CONFIG write) {
  return !RefreshReentered && !Gu.pci && Gu.bs==bs && Gu.handle==handle && bs->HandleProtocol==resolve &&
    pci->GetLocation==location && pci->Pci.Read==read && pci->Pci.Write==write;
}
EFI_STATUS GuRefreshTargetPci(VOID) {
  Gu.pci=NULL;
  if(RefreshBusy){RefreshReentered=TRUE;return EFI_NOT_READY;}
  RefreshBusy=TRUE;RefreshReentered=FALSE;
  EFI_STATUS status=EFI_NOT_FOUND;
  EFI_BOOT_SERVICES *bs=Gu.bs;EFI_HANDLE handle=Gu.handle;
  EFI_PCI_IO_PROTOCOL *fresh=NULL;
  if(!bs || !handle || !bs->HandleProtocol)goto done;
  EFI_HANDLE_PROTOCOL resolve=bs->HandleProtocol;
  status=resolve(handle,&PciGuid,(VOID **)&fresh);
  if(status!=EFI_SUCCESS){status=RefreshError(status);goto done;}
  if(!fresh || !fresh->GetLocation || !fresh->Pci.Read || !fresh->Pci.Write){status=EFI_NOT_FOUND;goto done;}
  EFI_PCI_IO_PROTOCOL_GET_LOCATION location=fresh->GetLocation;
  EFI_PCI_IO_PROTOCOL_CONFIG read=fresh->Pci.Read,write=fresh->Pci.Write;
  if(!RefreshStable(bs,handle,resolve,fresh,location,read,write)){status=EFI_NOT_READY;goto done;}
  UINTN segment=~(UINTN)0,bus=~(UINTN)0,device=~(UINTN)0,function=~(UINTN)0;
  status=location(fresh,&segment,&bus,&device,&function);
  if(status!=EFI_SUCCESS){status=RefreshError(status);goto done;}
  UINT32 config[64];for(UINTN i=0;i<64;i++)config[i]=0xffffffffU;
  const UINT32 offsets[]={0,8,12,44};
  for(UINTN i=0;i<4;i++) {
    if(!RefreshStable(bs,handle,resolve,fresh,location,read,write)){status=EFI_NOT_READY;goto done;}
    status=read(fresh,EfiPciIoWidthUint32,offsets[i],1,&config[offsets[i]/4]);
    if(status!=EFI_SUCCESS){status=RefreshError(status);goto done;}
  }
  if(!RefreshStable(bs,handle,resolve,fresh,location,read,write)){status=EFI_NOT_READY;goto done;}
  /* This is identity resolution for containment too: do not require MEM-on or
   * BME-off, and allow a newly resolved valid interface after firmware release. */
  if(!GuTargetConfig(config,segment,bus,device,function)){status=EFI_UNSUPPORTED;goto done;}
  UINTN s=~(UINTN)0,b=~(UINTN)0,d=~(UINTN)0,f=~(UINTN)0;
  status=location(fresh,&s,&b,&d,&f);
  if(status!=EFI_SUCCESS){status=RefreshError(status);goto done;}
  if(!RefreshStable(bs,handle,resolve,fresh,location,read,write) || s!=segment || b!=bus || d!=device || f!=function){status=EFI_NOT_READY;goto done;}
  Gu.pci=fresh;
done:
  if(status!=EFI_SUCCESS)Gu.pci=NULL;
  RefreshBusy=FALSE;return status;
}

#endif /* GU_OWNERSHIP_CLAIM_ONLY: share acquisition without release/reset helpers. */
