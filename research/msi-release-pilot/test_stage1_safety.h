/* GPL-2.0-only. Raw-read boundaries and reconstructable failure evidence. */
#pragma once
static VOID FailureField(const char *name,UINT64 value) {
  char line[160];snprintf(line,sizeof(line),"stage1-failure-%s %016llx",name,(unsigned long long)value);
  Check(strstr(TestReport,line)!=NULL,"failure field preserves exact observed value or validity");
}
static VOID FailureCheckpoint(const char *name,const char *phase) {
  char line[160];snprintf(line,sizeof(line),"stage1-failure-checkpoint=%s ",name);
  Check(strstr(TestReport,line)!=NULL,"failure checkpoint accurately identifies rejected prerequisite");
  snprintf(line,sizeof(line),"stage1-failure-phase=%s ",phase);
  Check(strstr(TestReport,line)!=NULL,"failure lifecycle phase identifies the verification boundary");
}
static VOID FailureContained(EFI_STATUS s,EFI_STATUS expected,int calls) {
  Check(s==expected && TestDisconnects==calls && !ClosedMarker,"failure preserves original status and effect boundary");
  Check(!TestPromPool && TestTpl==TPL_APPLICATION,"failure frees PROM memory and restores application TPL");
  FailureField("status",expected);FailureField("record-complete",EFI_SUCCESS);
}
static VOID FailureConfig(const UINT32 expected[64]) {
  const char *prefix="stage1-failure-pci-",*cursor=TestReport;
  BOOLEAN seen[64]={0};UINTN count=0;
  while((cursor=strstr(cursor,prefix))!=NULL) {
    cursor+=strlen(prefix);unsigned index=64;unsigned long long value=0;int consumed=0;
    int fields=sscanf(cursor,"%2x %16llx%n",&index,&value,&consumed);
    Check(fields==2 && consumed==19 && cursor[2]==' ' && (cursor[19]=='\r' || cursor[19]=='\n'),"PCI diagnostic record has exact index/value syntax");
    Check(index<64 && !seen[index],"PCI diagnostic indices are unique and bounded");
    seen[index]=TRUE;count++;
    Check(value==expected[index],"every PCI diagnostic word equals the independent expected observation");
    cursor+=consumed;
  }
  Check(count==64,"complete PCI diagnostic contains all 64 words");
}
static VOID Stage1SafetyCases(const char *selection) {
  Check(!strcmp(selection,"all") || !strcmp(selection,"raw") || !strcmp(selection,"evidence"),"known safety regression selection");
  int runs=Stage1Runs;
  TestRaw=2;TestAttributes=0x300;TestAllocationTarget=1;TestEvidenceTransport=0;
  if(!strcmp(selection,"all") || !strcmp(selection,"raw")) {
    for(int pair=0;pair<2;pair++) {
      TestAttributes=pair?0x300:0x200;
      Check(Stage1Case(F_ALLOC_NOOP,AT_SETUP,0)==EFI_SUCCESS && TestDisconnects==1 && Confirmed==1 && ClosedMarker,"safe allocation control completes one splitter operation");
      Check(TestAllocationInjected && TestMmio>0 && TestAllocations==TestPromFrees && !TestPromPool,"positive control exercises PROM reads and cleanup");
      Check(!strstr(TestReport,"stage1-failure-"),"successful run does not fabricate failure evidence");
    }
    TestAttributes=0x300;
    const int faults[]={F_ALLOC_MEM,F_ALLOC_BME,F_ALLOC_CONFIG,F_ALLOC_READ,F_ALLOC_PARTIAL,F_ALLOC_FREE_FAIL,F_FREE_FAIL,F_ALLOC_FAIL};
    const int targets[]={1,2,4,6};
    for(UINTN t=0;t<sizeof(targets)/sizeof(*targets);t++)for(UINTN f=0;f<sizeof(faults)/sizeof(*faults);f++) {
      TestAllocationTarget=targets[t];int fault=faults[f];
      EFI_STATUS expected=fault==F_ALLOC_BME?EFI_UNSUPPORTED:
        (fault==F_ALLOC_READ || fault==F_ALLOC_PARTIAL)?EFI_DEVICE_ERROR:
        fault==F_FREE_FAIL?EFI_WRITE_PROTECTED:fault==F_ALLOC_FAIL?EFI_OUT_OF_RESOURCES:EFI_NOT_READY;
      EFI_STATUS s=Stage1Case(fault,AT_SETUP,0);
      FailureContained(s,expected,targets[t]==6?1:0);
      Check(TestAllocations==targets[t] && TestPromFrees==targets[t]-(fault==F_ALLOC_FAIL),"all successful allocations are freed on failure");
      if(fault!=F_ALLOC_FAIL && fault!=F_FREE_FAIL)Check(TestMmio==TestMmioAtAllocation,"allocation-return fault blocks every dependent GPU read");
      FailureCheckpoint(fault==F_ALLOC_FAIL?"prom-allocate":fault==F_FREE_FAIL?"prom-free":"prom-raw-safety",
        targets[t]<=2?"preflight":targets[t]==4?"pre-disconnect-raised":"post-disconnect");
      if(fault==F_ALLOC_READ || fault==F_ALLOC_PARTIAL) {
        FailureField("config-valid",0);FailureField("config-read-status",EFI_DEVICE_ERROR);
        Check(!strstr(TestReport,"stage1-failure-pci-"),"failed or partial config buffer never becomes evidence");
      } else if(fault!=F_ALLOC_FAIL)FailureField("config-valid",1);
    }
    TestAllocationTarget=1;
    FailureContained(Stage1Case(F_BAR_MEM,AT_AFTER,0),EFI_NOT_READY,1);
    FailureCheckpoint("verify-cold-raw-safety","post-disconnect");
    FailureField("config-offset",4);FailureField("config-observed",FixtureFullConfig[1]&~2U);
    FailureContained(Stage1Case(F_COLD_BME,AT_AFTER,0),EFI_UNSUPPORTED,1);
    FailureCheckpoint("prom-raw-safety","post-disconnect");
    FailureField("config-offset",4);FailureField("config-observed",FixtureFullConfig[1]|4U);
  }
  if(!strcmp(selection,"all") || !strcmp(selection,"evidence")) {
    FailureContained(Stage1Case(F_ATTRIBUTES,AT_AFTER,8),EFI_NOT_READY,1);
    FailureCheckpoint("verify-attributes-match","post-disconnect");
    FailureField("attributes-expected",0x300);FailureField("attributes-observed",0x200);
    FailureField("config-valid",1);FailureField("attributes-valid",1);FailureField("config-different",0);
    FailureConfig(FixtureFullConfig);
    static char attributes[sizeof(TestReport)];strcpy(attributes,TestReport);
    FailureContained(Stage1Case(F_CONFIG,AT_AFTER,30),EFI_NOT_READY,1);
    FailureCheckpoint("verify-config-match","post-disconnect");
    FailureField("config-offset",0x78);FailureField("config-expected",FixtureFullConfig[30]);
    FailureField("config-observed",FixtureFullConfig[30]^0x10);FailureField("attributes-attempted",0);
    Check(strcmp(attributes,TestReport)!=0,"distinct post-Stop state failures produce distinct complete reports");
    Check(!strstr(TestReport,"stage1-failure-attributes-observed"),"unattempted attributes are not invented");
    UINT32 expected[64];memcpy(expected,FixtureFullConfig,sizeof(expected));expected[30]^=0x10;
    FailureConfig(expected);
    FailureContained(Stage1Case(F_CONFIG,AT_AFTER,63),EFI_NOT_READY,1);
    memcpy(expected,FixtureFullConfig,sizeof(expected));expected[63]^=0x10;
    FailureConfig(expected);FailureField("config-offset",0xfc);
    FailureContained(Stage1Case(F_CONFIG_MULTI,AT_AFTER,0),EFI_NOT_READY,1);
    memcpy(expected,FixtureFullConfig,sizeof(expected));expected[29]^=0x4000;expected[30]^=0x10;expected[63]^=0x20;
    FailureConfig(expected);FailureField("config-offset",0x74);
    FailureContained(Stage1Case(F_CONFIG,AT_AFTER,7),EFI_NOT_READY,1);
    FailureCheckpoint("verify-alive","post-disconnect");FailureField("config-attempted",0);
    Check(!strstr(TestReport,"stage1-failure-pci-"),"early BAR identity rejection does not invent a later config snapshot");
    for(int fault=F_POST_CONFIG_READ;fault<=F_POST_ATTRIBUTE_READ;fault++) {
      FailureContained(Stage1Case(fault,AT_AFTER,0),EFI_DEVICE_ERROR,1);
      FailureCheckpoint(fault==F_POST_ATTRIBUTE_READ?"verify-attributes-read":"verify-config-read","post-disconnect");
      if(fault==F_POST_ATTRIBUTE_READ) {
        FailureField("config-valid",1);FailureField("attributes-valid",0);FailureField("attributes-read-status",EFI_DEVICE_ERROR);
        FailureConfig(FixtureFullConfig);
        Check(!strstr(TestReport,"stage1-failure-attributes-observed"),"failed attribute output is not reported as an observation");
        Check(strcmp(attributes,TestReport)!=0,"config and attribute read errors have distinct reports");
      } else {
        FailureField("config-attempted",1);FailureField("config-valid",0);FailureField("attributes-attempted",0);
        Check(!strstr(TestReport,"stage1-failure-pci-"),"failed post-Stop config buffer is not reported");
        strcpy(attributes,TestReport);
      }
    }
    const int points[]={AT_CONFIRM,AT_INTENT,AT_RESTORE,AT_KEY};
    const char *phases[]={"confirmation","pre-disconnect-raised","pre-disconnect","fresh-key"};
    for(UINTN i=0;i<4;i++) {
      FailureContained(Stage1Case(F_ATTRIBUTES,points[i],8),EFI_NOT_READY,i==3?1:0);
      FailureCheckpoint("verify-attributes-match",phases[i]);
    }
    FailureContained(Stage1Case(F_STOP_PARENT,AT_AFTER,0),EFI_DEVICE_ERROR,1);
    FailureCheckpoint("disconnect-controller","disconnect");
    FailureField("config-attempted",0);FailureField("attributes-attempted",0);
    for(TestEvidenceTransport=1;TestEvidenceTransport<=4;TestEvidenceTransport++) {
      Check(Stage1Case(F_ATTRIBUTES,AT_AFTER,8)==EFI_NOT_READY && TestDisconnects==1 && !ClosedMarker,"diagnostic transport/reentry keeps the first failure and never retries");
      if(TestEvidenceTransport==4) {
        FailureCheckpoint("verify-attributes-match","post-disconnect");FailureField("attributes-observed",0x200);
      } else Check(!strstr(TestReport,"stage1-failure-record-complete"),"incomplete diagnostic record cannot claim completion");
    }
    TestEvidenceTransport=0;
    /* Characterise an acknowledged limitation, not a success oracle. The
     * sidecar payload is checked byte-for-byte by PilotWrite in both runs. */
    Check(Stage1Case(F_NONE,AT_SETUP,0)==EFI_SUCCESS && ClosedMarker==1,"late-close comparison has a successful positive control");
    strcpy(attributes,TestReport);
    Check(Stage1Case(F_PARENT_CLOSE,AT_AFTER,0)==EFI_DEVICE_ERROR && ClosedMarker==1,"late parent close fails despite an existing primary-close marker");
    Check(!strcmp(attributes,TestReport),"late-close failure can retain the same primary report and sidecar as success");
  }
  printf("PASS Stage 1 safety/evidence regressions: %d cases (%s)\n",Stage1Runs-runs,selection);
}
