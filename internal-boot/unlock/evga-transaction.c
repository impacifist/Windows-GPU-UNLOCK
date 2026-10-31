/* GPL-2.0-only. EVGA transaction adapter, development only; not a boot image.
 * The wrapper must supply a graph verifier, fresh one-shot authorization, file
 * logging, asset setup and a permanent hold on any non-success return. */
#include "evga-transaction.h"
#include "evga-admission.h"
#include "ownership.h"
#include "compat/compat.h"
#include "vendor/unlock_610.h"
#include "sha256.h"
#include <IndustryStandard/Acpi.h>
#if !defined(GU_PROFILE_EVGA_Z790_3090) || !defined(GU_EVGA_TRANSACTION_DEVELOPMENT) || !defined(GU_EXPERIMENTAL_3090) || !defined(GU_ENTRY_LATCHED_SS1_FULL)
#error EVGA transaction adapter requires its development-only full profile
#endif
static BOOLEAN AttemptSpent;
static GU_EVGA_GRAPH_CHECK CurrentGraph;
#if defined(GU_EVGA_TEST_ONLY) && (__STDC_HOSTED__ != 1 || defined(GU_UEFI_PORT))
#error GU_EVGA_TEST_ONLY is prohibited in firmware builds
#endif
#if defined(GU_EVGA_TEST_ONLY)
VOID GuEvgaTestReset(VOID){AttemptSpent=FALSE;CurrentGraph=NULL;}
#endif
static BOOLEAN VerifyGraph(VOID) {
 EFI_GUID pciGuid=EFI_PCI_IO_PROTOCOL_GUID;VOID *current=NULL;
 if(!Gu.bs || !Gu.bs->HandleProtocol || !CurrentGraph ||
    Gu.bs->HandleProtocol(Gu.handle,&pciGuid,&current)!=EFI_SUCCESS || current!=Gu.pci)return FALSE;
 if(!CurrentGraph(Gu.handle,Gu.image,Gu.owned))return FALSE;
 current=NULL;
 return Gu.bs->HandleProtocol(Gu.handle,&pciGuid,&current)==EFI_SUCCESS && current==Gu.pci;
}
static const struct {const char *name;UINTN bytes;const char *hash;} Specs[]={
 {"gpu-unlock/610.43.03/booter_load_ga102.bin",61256,"056cbc064a888f80717726c54d7f64fcd68044d123870c95a74f37f4975dcaa4"},
 {"gpu-unlock/610.43.03/v67-ga102-610.43.03.bin",64000,"2a0aa1f86d0b6afff5214d641d87d46dc98ef06ed908ff05d0ddbad5ffb7e197"},
 {"gpu-unlock/610.43.03/gsp_ga10x.bin",84277400,"572373620a37418f24dc16b5031c39338778c3257e48e8408de9a57291b24f3a"},
 {"gpu-unlock/610.43.03/gsp_boot_ga102.bin",24576,"3df651acb174fd5f5561a1996965b192f50c5d763168446ed0d4ead19d895768"},
 {"gpu-unlock/610.43.03/gsp_boot_desc_ga102.bin",84,"cc5a3fc1f4c47f2fe107b1b68f0176d8a448e6dec84b575e5560ccef5262bf15"}};
