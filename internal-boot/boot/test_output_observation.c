/* GPL-2.0-only. Real metadata collectors with RAM-only protocols. */
#pragma push_macro("main")
#undef main
#define main PreflightMain
#include "test_ad104_preflight.c"
#undef main
#pragma pop_macro("main")
#include "diagnostic.h"
#include <Protocol/SimpleTextOut.h>
static BOOLEAN ProbeUnchanged(CAPTURE *c,EFI_HANDLE h,EFI_PCI_IO_PROTOCOL *io,const UINT32 *cfg,const UINTN *loc) {
  (void)c;(void)h;(void)io;(void)cfg;(void)loc;return TRUE;
}
VOID Text(CAPTURE *c,const char *s){while(*s && c->Used<sizeof(c->Buffer)-1)c->Buffer[c->Used++]=*s++;c->Buffer[c->Used]=0;}
VOID Hex(CAPTURE *c,UINT64 v,UINTN n){char b[17];for(UINTN i=0;i<n;i++)b[i]="0123456789abcdef"[(v>>((n-i-1)*4))&15];b[n]=0;Text(c,b);}
VOID End(CAPTURE *c){Text(c,"\n");}
VOID Problem(CAPTURE *c,const char *s,EFI_STATUS e){(void)s;(void)e;c->Complete=FALSE;}
#define PfPins Pins
#include "output-observation.h"
#undef PfPins
static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL TextOut;
static EFI_SIMPLE_TEXT_OUTPUT_MODE TextMode;
static EFI_GRAPHICS_OUTPUT_PROTOCOL Gop;
static EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GopMode;
static EFI_GRAPHICS_OUTPUT_MODE_INFORMATION Info;
static PCD_PROTOCOL Pcd;
static int Mode,BoolCalls,SizeCalls;
static EFI_STATUS EFIAPI Next(CONST EFI_GUID *g,UINTN *token) {
  Check(g==NULL,"native token namespace");
  if(Mode==1)return EFI_NOT_FOUND;
  if(Mode==2){*token=0;return EFI_SUCCESS;}
  if(Mode==3){*token=0x4ad;return EFI_SUCCESS;}
  if(Mode==4)return EFI_DEVICE_ERROR;
  *token=0x4ac;return EFI_SUCCESS;
}
static UINTN EFIAPI Size(UINTN token){Check(token==0x4ac,"only admitted token size queried");SizeCalls++;return Mode==5?2:1;}
static BOOLEAN EFIAPI Bool(UINTN token){
  Check(token==0x4ac && SizeCalls>0,"only admitted Boolean queried");BoolCalls++;
  if((Mode==9 && BoolCalls==1) || (Mode==10 && BoolCalls==2))TextMode.CursorColumn++;
  return Mode==6?2:(BOOLEAN)(Mode==7);
}
static EFI_STATUS EFIAPI OutputHandle(EFI_HANDLE h,EFI_GUID *g,VOID **out) {
  if(h==(VOID *)2 && PfEqual(g,&GopGuidL,sizeof(*g))){*out=&Gop;return EFI_SUCCESS;}
  if(h==(VOID *)40 && PfEqual(g,&OutPcdGuid,sizeof(*g))){*out=&Pcd;return EFI_SUCCESS;}
  return HandleMock(h,g,out);
}
static EFI_STATUS EFIAPI OutputLocate(EFI_LOCATE_SEARCH_TYPE type,EFI_GUID *g,VOID *key,UINTN *n,EFI_HANDLE **out) {
  if(type==ByProtocol && PfEqual(g,&OutPcdGuid,sizeof(*g))) {
    *n=Mode==8?2:1;*out=malloc(*n*sizeof(EFI_HANDLE));for(UINTN i=0;i<*n;i++)(*out)[i]=(VOID *)40;return EFI_SUCCESS;
  }
  return LocateMock(type,g,key,n,out);
}
static void OutputSetup(int mode) {
  Setup(0,-1);Mode=mode;BoolCalls=SizeCalls=0;
  Bs.HandleProtocol=OutputHandle;Bs.LocateHandleBuffer=OutputLocate;
  memset(&TextOut,0,sizeof(TextOut));memset(&TextMode,0,sizeof(TextMode));TextOut.Mode=&TextMode;
  T.ConOut=T.StdErr=&TextOut;
  memset(&Gop,0,sizeof(Gop));memset(&GopMode,0,sizeof(GopMode));memset(&Info,0,sizeof(Info));
  Gop.Mode=&GopMode;GopMode.Info=&Info;GopMode.MaxMode=1;GopMode.SizeOfInfo=sizeof(Info);
  memset(&Pcd,0,sizeof(Pcd));Pcd.GetBool=Bool;Pcd.GetSize=Size;Pcd.GetNextToken=Next;
}
int main(void) {
  OUT_STATE a,b;BOOLEAN value=FALSE;UINTN steps=0;
  OutputSetup(0);Check(OutCollect(&T,(VOID *)2,&a)==EFI_SUCCESS,"protocol metadata copied");
  Check(OutToken(&a,&value,&steps)==EFI_SUCCESS && !value && BoolCalls==1 && steps==1,"false is a valid observation");
  Check(OutCollect(&T,(VOID *)2,&b)==EFI_SUCCESS && PfEqual(&a,&b,sizeof(a)),"stable snapshots agree");
  for(int mode=1;mode<=6;mode++) {
    OutputSetup(mode);Check(OutCollect(&T,(VOID *)2,&a)==EFI_SUCCESS,"query rejection separate from metadata");
    Check(OutToken(&a,&value,&steps)!=EFI_SUCCESS,"absent/invalid/wrong-sized token and malformed Boolean rejected");
    Check(BoolCalls==(mode==6),"invalid token never passed to getter");
  }
  OutputSetup(7);Check(OutCollect(&T,(VOID *)2,&a)==EFI_SUCCESS && OutToken(&a,&value,&steps)==EFI_SUCCESS && value==1,"true observed without setters");
  OutputSetup(8);Check(OutCollect(&T,(VOID *)2,&a)!=EFI_SUCCESS && !BoolCalls,"ambiguous PCD instance rejected");
  OutputSetup(0);T.ConOut=NULL;Check(OutCollect(&T,(VOID *)2,&a)!=EFI_SUCCESS,"missing console rejected");
  OutputSetup(0);TextOut.Mode=NULL;Check(OutCollect(&T,(VOID *)2,&a)!=EFI_SUCCESS,"missing text mode rejected");
  OutputSetup(0);GopMode.Info=NULL;Check(OutCollect(&T,(VOID *)2,&a)!=EFI_SUCCESS,"missing GOP info rejected");
  OutputSetup(0);GopMode.SizeOfInfo=sizeof(Info)-1;Check(OutCollect(&T,(VOID *)2,&a)!=EFI_SUCCESS,"short GOP info rejected");
  OutputSetup(0);GopMode.SizeOfInfo=4097;Check(OutCollect(&T,(VOID *)2,&a)!=EFI_SUCCESS,"oversized GOP info rejected");
  OutputSetup(0);GopMode.Mode=1;Check(OutCollect(&T,(VOID *)2,&a)!=EFI_SUCCESS,"invalid current GOP mode rejected");
  OutputSetup(0);Pcd.GetBool=NULL;Check(OutCollect(&T,(VOID *)2,&a)!=EFI_SUCCESS,"missing getter rejected");
  OutputSetup(0);Check(OutCollect(&T,(VOID *)2,&a)==EFI_SUCCESS,"padding fixture collected");
  ((UINT8 *)&TextMode)[sizeof(TextMode)-1]=0xff;
  Check(OutCollect(&T,(VOID *)2,&b)==EFI_SUCCESS && PfEqual(&a,&b,sizeof(a)),"irrelevant text mode padding normalized");
  for(int mode=0;mode<=10;mode++)if(mode==0 || mode==1 || mode==9 || mode==10) {
    OutputSetup(mode);Data[0][0x546a]=1;*(UINT64 *)(Data[0]+0x5530)=1;*(UINT64 *)(Data[0]+0x5550)=1;
    PF_STATE pf;Check(PfCollect(&T,(VOID *)1,Pins,&pf)==EFI_SUCCESS,"observation starts from exact fixture");
    CAPTURE c;memset(&c,0,sizeof(c));c.Table=&T;c.Complete=TRUE;
    UINT32 cfg[64]={0};UINTN loc[4]={0};ObserveOutput(&c,&pf,(VOID *)1,NULL,cfg,loc);
    if(mode==0 || mode==1) {
      Check(strstr(c.Buffer,"snapshot-status=0x0000000000000000")!=NULL,"snapshot success separate from query result");
      Check(strstr(c.Buffer,"ad104-output-text-mode name=conout")!=NULL,"console modes preserved in log");
      if(mode==1)Check(strstr(c.Buffer,"first-value=")==NULL,"repeated query failure never invents false");
    } else Check(strstr(c.Buffer,"snapshot-status=0x8000000000000006")!=NULL,"drift after either getter rejected");
  }
  printf("PASS AD104 output observation: %d checks, RAM-only mocks\n",Checks);return 0;
}
