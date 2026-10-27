/* GPL-2.0-only. USB-only, positively confirmed pilot. Never chainloads. */
#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include "core.h"
#include "usb-path.h"
#ifndef GU_PILOT_STAGE
#error Explicit pilot stage required
#endif
#if GU_PILOT_STAGE != 1
#error Only fixed Stage 1 is available
#endif
#if defined(GU_PILOT_HOST_TEST) && __STDC_HOSTED__ != 1
#error Host halt hook must not enter firmware
#endif
static EFI_GUID Loaded=EFI_LOADED_IMAGE_PROTOCOL_GUID,Path=EFI_DEVICE_PATH_PROTOCOL_GUID,Fs=EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_FILE_PROTOCOL *Root,*Parent,*RunDir,*LogFile;
static EFI_SYSTEM_TABLE *Table;
static BOOLEAN OutputDisabled;
static EFI_STATUS Record(VOID *ctx,const char *event,EFI_STATUS status) {
  (void)ctx;if(!LogFile || !LogFile->Write || !LogFile->Flush)return EFI_DEVICE_ERROR;
  char line[256];UINTN n=0;const char *prefix="stage=";
  while(*prefix)line[n++]=*prefix++;line[n++]=(char)('0'+GU_PILOT_STAGE);line[n++]=' ';
  for(UINTN i=0;event[i];i++){if(n>210)return EFI_BAD_BUFFER_SIZE;line[n++]=event[i];}
  line[n++]=' ';for(int i=15;i>=0;i--)line[n++]="0123456789abcdef"[((UINT64)status>>(i*4))&15];
  line[n++]='\r';line[n++]='\n';UINTN written=n;
  EFI_STATUS s=LogFile->Write(LogFile,&written,line);if(s!=EFI_SUCCESS)return s;
  if(written!=n)return EFI_DEVICE_ERROR;
  return LogFile->Flush(LogFile);
}
static EFI_STATUS Drain(VOID) {
  if(!PilotInputStable())return EFI_NOT_READY;
  if(!Table->ConIn || !Table->ConIn->ReadKeyStroke)return EFI_UNSUPPORTED;
  for(UINTN i=0;i<64;i++){EFI_INPUT_KEY key;EFI_STATUS s=Table->ConIn->ReadKeyStroke(Table->ConIn,&key);if(s==EFI_NOT_READY)return EFI_SUCCESS;if(s!=EFI_SUCCESS)return s;}
  return EFI_NOT_READY; /* A stuck/repeating queue never becomes confirmation. */
}
static EFI_STATUS WaitKey(CHAR16 expected) {
  if(!Table->BootServices->Stall)return EFI_UNSUPPORTED;
  UINTN budget=expected=='C'?300:100;
  for(UINTN i=0;i<budget;i++) {
    if(!PilotInputStable())return EFI_NOT_READY;
    EFI_INPUT_KEY key;EFI_STATUS s=Table->ConIn->ReadKeyStroke(Table->ConIn,&key);
    if(s==EFI_SUCCESS){if(key.ScanCode==0x17 || key.UnicodeChar==27)return EFI_ABORTED;
      if(!key.ScanCode && key.UnicodeChar==expected)return EFI_SUCCESS;
    }else if(s!=EFI_NOT_READY)return s;
    s=Table->BootServices->Stall(100000);if(s!=EFI_SUCCESS)return s;
  }
  return EFI_TIMEOUT;
}
static EFI_STATUS OpenLog(EFI_HANDLE image) {
  EFI_BOOT_SERVICES *bs=Table->BootServices;EFI_LOADED_IMAGE_PROTOCOL *im=NULL;
  EFI_DEVICE_PATH_PROTOCOL *path=NULL;EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;
  EFI_STATUS s=bs->HandleProtocol(image,&Loaded,(VOID **)&im);
  if(s!=EFI_SUCCESS || !im || !im->DeviceHandle)return EFI_UNSUPPORTED;
  s=bs->HandleProtocol(im->DeviceHandle,&Path,(VOID **)&path);
  if(s!=EFI_SUCCESS || !LocalUsbPath(path))return EFI_UNSUPPORTED;
  s=bs->HandleProtocol(im->DeviceHandle,&Fs,(VOID **)&fs);
  if(s!=EFI_SUCCESS || !fs || !fs->OpenVolume)return EFI_UNSUPPORTED;
  s=fs->OpenVolume(fs,&Root);if(s!=EFI_SUCCESS || !Root)return EFI_DEVICE_ERROR;
  s=Root->Open(Root,&Parent,L"\\GPU-PILOT",EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,EFI_FILE_DIRECTORY);
  if(s!=EFI_SUCCESS || !Parent)return EFI_DEVICE_ERROR;
  for(UINTN i=0;i<100;i++) {
    CHAR16 name[]=L"S0RUN00";name[1]=(CHAR16)('0'+GU_PILOT_STAGE);name[5]=(CHAR16)('0'+i/10);name[6]=(CHAR16)('0'+i%10);
    EFI_FILE_PROTOCOL *old=NULL;s=Parent->Open(Parent,&old,name,EFI_FILE_MODE_READ,0);
    if(s==EFI_SUCCESS){if(!old || old->Close(old)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;continue;}
    if(s!=EFI_NOT_FOUND)return s;
    s=Parent->Open(Parent,&RunDir,name,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,EFI_FILE_DIRECTORY);
    if(s!=EFI_SUCCESS || !RunDir)return EFI_DEVICE_ERROR;
    return RunDir->Open(RunDir,&LogFile,L"REPORT.TXT",EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0);
  }
  return EFI_VOLUME_FULL;
}
static EFI_STATUS CloseFiles(VOID) {
  EFI_STATUS first=EFI_SUCCESS;
  EFI_FILE_PROTOCOL **files[]={&LogFile,&RunDir,&Parent,&Root};
  for(UINTN i=0;i<4;i++)if(*files[i]){EFI_STATUS s=(*files[i])->Close(*files[i]);*files[i]=NULL;if(first==EFI_SUCCESS)first=s;}
  return first;
}
static EFI_STATUS PilotRun(EFI_HANDLE image,EFI_SYSTEM_TABLE *t) {
  Table=t;Root=Parent=RunDir=LogFile=NULL;OutputDisabled=FALSE;
  if(!t || !t->BootServices || !t->ConIn || !t->ConOut || !t->ConOut->OutputString || !t->BootServices->HandleProtocol || !t->BootServices->SetWatchdogTimer)return EFI_INVALID_PARAMETER;
  EFI_STATUS s=t->BootServices->SetWatchdogTimer(0,0,0,NULL);if(s!=EFI_SUCCESS)return s;
  CHAR16 banner[]=L"MSI X570 / RTX 3080 Ti PILOT STAGE @\r\nNo unlock payload. No Windows handoff. One run then halt.\r\nAfter any release the screen may freeze. When requested before release,\r\nafter EACH step wait 5 seconds, then press/release K. Repeat for each step.\r\nPower off after completion or the agreed external deadline.\r\n";
  for(UINTN i=0;banner[i];i++)if(banner[i]=='@'){banner[i]=(CHAR16)('0'+GU_PILOT_STAGE);break;}
  s=t->ConOut->OutputString(t->ConOut,banner);if(s!=EFI_SUCCESS)return s;
  s=OpenLog(image);if(s!=EFI_SUCCESS)return s;
  if(!LogFile || !LogFile->Close || !LogFile->Write || !LogFile->Flush)return EFI_UNSUPPORTED;
  s=Record(NULL,"pilot-begin",EFI_SUCCESS);
  if(s==EFI_SUCCESS)s=PilotPreflight(t,image,Record,NULL);
  if(s==EFI_SUCCESS)s=Record(NULL,"pilot-preflight",EFI_SUCCESS);
  if(s==EFI_SUCCESS)s=Drain();
  if(s==EFI_SUCCESS)s=t->ConOut->OutputString(t->ConOut,L"Press uppercase C within 30 seconds to confirm this stage; Esc/timeout cancels.\r\nAfter each step, wait 5 seconds then press/release uppercase K. Repeat per step. No later stage is selected.\r\n");
  if(s==EFI_SUCCESS)s=WaitKey('C');
  if(s==EFI_SUCCESS)s=Record(NULL,"pilot-confirmed",EFI_SUCCESS);
  if(s==EFI_SUCCESS)s=Drain();
  if(s==EFI_SUCCESS)s=PilotConfirm();
  for(UINTN step=1;s==EFI_SUCCESS && step<=GU_PILOT_STAGE;step++) {
    OutputDisabled=TRUE;s=PilotAdvance(step);
    if(s==EFI_SUCCESS)s=Record(NULL,"pilot-step-verified",step);
    if(s==EFI_SUCCESS) {
    s=Drain(); /* Removes keys pressed before cleanup returned. */
    if(s==EFI_SUCCESS)s=Record(NULL,"pilot-wait-new-key",EFI_SUCCESS);
    /* Give the operator a defined pause without consuming a prequeued key. */
    if(s==EFI_SUCCESS)s=t->BootServices->Stall(5000000);
    if(s==EFI_SUCCESS)s=Drain();
    if(s==EFI_SUCCESS)s=WaitKey('K');
    if(s==EFI_SUCCESS)s=Record(NULL,"pilot-fresh-key",EFI_SUCCESS);
    if(s==EFI_SUCCESS)s=PilotVerifyAt(step,"fresh-key");
    }
  }
  if(s==EFI_SUCCESS)s=PilotEvidence();
  if(s==EFI_SUCCESS)s=PilotVerifyAt(GU_PILOT_STAGE,"final-evidence");
  if(s==EFI_SUCCESS)s=Record(NULL,"stage1-splitter-only-pass-close-pending",EFI_SUCCESS);
  if(s!=EFI_SUCCESS){PilotReportFailure(Record,NULL,s);(void)Record(NULL,"pilot-stop",s);}
  if(LogFile) {
    EFI_STATUS closed=LogFile->Close(LogFile);LogFile=NULL;
    if(s==EFI_SUCCESS)s=closed;
  }
  /* This sidecar attests the primary report closed successfully. It is never
   * produced after any failed guard, input wait, write, flush or report close. */
  if(s==EFI_SUCCESS)s=PilotVerifyAt(GU_PILOT_STAGE,"primary-closed");
  if(s==EFI_SUCCESS && RunDir) {
    EFI_FILE_PROTOCOL *done=NULL;
    s=RunDir->Open(RunDir,&done,L"REPORT-CLOSED.OK",EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0);
    if(s==EFI_SUCCESS && done && done->Write && done->Flush && done->Close) {
      char proof[]="Stage 1 splitter-only primary report closed. No GPU release/decode permit. Marker alone is not success or recovery proof.\r\n";
      UINTN bytes=sizeof(proof)-1;
      s=done->Write(done,&bytes,proof);
      if(s==EFI_SUCCESS && bytes!=sizeof(proof)-1)s=EFI_DEVICE_ERROR;
      if(s==EFI_SUCCESS)s=done->Flush(done);
      EFI_STATUS closed=done->Close(done);if(s==EFI_SUCCESS)s=closed;
    }else if(s==EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  }
  EFI_STATUS close=CloseFiles();if(s==EFI_SUCCESS)s=close;
  if(s==EFI_SUCCESS)s=PilotVerifyAt(1,"files-closed");
  return s;
}
static VOID Halt(EFI_STATUS status) {
#ifdef GU_PILOT_HOST_TEST
  extern VOID PilotHostHalt(EFI_STATUS);PilotHostHalt(status);
#else
  (void)status;
  for(;;)__asm__ volatile("pause");
#endif
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *t) {
  EFI_STATUS s=PilotRun(image,t);
  PilotEnd();
  Halt(s);return EFI_ABORTED;
}
