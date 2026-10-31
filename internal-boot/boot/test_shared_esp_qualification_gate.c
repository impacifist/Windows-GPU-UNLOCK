/* Synthetic one-shot entry gate only; hardware/release functions are stubs. */
#define GU_DIAGNOSTIC_ONLY
#define GU_EVGA_STOP_ONLY
#define GU_EVGA_SHARED_ESP_ADMISSION
#define GU_EVGA_PROBE_RESOURCES
#include "shared-esp-qualification.c"
static EFI_FILE_PROTOCOL GateRoot,Gate;static UINT8 Data[512];static UINTN Position,Flushes;static int Fail;
static EFI_STATUS EFIAPI Open(EFI_FILE_PROTOCOL *r,EFI_FILE_PROTOCOL **f,CHAR16 *p,UINT64 m,UINT64 a){(void)r;const CHAR16 wanted[]=L"\\RTX3090-UNLOCK\\QUAL01.ARMED";for(UINTN i=0;i<sizeof(wanted)/sizeof(CHAR16);i++)if(p[i]!=wanted[i])return EFI_NOT_FOUND;if(m!=(EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE) || a)return EFI_ACCESS_DENIED;Position=0;*f=&Gate;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Read(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *b){(void)f;if(*n>512-Position)*n=512-Position;for(UINTN i=0;i<*n;i++)((UINT8 *)b)[i]=Data[Position+i];Position+=*n;if(Fail==1 && Flushes && *n)((UINT8 *)b)[0]^=1;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Write(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *b){(void)f;if(Position || *n!=512)return EFI_BAD_BUFFER_SIZE;for(UINTN i=0;i<*n;i++)Data[i]=((UINT8 *)b)[i];Position+=*n;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Set(EFI_FILE_PROTOCOL *f,UINT64 p){(void)f;if(p>512)return EFI_INVALID_PARAMETER;Position=(UINTN)p;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Flush(EFI_FILE_PROTOCOL *f){(void)f;Flushes++;return Fail==2?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI Close(EFI_FILE_PROTOCOL *f){(void)f;return EFI_SUCCESS;}
static void Reset(void){Position=Flushes=0;for(UINTN i=0;i<512;i++)Data[i]=0;const char p[]="PENDING QUAL01\r\n";for(UINTN i=0;i<sizeof(p)-1;i++)Data[i]=p[i];}
int main(void){
 GateRoot.Open=Open;Gate.Read=Read;Gate.Write=Write;Gate.SetPosition=Set;Gate.Flush=Flush;Gate.Close=Close;EFI_FILE_PROTOCOL *gate=NULL;
 Reset();if(OpenReceipt(&GateRoot,&gate)!=EFI_SUCCESS || gate!=&Gate)return 1;
 if(WriteReceipt(gate,"STARTED QUAL01")!=EFI_SUCCESS || Flushes!=1)return 2;
 gate=NULL;if(OpenReceipt(&GateRoot,&gate)!=EFI_ACCESS_DENIED || gate)return 3;
 Reset();Data[511]=1;if(OpenReceipt(&GateRoot,&gate)!=EFI_ACCESS_DENIED)return 4;
 Reset();Fail=1;if(WriteReceipt(&Gate,"STARTED")!=EFI_COMPROMISED_DATA)return 5;
 Reset();Fail=2;if(WriteReceipt(&Gate,"STARTED")!=EFI_DEVICE_ERROR)return 6;
 return 0;
}
EFI_STATUS GuCaptureGraphOnVolume(EFI_SYSTEM_TABLE *t,EFI_FILE_PROTOCOL *f,EFI_FILE_PROTOCOL *r){(void)t;(void)f;(void)r;return EFI_UNSUPPORTED;}
EFI_STATUS GuEvgaTextStop(EFI_SYSTEM_TABLE *t,EFI_FILE_PROTOCOL *r,GU_EVGA_RECORD f,BOOLEAN *s){(void)t;(void)r;(void)f;(void)s;return EFI_UNSUPPORTED;}
EFI_STATUS GuEvgaTextStopVerify(EFI_SYSTEM_TABLE *t){(void)t;return EFI_UNSUPPORTED;}
EFI_STATUS GuEvgaResourcesPrepare(EFI_SYSTEM_TABLE *t,EFI_FILE_PROTOCOL *r,EFI_HANDLE i,GU_EVGA_RECORD f){(void)t;(void)r;(void)i;(void)f;return EFI_UNSUPPORTED;}
