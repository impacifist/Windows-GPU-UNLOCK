/* Synthetic EFI callbacks only; no GPU/file/device access. */
#include "evga-resources.c"
static EFI_BOOT_SERVICES Bs;static EFI_SYSTEM_TABLE Table;static EFI_FILE_PROTOCOL Root,File;
static EFI_PCI_IO_PROTOCOL Pci;static UINT8 Buffer[84277400];
static UINTN AssetIndex,ReadOffset,Allocs,Frees,Closes,Hashes,Claims,Releases,Records;
static BOOLEAN BadSize,ShortRead,BadHash,FailAlloc,FailClose,FailFree,ActiveOwner,FailOpen,WrongInterface,FailRelease,LogFail;
static EFI_GUID PG=EFI_PCI_IO_PROTOCOL_GUID;
static EFI_STATUS EFIAPI Allocate(EFI_MEMORY_TYPE type,UINTN n,VOID **out){(void)type;Allocs++;if(FailAlloc)return EFI_OUT_OF_RESOURCES;if(n>sizeof(Buffer))return EFI_OUT_OF_RESOURCES;*out=Buffer;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Free(VOID *p){if(p==Buffer){Frees++;if(FailFree)return EFI_DEVICE_ERROR;}return EFI_SUCCESS;}
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
static EFI_STATUS Record(VOID *p,const char *name,EFI_STATUS s){(void)p;(void)s;Records++;if(LogFail && !strcmp(name,"evga-shared-claim-return"))return EFI_DEVICE_ERROR;return EFI_SUCCESS;}
static VOID Init(VOID){
 BadSize=ShortRead=BadHash=FailAlloc=FailClose=FailFree=ActiveOwner=FailOpen=WrongInterface=FailRelease=LogFail=FALSE;
 AssetIndex=ReadOffset=Allocs=Frees=Closes=Hashes=Claims=Releases=Records=0;
 Bs=(EFI_BOOT_SERVICES){0};Bs.AllocatePool=Allocate;Bs.FreePool=Free;Bs.RaiseTPL=Raise;Bs.RestoreTPL=Restore;Bs.ProtocolsPerHandle=Protocols;Bs.OpenProtocolInformation=Owners;Bs.OpenProtocol=Claim;Bs.CloseProtocol=Release;
 Table=(EFI_SYSTEM_TABLE){0};Table.BootServices=&Bs;
 Root=(EFI_FILE_PROTOCOL){0};Root.Open=Open;File=(EFI_FILE_PROTOCOL){0};File.Read=Read;File.GetInfo=Info;File.Close=Close;
 OwnImage=(EFI_HANDLE)1;
}
int main(VOID){EFI_STATUS s;
 Init();s=GuEvgaResourcesPrepare(&Table,&Root,(EFI_HANDLE)1,Record);if(s!=EFI_SUCCESS||Allocs!=5||Frees!=5||Closes!=5||Hashes!=5)return 1;
 Init();BadSize=TRUE;s=GuEvgaResourcesPrepare(&Table,&Root,(EFI_HANDLE)1,Record);if(s==EFI_SUCCESS||Allocs||Closes!=1)return 2;
 Init();ShortRead=TRUE;s=GuEvgaResourcesPrepare(&Table,&Root,(EFI_HANDLE)1,Record);if(s!=EFI_END_OF_FILE||Frees!=1||Closes!=1||Hashes)return 3;
 Init();BadHash=TRUE;s=GuEvgaResourcesPrepare(&Table,&Root,(EFI_HANDLE)1,Record);if(s!=EFI_CRC_ERROR||Frees!=1)return 4;
 Init();FailAlloc=TRUE;s=GuEvgaResourcesPrepare(&Table,&Root,(EFI_HANDLE)1,Record);if(s!=EFI_OUT_OF_RESOURCES||Frees||Closes!=1)return 5;
 Init();FailClose=TRUE;s=GuEvgaResourcesPrepare(&Table,&Root,(EFI_HANDLE)1,Record);if(s==EFI_SUCCESS||Frees!=1||Hashes)return 6;
 Init();FailFree=TRUE;s=GuEvgaResourcesPrepare(&Table,&Root,(EFI_HANDLE)1,Record);if(s==EFI_SUCCESS||Hashes!=1)return 7;
 Init();s=GuEvgaResourcesClaim(&Table,(EFI_HANDLE)2,&Pci,Record);if(s!=EFI_SUCCESS||Claims!=1||Releases!=1||Gu.owned)return 8;
 Init();ActiveOwner=TRUE;s=GuEvgaResourcesClaim(&Table,(EFI_HANDLE)2,&Pci,Record);if(s!=EFI_ACCESS_DENIED||Claims||Releases)return 9;
 Init();FailOpen=TRUE;s=GuEvgaResourcesClaim(&Table,(EFI_HANDLE)2,&Pci,Record);if(s!=EFI_ACCESS_DENIED||Claims!=1||Releases)return 10;
 Init();WrongInterface=TRUE;s=GuEvgaResourcesClaim(&Table,(EFI_HANDLE)2,&Pci,Record);if(s==EFI_SUCCESS||Releases!=1||Gu.owned)return 11;
 Init();FailRelease=TRUE;s=GuEvgaResourcesClaim(&Table,(EFI_HANDLE)2,&Pci,Record);if(s==EFI_SUCCESS||Releases!=1||!Gu.owned)return 12;
 Init();LogFail=TRUE;s=GuEvgaResourcesClaim(&Table,(EFI_HANDLE)2,&Pci,Record);if(s==EFI_SUCCESS||Releases!=1||Gu.owned)return 13;
 return 0;
}
