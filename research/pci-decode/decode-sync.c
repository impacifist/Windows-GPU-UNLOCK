/* GPL-2.0-only. Independent research implementation informed by issues
 * impacifist/Windows-GPU-UNLOCK #102/#121/#129. No EFI entry point, discovery,
 * disk/boot changes, MMIO, generic Attributes or PCI-write capability exists here. */
#include "decode-sync.h"

static EFI_STATUS Read(const GU_DS_OPS *o,GU_DS_SNAPSHOT *s) {
  /* A missing/partial output cannot reuse a preceding successful observation. */
  *s=(GU_DS_SNAPSHOT){0};
  return o->Read(o->Context,s);
}
static GU_DS_RESULT Finish(GU_DS_TRANSACTION *t,GU_DS_RESULT r) {t->Result=r;return r;}

GU_DS_RESULT GuDecodeSync(GU_DS_TRANSACTION *t,const GU_DS_OPS *ops,const GU_DS_SNAPSHOT *expected) {
  if(!t)return GU_DS_INVALID;
  if(t->Started!=FALSE)return GU_DS_REUSED;
  t->Started=TRUE;t->Attempted=t->PostObserved=FALSE;
  t->DisableStatus=t->ObservationStatus=EFI_NOT_STARTED;t->Result=GU_DS_INVALID;
  if(!ops || !expected || !ops->Read || !ops->DisableIo || !ops->Log)return t->Result;
  /* Freeze both the policy and capabilities before calling caller/firmware code. */
  const GU_DS_OPS o=*ops;
  const GU_DS_SNAPSHOT pin=*expected;
  GU_DS_SNAPSHOT now;
  if(!GuDecodeSnapshotValid(&pin))return t->Result;
  EFI_STATUS status=Read(&o,&now);
  if(status!=EFI_SUCCESS)return Finish(t,GU_DS_READ_FAILED);
  if(!GuDecodeSnapshotMatches(&now,&pin,FALSE))return Finish(t,GU_DS_DRIFT);
  BOOLEAN consistent=pin.Gpu.Attributes==GU_DS_MEMORY;
  if(o.Log(o.Context,consistent?"decode-already-consistent":"decode-disable-io-intent")!=TRUE)return Finish(t,GU_DS_LOG_FAILED);
  /* A logging callback can change hardware. No callback is allowed between this
   * fresh full comparison and the single permitted write. */
  status=Read(&o,&now);
  if(status!=EFI_SUCCESS)return Finish(t,GU_DS_READ_FAILED);
  if(!GuDecodeSnapshotMatches(&now,&pin,FALSE))return Finish(t,GU_DS_DRIFT);
  if(consistent)return Finish(t,GU_DS_ALREADY_CONSISTENT);
  t->Attempted=TRUE;
  t->DisableStatus=o.DisableIo(o.Context);
  /* Even a failing write may have changed firmware state. Preserve a separate
   * bounded observation, never retry, enable, roll back or start another image. */
  t->ObservationStatus=Read(&o,&now);
  t->PostObserved=t->ObservationStatus==EFI_SUCCESS && now.Complete==TRUE;
  if(t->DisableStatus!=EFI_SUCCESS)return Finish(t,GU_DS_DISABLE_FAILED);
  if(t->ObservationStatus!=EFI_SUCCESS)return Finish(t,GU_DS_READ_FAILED);
  if(!GuDecodeSnapshotMatches(&now,&pin,TRUE))return Finish(t,GU_DS_DRIFT);
  /* These are logical samples, not timed stability evidence. Any future EFI
   * adapter needs its own freshness, ownership and physical validation. */
  for(UINTN i=0;i<GU_DS_FOLLOWUPS;i++) {
    if(o.Log(o.Context,"decode-verify-full-snapshot")!=TRUE)return Finish(t,GU_DS_LOG_FAILED);
    status=Read(&o,&now);
    if(status!=EFI_SUCCESS)return Finish(t,GU_DS_READ_FAILED);
    if(!GuDecodeSnapshotMatches(&now,&pin,TRUE))return Finish(t,GU_DS_DRIFT);
  }
  return Finish(t,GU_DS_FIXED);
}
