/* SPDX-License-Identifier: GPL-2.0-only */
/* Reuse the complete RAM service model, but never enter the release stage. */
#define main AggregateFullReplayMain
#include "test_aggregate_full.c"
#undef main

static unsigned PrepareFault;
static EFI_STATUS PrepareLog(VOID *context,const char *event,EFI_STATUS status) {
  (void)context;(void)status;
  if(!strcmp(event,"console-prepare-preflight")) {
    if(PrepareFault==10)return EFI_DEVICE_ERROR;
    if(PrepareFault==11)return EFI_WARN_UNKNOWN_GLYPH;
  }
  if(!strcmp(event,"stage2a-success-no-unlock-no-gop-removal")) {
    if(PrepareFault==12)return EFI_DEVICE_ERROR;
    if(PrepareFault==13)return EFI_WARN_UNKNOWN_GLYPH;
    if(PrepareFault==14)Input.WaitForKey=NULL;
  }
  return EFI_SUCCESS;
}
static void Damage(unsigned fault) {
  switch(fault) {
    case 1:Config[0]^=0x10000;break;
    case 2:Bindings[2].Stop=(EFI_DRIVER_BINDING_STOP)(ComboMemory+GU_COMBO_STOP+1);break;
    case 3:Bindings[3].Version++;break;
    case 4:PhysicalMode.FrameBufferBase=0;break;
    case 5:DisplayPaths[0][17]^=1;break;
    case 6:Input.WaitForKey=NULL;break;
    case 7:Live.Edges[0].Count++;break;
    case 8:Table.StandardErrorHandle=H(PCI);break;
    case 9:Edge(PRIMARY,OBSERVER,PRIMARY,&GopGuid,EFI_OPEN_PROTOCOL_BY_DRIVER);break;
    case 15:ParentDrift=TRUE;break;
    case 16:ChangePathLength(FALSE);break;
    case 17:ChangePathLength(TRUE);break;
    case 18:SegmentDrift=TRUE;break;
  }
}
int main(void) {
  Mode=0;
  for(PrepareFault=0;PrepareFault<=18;PrepareFault++) {
    Setup();Damage(PrepareFault);
    UINTN original_count=Live.Count;
    EFI_STATUS status=GuPrepareConsole(&Table,PrepareLog,NULL);
    Check((status==EFI_SUCCESS)==(PrepareFault==0),"aggregate prepare accepts only intact graph and successful durable logs");
    Check(!Calls && !EdidCalls && !NvCalls && !Enables && !Claims && !Closes,
          "prepare never detaches, enables or claims the GPU");
    Check(Live.Count==original_count,"prepare preserves every graph edge");
    if(!PrepareFault)Check(GuVerifyPreparedConsole(&Table)==EFI_SUCCESS,"fresh prepared graph verifies");
    else Check(GuVerifyPreparedConsole(&Table)!=EFI_SUCCESS,"failed prepare cannot leave reusable prepared state");
  }
  PrepareFault=0;
  for(unsigned late=1;late<=9;late++) {
    Setup();Check(GuPrepareConsole(&Table,PrepareLog,NULL)==EFI_SUCCESS,"prepare before late drift");
    Damage(late);
    Check(GuVerifyPreparedConsole(&Table)!=EFI_SUCCESS,"late graph, driver, route, framebuffer and input drift rejected");
    Check(!Calls && !Enables && !Claims && !Closes,"late refusal has no hardware mutation");
  }
  printf("PASS aggregate prepare: %u checks, %u children, 28 scenarios; RAM-only, no detach\n",Checks,TEST_CHILDREN);
  return 0;
}
