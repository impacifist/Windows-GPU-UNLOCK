/* GPL-2.0-only. Real two-stage engine and graph/protocol gates, RAM services. */
#define GU_AD104_TEXT_TEST
#define GU_AD104_GOP_TEST
#define main TextMain
#include "test_ad104_edid.c"
#undef main
int main(void) {
  CAPTURE *c=EdSetup(0);
  Check(EdRun(c,(VOID *)1,&Io,Pins)==EFI_SUCCESS && Calls==2 && Stage==2,"text then GOP transition qualifies quiet handoff");
  Check(GopStep(c,(VOID *)1,&Io,Pins,NULL,NULL,NULL)==EFI_ALREADY_STARTED && Calls==2,"GOP retry forbidden");
  for(int f=1;f<=12;f++) {
    c=EdSetup(0);GopFault=f;
    Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS,"protocol/callback/graph/config/query/logging/input failures block handoff");
    Check(Calls==(f<=2 || f==12?1:2),"GOP only runs after both protocol presence/identity checks pass");
    if(f>=3 && f!=7 && f!=12)Check(Collections>=8,"error callback still followed by fresh collections");
  }
  c=EdSetup(20);Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !Calls,"wrong original text state prevents both calls");
  c=EdSetup(5);Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && Calls==1,"failed text callback prevents GOP call");
  printf("PASS GOP consumer lifecycle: %d checks, RAM-only mocks\n",Checks);return 0;
}
