/* GPL-2.0-only. Three real lifecycle stages with RAM-only firmware mocks. */
#define GU_AD104_TEXT_TEST
#define GU_AD104_GOP_TEST
#define GU_AD104_CONSUMERS_TEST
#define main OriginalMain
#include "test_ad104_edid.c"
#undef main
int main(void) {
  CAPTURE *c=EdSetup(0);
  Check(EdRun(c,(VOID *)1,&Io,Pins)==EFI_SUCCESS && Calls==3 && Stage==3,"three consumers qualify quiet handoff");
  Check(ConsumerEdidStep(c,(VOID *)1,&Io,Pins,NULL,NULL)==EFI_ALREADY_STARTED && Calls==3,"third stop cannot retry");
  for(int f=1;f<=11;f++) {
    c=EdSetup(0);ConsumerFault=f;
    Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && Calls==(f==9?2:3),"third callback/graph/flag/input/config/log faults block handoff");
    Check(Collections>=(f==9?8:10),"third callback errors still collect poststate");
  }
  c=EdSetup(0);GopFault=3;
  Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && Calls==2,"failed GOP prevents EDID");
  c=EdSetup(5);
  Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && Calls==1,"failed text prevents later stages");
  c=EdSetup(18);
  Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && Calls==3,"final input config drift blocks handoff");
  printf("PASS three-consumer lifecycle: %d checks, RAM-only mocks\n",Checks);return 0;
}
