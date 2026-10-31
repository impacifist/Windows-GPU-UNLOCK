/* Synthetic EFI callbacks only; no GPU/file/device access. */
#include "evga-resources.c"
GU_PLATFORM Gu;GU_ASSET GuAssets[5];
static EFI_BOOT_SERVICES Bs;static EFI_SYSTEM_TABLE Table;static EFI_FILE_PROTOCOL Root,File;
static EFI_PCI_IO_PROTOCOL Pci;static UINT8 Buffer[84427316];
static VOID *Allocations[5];static BOOLEAN FreedSlot[5];static UINTN NextOffset;
static UINTN AssetIndex,ReadOffset,Allocs,Frees,Closes,Hashes,Claims,Releases,Records,FailRecordAt;
static BOOLEAN BadSize,ShortRead,BadHash,FailAlloc,FailClose,FailFree,ActiveOwner,FailOpen,WrongInterface,FailRelease,LogFail;
static EFI_GUID PG=EFI_PCI_IO_PROTOCOL_GUID;
static EFI_STATUS EFIAPI Allocate(EFI_MEMORY_TYPE type,UINTN n,VOID **out){
 (void)type;Allocs++;if(FailAlloc)return EFI_OUT_OF_RESOURCES;
 if(Allocs>5 || n>sizeof(Buffer)-NextOffset)return EFI_OUT_OF_RESOURCES;
 *out=Buffer+NextOffset;Allocations[Allocs-1]=*out;NextOffset+=n;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Free(VOID *p){
 for(UINTN i=0;i<5;i++)if(Allocations[i]==p && p){
  if(FreedSlot[i])return EFI_INVALID_PARAMETER;Frees++;if(FailFree)return EFI_DEVICE_ERROR;
  FreedSlot[i]=TRUE;return EFI_SUCCESS;
 }
 return EFI_INVALID_PARAMETER;
}
static EFI_STATUS EFIAPI Close(EFI_FILE_PROTOCOL *f){(void)f;Closes++;return FailClose?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI Read(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *v){(void)f;(void)v;if(ShortRead && *n)(*n)--;ReadOffset+=*n;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Info(EFI_FILE_PROTOCOL *f,EFI_GUID *g,UINTN *n,VOID *v){(void)f;(void)g;EFI_FILE_INFO *i=v;static const UINTN sizes[]={61256,64000,84277400,24576,84};i->Size=SIZE_OF_EFI_FILE_INFO;i->Attribute=0;i->FileSize=BadSize?1:sizes[AssetIndex];*n=SIZE_OF_EFI_FILE_INFO;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Open(EFI_FILE_PROTOCOL *f,EFI_FILE_PROTOCOL **out,CHAR16 *path,UINT64 mode,UINT64 attrs){(void)f;(void)path;(void)mode;(void)attrs;*out=&File;AssetIndex=Hashes;ReadOffset=0;return EFI_SUCCESS;}
BOOLEAN GuHashMatches(const VOID *data,UINTN n,const char *hash){(void)data;(void)hash;Hashes++;return !BadHash && ReadOffset==n;}
static EFI_TPL EFIAPI Raise(EFI_TPL tpl){(void)tpl;return TPL_APPLICATION;}
static VOID EFIAPI Restore(EFI_TPL tpl){(void)tpl;}
static EFI_STATUS EFIAPI Protocols(EFI_HANDLE h,EFI_GUID ***p,UINTN *n){(void)h;static EFI_GUID *gs[1];gs[0]=&PG;*p=gs;*n=1;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Owners(EFI_HANDLE h,EFI_GUID *g,EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **p,UINTN *n){(void)h;(void)g;static EFI_OPEN_PROTOCOL_INFORMATION_ENTRY e;e=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){0};e.Attributes=ActiveOwner?EFI_OPEN_PROTOCOL_BY_DRIVER:EFI_OPEN_PROTOCOL_GET_PROTOCOL;e.OpenCount=1;*p=&e;*n=1;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Claim(EFI_HANDLE h,EFI_GUID *g,VOID **p,EFI_HANDLE image,EFI_HANDLE c,UINT32 attrs){(void)h;(void)g;(void)image;(void)c;if(attrs!=EFI_OPEN_PROTOCOL_EXCLUSIVE)return EFI_INVALID_PARAMETER;Claims++;if(FailOpen)return EFI_ACCESS_DENIED;*p=WrongInterface?NULL:&Pci;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Release(EFI_HANDLE h,EFI_GUID *g,EFI_HANDLE image,EFI_HANDLE c){(void)h;(void)g;(void)image;(void)c;Releases++;return FailRelease?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS Record(VOID *p,const char *name,EFI_STATUS s){(void)p;(void)s;Records++;if(FailRecordAt && Records==FailRecordAt)return EFI_DEVICE_ERROR;if(LogFail && !strcmp(name,"evga-shared-claim-return"))return EFI_DEVICE_ERROR;return EFI_SUCCESS;}
static VOID Init(VOID){
 BadSize=ShortRead=BadHash=FailAlloc=FailClose=FailFree=ActiveOwner=FailOpen=WrongInterface=FailRelease=LogFail=FALSE;
 Gu=(GU_PLATFORM){0};NextOffset=0;for(UINTN i=0;i<5;i++){GuAssets[i]=(GU_ASSET){0};Allocations[i]=NULL;FreedSlot[i]=FALSE;}
 AssetIndex=ReadOffset=Allocs=Frees=Closes=Hashes=Claims=Releases=Records=FailRecordAt=0;
 Bs=(EFI_BOOT_SERVICES){0};Bs.AllocatePool=Allocate;Bs.FreePool=Free;Bs.RaiseTPL=Raise;Bs.RestoreTPL=Restore;Bs.ProtocolsPerHandle=Protocols;Bs.OpenProtocolInformation=Owners;Bs.OpenProtocol=Claim;Bs.CloseProtocol=Release;
 Table=(EFI_SYSTEM_TABLE){0};Table.BootServices=&Bs;
 Root=(EFI_FILE_PROTOCOL){0};Root.Open=Open;File=(EFI_FILE_PROTOCOL){0};File.Read=Read;File.GetInfo=Info;File.Close=Close;
 OwnImage=(EFI_HANDLE)1;
}
int main(VOID){EFI_STATUS s;
 Init();s=GuEvgaResourcesPrepare(&Table,&Root,(EFI_HANDLE)1,Record);if(s!=EFI_SUCCESS || Allocs!=5 || Frees || Closes!=5 || Hashes!=5)return 1;
 for(UINTN i=0;i<5;i++){if(!GuAssets[i].firmware.data)return 2;for(UINTN j=0;j<i;j++)if(GuAssets[i].firmware.data==GuAssets[j].firmware.data)return 2;}
 s=GuEvgaResourcesRelease(&Table);if(s!=EFI_SUCCESS || Frees!=5)return 3;
 for(UINTN i=0;i<5;i++)if(GuAssets[i].firmware.data)return 4;
 Init();BadHash=TRUE;s=GuEvgaResourcesPrepare(&Table,&Root,(EFI_HANDLE)1,Record);if(s!=EFI_CRC_ERROR || Frees!=1 || GuAssets[0].firmware.data)return 5;
 Init();FailRecordAt=2;s=GuEvgaResourcesPrepare(&Table,&Root,(EFI_HANDLE)1,Record);if(s==EFI_SUCCESS || Frees!=2 || GuAssets[0].firmware.data || GuAssets[1].firmware.data)return 6;
 Init();s=GuEvgaResourcesPrepare(&Table,&Root,(EFI_HANDLE)1,Record);if(s!=EFI_SUCCESS)return 7;Gu.retained=TRUE;s=GuEvgaResourcesRelease(&Table);if(s==EFI_SUCCESS || Frees)return 8;
 Init();s=GuEvgaResourcesPrepare(&Table,&Root,(EFI_HANDLE)1,Record);if(s!=EFI_SUCCESS)return 9;FailFree=TRUE;s=GuEvgaResourcesRelease(&Table);if(s==EFI_SUCCESS || !Gu.retained || !GuAssets[0].firmware.data)return 10;
 return 0;
}
