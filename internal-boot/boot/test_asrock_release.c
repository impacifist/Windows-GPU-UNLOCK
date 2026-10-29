/* ASRock port changes: 2026-10-27; publication notice: 2026-10-28. GPL-2.0-only; see CHANGES.md. */
/* GPL-2.0-only. Real captured code/graph as inert data; all EFI calls simulated. */
#include "test_full.c"
static void ReadExact(const char *path,void *target,size_t bytes) {
  FILE *f=fopen(path,"rb");Check(f!=NULL,"fixture exists");
  Check(fread(target,1,bytes,f)==bytes && fgetc(f)==EOF,"exact fixture size");
  fclose(f);
}
int main(int argc,char **argv) {
  Check(argc==5,"EDID, NVIDIA, PROM and config fixtures required");
  ReadExact(argv[1],MouseMemory,sizeof(MouseMemory));
  ReadExact(argv[2],NvMemory,sizeof(NvMemory));
  ReadExact(argv[3],PromMemory,sizeof(PromMemory));
  ReadExact(argv[4],InitialFullConfig,sizeof(InitialFullConfig));
  Check(GuHashMatches(PromMemory,sizeof(PromMemory),GU_PROM_SHA256),"PROM matches capture");
  DecodeTests();
  EFI_STATUS status=RunFull(0);
  if(status!=EFI_SUCCESS)fprintf(stderr,"release status=0x%llx stage=%s\n",(unsigned long long)status,GuLastVerifyDiagnostic()->Stage);
  Check(status==EFI_SUCCESS && Calls==4 && EnableCalls==1 && ClaimCalls==1 && CloseCalls==1,"single-child complete release");
  Check(GuVerifyReleased(LastTable,TRUE)==EFI_SUCCESS,"released graph and cold state verified");
  const int faults[]={1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20};
  for(unsigned i=0;i<sizeof(faults)/sizeof(faults[0]);i++) {
    Check(RunFull(faults[i])!=EFI_SUCCESS,"injected release fault fails closed");
  }
  Check(RunFull(0)==EFI_SUCCESS,"fresh session");
  MockConfig[1]|=4;
  Check(GuVerifyReleased(LastTable,FALSE)!=EFI_SUCCESS,"late BME drift blocks handoff");
  printf("PASS ASRock release checks: %d; captured fixtures and simulated EFI only\n",Checks);
  return 0;
}
