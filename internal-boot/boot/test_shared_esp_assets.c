/* Host mocks for read-only file handling; no firmware or GPU access. */
#include "shared-esp-assets.c"
static EFI_FILE_PROTOCOL file;
static UINT8 buffer[3];static UINTN reads,closes,frees;static int mode;
static EFI_STATUS EFIAPI Open(EFI_FILE_PROTOCOL *r,EFI_FILE_PROTOCOL **f,CHAR16 *p,UINT64 m,UINT64 a){
 (void)r;(void)p;if(m!=EFI_FILE_MODE_READ || a)return EFI_ACCESS_DENIED;
 if(mode==1)return EFI_NOT_FOUND;*f=&file;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Read(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *b){
 (void)f;reads++;if(mode==2)return EFI_DEVICE_ERROR;
 if(reads==1){if(mode==3){*n=2;return EFI_SUCCESS;}((UINT8 *)b)[0]='a';((UINT8 *)b)[1]='b';((UINT8 *)b)[2]=mode==4?'d':'c';}
 else *n=mode==5?1:0;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Close(EFI_FILE_PROTOCOL *f){(void)f;closes++;return mode==6?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI Allocate(EFI_MEMORY_TYPE t,UINTN n,VOID **p){(void)t;if(n!=3)return EFI_OUT_OF_RESOURCES;if(mode==7)return EFI_OUT_OF_RESOURCES;*p=buffer;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Free(VOID *p){(void)p;frees++;return EFI_SUCCESS;}
int main(void){
 EFI_BOOT_SERVICES bs={0};EFI_FILE_PROTOCOL root={0};bs.AllocatePool=Allocate;bs.FreePool=Free;root.Open=Open;file.Read=Read;file.Close=Close;
 GU_ASSET_SPEC spec={L"test",3,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"};
 const EFI_STATUS expected[]={EFI_SUCCESS,EFI_NOT_FOUND,EFI_DEVICE_ERROR,EFI_BAD_BUFFER_SIZE,EFI_COMPROMISED_DATA,EFI_BAD_BUFFER_SIZE,EFI_DEVICE_ERROR,EFI_OUT_OF_RESOURCES};
 for(mode=0;mode<8;mode++){
  reads=closes=frees=0;
  if(CheckOneAsset(&bs,&root,&spec)!=expected[mode])return 10+mode;
  if(closes!=(mode==1?0U:1U) || frees!=((mode==1 || mode==7)?0U:1U))return 20+mode;
 }
 return 0;
}
