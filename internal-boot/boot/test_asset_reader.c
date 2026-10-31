/* Known SHA256 vector through real reader/hash; EFI file/pool callbacks mocked. */
#include "../unlock/asset-reader.h"
static UINT8 Data[3];static EFI_FILE_PROTOCOL File,Root;static EFI_BOOT_SERVICES Bs;
static UINTN Freed,Closed;static unsigned Mode;
static EFI_STATUS EFIAPI Open(EFI_FILE_PROTOCOL *f,EFI_FILE_PROTOCOL **p,CHAR16 *name,UINT64 mode,UINT64 attrs){(void)f;(void)name;(void)mode;(void)attrs;*p=&File;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Info(EFI_FILE_PROTOCOL *f,EFI_GUID *g,UINTN *n,VOID *data){(void)f;(void)g;EFI_FILE_INFO *i=data;i->Size=SIZE_OF_EFI_FILE_INFO;i->FileSize=3;i->Attribute=Mode==4?EFI_FILE_DIRECTORY:0;*n=SIZE_OF_EFI_FILE_INFO;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Read(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *data){(void)f;UINT8 *d=data;d[0]=Mode==2?'z':'a';d[1]='b';d[2]='c';if(Mode==3)*n=2;return Mode==5?EFI_WARN_UNKNOWN_GLYPH:EFI_SUCCESS;}
static EFI_STATUS EFIAPI Close(EFI_FILE_PROTOCOL *f){(void)f;Closed++;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Allocate(EFI_MEMORY_TYPE t,UINTN n,VOID **p){(void)t;if(n!=3)return EFI_OUT_OF_RESOURCES;*p=Data;return Mode==6?EFI_WARN_UNKNOWN_GLYPH:EFI_SUCCESS;}
static EFI_STATUS EFIAPI Free(VOID *p){if(p!=Data)return EFI_INVALID_PARAMETER;Freed++;return EFI_SUCCESS;}
int main(VOID){
 Bs=(EFI_BOOT_SERVICES){0};Bs.AllocatePool=Allocate;Bs.FreePool=Free;Root=(EFI_FILE_PROTOCOL){0};Root.Open=Open;
 File=(EFI_FILE_PROTOCOL){0};File.GetInfo=Info;File.Read=Read;File.Close=Close;
 for(Mode=1;Mode<=6;Mode++) {
  Freed=Closed=0;VOID *out=NULL;
  EFI_STATUS s=GuReadVerifiedAsset(&Bs,&Root,L"abc",3,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",&out);
  if(Closed!=1)return 10+Mode;
  if(Mode==1){if(s!=EFI_SUCCESS || out!=Data || Freed)return 1;Free(out);}
  else {if(s==EFI_SUCCESS || out || (Mode!=4 && Freed!=1) || (Mode==4 && Freed))return 20+Mode;}
 }
 return 0;
}
