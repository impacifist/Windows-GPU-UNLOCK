/* Freestanding host simulation: no GPU/firmware/hardware access. */
#include "evga-transaction.h"
#include "evga-admission.h"
#include "compat/compat.h"
#include "vendor/unlock_610.h"
#include <IndustryStandard/Acpi.h>
GU_PLATFORM Gu;GU_ASSET GuAssets[5];
static EFI_SYSTEM_TABLE Table;static EFI_BOOT_SERVICES Bs;static EFI_PCI_IO_PROTOCOL Pci;
static UINT32 Config[64]={0x220410deU,0x00100002U,0x030000a1U,0x00800010U,0x84000000U,0x0000000cU,0x00000040U,0x0000000cU,0x00000048U,0x00005001U,0x00000000U,0x33873842U,0x85000000U,0x00000060U,0x00000000U,0x000001ffU,0x33873842U,0x00000000U,0x00000000U,0x00000000U,0x00000000U,0x00000001U,0x0023d6ceU,0x00000000U,0x48036801U,0x00000008U,0x00807805U,0x00000000U,0x00000000U,0x00000000U,0x0012b410U,0x112c8de1U,0x00092930U,0x00454d04U,0x11040140U,0x00000000U,0x00000000U,0x00000000U,0x00000000U,0x00070813U,0x00000400U,0x0180001eU,0x001e0004U,0x00000000U,0x00000000U,0x01140009U,0x00000000U,0x00000000U,0x00000000U,0x00000000U,0x00050011U,0x00b90000U,0x00ba0000U,0x00000000U,0x00000000U,0x00000000U,0x00000000U,0x00000000U,0x00000000U,0x00000000U,0x00000000U,0x00000000U,0x00000000U,0x00000000U};
static UINT8 Prom[GU_EVGA_PROM_BYTES],Asset;
static unsigned Claims,Graphs,ConfigReads,Unlocks,Clears,Releases,HashCalls,LogChecks,Handles;static unsigned FailLogAt,FailGraphAt,FailHandleAt;
static BOOLEAN BadAsset,BadGraph,BadClaim,BadBar,BadConfig,BadCold,BadLatch,FailLoad,FailUnlock,FailClear,FailSafe;static UINT32 TestSs0;
static EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR Bar;static UINT8 *Unused;
static struct {EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR desc;UINT8 end[2];} Descriptor;
void GuInit(EFI_BOOT_SERVICES *b,EFI_PCI_IO_PROTOCOL *p){memset(&Gu,0,sizeof(Gu));Gu.bs=b;Gu.pci=p;}
void GuLog(const char *fmt,...){(void)fmt;}
BOOLEAN GuHashMatches(const VOID *p,UINTN n,const char *h){(void)h;HashCalls++;return p && n && !BadAsset;}
VOID *GuAlloc(size_t n){return n==sizeof(Prom)?Prom:NULL;}
VOID GuFree(VOID *p){(void)p;}
VOID GuCopyFromIo(VOID *d,const VOID *s,size_t n){(void)d;(void)s;(void)n;}
UINT32 GuRead32(UINT32 o){switch(o){case 0:return 0xb72000a1;case 0x118128:return 0x8b8f;case 0x118234:return 0x3ff;case 0x110100:return 0x10;case 0x1fa828:return 0;case 0x823804:return 0xffffff8f;case 0x82381c:return BadCold?0xffffffffU:TestSs0;case 0x823820:return 7;case 0x1183a4:return 0x6000;default:Gu.fault=TRUE;return 0;}}
BOOLEAN GuLatchEntrySelectors(UINT32 a,UINT32 b){return !BadLatch && Gu.prom_verified && a==TestSs0 && b==7;}
EFI_STATUS GuClaimController(VOID){Claims++;if(BadClaim)return EFI_ACCESS_DENIED;Gu.owned=TRUE;return EFI_SUCCESS;}
EFI_STATUS GuClearMaster(VOID){Clears++;return FailClear?EFI_DEVICE_ERROR:EFI_SUCCESS;}
BOOLEAN GuSafeToReturn(VOID){return !FailSafe;}
int gpu_unlock_610_load_shared(struct pci_dev *p,struct gpu_unlock_610_shared *s){(void)p;(void)s;return FailLoad?-1:0;}
int gpu_unlock_610_unlock(struct pci_dev *p,const struct gpu_unlock_610_shared *s){(void)p;(void)s;Unlocks++;return FailUnlock?-1:0;}
void gpu_unlock_610_release_shared(struct gpu_unlock_610_shared *s){(void)s;Releases++;}
static EFI_STATUS EFIAPI Location(EFI_PCI_IO_PROTOCOL *p,UINTN *s,UINTN *b,UINTN *d,UINTN *f){(void)p;*s=*d=*f=0;*b=1;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Read(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 off,UINTN n,VOID *out){(void)p;(void)w;if(off || n!=64)return EFI_INVALID_PARAMETER;ConfigReads++;memcpy(out,Config,sizeof(Config));if(BadConfig && ConfigReads>1)((UINT32 *)out)[4]^=1;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI BarQuery(EFI_PCI_IO_PROTOCOL *p,UINT8 i,UINT64 *a,VOID **out){(void)p;(void)i;(void)a;Descriptor.desc=Bar;Descriptor.desc.AddrRangeMin=BadBar?0:GU_EVGA_BAR_BASE;Descriptor.end[0]=ACPI_END_TAG_DESCRIPTOR;Descriptor.end[1]=0;*out=&Descriptor;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Attr(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION op,UINT64 v,UINT64 *out){(void)p;(void)op;(void)v;*out=EFI_PCI_IO_ATTRIBUTE_MEMORY;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Free(VOID *p){(void)p;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Handle(EFI_HANDLE h,EFI_GUID *g,VOID **out){(void)h;(void)g;Handles++;*out=Handles==FailHandleAt?NULL:&Pci;return EFI_SUCCESS;}
static BOOLEAN Graph(EFI_HANDLE p,EFI_HANDLE i,BOOLEAN owned){(void)p;(void)i;(void)owned;Graphs++;return !BadGraph && (!FailGraphAt || Graphs!=FailGraphAt);}
static VOID Init(VOID){
 GuEvgaTestReset();Claims=Graphs=ConfigReads=Unlocks=Clears=Releases=HashCalls=LogChecks=Handles=FailLogAt=FailGraphAt=FailHandleAt=0;
 BadAsset=BadGraph=BadClaim=BadBar=BadConfig=BadCold=BadLatch=FailLoad=FailUnlock=FailClear=FailSafe=FALSE;
 TestSs0=0x30053014U;
 Pci=(EFI_PCI_IO_PROTOCOL){0};Pci.GetLocation=Location;Pci.GetBarAttributes=BarQuery;Pci.Attributes=Attr;Pci.Pci.Read=Read;
 /* Non-null unused methods pass adapter availability gates; no mock calls them. */
 Pci.Pci.Write=(EFI_PCI_IO_PROTOCOL_CONFIG)1;Pci.Mem.Read=(EFI_PCI_IO_PROTOCOL_IO_MEM)1;Pci.Mem.Write=(EFI_PCI_IO_PROTOCOL_IO_MEM)1;
 Pci.AllocateBuffer=(EFI_PCI_IO_PROTOCOL_ALLOCATE_BUFFER)1;Pci.FreeBuffer=(EFI_PCI_IO_PROTOCOL_FREE_BUFFER)1;Pci.Map=(EFI_PCI_IO_PROTOCOL_MAP)1;Pci.Unmap=(EFI_PCI_IO_PROTOCOL_UNMAP)1;
 Bs=(EFI_BOOT_SERVICES){0};Bs.FreePool=Free;Bs.HandleProtocol=Handle;
 Bs.RaiseTPL=(EFI_RAISE_TPL)1;Bs.RestoreTPL=(EFI_RESTORE_TPL)1;Bs.AllocatePool=(EFI_ALLOCATE_POOL)1;Bs.Stall=(EFI_STALL)1;
 Bs.ProtocolsPerHandle=(EFI_PROTOCOLS_PER_HANDLE)1;Bs.OpenProtocolInformation=(EFI_OPEN_PROTOCOL_INFORMATION)1;Bs.OpenProtocol=(EFI_OPEN_PROTOCOL)1;Table=(EFI_SYSTEM_TABLE){0};Table.BootServices=&Bs;
 Bar=(EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR){0};Bar.Desc=ACPI_ADDRESS_SPACE_DESCRIPTOR;Bar.Len=sizeof(Bar)-3;Bar.AddrSpaceGranularity=32;Bar.AddrRangeMax=0x84ffffff;Bar.AddrLen=GU_EVGA_BAR_BYTES;
 static const char *names[]={"gpu-unlock/610.43.03/booter_load_ga102.bin","gpu-unlock/610.43.03/v67-ga102-610.43.03.bin","gpu-unlock/610.43.03/gsp_ga10x.bin","gpu-unlock/610.43.03/gsp_boot_ga102.bin","gpu-unlock/610.43.03/gsp_boot_desc_ga102.bin"};
 static const UINTN sizes[]={61256,64000,84277400,24576,84};
 for(UINTN i=0;i<5;i++){GuAssets[i].name=names[i];GuAssets[i].firmware.data=&Asset;GuAssets[i].firmware.size=sizes[i];}
 (void)Unused;
}
static BOOLEAN LogReady(VOID){LogChecks++;return !FailLogAt || LogChecks!=FailLogAt;}
static EFI_STATUS Run(VOID){return GuEvgaTransaction(&Table,(EFI_HANDLE)1,(EFI_HANDLE)2,&Pci,Graph,LogReady);}
int main(VOID){EFI_STATUS s;
 Init();s=Run();if(s!=EFI_SUCCESS || Claims!=1 || Unlocks!=1 || Clears!=1 || Releases!=1 || !Gu.owned || Gu.armed || HashCalls!=6)return 1;
 Init();BadAsset=TRUE;s=Run();if(s==EFI_SUCCESS || Claims || Unlocks)return 2;
 Init();BadGraph=TRUE;s=Run();if(s==EFI_SUCCESS || Claims || Unlocks)return 3;
 Init();BadClaim=TRUE;s=Run();if(s==EFI_SUCCESS || Claims!=1 || Unlocks)return 4;
 Init();BadBar=TRUE;s=Run();if(s==EFI_SUCCESS || Unlocks || !Gu.owned)return 5;
 Init();BadCold=TRUE;s=Run();if(s==EFI_SUCCESS || Unlocks)return 6;
 Init();BadLatch=TRUE;s=Run();if(s==EFI_SUCCESS || Unlocks)return 7;
 Init();BadConfig=TRUE;s=Run();if(s==EFI_SUCCESS || Unlocks)return 8;
 Init();FailLoad=TRUE;s=Run();if(s==EFI_SUCCESS || Unlocks || !Gu.retained || Releases)return 9;
 Init();FailUnlock=TRUE;s=Run();if(s==EFI_SUCCESS || Unlocks!=1 || !Gu.retained || Releases)return 10;
 Init();FailClear=TRUE;s=Run();if(s==EFI_SUCCESS || !Gu.retained || Releases)return 11;
 Init();FailSafe=TRUE;s=Run();if(s==EFI_SUCCESS || !Gu.retained || Releases)return 12;
 Init();FailLogAt=1;s=Run();if(s==EFI_SUCCESS || Claims || Unlocks)return 13;
 Init();FailLogAt=2;s=Run();if(s==EFI_SUCCESS || Unlocks)return 14;
 Init();FailLogAt=5;s=Run();if(s==EFI_SUCCESS || Unlocks!=1 || !Gu.retained)return 15;
 Init();s=Run();if(s!=EFI_SUCCESS || Run()!=EFI_ALREADY_STARTED || Unlocks!=1)return 16;
 Init();FailHandleAt=3;s=Run();if(s==EFI_SUCCESS || Unlocks)return 17;
 Init();FailGraphAt=3;s=Run();if(s==EFI_SUCCESS || Unlocks)return 18;
 Init();FailHandleAt=6;s=Run();if(s==EFI_SUCCESS || Unlocks)return 19;
 Init();FailLogAt=2;s=Run();if(s==EFI_SUCCESS || Unlocks)return 20;
 Init();FailLogAt=3;s=Run();if(s==EFI_SUCCESS || Unlocks)return 21;
 Init();TestSs0=0x12053015U;s=Run();if(s!=EFI_SUCCESS || Unlocks!=1)return 22;
 return 0;
}
