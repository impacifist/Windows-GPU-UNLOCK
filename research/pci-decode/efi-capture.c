/* GPL-2.0-only. Only Locate/HandleProtocol, GetLocation, Read64 and Get/Supported.
 * Caller retains interface lifetimes and serializes operations. No ownership,
 * allocation, MMIO, PCI writes, logging, boot or device correction capability. */
#include "efi-capture.h"
#include "efi-path.h"

typedef struct {
  EFI_BOOT_SERVICES *Boot;
  EFI_HANDLE_PROTOCOL Handle;
  EFI_LOCATE_DEVICE_PATH Locate;
  GU_DS_CAPTURE_TARGET Target;
  UINTN Count,Bound;
  GU_DS_EFI_DEVICE Devices[1+GU_DS_EFI_MAX_PARENTS];
} WORK;
/* Bounded per-image scratch avoids firmware stack probing. This is a reentry
 * guard, not a multiprocessor lock; callers still serialize operations. */
static WORK ActiveWork;
static GU_DS_SNAPSHOT ActiveSnapshots[2];
static BOOLEAN ActiveBusy;
static EFI_GUID PciGuid=EFI_PCI_IO_PROTOCOL_GUID,PathGuid=EFI_DEVICE_PATH_PROTOCOL_GUID;
static BOOLEAN Same(const VOID *a,const VOID *b,UINTN n) {
  const UINT8 *x=a,*y=b;for(UINTN i=0;i<n;i++)if(x[i]!=y[i])return FALSE;return TRUE;
}
static BOOLEAN Overlap(const VOID *a,UINTN an,const VOID *b,UINTN bn) {
  UINTN x=(UINTN)a,y=(UINTN)b;return x<=y?y-x<an:x-y<bn;
}
static BOOLEAN Caps(const WORK *w) {
  if(w->Boot->HandleProtocol!=w->Handle || w->Boot->LocateDevicePath!=w->Locate)return FALSE;
  for(UINTN i=0;i<w->Bound;i++) {
    const GU_DS_EFI_DEVICE *d=&w->Devices[i];
    if(d->Pin.Io->GetLocation!=d->Location || d->Pin.Io->Pci.Read!=d->Read || d->Pin.Io->Attributes!=d->Attributes)return FALSE;
  }
  return TRUE;
}
static EFI_STATUS Resolve(WORK *w,UINTN index,BOOLEAN first) {
  GU_DS_EFI_DEVICE *d=&w->Devices[index];UINT8 copy[GU_DS_EFI_MAX_PATH];
  for(UINTN i=0;i<d->Pin.PathBytes;i++)copy[i]=d->Pin.Path[i];
  EFI_DEVICE_PATH_PROTOCOL *rest=(VOID *)copy;EFI_HANDLE h=NULL;VOID *io=NULL,*path=NULL;
  if(!Caps(w))return EFI_NOT_READY;
  EFI_STATUS s=w->Locate(&PciGuid,&rest,&h);
  if(s!=EFI_SUCCESS || !h || !Caps(w) || (UINTN)rest!=(UINTN)copy+d->Pin.PathBytes-4 ||
     !Same(copy,d->Pin.Path,d->Pin.PathBytes) || (!index && h!=w->Target.Handle) ||
     (!first && h!=d->Pin.Handle))return EFI_NOT_READY;
  s=w->Handle(h,&PciGuid,&io);
  if(s!=EFI_SUCCESS || !io || !Caps(w) || (!first && io!=d->Pin.Io))return EFI_NOT_READY;
  if(first) {
    EFI_PCI_IO_PROTOCOL *p=io;
    if(!p->GetLocation || !p->Pci.Read || !p->Attributes)return EFI_UNSUPPORTED;
    d->Pin.Handle=h;d->Pin.Io=p;d->Location=p->GetLocation;d->Read=p->Pci.Read;d->Attributes=p->Attributes;
    w->Bound=index+1;
  }
  s=w->Handle(h,&PathGuid,&path);
  if(s!=EFI_SUCCESS || !path || !Caps(w) || !Same(path,d->Pin.Path,d->Pin.PathBytes))return EFI_NOT_READY;
  return EFI_SUCCESS;
}
static EFI_STATUS ReadOne(WORK *w,UINTN index,BOOLEAN first,GU_DS_DEVICE *out) {
  GU_DS_EFI_DEVICE *d=&w->Devices[index];
  if(Resolve(w,index,first)!=EFI_SUCCESS)return EFI_NOT_READY;
  out->Handle=(UINTN)d->Pin.Handle;out->Interface=(UINTN)d->Pin.Io;
  out->Segment=out->Bus=out->Device=out->Function=~(UINTN)0;
  EFI_STATUS s=d->Location(d->Pin.Io,&out->Segment,&out->Bus,&out->Device,&out->Function);
  UINTN tail=d->Pin.PathBytes-4;
  if(s!=EFI_SUCCESS || !Caps(w) || out->Segment!=w->Target.Segment || out->Bus>255 ||
     out->Device!=d->Pin.Path[tail-1] || out->Function!=d->Pin.Path[tail-2] ||
     (!index && out->Bus!=w->Target.Bus))return EFI_NOT_READY;
  for(UINTN i=0;i<64;i++)out->Config[i]=first?0xa5a5a5a5U:0x5a5a5a5aU;
  s=d->Read(d->Pin.Io,EfiPciIoWidthUint32,0,64,out->Config);
  if(s!=EFI_SUCCESS || !Caps(w))return EFI_NOT_READY;
  if(!index && (out->Config[0]!=w->Target.PciId || out->Config[11]!=w->Target.Subsystem || out->Config[4]!=w->Target.Bar0))return EFI_NOT_READY;
  out->Attributes=out->Supported=~0ULL;
  s=d->Attributes(d->Pin.Io,EfiPciIoAttributeOperationGet,0,&out->Attributes);
  if(s!=EFI_SUCCESS || out->Attributes==~0ULL || !Caps(w))return EFI_NOT_READY;
  s=d->Attributes(d->Pin.Io,EfiPciIoAttributeOperationSupported,0,&out->Supported);
  if(s!=EFI_SUCCESS || out->Supported==~0ULL || !Caps(w))return EFI_NOT_READY;
  return Resolve(w,index,FALSE);
}
EFI_STATUS GuDsEfiCapture(GU_DS_CAPTURE *c,EFI_BOOT_SERVICES *boot,const GU_DS_CAPTURE_TARGET *target) {
  if(!c || (boot && Overlap(c,sizeof(*c),boot,sizeof(*boot))) ||
     (target && Overlap(c,sizeof(*c),target,sizeof(*target))) ||
     (boot && target && Overlap(boot,sizeof(*boot),target,sizeof(*target))))return EFI_INVALID_PARAMETER;
  if(c->Used!=FALSE || ActiveBusy!=FALSE)return EFI_ALREADY_STARTED;
  ActiveBusy=TRUE;
  c->Used=TRUE;c->Busy=TRUE;c->Ready=FALSE;c->Count=0;
  for(UINTN i=0;i<1+GU_DS_EFI_MAX_PARENTS;i++)c->Bindings[i]=(GU_DS_EFI_BINDING){0};
  c->Snapshot=(GU_DS_SNAPSHOT){0};c->Status=EFI_INVALID_PARAMETER;
  EFI_STATUS s=EFI_INVALID_PARAMETER;WORK *w=&ActiveWork;GU_DS_SNAPSHOT *snapshots=ActiveSnapshots;UINTN parents=0;
  *w=(WORK){0};snapshots[0]=(GU_DS_SNAPSHOT){0};snapshots[1]=(GU_DS_SNAPSHOT){0};
  if(!boot || !target || !boot->HandleProtocol || !boot->LocateDevicePath)goto done;
  w->Boot=boot;w->Handle=boot->HandleProtocol;w->Locate=boot->LocateDevicePath;w->Target=*target;
  if(!w->Target.Handle || w->Target.Segment>0xffffU || !w->Target.Bus || w->Target.Bus>255 ||
     (w->Target.PciId&0xffffU)!=0x10de || !w->Target.Subsystem || w->Target.Subsystem==0xffffffffU ||
     !w->Target.Bar0 || (w->Target.Bar0&0xffffffU) || !GuDsControllerParents(w->Target.Path,w->Target.PathBytes,&parents))goto done;
  w->Count=parents+1;
  for(UINTN i=0;i<w->Count;i++) {
    GU_DS_EFI_BINDING *p=&w->Devices[i].Pin;
    UINTN body=i?12+6*i:w->Target.PathBytes-4;
    for(UINTN j=0;j<body;j++)p->Path[j]=w->Target.Path[j];
    p->Path[body]=0x7f;p->Path[body+1]=0xff;p->Path[body+2]=4;p->Path[body+3]=0;p->PathBytes=body+4;
  }
  s=EFI_NOT_READY;
  for(UINTN pass=0;pass<2;pass++) {
    GU_DS_SNAPSHOT *v=&snapshots[pass];v->ParentCount=parents;
    for(UINTN i=0;i<w->Count;i++)if(ReadOne(w,i,pass==0,i?&v->Parents[i-1]:&v->Gpu)!=EFI_SUCCESS)goto done;
    v->Complete=TRUE;
    if(!Caps(w) || !GuDecodeSnapshotValid(v))goto done;
  }
  if(!Caps(w) || !GuDecodeSnapshotMatches(&snapshots[1],&snapshots[0],FALSE))goto done;
  for(UINTN i=0;i<w->Count;i++)c->Bindings[i]=w->Devices[i].Pin;
  c->Snapshot=snapshots[1];c->Count=w->Count;c->Ready=TRUE;s=EFI_SUCCESS;
done:
  /* Nothing partial is published, even if a callback touched public outputs. */
  if(s!=EFI_SUCCESS) {
    c->Snapshot=(GU_DS_SNAPSHOT){0};c->Count=0;c->Ready=FALSE;
    for(UINTN i=0;i<1+GU_DS_EFI_MAX_PARENTS;i++)c->Bindings[i]=(GU_DS_EFI_BINDING){0};
  }
  c->Used=TRUE;c->Busy=FALSE;c->Status=s;ActiveBusy=FALSE;return s;
}
