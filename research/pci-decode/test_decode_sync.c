/* GPL-2.0-only. RAM-only adversarial callbacks; no hardware services are linked. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef NULL
#include "decode-sync.h"

static unsigned Checks;
static void Check(int ok,const char *label) {
  Checks++;if(!ok){fprintf(stderr,"FAIL %s\n",label);exit(1);}
}
typedef struct {
  GU_DS_SNAPSHOT State,Expected;
  GU_DS_TRANSACTION Transaction;
  GU_DS_OPS Ops;
  unsigned Reads,Logs,Disables,ReadFault,Incomplete,LogFault,LogDrift,DriftRead,DriftDevice,DriftWord;
  unsigned DriftField,Behavior,Reenter,FreezeInputs,NonbooleanLog,FaultWithOutput;
  EFI_STATUS ReadStatus,DisableStatus;
} MODEL;
static void Reenter(MODEL *m) {
  if(m->Reenter)Check(GuDecodeSync(&m->Transaction,&m->Ops,&m->Expected)==GU_DS_REUSED,"reentry refuses before another callback");
}
static EFI_STATUS Read(VOID *context,GU_DS_SNAPSHOT *out) {
  MODEL *m=context;m->Reads++;Reenter(m);
  if(m->ReadFault==m->Reads && !m->FaultWithOutput)return m->ReadStatus;
  *out=m->State;
  if(m->Incomplete==m->Reads)out->Complete=FALSE;
  if(m->DriftRead==m->Reads) {
    GU_DS_DEVICE *d=m->DriftDevice?&out->Parents[m->DriftDevice-1]:&out->Gpu;
    switch(m->DriftField) {
      case 0:d->Config[m->DriftWord]^=1U;break;
      case 1:d->Attributes^=0x80000000ULL;break;
      case 2:d->Supported^=0x80000000ULL;break;
      case 3:d->Segment^=1U;break;
      case 4:d->Bus^=1U;break;
      case 5:d->Device^=1U;break;
      case 6:d->Function^=1U;break;
      case 7:d->Handle^=1U;break;
      case 8:d->Interface^=1U;break;
      case 9:out->ParentCount++;break;
    }
  }
  return m->ReadFault==m->Reads?m->ReadStatus:EFI_SUCCESS;
}
static EFI_STATUS Disable(VOID *context) {
  MODEL *m=context;m->Disables++;Reenter(m);
  Check(m->Transaction.Attempted,"attempt recorded before write callback");
  if(m->Behavior!=1)m->State.Gpu.Attributes&=~GU_DS_IO;
  if(m->Behavior==2)m->State.Gpu.Config[1]|=1U;
  if(m->Behavior==3)m->State.Parents[0].Config[63]^=1U;
  if(m->Behavior==4)m->State.Gpu.Attributes|=GU_DS_MASTER;
  if(m->Behavior==5)m->State.Gpu.Attributes|=0x100000000ULL;
  return m->DisableStatus;
}
static BOOLEAN Log(VOID *context,const char *event) {
  MODEL *m=context;m->Logs++;Check(event!=NULL,"log event exists");Reenter(m);
  if(m->FreezeInputs) {
    m->Expected.Gpu.Config[20]^=1U;
    m->Ops.Read=NULL;m->Ops.DisableIo=NULL;m->Ops.Log=NULL;
  }
  if(m->LogDrift==m->Logs)m->State.Parents[0].Config[20]^=1U;
  return m->NonbooleanLog==m->Logs?2:m->LogFault!=m->Logs;
}
static void Setup(MODEL *m,unsigned parents,UINTN segment) {
  memset(m,0,sizeof(*m));m->State.Complete=TRUE;m->State.ParentCount=parents;
  GU_DS_DEVICE *g=&m->State.Gpu;
  g->Handle=0x300;g->Interface=0x400;g->Segment=segment;g->Bus=parents;
  g->Config[0]=0x220410de;g->Config[1]=0x00100002;g->Config[2]=0x030000a1;
  g->Config[4]=0xf5000000;g->Config[11]=0x403a1458;g->Attributes=0x300;g->Supported=0x700;
  for(unsigned i=0;i<parents;i++) {
    GU_DS_DEVICE *p=&m->State.Parents[i];
    p->Handle=0x100+i;p->Interface=0x200+i;p->Segment=segment;p->Bus=i;
    p->Config[0]=0x14831022;p->Config[1]=7;p->Config[2]=0x06040000;p->Config[3]=0x00010000;
    p->Config[6]=i|((i+1)<<8)|(parents<<16);p->Attributes=p->Supported=0x700;
  }
  m->Expected=m->State;m->Ops=(GU_DS_OPS){m,Read,Disable,Log};
}
static GU_DS_RESULT Run(MODEL *m) {return GuDecodeSync(&m->Transaction,&m->Ops,&m->Expected);}
static void Refuse(MODEL *m,unsigned writes) {
  GU_DS_RESULT result=Run(m);
  Check(result!=GU_DS_FIXED && result!=GU_DS_ALREADY_CONSISTENT,"unsafe snapshot or callback result rejected");
  Check(m->Disables==writes,"refusal makes only expected bounded Disable calls");
  Check(m->Reads<=3+GU_DS_FOLLOWUPS,"bounded observations");
  unsigned before=m->Disables,reads=m->Reads,logs=m->Logs;
  Check(Run(m)==GU_DS_REUSED,"same transaction cannot retry");
  Check(m->Disables==before && m->Reads==reads && m->Logs==logs,"retry cannot call firmware or log");
}
int main(void) {
  MODEL m;
  const UINT32 cards[]={0x220410de,0x220310de,0x248410de,0x248710de,0x250410de};
  for(unsigned parents=1;parents<=GU_DS_MAX_PARENTS;parents++)for(unsigned seg=0;seg<3;seg++) {
    Setup(&m,parents,seg==2?0xffffU:seg);m.Reenter=1;
    m.Expected.Gpu.Config[0]=m.State.Gpu.Config[0]=cards[parents%5];
    Check(Run(&m)==GU_DS_FIXED,"valid bounded ancestry and captured segment synchronize");
    Check(m.Disables==1 && m.Reads==3+GU_DS_FOLLOWUPS && m.Logs==1+GU_DS_FOLLOWUPS,"success has one write and fixed callback bounds");
    Check(m.Transaction.Attempted && m.Transaction.PostObserved && m.State.Gpu.Attributes==0x200 &&
      m.State.Gpu.Config[1]==0x00100002,"only cached IO clears, raw command preserved");
    Check(Run(&m)==GU_DS_REUSED && m.Disables==1,"successful transaction cannot run again");
  }
  Setup(&m,2,1);m.State.Gpu.Attributes=m.Expected.Gpu.Attributes=0x200;
  Check(Run(&m)==GU_DS_ALREADY_CONSISTENT && !m.Disables && m.Reads==2,"consistent state has no write");
  Setup(&m,2,0);m.FreezeInputs=1;
  Check(Run(&m)==GU_DS_FIXED && m.Disables==1,"caller policy and callbacks frozen before log can alter them");
  for(unsigned at=1;at<=3+GU_DS_FOLLOWUPS;at++)for(unsigned kind=0;kind<5;kind++) {
    Setup(&m,2,0);m.ReadFault=at;m.FaultWithOutput=kind>=3;
    m.ReadStatus=kind==2?EFI_SUCCESS:(kind==1 || kind==4)?EFI_WARN_UNKNOWN_GLYPH:EFI_DEVICE_ERROR;
    Refuse(&m,at<=2?0:1);
    Setup(&m,2,0);m.Incomplete=at;Refuse(&m,at<=2?0:1);
  }
  for(unsigned at=1;at<=1+GU_DS_FOLLOWUPS;at++) {
    Setup(&m,2,0);m.LogFault=at;Refuse(&m,at==1?0:1);
    Setup(&m,2,0);m.LogDrift=at;Refuse(&m,at==1?0:1);
    Setup(&m,2,0);m.NonbooleanLog=at;Refuse(&m,at==1?0:1);
  }
  for(unsigned behavior=1;behavior<=5;behavior++) {
    Setup(&m,2,0);m.Behavior=behavior;Refuse(&m,1);
  }
  for(unsigned warning=0;warning<2;warning++) {
    Setup(&m,2,0);m.DisableStatus=warning?EFI_WARN_UNKNOWN_GLYPH:EFI_DEVICE_ERROR;Refuse(&m,1);
    Check(m.Transaction.Attempted && m.Transaction.PostObserved && m.Reads==3 && m.State.Gpu.Attributes==0x200,
      "failing write may change cache; record one post-observation without retry");
    Setup(&m,2,0);m.DisableStatus=EFI_DEVICE_ERROR;m.ReadFault=3;m.ReadStatus=EFI_DEVICE_ERROR;Refuse(&m,1);
    Check(!m.Transaction.PostObserved && m.Transaction.ObservationStatus==EFI_DEVICE_ERROR,"unreadable failure is not absence of mutation");
  }
  /* Every dword of the GPU and all 16 ancestors, both before write and in each
   * follow-up. This catches late bridge changes hidden by command-only checks. */
  for(unsigned at=2;at<=3+GU_DS_FOLLOWUPS;at++)for(unsigned device=0;device<=GU_DS_MAX_PARENTS;device++)for(unsigned word=0;word<64;word++) {
    Setup(&m,GU_DS_MAX_PARENTS,0xffff);m.DriftRead=at;m.DriftDevice=device;m.DriftWord=word;
    Refuse(&m,at==2?0:1);
  }
  for(unsigned field=1;field<=9;field++)for(unsigned device=0;device<=GU_DS_MAX_PARENTS;device++)for(unsigned at=2;at<=3+GU_DS_FOLLOWUPS;at++) {
    Setup(&m,GU_DS_MAX_PARENTS,1);m.DriftRead=at;m.DriftDevice=device;m.DriftField=field;
    Refuse(&m,at==2?0:1);
  }
  for(unsigned bad=0;bad<28;bad++) {
    Setup(&m,2,0);GU_DS_DEVICE *g=&m.Expected.Gpu,*p=&m.Expected.Parents[0];
    switch(bad) {
      case 0:m.Expected.ParentCount=0;break;
      case 1:m.Expected.ParentCount=GU_DS_MAX_PARENTS+1;break;
      case 2:m.Expected.Complete=FALSE;break;
      case 3:g->Segment=0x10000;break;
      case 4:g->Bus=256;break;
      case 5:g->Device=1;break;
      case 6:g->Function=1;break;
      case 7:g->Config[1]|=1;break;
      case 8:g->Config[1]|=4;break;
      case 9:g->Config[4]|=4;break;
      case 10:g->Attributes|=0x100000000ULL;break;
      case 11:g->Supported=0;break;
      case 12:g->Config[0]=0xdead10de;break;
      case 13:g->Config[2]^=1;break;
      case 14:g->Config[11]=0xffffffff;break;
      case 15:p->Segment=1;break;
      case 16:p->Handle=g->Handle;break;
      case 17:p->Interface=g->Interface;break;
      case 18:p->Handle=g->Interface;break;
      case 19:p->Device=32;break;
      case 20:p->Config[3]=0;break;
      case 21:p->Config[6]^=1;break;
      case 22:p->Attributes=0x300;break;
      case 23:m.Expected.Parents[1]=*p;break;
      case 24:m.Expected.Parents[1].Config[6]=0;break;
      case 25:g->Handle=0;break;
      case 26:g->Supported=~0ULL;break;
      case 27:p->Config[1]=0xffff;p->Attributes=0x700;break;
    }
    Refuse(&m,0);Check(!m.Reads && !m.Logs,"invalid policy rejected before any callback");
  }
  Setup(&m,2,0);m.Ops.Log=NULL;Refuse(&m,0);
  Setup(&m,2,0);m.Ops.Read=NULL;Refuse(&m,0);
  Setup(&m,2,0);m.Ops.DisableIo=NULL;Refuse(&m,0);
  /* Unique BDFs can still form a bus cycle: 2:0 -> 1:0 -> 2:1 -> GPU bus 3. */
  Setup(&m,3,0);
  m.State.Parents[0].Bus=2;m.State.Parents[0].Config[6]=2U|(1U<<8)|(3U<<16);
  m.State.Parents[1].Bus=1;m.State.Parents[1].Config[6]=1U|(2U<<8)|(3U<<16);
  m.State.Parents[2].Bus=2;m.State.Parents[2].Device=1;m.State.Parents[2].Config[6]=2U|(3U<<8)|(3U<<16);
  m.Expected=m.State;Refuse(&m,0);Check(!m.Reads && !m.Logs,"duplicate parent bus rejected before callbacks");
  Setup(&m,3,0);
  m.State.Parents[0].Bus=3;m.State.Parents[0].Device=1;
  m.State.Parents[0].Config[6]=3U|(1U<<8)|(3U<<16);
  m.Expected=m.State;Refuse(&m,0);Check(!m.Reads && !m.Logs,"GPU bus cannot recur as an ancestor");
  Check(GuDecodeSync(NULL,NULL,NULL)==GU_DS_INVALID,"null transaction rejected");
  Check(!GuDecodeSnapshotValid(NULL),"null snapshot predicate rejected");
  Setup(&m,2,0);
  Check(!GuDecodeSnapshotMatches(NULL,&m.Expected,FALSE) && !GuDecodeSnapshotMatches(&m.Expected,NULL,FALSE),"null comparison rejected");
  Check(!GuDecodeSnapshotMatches(&m.Expected,&m.Expected,2),"nonboolean comparison mode rejected");
  m.Expected.ParentCount=GU_DS_MAX_PARENTS+1;
  Check(!GuDecodeSnapshotMatches(&m.Expected,&m.Expected,FALSE),"oversized comparison rejected before array reads");
  printf("PASS %u decode-sync assertions; RAM-only, research core, no EFI or hardware execution\n",Checks);
  return 0;
}
