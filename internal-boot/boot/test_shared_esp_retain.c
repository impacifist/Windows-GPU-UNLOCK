/* Host-only lifetime and receipt mocks; no hardware access. */
#include "shared-esp-retain.c"
static EFI_FILE_PROTOCOL asset,receipt,root;static UINT8 pools[5][3],record[512];
static UINTN allocations,frees,closes,position,assetReads,flushes;static int mode;
static EFI_STATUS EFIAPI Open(EFI_FILE_PROTOCOL *r,EFI_FILE_PROTOCOL **f,CHAR16 *p,UINT64 m,UINT64 a){
 (void)r;if(a)return EFI_ACCESS_DENIED;
 if(p[0]=='t'){if(m!=EFI_FILE_MODE_READ)return EFI_ACCESS_DENIED;assetReads=0;*f=&asset;}
 else {if(m!=(EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE))return EFI_ACCESS_DENIED;position=0;*f=&receipt;}
 return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Read(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *b){
 if(f==&asset){if(assetReads++){*n=0;return EFI_SUCCESS;}if(*n!=3)return EFI_BAD_BUFFER_SIZE;
 ((UINT8 *)b)[0]='a';((UINT8 *)b)[1]='b';((UINT8 *)b)[2]='c';return EFI_SUCCESS;}
 UINTN left=sizeof(record)-position;if(*n>left)*n=left;
 for(UINTN i=0;i<*n;i++)((UINT8 *)b)[i]=record[position+i];position+=*n;
 if(mode==5 && flushes && *n)((UINT8 *)b)[0]^=1;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Write(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *b){
 (void)f;if(mode==4){*n=2;return EFI_SUCCESS;}if(*n!=512 || position)return EFI_BAD_BUFFER_SIZE;
 for(UINTN i=0;i<*n;i++)record[i]=((UINT8 *)b)[i];position+=*n;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Position(EFI_FILE_PROTOCOL *f,UINT64 p){(void)f;if(p>512)return EFI_INVALID_PARAMETER;position=(UINTN)p;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Flush(EFI_FILE_PROTOCOL *f){(void)f;flushes++;return mode==6?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI Close(EFI_FILE_PROTOCOL *f){(void)f;closes++;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Allocate(EFI_MEMORY_TYPE t,UINTN n,VOID **p){
 (void)t;if(n!=3 || allocations==5)return EFI_OUT_OF_RESOURCES;
 if(mode==1 && allocations==2)return EFI_OUT_OF_RESOURCES;
 if(mode==2 && allocations==4)pools[0][0]='z';
 *p=pools[allocations++];return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Free(VOID *p){(void)p;frees++;return EFI_SUCCESS;}
static void Reset(void){allocations=frees=closes=position=assetReads=flushes=0;for(UINTN i=0;i<512;i++)record[i]=0;const char pending[]="PENDING RETAIN01\r\n";for(UINTN i=0;i<sizeof(pending)-1;i++)record[i]=pending[i];}
int main(void){
 EFI_BOOT_SERVICES bs={0};bs.AllocatePool=Allocate;bs.FreePool=Free;root.Open=Open;asset.Read=Read;asset.Close=Close;
 receipt.Read=Read;receipt.Write=Write;receipt.SetPosition=Position;receipt.Flush=Flush;receipt.Close=Close;
 GU_ASSET_SPEC specs[5];for(UINTN i=0;i<5;i++){specs[i].path=L"test";specs[i].bytes=3;specs[i].hash="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";}
 Reset();mode=0;if(RetainAll(&bs,&root,specs,5)!=EFI_SUCCESS || allocations!=5 || frees!=5 || closes!=5)return 1;
 Reset();mode=1;if(RetainAll(&bs,&root,specs,5)!=EFI_OUT_OF_RESOURCES || frees!=2 || closes!=3)return 2;
 Reset();mode=2;if(RetainAll(&bs,&root,specs,5)!=EFI_COMPROMISED_DATA || frees!=5)return 3;
 Reset();mode=0;EFI_FILE_PROTOCOL *opened=NULL;if(OpenReceipt(&root,&opened)!=EFI_SUCCESS || opened!=&receipt)return 4;
 if(WriteReceipt(opened,"STARTED\r\n")!=EFI_SUCCESS || flushes!=1)return 5;
 opened=NULL;if(OpenReceipt(&root,&opened)!=EFI_ACCESS_DENIED || opened)return 6;
 Reset();mode=4;if(WriteReceipt(&receipt,"STARTED")!=EFI_BAD_BUFFER_SIZE || flushes)return 7;
 Reset();mode=5;if(WriteReceipt(&receipt,"STARTED")!=EFI_COMPROMISED_DATA)return 8;
 Reset();mode=6;if(WriteReceipt(&receipt,"STARTED")!=EFI_DEVICE_ERROR)return 9;
 return 0;
}
