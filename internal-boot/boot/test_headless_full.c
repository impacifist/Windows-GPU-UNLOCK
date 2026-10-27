/* GPL-2.0-only. Two-child exact-profile headless release host replay. */
#include "test_full.c"
static VOID PostUnlockStatusTest(VOID) {
  Check(RunFull(0)==EFI_SUCCESS,"new session for post-unlock status policy");
  UINT32 original=MockConfig[32];
  MockConfig[32]&=~0x00090000U;
  BOOLEAN permitted=MockConfig[32]==original || InitialFullConfig[0x78/4]==0x0012b410U;
  Check((GuVerifyReleased(LastTable,FALSE)==EFI_SUCCESS)==permitted,"old status clearing is limited to the reviewed capability layout");
  MockConfig[32]=original^0x00020000U;
  Check(GuVerifyReleased(LastTable,FALSE)!=EFI_SUCCESS,"other status drift is never waived");
  MockConfig[32]=original;
}
#if GU_NO_EDID_DRIVER
/* Firmware that binds no EDID driver: the release is the console detach plus one NVIDIA GOP stop.
 * The driver image is accepted either as a PE file or as a loaded-image dump of exactly the
 * profile's image size (what the probe saves as IMGnn.BIN). */
static VOID LoadNv(const char *path) {
  FILE *f=fopen(path,"rb");Check(f!=NULL,"nvgop image fixture exists");
  fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
  if((UINTN)n==sizeof(NvMemory)) {
    Check(fread(NvMemory,1,sizeof(NvMemory),f)==sizeof(NvMemory),"read nvgop loaded-image dump");fclose(f);
  } else {
    fclose(f);LoadMapped(path,NvMemory,sizeof(NvMemory));
  }
}
int main(int argc,char **argv) {
  Check(argc==4,"nvgop image, PROM and full PCI fixture required");
  LoadNv(argv[1]);
  FILE *f=fopen(argv[2],"rb");Check(f && fread(PromMemory,1,sizeof(PromMemory),f)==sizeof(PromMemory),"read pinned PROM");fclose(f);
  f=fopen(argv[3],"rb");Check(f && fread(InitialFullConfig,1,sizeof(InitialFullConfig),f)==sizeof(InitialFullConfig),"read exact full config");fclose(f);
  Check(GuHashMatches(PromMemory,sizeof(PromMemory),GU_PROM_SHA256),"PROM fixture matches the profile pin");
  DecodeTests();
  Check(Run(31)!=EFI_SUCCESS && !Calls,"missing Reset rejected before any device detach");
  Check(RunFull(0)==EFI_SUCCESS && Calls==3 && Full.EdidDetached==0 && !Full.EdidAgent && EnableCalls==1 && ClaimCalls==1 && CloseCalls==1,"NV stop only after the console detach, then bounded IO/MEM-only restore; no EDID stage");
  Check(GuVerifyReleased(LastTable,TRUE)==EFI_SUCCESS,"zero physical GOP and protected graph verified");
  Check(Live.Count+4+Full.ChildCount==sizeof(FixtureEdges)/sizeof(FixtureEdges[0]),"only three console edges, NV PCI owner and exact display children removed");
  /* Scenarios 1, 2, 17, 28, 29 and 32 exercise the EDID stage, which does not exist here. */
  const int refuse[]={3,4,5,6,7,8,9,10,11,12,13,14,15,16,18,19,20};
  for(UINTN i=0;i<sizeof(refuse)/sizeof(refuse[0]);i++) {
    int scenario=refuse[i];
    Check(RunFull(scenario)!=EFI_SUCCESS,"partial failure/code/config/cold/graph/owner/log fault stops");
    if(scenario>=8 && scenario<=13)Check(EnableCalls==1,"failure after effect remains failure");
    else Check(!EnableCalls,"pre-write fault cannot enable decode");
    if(scenario==18)Check(!ClaimCalls,"new owner blocks exclusive auto-disconnect");
    if(scenario==19)Check(Calls==2,"late cold-read new child blocks the NVIDIA stop");
  }
  const int blocked[]={23,24,25,26,27,30,33,34};
  for(UINTN i=0;i<sizeof(blocked)/sizeof(blocked[0]);i++) {
    int scenario=blocked[i];
    Check(RunFull(scenario)!=EFI_SUCCESS && !EnableCalls && !ClaimCalls,"extra child/consumer/path/code/EDID driver/warning/partial NV stop blocks decode and unlock");
    if(scenario==25 || scenario==26 || scenario==33 || scenario==34)Check(Calls==2,"late second child/code mutation or an EDID driver appearing blocks the NV stop");
    if(scenario==30)Check(Calls==3 && !Full.Released,"NV partial child cleanup error cannot be promoted to release success");
  }
  Check(RunFull(0)==EFI_SUCCESS,"new release session");
  Phase=3;Check(GuVerifyReleased(LastTable,FALSE)!=EFI_SUCCESS,"reappeared physical GOP blocks unlock/Windows");Phase=4;
  Check(RunFull(0)==EFI_SUCCESS,"new session for late input change");
  Keys.WaitForKey=NULL;Check(GuVerifyReleased(LastTable,FALSE)!=EFI_SUCCESS,"late input protocol drift blocks handoff");
  PostUnlockStatusTest();
  MockConfig[1]|=4;Check(GuVerifyReleased(LastTable,FALSE)!=EFI_SUCCESS,"BME remaining set blocks handoff");
  printf("PASS headless no-EDID-driver release checks: %d; real graph/code/PROM fixtures, no GPU access\n",Checks);
  return 0;
}
#else
int main(int argc,char **argv) {
  Check(argc==5,"two driver code fixtures (PE or loaded-image dump), PROM and full PCI fixture required");
  LoadDriver(argv[1],MouseMemory,sizeof(MouseMemory));LoadDriver(argv[2],NvMemory,sizeof(NvMemory));
  FILE *f=fopen(argv[3],"rb");Check(f && fread(PromMemory,1,sizeof(PromMemory),f)==sizeof(PromMemory),"read pinned PROM");fclose(f);
  f=fopen(argv[4],"rb");Check(f && fread(InitialFullConfig,1,sizeof(InitialFullConfig),f)==sizeof(InitialFullConfig),"read exact full config");fclose(f);
  DecodeTests();
  Check(Run(31)!=EFI_SUCCESS && !Calls,"missing Reset rejected before any device detach");
  const UINTN kids=FX_EdidChild?2:1;
  Check(RunFull(0)==EFI_SUCCESS && Calls==3+kids && Full.EdidDetached==kids && EnableCalls==1 && ClaimCalls==1 && CloseCalls==1,"EDID stop per display child, NV stop and bounded IO/MEM-only restore");
  Check(GuVerifyReleased(LastTable,TRUE)==EFI_SUCCESS,"zero physical GOP and protected graph verified");
  Check(Live.Count+3+kids+1+kids==sizeof(FixtureEdges)/sizeof(FixtureEdges[0]),"three console edges, the EDID edges, the NV PCI owner and one child path per display child removed only");
  for(int scenario=1;scenario<=20;scenario++) {
    Check(RunFull(scenario)!=EFI_SUCCESS,"partial failure/code/config/cold/graph/owner/log fault stops");
    if(scenario==8 || scenario==9 || scenario==10 || scenario==11 || scenario==12 || scenario==13)Check(EnableCalls==1,"failure after effect remains failure");
    else Check(!EnableCalls,"pre-write fault cannot enable decode");
    if(scenario==18)Check(!ClaimCalls,"new owner blocks exclusive auto-disconnect");
  }
  for(int scenario=23;scenario<=30;scenario++) {
    Check(RunFull(scenario)!=EFI_SUCCESS && !EnableCalls && !ClaimCalls,"extra child/consumer/path/code/missing EDID/warning/partial NV stop blocks decode and unlock");
    if(scenario==25 || scenario==26)Check(Calls==2+kids,"late second child/code mutation blocks recursive NV stop");
    if(scenario==30)Check(Calls==3+kids && !Full.Released,"NV partial child cleanup error cannot be promoted to release success");
  }
  Check(RunFull(32)!=EFI_SUCCESS && Calls==3 && !EnableCalls && !ClaimCalls,"last cold-read GOP info mutation blocks next detach");
  Check(RunFull(0)==EFI_SUCCESS,"new release session");
  Phase=3;Check(GuVerifyReleased(LastTable,FALSE)!=EFI_SUCCESS,"reappeared physical GOP blocks unlock/Windows");Phase=4;
  Check(RunFull(0)==EFI_SUCCESS,"new session for late input change");
  Keys.WaitForKey=NULL;Check(GuVerifyReleased(LastTable,FALSE)!=EFI_SUCCESS,"late input protocol drift blocks handoff");
  PostUnlockStatusTest();
  MockConfig[1]|=4;Check(GuVerifyReleased(LastTable,FALSE)!=EFI_SUCCESS,"BME remaining set blocks handoff");
  printf("PASS headless full-release checks: %d; real graph/code/PROM fixtures, no GPU access\n",Checks);
  return 0;
}
#endif
