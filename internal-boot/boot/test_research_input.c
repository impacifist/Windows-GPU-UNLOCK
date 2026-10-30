/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual input and console-flow policy against bounded RAM-only callbacks. */
#include "research-input.h"
#include "console-flow.h"
#include <stdio.h>
#include <stdlib.h>

static int Scenario,Reads,Stalls,Resets,Logs,Checks;
static EFI_SYSTEM_TABLE Table;
static void Check(int good,const char *why) {
  Checks++;if(!good){fprintf(stderr,"FAIL research input: %s scenario=%d\n",why,Scenario);exit(1);}
}
static EFI_STATUS EFIAPI Reset(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *input,BOOLEAN extended) {
  (void)input;(void)extended;Resets++;Reads=0;
  return Scenario==1?EFI_DEVICE_ERROR:Scenario==2?EFI_WARN_UNKNOWN_GLYPH:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Read(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *input,EFI_INPUT_KEY *key) {
  (void)input;Reads++;*key=(EFI_INPUT_KEY){0};
  if(Scenario==3)return EFI_DEVICE_ERROR;
  if(Scenario==4)return EFI_WARN_UNKNOWN_GLYPH;
  if(Scenario==5 || Scenario==7 || Scenario==8)return EFI_NOT_READY;
  if(Scenario==6){key->ScanCode=0x17;return EFI_SUCCESS;}
  if(Reads==1){key->UnicodeChar='T';return EFI_SUCCESS;}
  if(Reads<3)return EFI_NOT_READY;
  key->UnicodeChar='\r';return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Stall(UINTN us) {
  Check(us==100000,"bounded poll interval");Stalls++;
  return Scenario==7?EFI_DEVICE_ERROR:Scenario==8?EFI_WARN_UNKNOWN_GLYPH:EFI_SUCCESS;
}
static EFI_STATUS Log(VOID *ctx,const char *event,EFI_STATUS status) {
  (void)ctx;(void)event;(void)status;Logs++;
  return (Scenario==9 || (Scenario==10 && Logs==2))?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
typedef struct {unsigned detaches,boots,captures,verifies;} FLOW;
static EFI_STATUS Prepare(VOID *ctx){(void)ctx;return EFI_SUCCESS;}
static EFI_STATUS Choose(VOID *ctx,BOOLEAN *skip){(void)ctx;return GuResearchChooseInput(&Table,skip);}
static EFI_STATUS Capture(VOID *ctx){((FLOW *)ctx)->captures++;return EFI_SUCCESS;}
static EFI_STATUS Detach(VOID *ctx){((FLOW *)ctx)->detaches++;return EFI_SUCCESS;}
static EFI_STATUS Verify(VOID *ctx){((FLOW *)ctx)->verifies++;return EFI_SUCCESS;}
static EFI_STATUS Windows(VOID *ctx){((FLOW *)ctx)->boots++;return EFI_SUCCESS;}
int main(void) {
  EFI_BOOT_SERVICES boot={0};EFI_SIMPLE_TEXT_INPUT_PROTOCOL keys={0};
  boot.Stall=Stall;keys.Reset=Reset;keys.ReadKeyStroke=Read;Table.BootServices=&boot;Table.ConIn=&keys;
  for(Scenario=0;Scenario<=10;Scenario++) {
    Reads=Stalls=Resets=Logs=0;
    EFI_STATUS status=GuResearchConfirmInput(&Table,Log,NULL);
    int success=Scenario==0 || Scenario==5 || Scenario==6 || Scenario==7 || Scenario==8;
    Check(success==(status==EFI_SUCCESS),"single probe accepts no key and rejects read/reset/log errors or warnings");
    Check(Resets==(Scenario==9?0:1),"queued input reset before post-detach read");
    if(success)Check(Reads==1 && !Stalls && Logs==2,"one probe without waiting for a person");
  }
  for(Scenario=0;Scenario<=8;Scenario++) {
    Reads=Stalls=Resets=Logs=0;FLOW flow={0};
    GU_CONSOLE_FLOW_OPS ops={&flow,Prepare,Choose,Capture,Detach,Prepare,Verify,Windows,Log};
    EFI_STATUS status=GuConsoleFlow(&ops);
    if(Scenario==0 || Scenario==5) {
      Check(status==EFI_ABORTED && flow.detaches==1 && flow.boots==1 && Stalls==50,"bounded automatic selection executes once");
      Check(flow.captures==2 && flow.verifies==1,"transaction capture and final verification preserved");
    } else if(Scenario==6)Check(status==EFI_ABORTED && !flow.detaches && flow.boots==1 && !flow.captures,"Esc boots without mutation");
    else Check(status!=EFI_SUCCESS && !flow.detaches && !flow.boots && !flow.captures,"selection errors and warnings cannot enter mutation");
  }
  printf("PASS research input policy: %d checks, 20 scenarios; RAM-only\n",Checks);return 0;
}
