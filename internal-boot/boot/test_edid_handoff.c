/* GPL-2.0-only. Real handoff routing with synthetic EFI callbacks. */
#ifndef GU_AD104_TEXT_TEST
#define GU_AD104_EDID_TEST
#endif
#define main MbrMain
#include "test_mbr_handoff.c"
#undef main
static int RunCalls,StartCalls,Failure;
#ifdef GU_AD104_DECODE_READ_TEST
static EFI_FILE_PROTOCOL WitnessFile;
static int WitnessWrites,WitnessFlushes;
static char WitnessText[4096];static UINTN WitnessBytes;
static EFI_STATUS EFIAPI WitnessWriteMock(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *p) {
  CHECK(f==&WitnessFile && !WitnessExited);WitnessWrites++;
  if(Failure==8)return EFI_DEVICE_ERROR;
  if(Failure==21){(*n)--;return EFI_SUCCESS;}
  CHECK(WitnessBytes+*n<sizeof(WitnessText));memcpy(WitnessText+WitnessBytes,p,*n);WitnessBytes+=*n;WitnessText[WitnessBytes]=0;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI WitnessFlushMock(EFI_FILE_PROTOCOL *f){CHECK(f==&WitnessFile && !WitnessExited);WitnessFlushes++;return Failure==9?EFI_DEVICE_ERROR:EFI_SUCCESS;}
#endif
#ifdef GU_AD104_TEXT_TEST
BOOLEAN GuTextOutputDisabled;
static int OutputCalls;
static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL QuietOutput;
static EFI_STATUS EFIAPI QuietPrint(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *o,CHAR16 *s){(void)o;(void)s;OutputCalls++;return EFI_SUCCESS;}
#endif
EFI_STATUS GuEdidExperiment(EFI_SYSTEM_TABLE *t,EFI_FILE_PROTOCOL *f) {
  (void)t;(void)f;RunCalls++;return Failure==1?EFI_NOT_READY:EFI_SUCCESS;
}
#ifdef GU_AD104_PROM_FULL_TEST
EFI_STATUS GuEdidExperimentOnVolume(EFI_SYSTEM_TABLE *t,EFI_FILE_PROTOCOL *f,EFI_FILE_PROTOCOL *r,EFI_FILE_PROTOCOL *w) {
  CHECK(r==Root && w==Witness && r!=f && w!=f && r!=w);return GuEdidExperiment(t,f);
}
#endif
static EFI_STATUS EFIAPI LogWrite(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *p) {
  (void)f;(void)n;(void)p;return Failure==2?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI LogFlush(EFI_FILE_PROTOCOL *f){(void)f;return Failure==3?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI BootClose(EFI_FILE_PROTOCOL *f) {
  return (Failure==4 && f==&file)||(Failure==5 && f==&root)?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Start(EFI_HANDLE h,UINTN *n,CHAR16 **s) {
  (void)h;(void)n;(void)s;StartCalls++;
#ifdef GU_AD104_DECODE_READ_TEST
  CHECK(Witness==&WitnessFile && Log==NULL && Root==NULL);
  CHECK(strstr(WitnessText,"main-log-close-returned status=0x0000000000000000"));
  CHECK(strstr(WitnessText,"windows-StartImage-next"));
  if(Failure==7)WitnessExit(NULL,NULL);
  if(Failure==6 || Failure==7)return EFI_DEVICE_ERROR;
#endif
  return EFI_SUCCESS;
}
static void BootSetup(int f) {
  Failure=f;RunCalls=StartCalls=0;
#ifdef GU_AD104_DECODE_READ_TEST
  memset(&WitnessFile,0,sizeof(WitnessFile));Witness=&WitnessFile;WitnessExited=FALSE;WitnessRecords=0;
  WitnessWrites=WitnessFlushes=0;WitnessBytes=0;WitnessText[0]=0;
  WitnessFile.Write=WitnessWriteMock;WitnessFile.Flush=WitnessFlushMock;
#endif
#ifdef GU_AD104_TEXT_TEST
  GuTextOutputDisabled=TRUE;OutputCalls=0;QuietOutput.OutputString=QuietPrint;
#endif
  memset(&bs,0,sizeof(bs));memset(&table,0,sizeof(table));memset(&file,0,sizeof(file));memset(&root,0,sizeof(root));
  table.BootServices=&bs;Table=&table;Log=&file;Root=&root;WindowsImage=(VOID *)100;
#ifdef GU_AD104_TEXT_TEST
  table.ConOut=&QuietOutput;
#endif
  bs.StartImage=Start;file.Write=LogWrite;file.Flush=LogFlush;file.Close=root.Close=BootClose;
}
#ifdef GU_AD104_DECODE_READ_TEST
static int WitnessOpens;
static EFI_STATUS EFIAPI WitnessOpenMock(EFI_FILE_PROTOCOL *f,EFI_FILE_PROTOCOL **out,CHAR16 *name,UINT64 mode,UINT64 attr) {
  CHECK(f==&root && name && !attr);WitnessOpens++;*out=NULL;
  if(mode==EFI_FILE_MODE_READ){if(Failure==10){*out=&file;return EFI_SUCCESS;}return Failure==11?EFI_DEVICE_ERROR:EFI_NOT_FOUND;}
  CHECK(mode==(EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE));
  if(Failure==12)return EFI_DEVICE_ERROR;
  if(Failure==14)return EFI_SUCCESS;
  if(Failure==15){*out=&root;return EFI_SUCCESS;}
  if(Failure==16){*out=&file;return EFI_SUCCESS;}
  if(Failure==17)WitnessFile.Write=NULL;
  if(Failure==18)WitnessFile.Flush=NULL;
  *out=&WitnessFile;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI WitnessEventMock(UINT32 type,EFI_TPL tpl,EFI_EVENT_NOTIFY callback,VOID *context,EFI_EVENT *event) {
  CHECK(type==EVT_SIGNAL_EXIT_BOOT_SERVICES && tpl==TPL_NOTIFY && callback==WitnessExit && !context);
  if(Failure==13)return EFI_OUT_OF_RESOURCES;
  *event=Failure==19?NULL:(VOID *)12;return EFI_SUCCESS;
}
#endif

int main(void) {
  for(int f=0;f<=5;f++) {
    BootSetup(f);EFI_STATUS s=EdidThenWindows(EFI_NOT_READY,EFI_SUCCESS);
    CHECK((s==EFI_SUCCESS)==(f==0));CHECK(RunCalls==1 && StartCalls==(f==0));
#ifdef GU_AD104_TEXT_TEST
    Say(L"must remain quiet");CHECK(!OutputCalls);
#endif
  }
  BootSetup(0);CHECK(EdidThenWindows(EFI_NOT_READY,EFI_DEVICE_ERROR)!=EFI_SUCCESS && !RunCalls && !StartCalls);
  BootSetup(0);CHECK(EdidThenWindows(EFI_DEVICE_ERROR,EFI_SUCCESS)!=EFI_SUCCESS && !RunCalls && !StartCalls);
  BootSetup(0);WindowsImage=NULL;CHECK(EdidThenWindows(EFI_NOT_READY,EFI_SUCCESS)!=EFI_SUCCESS && !RunCalls && !StartCalls);
#ifdef GU_AD104_DECODE_READ_TEST
  for(int f=0;f<=4;f++) {
    BootSetup(f?f+9:0);Witness=NULL;WitnessOpens=0;root.Open=WitnessOpenMock;bs.CreateEvent=WitnessEventMock;
    EFI_STATUS opened=WitnessOpen();CHECK((opened==EFI_SUCCESS)==!f);
    CHECK(WitnessOpens==((f==1 || f==2)?1:2));
    if(!f)CHECK(strstr(WitnessText,"witness-open-before-detach"));
    CHECK(!RunCalls && !StartCalls);
  }
  for(int f=14;f<=19;f++) {
    BootSetup(f);Witness=NULL;WitnessExitEvent=NULL;root.Open=WitnessOpenMock;bs.CreateEvent=WitnessEventMock;
    CHECK(WitnessOpen()!=EFI_SUCCESS && !WitnessWrites && !StartCalls);
  }
  {int faults[]={8,9,21};for(UINTN i=0;i<3;i++) {
    BootSetup(faults[i]);Witness=NULL;root.Open=WitnessOpenMock;bs.CreateEvent=WitnessEventMock;
    CHECK(WitnessOpen()!=EFI_SUCCESS && !StartCalls);
  }}
  BootSetup(6);CHECK(EdidThenWindows(EFI_NOT_READY,EFI_SUCCESS)==EFI_DEVICE_ERROR && StartCalls==1);
  CHECK(strstr(WitnessText,"windows-StartImage-returned status=0x8000000000000007"));
  BootSetup(7);CHECK(EdidThenWindows(EFI_NOT_READY,EFI_SUCCESS)==EFI_ABORTED && StartCalls==1);
  CHECK(!strstr(WitnessText,"windows-StartImage-returned"));
  for(int f=8;f<=9;f++){BootSetup(f);CHECK(EdidThenWindows(EFI_NOT_READY,EFI_SUCCESS)!=EFI_SUCCESS && !StartCalls);}
  BootSetup(4);CHECK(EdidThenWindows(EFI_NOT_READY,EFI_SUCCESS)==EFI_DEVICE_ERROR);
  CHECK(strstr(WitnessText,"main-log-close-returned status=0x8000000000000007") && !StartCalls);
  BootSetup(0);WitnessRecords=16;CHECK(WitnessRecord("bounded",EFI_SUCCESS)==EFI_NOT_READY && !WitnessWrites);
#endif
#ifdef GU_AD104_FALCON_REFUSAL_TEST
  BootSetup(0);CHECK(GuRefusalWitness("falcon-observation-refused",EFI_NOT_READY)==EFI_SUCCESS);
  CHECK(strstr(WitnessText,"falcon-observation-refused status=0x8000000000000006"));
  BootSetup(8);CHECK(GuRefusalWitness("refusal-rechecks-complete",EFI_SUCCESS)!=EFI_SUCCESS);
  BootSetup(9);CHECK(GuRefusalWitness("refusal-rechecks-complete",EFI_SUCCESS)!=EFI_SUCCESS);
  BootSetup(0);WitnessRecords=16;CHECK(GuRefusalWitness("bounded",EFI_SUCCESS)==EFI_NOT_READY && !WitnessWrites);
  BootSetup(0);WitnessExited=TRUE;CHECK(GuRefusalWitness("no-post-EBS",EFI_SUCCESS)==EFI_NOT_READY && !WitnessWrites);
#endif
  printf("PASS EDID Windows handoff: %d checks, RAM-only mocks\n",checks);return 0;
}
