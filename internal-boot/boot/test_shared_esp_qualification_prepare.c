/* Direct production Prepare/Record/Windows mocks. No GPU or firmware access. */
#define GU_DIAGNOSTIC_ONLY
#define GU_EVGA_SHARED_ESP_ADMISSION
#include "handoff.c"
typedef struct {HARDDRIVE_DEVICE_PATH hd;EFI_DEVICE_PATH_PROTOCOL end;} TEST_PATH;
static TEST_PATH correct,wrong;static EFI_HANDLE handles[2];static UINT8 pathBuffer[512];
static EFI_SIMPLE_FILE_SYSTEM_PROTOCOL fs;static EFI_FILE_PROTOCOL root,log;static EFI_LOADED_IMAGE_PROTOCOL win;
static BOOLEAN duplicate,existing,flushError,closeError,wrongWindows;static UINTN creates,loads,starts,closes,writes;
static EFI_STATUS EFIAPI Locate(EFI_LOCATE_SEARCH_TYPE t,EFI_GUID *g,VOID *k,UINTN *n,EFI_HANDLE **p){(void)t;(void)g;(void)k;*n=duplicate?2:1;*p=handles;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Free(VOID *p){(void)p;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Allocate(EFI_MEMORY_TYPE t,UINTN n,VOID **p){(void)t;if(n>sizeof(pathBuffer))return EFI_OUT_OF_RESOURCES;*p=pathBuffer;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Protocol(EFI_HANDLE h,EFI_GUID *g,VOID **p){
 if(GuGuidEqual(g,&PathGuid)){*p=(h==handles[0] || duplicate)?(VOID *)&correct:(VOID *)&wrong;return EFI_SUCCESS;}
 if(GuGuidEqual(g,&FsGuid)){*p=&fs;return EFI_SUCCESS;}
 if(GuGuidEqual(g,&LoadedGuid)){*p=&win;return EFI_SUCCESS;}return EFI_NOT_FOUND;
}
static EFI_STATUS EFIAPI Close(EFI_FILE_PROTOCOL *f){(void)f;closes++;return closeError?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI Open(EFI_FILE_PROTOCOL *r,EFI_FILE_PROTOCOL **f,CHAR16 *name,UINT64 mode,UINT64 attrs){
 (void)r;(void)attrs;const CHAR16 expected[]=L"\\RTX3090-UNLOCK\\QUAL01.TXT";
 for(UINTN i=0;i<sizeof(expected)/sizeof(CHAR16);i++)if(name[i]!=expected[i])return EFI_ACCESS_DENIED;
 if(mode==EFI_FILE_MODE_READ){if(!existing)return EFI_NOT_FOUND;*f=&log;return EFI_SUCCESS;}
 if(mode!=(EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE))return EFI_ACCESS_DENIED;
 creates++;*f=&log;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Volume(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *f,EFI_FILE_PROTOCOL **r){(void)f;*r=&root;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Write(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *b){(void)f;(void)n;(void)b;writes++;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Flush(EFI_FILE_PROTOCOL *f){(void)f;return flushError?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI Load(BOOLEAN b,EFI_HANDLE p,EFI_DEVICE_PATH_PROTOCOL *d,VOID *s,UINTN n,EFI_HANDLE *h){(void)b;(void)p;(void)d;(void)s;(void)n;loads++;*h=(EFI_HANDLE)3;win.DeviceHandle=wrongWindows?handles[1]:handles[0];return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Start(EFI_HANDLE h,UINTN *n,CHAR16 **d){(void)h;(void)n;(void)d;starts++;return EFI_ABORTED;}
static EFI_STATUS Run(void){Root=Log=NULL;OwnDevice=handles[0];return Prepare(NULL);}
int main(void){
 EFI_BOOT_SERVICES bs={0};EFI_SYSTEM_TABLE table={0};bs.LocateHandleBuffer=Locate;bs.HandleProtocol=Protocol;bs.FreePool=Free;bs.AllocatePool=Allocate;bs.LoadImage=Load;bs.StartImage=Start;table.BootServices=&bs;Table=&table;Self=(EFI_HANDLE)4;
 fs.OpenVolume=Volume;root.Open=Open;root.Close=Close;log.Close=Close;log.Write=Write;log.Flush=Flush;
 correct.hd.Header.Type=MEDIA_DEVICE_PATH;correct.hd.Header.SubType=MEDIA_HARDDRIVE_DP;correct.hd.Header.Length[0]=sizeof(HARDDRIVE_DEVICE_PATH);correct.hd.PartitionSize=1;correct.hd.MBRType=2;correct.hd.SignatureType=2;
 for(UINTN i=0;i<16;i++)correct.hd.Signature[i]=((UINT8 *)&GuWindowsPartition)[i];correct.end.Type=END_DEVICE_PATH_TYPE;correct.end.SubType=END_ENTIRE_DEVICE_PATH_SUBTYPE;correct.end.Length[0]=4;
 wrong=correct;wrong.hd.Signature[0]^=1;handles[0]=(EFI_HANDLE)1;handles[1]=(EFI_HANDLE)2;
 if(Run()!=EFI_SUCCESS || creates!=1 || loads!=1 || writes!=1)return 1;
 if(Windows(NULL)!=EFI_ABORTED || closes!=2 || starts!=1)return 2;
 existing=TRUE;if(Run()!=EFI_ALREADY_STARTED || creates!=1 || loads!=1)return 3;existing=FALSE;
 duplicate=TRUE;if(Run()!=EFI_COMPROMISED_DATA || creates!=1)return 4;duplicate=FALSE;
 flushError=TRUE;if(Run()!=EFI_DEVICE_ERROR || loads!=1)return 5;flushError=FALSE;
 wrongWindows=TRUE;if(Run()!=EFI_COMPROMISED_DATA)return 6;wrongWindows=FALSE;
 if(Run()!=EFI_SUCCESS)return 7;closeError=TRUE;
 if(Windows(NULL)!=EFI_DEVICE_ERROR || starts!=1 || Root || Log)return 8;
 return 0;
}
EFI_STATUS GuCaptureGraphOnVolume(EFI_SYSTEM_TABLE *t,EFI_FILE_PROTOCOL *f,EFI_FILE_PROTOCOL *r){(void)t;(void)f;(void)r;return EFI_UNSUPPORTED;}
