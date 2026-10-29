/* Local handoff regression tests. GPL-2.0-only. No GPU/firmware access. */
#include "handoff.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks,failed;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);failed++;}}while(0)
typedef struct {EFI_STATUS prepare,select,unlock,windows;int records,recordFail,unlocks,boots;BOOLEAN skip;} FLOW;
static EFI_STATUS prep(VOID *p){return ((FLOW *)p)->prepare;}
static EFI_STATUS select_flow(VOID *p,BOOLEAN *skip){*skip=((FLOW *)p)->skip;return ((FLOW *)p)->select;}
static EFI_STATUS record_flow(VOID *p,const char *e,EFI_STATUS s){(void)e;(void)s;FLOW*f=p;return ++f->records==f->recordFail?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS unlock_flow(VOID *p){FLOW*f=p;f->unlocks++;return f->unlock;}
static EFI_STATUS windows_flow(VOID *p){FLOW*f=p;f->boots++;return f->windows;}
static EFI_STATUS run(FLOW *f){GU_HANDOFF_OPS ops={f,prep,select_flow,record_flow,unlock_flow,windows_flow};return GuHandoffFlow(&ops);}
static VOID flow_tests(VOID) {
  FLOW f={0};CHECK(run(&f)==EFI_ABORTED && f.unlocks==1 && f.boots==1);
  f=(FLOW){.skip=TRUE};CHECK(run(&f)==EFI_ABORTED && !f.unlocks && f.boots==1);
  EFI_STATUS rejected[]={EFI_NOT_FOUND,EFI_SECURITY_VIOLATION,EFI_WARN_UNKNOWN_GLYPH};
  for(UINTN i=0;i<3;i++){f=(FLOW){.prepare=rejected[i]};CHECK(run(&f)==rejected[i] && !f.unlocks && !f.boots);}
  EFI_STATUS bad[]={EFI_ABORTED,EFI_TIMEOUT,EFI_DEVICE_ERROR,EFI_WARN_UNKNOWN_GLYPH};
  for(UINTN i=0;i<4;i++){f=(FLOW){.unlock=bad[i]};CHECK(run(&f)==bad[i] && f.unlocks==1 && !f.boots);}
  f=(FLOW){.select=EFI_DEVICE_ERROR};CHECK(run(&f)==EFI_DEVICE_ERROR && !f.unlocks && !f.boots);
  for(int i=1;i<=4;i++){f=(FLOW){.recordFail=i};CHECK(run(&f)==EFI_DEVICE_ERROR && f.boots==0 && f.unlocks==(i>=3));}
  f=(FLOW){.windows=EFI_LOAD_ERROR};CHECK(run(&f)==EFI_LOAD_ERROR && f.unlocks==1 && f.boots==1);
}

typedef struct {HARDDRIVE_DEVICE_PATH hd;EFI_DEVICE_PATH_PROTOCOL end;} DISK_PATH;
static DISK_PATH internal,windows,wrong;
static VOID path_init(DISK_PATH *p,const EFI_GUID *g) {
  memset(p,0,sizeof(*p));p->hd.Header.Type=MEDIA_DEVICE_PATH;p->hd.Header.SubType=MEDIA_HARDDRIVE_DP;
  p->hd.Header.Length[0]=sizeof(HARDDRIVE_DEVICE_PATH);p->hd.PartitionNumber=4;p->hd.PartitionSize=2097152;
  p->hd.MBRType=p->hd.SignatureType=2;memcpy(p->hd.Signature,g,16);
  p->end.Type=END_DEVICE_PATH_TYPE;p->end.SubType=END_ENTIRE_DEVICE_PATH_SUBTYPE;p->end.Length[0]=4;
}
static VOID path_tests(VOID) {
  path_init(&internal,&GuInternalPartition);path_init(&windows,&GuWindowsPartition);
  CHECK(GuAllowedMediaPath(&internal.hd.Header,TRUE));CHECK(!GuAllowedMediaPath(&internal.hd.Header,FALSE));
  CHECK(!GuAllowedMediaPath(&windows.hd.Header,TRUE));CHECK(GuPathIsPartition(&windows.hd.Header,&GuWindowsPartition));
  wrong=internal;wrong.hd.Signature[0]^=1;CHECK(!GuAllowedMediaPath(&wrong.hd.Header,TRUE));
  wrong=internal;wrong.hd.SignatureType=1;CHECK(!GuAllowedMediaPath(&wrong.hd.Header,TRUE));
  wrong=internal;wrong.hd.MBRType=1;CHECK(!GuAllowedMediaPath(&wrong.hd.Header,TRUE));
  wrong=internal;wrong.hd.Header.Length[0]=3;CHECK(!GuAllowedMediaPath(&wrong.hd.Header,TRUE));
  wrong=internal;wrong.end.SubType=END_INSTANCE_DEVICE_PATH_SUBTYPE;CHECK(!GuAllowedMediaPath(&wrong.hd.Header,TRUE));
  struct {USB_DEVICE_PATH usb;EFI_DEVICE_PATH_PROTOCOL end;} usb={0};
  usb.usb.Header.Type=MESSAGING_DEVICE_PATH;usb.usb.Header.SubType=MSG_USB_DP;usb.usb.Header.Length[0]=sizeof(USB_DEVICE_PATH);
  usb.end=internal.end;CHECK(GuAllowedMediaPath(&usb.usb.Header,FALSE));CHECK(GuAllowedMediaPath(&usb.usb.Header,TRUE));
}

static EFI_BOOT_SERVICES bs;
static EFI_SYSTEM_TABLE table;
static EFI_LOADED_IMAGE_PROTOCOL loaded;
static EFI_FILE_PROTOCOL root,file;
static UINT8 *imageBytes;static UINTN imageSize;
static UINTN metadataSize;static BOOLEAN badHash,shortRead,badInfo;static int loadCalls;
static EFI_STATUS loadResult;static int devices=2;static BOOLEAN duplicate;
static EFI_HANDLE own=(VOID *)1,win=(VOID *)2;
static EFI_STATUS EFIAPI alloc(EFI_MEMORY_TYPE t,UINTN n,VOID **p){(void)t;*p=malloc(n);return *p?EFI_SUCCESS:EFI_OUT_OF_RESOURCES;}
static EFI_STATUS EFIAPI free_pool(VOID*p){free(p);return EFI_SUCCESS;}
static EFI_STATUS EFIAPI handles(EFI_LOCATE_SEARCH_TYPE t,EFI_GUID*g,VOID*k,UINTN*n,EFI_HANDLE**out){(void)t;(void)g;(void)k;*n=devices;*out=malloc(sizeof(EFI_HANDLE)*devices);for(int i=0;i<devices;i++)(*out)[i]=i==0?own:win;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI protocol(EFI_HANDLE h,EFI_GUID*g,VOID**p){
  if(GuGuidEqual(g,&PathGuid)){*p=h==own?(VOID *)&internal:(VOID *)(duplicate?&internal:&windows);return EFI_SUCCESS;}
  if(GuGuidEqual(g,&LoadedGuid)){*p=&loaded;return EFI_SUCCESS;}
  return EFI_NOT_FOUND;
}
static EFI_STATUS EFIAPI open_file(EFI_FILE_PROTOCOL*r,EFI_FILE_PROTOCOL**f,CHAR16*n,UINT64 mode,UINT64 a){(void)r;(void)n;(void)mode;(void)a;*f=&file;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI get_info(EFI_FILE_PROTOCOL*f,EFI_GUID*g,UINTN*n,VOID*p){(void)f;(void)g;EFI_FILE_INFO*info=p;memset(info,0,sizeof(*info));info->FileSize=metadataSize;info->Size=sizeof(*info);*n=badInfo?1:sizeof(*info);return EFI_SUCCESS;}
static EFI_STATUS EFIAPI read_file(EFI_FILE_PROTOCOL*f,UINTN*n,VOID*p){(void)f;memcpy(p,imageBytes,*n);if(badHash)((UINT8*)p)[0]^=1;if(shortRead)(*n)--;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI close_file(EFI_FILE_PROTOCOL*f){(void)f;return EFI_SUCCESS;}
static EFI_FILE_PROTOCOL closeLog,closeRoot;
static EFI_STATUS closeLogResult,closeRootResult;
static int closeLogCalls,closeRootCalls,startCalls;
static EFI_STATUS EFIAPI tracked_close(EFI_FILE_PROTOCOL *f) {
  if(f==&closeLog){closeLogCalls++;return closeLogResult;}
  if(f==&closeRoot){closeRootCalls++;return closeRootResult;}
  abort();
}
static EFI_STATUS EFIAPI start_image(EFI_HANDLE image,UINTN *size,CHAR16 **data) {
  (void)image;(void)size;(void)data;startCalls++;return EFI_ABORTED;
}
static VOID close_tests(VOID) {
  closeLog.Close=tracked_close;closeRoot.Close=tracked_close;
  bs.StartImage=start_image;
  for(int fault=0;fault<4;fault++) {
    Log=&closeLog;Root=&closeRoot;closeLogCalls=closeRootCalls=startCalls=0;
    closeLogResult=fault&1?EFI_DEVICE_ERROR:EFI_SUCCESS;
    closeRootResult=fault&2?EFI_ACCESS_DENIED:EFI_SUCCESS;
    EFI_STATUS s=Windows(NULL);
    CHECK(s==(fault&1?EFI_DEVICE_ERROR:fault&2?EFI_ACCESS_DENIED:EFI_ABORTED));
    CHECK(closeLogCalls==1 && closeRootCalls==1 && !Log && !Root);
    CHECK(startCalls==(fault==0));
  }
}
static EFI_STATUS EFIAPI load_image(BOOLEAN policy,EFI_HANDLE parent,EFI_DEVICE_PATH_PROTOCOL*p,VOID*data,UINTN n,EFI_HANDLE*out){
  (void)parent;CHECK(!policy && p && data && n==imageSize);CHECK(!memcmp(data,imageBytes,n));loadCalls++;*out=(VOID*)3;loaded.DeviceHandle=own;return loadResult;
}
static VOID service_tests(VOID) {
  bs.AllocatePool=alloc;bs.FreePool=free_pool;bs.LocateHandleBuffer=handles;bs.HandleProtocol=protocol;bs.LoadImage=load_image;
  table.BootServices=&bs;Table=&table;Root=&root;OwnDevice=own;Self=(VOID*)5;
  root.Open=open_file;file.GetInfo=get_info;file.Read=read_file;file.Close=close_file;
  EFI_HANDLE found=NULL;
  CHECK(FindPartition(&GuInternalPartition,&found,FALSE)==EFI_SUCCESS && found==own);
  duplicate=TRUE;CHECK(FindPartition(&GuInternalPartition,&found,FALSE)==EFI_COMPROMISED_DATA);duplicate=FALSE;
  devices=1;CHECK(FindPartition(&GuWindowsPartition,&found,TRUE)==EFI_NOT_FOUND);devices=2;
  metadataSize=imageSize;CHECK(LoadUnlock()==EFI_SUCCESS && loadCalls==1);
  metadataSize=imageSize+1;CHECK(LoadUnlock()==EFI_BAD_BUFFER_SIZE && loadCalls==1);metadataSize=imageSize;
  badInfo=TRUE;CHECK(LoadUnlock()==EFI_BAD_BUFFER_SIZE && loadCalls==1);badInfo=FALSE;
  badHash=TRUE;CHECK(LoadUnlock()==EFI_CRC_ERROR && loadCalls==1);badHash=FALSE;
  shortRead=TRUE;CHECK(LoadUnlock()==EFI_END_OF_FILE && loadCalls==1);shortRead=FALSE;
  loadResult=EFI_SECURITY_VIOLATION;CHECK(LoadUnlock()==EFI_SECURITY_VIOLATION && loadCalls==2);
}
int main(int argc,char **argv) {
  if(argc!=2)return 2;
  FILE*f=fopen(argv[1],"rb");if(!f)return 2;
  fseek(f,0,SEEK_END);imageSize=(UINTN)ftell(f);rewind(f);imageBytes=malloc(imageSize);
  if(!imageBytes || fread(imageBytes,1,imageSize,f)!=imageSize)return 2;fclose(f);
  CHECK(imageSize==GU_UNLOCK_BYTES);flow_tests();path_tests();service_tests();close_tests();free(imageBytes);
  printf("%d handoff flow/media/image checks; failures=%d. No GPU access.\n",checks,failed);
  return failed?1:0;
}
