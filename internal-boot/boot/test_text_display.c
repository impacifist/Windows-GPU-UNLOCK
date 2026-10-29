/* GPL-2.0-only. Real text display collector; capture trust is a RAM mock. */
#define main OutputMain
#include "test_output_observation.c"
#undef main
static BOOLEAN Captured;
static int DisplayFault,GetterCalls;
static BOOLEAN PcdVerifyCaptured(EFI_SYSTEM_TABLE *t,const OUT_STATE *o){(void)t;(void)o;return Captured;}
#include "text-display.h"
static EFI_STATUS EFIAPI DisplayLocate(EFI_GUID *g,VOID *registration,VOID **p) {
  (void)registration;if(!PfEqual(g,&OutPcdGuid,sizeof(*g)))return EFI_NOT_FOUND;*p=&Pcd;return EFI_SUCCESS;
}
static EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE OtherMode;
static BOOLEAN EFIAPI DisplayBool(UINTN token) {
  Check(token==0x4ac,"exact display predicate");GetterCalls++;
  if(DisplayFault==1)GopMode.Mode=3;
  if(DisplayFault==2)Gop.Mode=&OtherMode;
  if(DisplayFault==3)Info.HorizontalResolution++;
  if(DisplayFault==4)Captured=FALSE;
  return DisplayFault==5;
}
static void DisplaySetup(int fault) {
  OutputSetup(0);Captured=TRUE;DisplayFault=fault;GetterCalls=0;
  Bs.LocateProtocol=DisplayLocate;
  GopMode.MaxMode=5;GopMode.Mode=2;GopMode.FrameBufferBase=0xd0000000;GopMode.FrameBufferSize=0x300000;
  Info.HorizontalResolution=1024;Info.VerticalResolution=768;Info.PixelFormat=PixelBlueGreenRedReserved8BitPerColor;Info.PixelsPerScanLine=1024;
  Pcd.GetBool=DisplayBool;OtherMode=GopMode;
}
int main(void) {
  TEXT_DISPLAY out={0};DisplaySetup(0);
  Check(TextDisplayCollect(&T,(VOID *)2,&out)==EFI_SUCCESS && GetterCalls==1,"stable display and captured false predicate admitted");
  for(int f=1;f<=5;f++){DisplaySetup(f);memset(&out,0,sizeof(out));Check(TextDisplayCollect(&T,(VOID *)2,&out)!=EFI_SUCCESS,"getter-side mode/interface/info/code drift or true predicate rejected");}
  DisplaySetup(0);Captured=FALSE;Check(TextDisplayCollect(&T,(VOID *)2,&out)!=EFI_SUCCESS && !GetterCalls,"missing capture never queries predicate");
  DisplaySetup(0);T.ConOut=T.StdErr=NULL;Check(TextDisplayCollect(&T,(VOID *)2,&out)==EFI_SUCCESS,"post collector never requires text output");
  DisplaySetup(0);memset(&out,0,sizeof(out));Bs.HandleProtocol=NULL;Gop.Mode=(VOID *)1;
  Check(TextPcdCollect(&T,&out)==EFI_SUCCESS && GetterCalls==1 && !out.Gop && !out.Mode.Info,
        "PCD-only collector cannot query or dereference freed GOP");
  printf("PASS text display checks: %d checks, RAM-only mocks\n",Checks);return 0;
}