static BOOLEAN AssetsMatch(VOID) {
 for(UINTN i=0;i<5;i++)if(!GuAssets[i].name || strcmp(GuAssets[i].name,Specs[i].name) ||
  !GuAssets[i].firmware.data || GuAssets[i].firmware.size!=Specs[i].bytes ||
  !GuHashMatches(GuAssets[i].firmware.data,Specs[i].bytes,Specs[i].hash))return FALSE;
 return TRUE;
}
static EFI_STATUS ReadConfig(UINT32 config[64],UINTN *seg,UINTN *bus,UINTN *dev,UINTN *fn) {
 EFI_STATUS s=Gu.pci->GetLocation(Gu.pci,seg,bus,dev,fn);
 return s==EFI_SUCCESS?Gu.pci->Pci.Read(Gu.pci,EfiPciIoWidthUint32,0,64,config):s;
}
static EFI_STATUS CheckBar(VOID) {
 EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *r=NULL;
 EFI_STATUS s=Gu.pci->GetBarAttributes(Gu.pci,0,NULL,(VOID **)&r);
 if(s!=EFI_SUCCESS || !r){if(r)Gu.bs->FreePool(r);return s==EFI_SUCCESS?EFI_UNSUPPORTED:s;}
 BOOLEAN valid=r->Desc==ACPI_ADDRESS_SPACE_DESCRIPTOR && r->Len==sizeof(*r)-3 && r->ResType==0 &&
  r->AddrRangeMin==GU_EVGA_BAR_BASE && r->AddrLen==GU_EVGA_BAR_BYTES && !r->AddrTranslationOffset &&
  (r->AddrRangeMax==0x84ffffffULL || r->AddrRangeMax==0xffffffULL) &&
  (r->AddrSpaceGranularity==32 || r->AddrSpaceGranularity==64);
 EFI_STATUS released=Gu.bs->FreePool(r);if(released!=EFI_SUCCESS)return released;
 return valid?EFI_SUCCESS:EFI_UNSUPPORTED;
}
static GU_EVGA_COLD ReadCold(VOID) {
 GU_EVGA_COLD c={0};c.chip=GuRead32(0);c.gfw_plm=GuRead32(0x118128);c.gfw_boot=GuRead32(0x118234);
 c.gfw_cpuctl=GuRead32(0x110100);c.wpr_hi=GuRead32(0x1fa828);c.feature_plm=GuRead32(0x823804);
 c.ss0=GuRead32(0x82381c);c.ss1=GuRead32(0x823820);c.memory=GuRead32(0x1183a4);return c;
}
EFI_STATUS GuEvgaTransaction(EFI_SYSTEM_TABLE *t,EFI_HANDLE image,EFI_HANDLE parent,
 EFI_PCI_IO_PROTOCOL *pci,GU_EVGA_GRAPH_CHECK graph,GU_EVGA_LOG_READY logReady) {
 if(!t || !t->BootServices || !image || !parent || !pci || !graph || !logReady || !pci->Pci.Read ||
  !pci->Pci.Write || !pci->GetLocation || !pci->GetBarAttributes || !pci->Attributes ||
  !pci->Mem.Read || !pci->Mem.Write || !pci->AllocateBuffer || !pci->FreeBuffer || !pci->Map || !pci->Unmap)
  return EFI_INVALID_PARAMETER;
 if(!t->BootServices->RaiseTPL || !t->BootServices->RestoreTPL || !t->BootServices->AllocatePool ||
    !t->BootServices->FreePool || !t->BootServices->Stall || !t->BootServices->HandleProtocol ||
    !t->BootServices->ProtocolsPerHandle || !t->BootServices->OpenProtocolInformation || !t->BootServices->OpenProtocol)
  return EFI_UNSUPPORTED;
 /* Never clear retained resources or re-arm after a failed/partial attempt. */
 if(AttemptSpent)return EFI_ALREADY_STARTED;AttemptSpent=TRUE;
 GuInit(t->BootServices,pci);Gu.image=image;Gu.handle=parent;Gu.bar_base=GU_EVGA_BAR_BASE;Gu.bar_size=GU_EVGA_BAR_BYTES;
 CurrentGraph=graph;Gu.display_check=VerifyGraph;
 if(!logReady() || !AssetsMatch() || !VerifyGraph())return EFI_ACCESS_DENIED;
 EFI_STATUS s=GuClaimController();if(s!=EFI_SUCCESS || !Gu.owned)return s==EFI_SUCCESS?EFI_ACCESS_DENIED:s;
 if(!VerifyGraph())return EFI_ACCESS_DENIED;
 UINT32 cfg[64],again[64];UINTN seg,bus,dev,fn;GU_CAPS caps;
 s=ReadConfig(cfg,&seg,&bus,&dev,&fn);if(s!=EFI_SUCCESS)return s;
 UINT64 attrs=0;s=pci->Attributes(pci,EfiPciIoAttributeOperationGet,0,&attrs);
 if(s!=EFI_SUCCESS || (attrs&(EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY|EFI_PCI_IO_ATTRIBUTE_BUS_MASTER|EFI_PCI_IO_ATTRIBUTE_VGA_MEMORY|EFI_PCI_IO_ATTRIBUTE_VGA_IO))!=EFI_PCI_IO_ATTRIBUTE_MEMORY)return EFI_ACCESS_DENIED;
 s=CheckBar();if(s!=EFI_SUCCESS)return s;
 VOID *prom=GuAlloc(GU_EVGA_PROM_BYTES);if(!prom)return EFI_OUT_OF_RESOURCES;
 GU_EVGA_COLD cold=ReadCold();
 GuLog("EVGA admission cold: chip=%#x gfw=%#x/%#x/%#x wpr=%#x plm=%#x ss0=%#x ss1=%#x memory=%#x fault=%u\n",
  cold.chip,cold.gfw_plm,cold.gfw_boot,cold.gfw_cpuctl,cold.wpr_hi,cold.feature_plm,cold.ss0,cold.ss1,cold.memory,Gu.fault);
 if(!logReady() || Gu.fault){GuFree(prom);return EFI_ACCESS_DENIED;}
 GuCopyFromIo(prom,(VOID *)(UINTN)(GU_EVGA_BAR_BASE+0x300000),GU_EVGA_PROM_BYTES);
 BOOLEAN admitted=!Gu.fault && GuEvgaAdmit(cfg,seg,bus,dev,fn,&cold,prom,GU_EVGA_PROM_BYTES,&caps);
 GuFree(prom);GuLog("EVGA admission result: admitted=%u fault=%u\n",admitted,Gu.fault);
 if(!logReady() || !admitted || Gu.fault)return EFI_ACCESS_DENIED;
 Gu.pm_cap=caps.pm;Gu.pcie_cap=caps.pcie;Gu.pcie_version=caps.pcie_version;Gu.msi_cap=caps.msi;Gu.msix_cap=caps.msix;
 Gu.prom_verified=TRUE;if(!GuLatchEntrySelectors(cold.ss0,cold.ss1))return EFI_ACCESS_DENIED;
 Gu.profile_verified=TRUE;Gu.display_ok=TRUE;
 GuLog("EVGA transaction: exact profile admitted, ownership held, one attempt follows.\n");
 if(Gu.fault || !logReady() || !VerifyGraph())return EFI_ACCESS_DENIED;
 s=ReadConfig(again,&seg,&bus,&dev,&fn);if(s!=EFI_SUCCESS || seg || bus!=1 || dev || fn || memcmp(cfg,again,sizeof(cfg)))return EFI_ACCESS_DENIED;
 GU_EVGA_COLD final=ReadCold();if(Gu.fault || memcmp(&cold,&final,sizeof(cold)))return EFI_ACCESS_DENIED;
 Gu.armed=TRUE;UINT64 mask=0xffffffffULL;
 struct pci_dev device={.dev={.dma_mask=&mask,.coherent_dma_mask=0xffffffffULL},.pm_cap=Gu.pm_cap,.refs=1};
 struct gpu_unlock_610_shared shared={0};
 int result=gpu_unlock_610_load_shared(&device,&shared);
 if(!result)result=gpu_unlock_610_unlock(&device,&shared);
 EFI_STATUS contained=GuClearMaster();
 GuLog("EVGA transaction: result=%d contained=%#llx started=%u quiesced=%u fault=%u retained=%u maps=%u FLR=%u\n",
  result,(unsigned long long)contained,Gu.started,Gu.quiesced,Gu.fault,Gu.retained,Gu.dma_count,Gu.flrs);
 if(contained!=EFI_SUCCESS || result || !logReady() || !GuSafeToReturn() || Gu.allocations || Gu.dma_count || Gu.fault) {
  Gu.retained=TRUE;return EFI_DEVICE_ERROR;
 }
 gpu_unlock_610_release_shared(&shared);Gu.armed=FALSE;
 /* Success retains ownership and assets; the wrapper must close/release them
    only after independent clean-state and logging checks. */
 return EFI_SUCCESS;
}
