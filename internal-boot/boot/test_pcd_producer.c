/* GPL-2.0-only. Captures a synthetic PE from RAM, never executes its bytes. */
#include "diagnostic.h"
static BOOLEAN AllowNew;
static BOOLEAN NewDump(CAPTURE *c,EFI_FILE_PROTOCOL *r,CHAR16 *n){(void)c;(void)r;(void)n;return AllowNew;}
#define GU_AD104_PCD_CAPTURE
#define GU_AD104_TEXT_TEST
#define GU_AD104_GOP_TEST
#define main OutputMain
#include "test_output_observation.c"
#undef main
static EFI_LOADED_IMAGE_PROTOCOL Producer;
static UINT8 ProducerBytes[PCD_PRODUCER_BYTES],ProducerPath[24];
static EFI_FILE_PROTOCOL ProducerRoot,ProducerFile;
static int ProducerFault,ProducerWrites,ProducerFlushes,ProducerCloses;
static void Put16(UINT8 *p,UINT16 v){memcpy(p,&v,2);}
static void Put32(UINT8 *p,UINT32 v){memcpy(p,&v,4);}
static EFI_STATUS EFIAPI ProducerHandle(EFI_HANDLE h,EFI_GUID *g,VOID **p) {
  if(h==(VOID *)50 && PfEqual(g,&ImgGuidL,sizeof(*g))){*p=&Producer;return EFI_SUCCESS;}
  return OutputHandle(h,g,p);
}
static EFI_STATUS EFIAPI ProducerLocate(EFI_GUID *g,VOID *registration,VOID **p) {
  (void)registration;if(!PfEqual(g,&OutPcdGuid,sizeof(*g)))return EFI_NOT_FOUND;
  *p=&Pcd;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI ProducerAlloc(EFI_MEMORY_TYPE type,UINTN bytes,VOID **p) {
  (void)type;*p=ProducerFault==12?NULL:malloc(bytes);return *p?EFI_SUCCESS:EFI_OUT_OF_RESOURCES;
}
static EFI_STATUS EFIAPI ProducerFree(VOID *p){free(p);return ProducerFault==13?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI ProducerOpen(EFI_FILE_PROTOCOL *r,EFI_FILE_PROTOCOL **out,CHAR16 *name,UINT64 mode,UINT64 attr) {
  (void)r;(void)attr;const CHAR16 expected[]=L"\\RTX3090-UNLOCK\\PCDPROD.BIN";
  Check(!memcmp(name,expected,sizeof(expected)) && mode==(EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE),"exact separate private output file");
  *out=&ProducerFile;if(ProducerFault==6)return EFI_DEVICE_ERROR;
  if(ProducerFault==7)*out=NULL;
  if(ProducerFault==16)*out=&ProducerRoot;
  if(ProducerFault==10)ProducerBytes[0x800]^=1;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI ProducerWrite(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *data) {
  (void)f;ProducerWrites++;Check(*n==PCD_PRODUCER_BYTES && !memcmp(data,ProducerBytes,*n),"exact admitted RAM copied");
  if(ProducerFault==8)(*n)--;
  if(ProducerFault==11)ProducerBytes[0x800]^=1;
  return ProducerFault==14?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI ProducerFlush(EFI_FILE_PROTOCOL *f){(void)f;ProducerFlushes++;return ProducerFault==9?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI ProducerClose(EFI_FILE_PROTOCOL *f){(void)f;ProducerCloses++;return ProducerFault==15?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static OUT_STATE ProducerSetup(int fault,CAPTURE *c) {
  OutputSetup(0);ProducerFault=fault;ProducerWrites=ProducerFlushes=ProducerCloses=0;AllowNew=TRUE;
  memset(ProducerBytes,0,sizeof(ProducerBytes));ProducerBytes[0]='M';ProducerBytes[1]='Z';Put32(ProducerBytes+60,0x40);
  Put32(ProducerBytes+0x40,0x4550);Put16(ProducerBytes+0x44,0x8664);Put16(ProducerBytes+0x46,1);Put16(ProducerBytes+0x54,0xf0);
  Put16(ProducerBytes+0x58,0x20b);Put32(ProducerBytes+0x90,PCD_PRODUCER_BYTES);
  UINT8 *section=ProducerBytes+0x148;Put32(section+8,0x1000);Put32(section+12,0x2a0);Put32(section+36,0x20000000);
  UINT8 path[]={4,6,20,0,0x57,0x72,0xcf,0x80,0xab,0x87,0xf9,0x47,0xa3,0xfe,0xd5,0x0b,0x76,0xd8,0x95,0x41,0x7f,0xff,4,0};
  memcpy(ProducerPath,path,sizeof(path));memset(&Producer,0,sizeof(Producer));Producer.ImageBase=ProducerBytes;
  Producer.ImageSize=sizeof(ProducerBytes);Producer.ImageCodeType=EfiBootServicesCode;Producer.FilePath=(VOID *)ProducerPath;
  Pcd.GetBool=(PCD_PROTOCOL_GET_BOOLEAN)(ProducerBytes+0x4f4);Pcd.GetNextToken=(PCD_PROTOCOL_GET_NEXT_TOKEN)(ProducerBytes+0x600);Pcd.GetSize=(PCD_PROTOCOL_GET_SIZE)(ProducerBytes+0x650);
  OUT_STATE o={0};o.Pcd=&Pcd;o.GetBool=Pcd.GetBool;o.Next=Pcd.GetNextToken;o.Size=Pcd.GetSize;
  Bs.HandleProtocol=ProducerHandle;Bs.LocateProtocol=ProducerLocate;Bs.AllocatePool=ProducerAlloc;Bs.FreePool=ProducerFree;
  memset(&ProducerRoot,0,sizeof(ProducerRoot));ProducerRoot.Open=ProducerOpen;memset(&ProducerFile,0,sizeof(ProducerFile));
  ProducerFile.Write=ProducerWrite;ProducerFile.Flush=ProducerFlush;ProducerFile.Close=ProducerClose;
  memset(c,0,sizeof(*c));c->Table=&T;c->DumpRoot=&ProducerRoot;c->Io=EFI_SUCCESS;
  if(fault==1)Producer.ImageSize--;
  if(fault==2)ProducerPath[4]^=1;
  if(fault==3)Producer.ImageCodeType=EfiBootServicesData;
  if(fault==4)ProducerBytes[0]='X';
  if(fault==5)Put32(section+36,0);
  return o;
}
int main(void) {
  CAPTURE c;OUT_STATE o=ProducerSetup(0,&c);EFI_HANDLE image=(VOID *)50;
  Check(CapturePcdProducer(&c,&o,&image,1)==EFI_SUCCESS && PcdProducerCaptured,"one exact synthetic producer captured");
  Check(ProducerWrites==1 && ProducerFlushes==1 && ProducerCloses==1,"complete file lifecycle");
  Check(PcdVerifyCaptured(&T,&o),"same-run fingerprint retained");ProducerBytes[0x800]^=1;
  Check(!PcdKnownText(ProducerBytes,sizeof(ProducerBytes)),"unknown synthetic producer rejected by reviewed text hash");
  Check(!PcdVerifyCaptured(&T,&o),"changed producer cannot authorize later stage");
  for(int f=1;f<=16;f++) {
    o=ProducerSetup(f,&c);Check(CapturePcdProducer(&c,&o,&image,1)!=EFI_SUCCESS && !PcdProducerCaptured,"identity/PE/stability/file failures block capture gate");
    if(f<=7 || f==10 || f==12)Check(!ProducerWrites,"no write before admission");
    if(f==16)Check(!ProducerWrites && !ProducerCloses,"root alias never written or closed");
  }
  o=ProducerSetup(0,&c);AllowNew=FALSE;Check(CapturePcdProducer(&c,&o,&image,1)!=EFI_SUCCESS && !ProducerWrites,"existing capture never overwritten");
  o=ProducerSetup(0,&c);EFI_HANDLE two[]={image,image};Check(CapturePcdProducer(&c,&o,two,2)!=EFI_SUCCESS,"duplicate owner refused");
  o=ProducerSetup(0,&c);o.GetBool=(PCD_PROTOCOL_GET_BOOLEAN)(ProducerBytes+0x4f5);Check(!PcdMatches(&Producer,&o),"wrong getter RVA refused");
  o=ProducerSetup(0,&c);o.Next=(PCD_PROTOCOL_GET_NEXT_TOKEN)(ProducerBytes+sizeof(ProducerBytes));Check(!PcdMatches(&Producer,&o),"method outside producer refused");
  printf("PASS PCD producer capture: %d checks, RAM-only mocks\n",Checks);return 0;
}
