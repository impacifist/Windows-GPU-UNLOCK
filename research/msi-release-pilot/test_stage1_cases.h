/* GPL-2.0-only. Actual entry/core adversarial acceptance for Stage 1. */
#pragma once
static int Stage1Runs;
static EFI_STATUS Stage1Case(int fault,int at,int word) {
  Stage1Testing=TRUE;TestFault=fault;TestAt=at;TestWord=word;Stage1Runs++;
  EFI_STATUS s=TestPilot(60);
  Check(!EnableCalls && !ClaimCalls && !CloseCalls && !GfxStops && !ChildStops && !ParentStops,"Stage 1 never calls graphics, NVIDIA, claim or decode services");
  int before=TestDisconnects;
  Check(PilotConfirm()==EFI_ACCESS_DENIED && PilotAdvance(1)==EFI_ACCESS_DENIED && TestDisconnects==before,"terminal Stage 1 authority cannot confirm or retry");
  Check(PilotPreflight(NULL,NULL,NULL,NULL)==EFI_ACCESS_DENIED,"same-boot preflight cannot renew authority");
  Full.Captured=Full.Released=Full.Owned=Full.Restored=PreparedReady=FALSE; /* Remove only test forgeries. */
  Stage1Testing=FALSE;return s;
}
static VOID Stage1Reject(int fault,int at,int word) {
  EFI_STATUS s=Stage1Case(fault,at,word);
  if(s==EFI_SUCCESS)fprintf(stderr,"unexpected Stage1 success fault=%d at=%d word=%d\n",fault,at,word);
  Check(s!=EFI_SUCCESS,"Stage 1 adversarial state or transport fault rejects");
  if(at==AT_SETUP || at==AT_CONFIRM || at==AT_INTENT || at==AT_FLUSH || at==AT_RESTORE)Check(!TestDisconnects,"pre-effect Stage 1 fault blocks the vendor call");
  else Check(TestDisconnects==1,"post-effect Stage 1 fault contains exactly one attempt");
}
static VOID Stage1Cases(VOID) {
  static const UINT64 attrs[]={0,0x100,0x200,0x300,0x400,0x600,0x700,0x800,0x1000000000000200ULL,0x1000000000000300ULL};
  for(TestRaw=0;TestRaw<8;TestRaw++)for(UINTN a=0;a<sizeof(attrs)/sizeof(*attrs);a++) {
    TestAttributes=attrs[a];EFI_STATUS s=Stage1Case(F_NONE,AT_SETUP,0);
    BOOLEAN eligible=TestRaw==2 && (attrs[a]==0x200 || attrs[a]==0x300);
    if(eligible) {
      Check(s==EFI_SUCCESS && TestDisconnects==1 && ClosedMarker==1 && TextChildStops==1 && TextParentStops==1,"Stage 1 truth table accepts only exact pairs and one dispatch sequence");
      Check(MockAttributes==attrs[a] && !memcmp(MockConfig,FixtureFullConfig,sizeof(MockConfig)),"Stage 1 positive keeps independent complete baselines");
      Check(Current.Count+2==Initial.Count && ExpectedGraph(&PreparedPlan,&Initial,&Current,1),"Stage 1 positive removes exactly two complete ownership tuples");
      Check(TestGraphSets==2 && TestGraphRows==TestGraphExpected && TestGraphExpected==Current.Count,"both complete active graphs are reconstructable from the simulated report");
    } else {
      Check(s!=EFI_SUCCESS && !TestDisconnects && !ClosedMarker,"Stage 1 truth table denies every other pair");
      if(TestRaw!=2)Check(!TestMmio,"unsafe or unreviewed raw decode prevents dependent MMIO");
    }
  }
  TestRaw=2;TestAttributes=0x300;
  static const int initial[]={F_CONFIG_READ,F_PARTIAL_READ,F_PROM_READ,F_COLD_READ,F_BAR,F_SNAPSHOT,F_OVERFLOW,F_SNAPSHOT_BAD,F_DUP_TARGET,F_SUBSYSTEM,F_REVISION,F_LOCATION,F_BAR_RESOURCE,F_PROM_HASH,F_COLD_VALUE,F_IMAGE_SIZE,F_GUID,F_STOP_RVA,F_BINDING_IMAGE,F_COUNT,F_DUP_EDGE,F_EDGE_ATTRIBUTES,F_ADD_EDGE,F_NO_TEXT,F_NO_GOP};
  for(UINTN i=0;i<sizeof(initial)/sizeof(*initial);i++)Stage1Reject(initial[i],AT_SETUP,0);
  for(int word=0;word<64;word++) {
    Stage1Reject(F_CONFIG,AT_INTENT,word);
    Stage1Reject(F_CONFIG,AT_KEY,word);
    Stage1Reject(F_ATTRIBUTES,AT_INTENT,word);
    Stage1Reject(F_ATTRIBUTES,AT_KEY,word);
  }
  static const int timing[]={AT_CONFIRM,AT_INTENT,AT_FLUSH,AT_RESTORE,AT_DISCONNECT,AT_AFTER,AT_KEY,AT_CLOSE,AT_DONE,AT_ROOT};
  for(UINTN at=0;at<sizeof(timing)/sizeof(*timing);at++) {
    Stage1Reject(F_ATTRIBUTES,timing[at],8);
    Stage1Reject(F_INPUT,timing[at],0);
    Stage1Reject(F_ADD_EDGE,timing[at],0);
    Stage1Reject(F_NV_HASH,timing[at],at&1);
    Stage1Reject(F_MEM_OFF,timing[at],0);
    Stage1Reject(F_BME,timing[at],0);
  }
  static const int stable[]={F_TEXT_POINTER,F_NO_TEXT,F_NO_GOP,F_GOP_MODE,F_GOP_POINTER,F_ERROR_POINTER,F_INPUT_HANDLE,F_PATH,F_BINDING};
  for(UINTN i=0;i<sizeof(stable)/sizeof(*stable);i++) {
    Stage1Reject(stable[i],AT_INTENT,0);Stage1Reject(stable[i],AT_AFTER,0);
  }
  for(int c=0;c<4;c++) {
    Stage1Reject(F_CONSOLE_HASH,AT_INTENT,c);Stage1Reject(F_CONSOLE_HASH,AT_AFTER,c);
  }
  Stage1Reject(F_NV_HASH,AT_SETUP,0);Stage1Reject(F_NV_HASH,AT_SETUP,1);
  for(int flag=0;flag<5;flag++)Stage1Reject(F_FLAGS,AT_INTENT,flag);
  Stage1Reject(F_PLAN,AT_INTENT,0);
  Stage1Reject(F_BASELINE,AT_INTENT,30);Stage1Reject(F_GRAPH_BASELINE,AT_INTENT,0);
  Stage1Reject(F_DROP_EDGE,AT_AFTER,0);
  Stage1Reject(F_SUBSTITUTE,AT_AFTER,0);
  Stage1Reject(F_UNCONFIRMED,AT_CONFIRM,0);
  Stage1Reject(F_REPREFLIGHT,AT_INTENT,0);
  for(int stage=0;stage<=4;stage++)if(stage!=1)Stage1Reject(F_WRONG_STAGE,AT_CONFIRM,stage);
  Stage1Reject(F_COMPLETED,AT_KEY,0);
  static const int stops[]={F_REENTER,F_STOP_ERROR,F_STOP_PARTIAL,F_STOP_PARENT,F_STOP_WARNING,F_LINGER,F_ONE_EDGE};
  for(UINTN i=0;i<sizeof(stops)/sizeof(*stops);i++)Stage1Reject(stops[i],AT_AFTER,0);
  static const int before[]={F_OPEN,F_LOG_WRITE,F_LOG_SHORT,F_LOG_FLUSH,F_KEY_ERROR,F_STALL};
  for(UINTN i=0;i<sizeof(before)/sizeof(*before);i++)Stage1Reject(before[i],AT_SETUP,0);
  static const int after[]={F_REPORT_CLOSE,F_DONE_WRITE,F_DONE_SHORT,F_DONE_FLUSH,F_DONE_CLOSE,F_PARENT_CLOSE,F_NO_K,F_LOWER_K,F_ESC_K};
  for(UINTN i=0;i<sizeof(after)/sizeof(*after);i++)Stage1Reject(after[i],AT_AFTER,0);
  Check(Stage1Case(F_REORDER,AT_INTENT,0)==EFI_SUCCESS,"Stage 1 accepts semantically identical reordered active graph");
  GRAPH beforeGraph=Live;
  Check(SplitterStop(0,NULL)==EFI_SUCCESS && Bytes(&beforeGraph,&Live,sizeof(Live)),"Stage 1 zero-child Stop alone never removes graph records");
  Check(SplitterStop(2,FX_Console)==EFI_UNSUPPORTED && Bytes(&beforeGraph,&Live,sizeof(Live)),"Stage 1 wrong Stop count rejected by model");
  Check(SplitterStop(1,FX_RtxPci)==EFI_UNSUPPORTED && Bytes(&beforeGraph,&Live,sizeof(Live)),"Stage 1 foreign Stop child rejected by model");
  printf("PASS Stage 1 adversarial entry/core cases: %d; exact pairs, fault timing and no later effects\n",Stage1Runs);
}
