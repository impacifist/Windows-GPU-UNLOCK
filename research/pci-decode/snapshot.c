/* GPL-2.0-only. Pure PCI snapshot predicates. No protocol or actuator calls. */
#include "snapshot.h"

/* Exact MSI PCI envelope, not firmware eligibility or ownership. */
static BOOLEAN GuDsMsi3080Ti(const GU_DS_SNAPSHOT *s) {
  const GU_DS_DEVICE *g=&s->Gpu,*p=&s->Parents[0];
  return s->ParentCount==1 && g->Segment==0 && g->Bus==0x2d &&
    g->Device==0 && g->Function==0 && g->Config[0]==0x220810deU &&
    g->Config[11]==0x88031043U && g->Config[4]==0xfa000000U &&
    p->Segment==0 && p->Bus==0 && p->Device==3 && p->Function==1 &&
    p->Config[0]==0x14831022U && p->Config[2]==0x06040000U &&
    (p->Config[6]&0xffffffU)==0x002d2d00U;
}


static BOOLEAN Location(const GU_DS_DEVICE *a,const GU_DS_DEVICE *b) {
  return a->Segment==b->Segment && a->Bus==b->Bus && a->Device==b->Device && a->Function==b->Function;
}
static BOOLEAN Bounded(const GU_DS_DEVICE *d) {
  return d->Handle && d->Interface && d->Segment<=0xffffU && d->Bus<=255U && d->Device<=31U && d->Function<=7U;
}
static UINT64 Decode(UINT32 command) {
  return (command&1?GU_DS_IO:0)|(command&2?GU_DS_MEMORY:0)|(command&4?GU_DS_MASTER:0);
}
static BOOLEAN Separate(const GU_DS_DEVICE *a,const GU_DS_DEVICE *b) {
  return !Location(a,b) && a->Handle!=b->Handle && a->Interface!=b->Interface &&
    a->Handle!=b->Interface && a->Interface!=b->Handle;
}
BOOLEAN GuDecodeSnapshotValid(const GU_DS_SNAPSHOT *s) {
  if(!s)return FALSE;
  const GU_DS_DEVICE *g=&s->Gpu;
  if(s->Complete!=TRUE || !s->ParentCount || s->ParentCount>GU_DS_MAX_PARENTS || !Bounded(g) ||
     !g->Bus || g->Device || g->Function || (g->Config[1]&0xffffU)!=2U ||
     g->Config[2]!=0x030000a1U || ((g->Config[3]>>16)&0x7fU) ||
     !g->Config[4] || (g->Config[4]&0xffffffU) ||
     !g->Config[11] || g->Config[11]==0xffffffffU || g->Supported==~0ULL || !(g->Supported&GU_DS_IO) ||
     (g->Attributes!=GU_DS_MEMORY && g->Attributes!=(GU_DS_IO|GU_DS_MEMORY)))return FALSE;
  switch(g->Config[0]) {
    case 0x220410deU:case 0x220310deU:case 0x248410deU:case 0x248710deU:case 0x250410deU:break;
    case 0x220810deU:if(!GuDsMsi3080Ti(s))return FALSE;break;
    default:return FALSE;
  }
  for(UINTN i=0;i<s->ParentCount;i++) {
    const GU_DS_DEVICE *p=&s->Parents[i];
    UINT32 buses=p->Config[6],secondary=(buses>>8)&255U,subordinate=(buses>>16)&255U;
    if(!Bounded(p) || p->Segment!=g->Segment || p->Bus==g->Bus || !Separate(p,g) ||
       !p->Config[0] || p->Config[0]==0xffffffffU || (p->Config[1]&0xffffU)==0xffffU ||
       ((p->Config[3]>>16)&0x7fU)!=1U ||
       (p->Config[2]&0xffffff00U)!=0x06040000U || (buses&255U)!=p->Bus ||
       !secondary || secondary==p->Bus || subordinate<secondary || g->Bus<secondary || g->Bus>subordinate ||
       (p->Attributes&(GU_DS_IO|GU_DS_MEMORY|GU_DS_MASTER))!=Decode(p->Config[1]))return FALSE;
    if(i && (p->Bus!=((s->Parents[i-1].Config[6]>>8)&255U) ||
             subordinate>((s->Parents[i-1].Config[6]>>16)&255U)))return FALSE;
    for(UINTN j=0;j<i;j++)if(p->Bus==s->Parents[j].Bus || !Separate(p,&s->Parents[j]))return FALSE;
    if(i+1==s->ParentCount && secondary!=g->Bus)return FALSE;
  }
  return TRUE;
}
static BOOLEAN DeviceEqual(const GU_DS_DEVICE *a,const GU_DS_DEVICE *b,BOOLEAN fixed) {
  if(!Location(a,b) || a->Handle!=b->Handle || a->Interface!=b->Interface || a->Supported!=b->Supported ||
     a->Attributes!=(fixed?(b->Attributes&~GU_DS_IO):b->Attributes))return FALSE;
  for(UINTN i=0;i<64;i++)if(a->Config[i]!=b->Config[i])return FALSE;
  return TRUE;
}
BOOLEAN GuDecodeSnapshotMatches(const GU_DS_SNAPSHOT *a,const GU_DS_SNAPSHOT *b,BOOLEAN fixed) {
  if(!a || !b || b->ParentCount>GU_DS_MAX_PARENTS || b->Complete!=TRUE || (fixed!=TRUE && fixed!=FALSE))return FALSE;
  if(a->Complete!=TRUE || a->ParentCount!=b->ParentCount || !DeviceEqual(&a->Gpu,&b->Gpu,fixed))return FALSE;
  for(UINTN i=0;i<b->ParentCount;i++)if(!DeviceEqual(&a->Parents[i],&b->Parents[i],FALSE))return FALSE;
  return TRUE;
}
