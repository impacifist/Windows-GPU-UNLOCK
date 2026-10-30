/* GPL-2.0-only. Optional bounded inventory in the generic diagnostic only. */
#include "probe-parent.h"
#if !defined(GU_DIAGNOSTIC_ONLY) || !defined(GU_DIAGNOSTIC_PROBE) || !defined(GU_PROBE_GENERIC) || defined(GU_FULL_UNLOCK) || defined(GU_EXPERIMENTAL_3090) || defined(GU_UEFI_PORT)
#error Parent baseline is restricted to the generic read-only diagnostic
#endif
VOID Text(CAPTURE *,const char *);
VOID Hex(CAPTURE *,UINT64,UINTN);
VOID End(CAPTURE *);
static EFI_GUID PathGuid=EFI_DEVICE_PATH_PROTOCOL_GUID;
static BOOLEAN Active;
static VOID Device(CAPTURE *c,const GU_DS_DEVICE *d,UINTN index) {
  Text(c,"parent-baseline-device index=0x");Hex(c,index,2);
  Text(c," segment=0x");Hex(c,d->Segment,4);Text(c," bus=0x");Hex(c,d->Bus,2);
  Text(c," device=0x");Hex(c,d->Device,2);Text(c," function=0x");Hex(c,d->Function,2);
  Text(c," attributes=0x");Hex(c,d->Attributes,16);Text(c," supported=0x");Hex(c,d->Supported,16);End(c);
  Text(c,"parent-baseline-config index=0x");Hex(c,index,2);Text(c," words=");
  for(UINTN i=0;i<64;i++){Hex(c,d->Config[i],8);Text(c," ");}End(c);
}
EFI_STATUS GuProbeParentBaseline(CAPTURE *c,GU_DS_CAPTURE *observation,EFI_HANDLE handle,
  EFI_PCI_IO_PROTOCOL *io,const UINT32 *cfg,UINTN segment,UINTN bus) {
  if(!c || !observation || !cfg || !handle || Active)return EFI_INVALID_PARAMETER;
  if(c->Io!=EFI_SUCCESS)return c->Io;
  Active=TRUE;GU_PROBE_PIN pin;EFI_STATUS s=EFI_UNSUPPORTED;
  BOOLEAN pinned=GuProbeTakePin(c,io,&pin);GU_DS_CAPTURE_TARGET target={0};
  if(pinned && pin.Locate) {
    EFI_DEVICE_PATH_PROTOCOL *path=NULL;s=pin.Handle(handle,&PathGuid,(VOID **)&path);
    if(s==EFI_SUCCESS && path && GuProbePinSame(c,&pin)) {
      const UINT8 *p=(const VOID *)path;UINTN at=0;
      s=EFI_UNSUPPORTED;
      while(at+4<=GU_DS_EFI_MAX_PATH) {
        UINTN n=p[at+2]+((UINTN)p[at+3]<<8);
        if(n<4 || n>GU_DS_EFI_MAX_PATH-at)break;
        for(UINTN j=0;j<n;j++)target.Path[at+j]=p[at+j];
        at+=n;
        if(p[at-n]==0x7f) {
          target.PathBytes=at;target.Handle=handle;target.Segment=segment;target.Bus=bus;
          target.PciId=cfg[0];target.Subsystem=cfg[11];target.Bar0=cfg[4];
          s=GuDsEfiCapture(observation,pin.Boot,&target);
          if(s==EFI_SUCCESS)for(UINTN j=0;j<64;j++)if(observation->Snapshot.Gpu.Config[j]!=cfg[j]){s=EFI_NOT_READY;break;}
          break;
        }
      }
    }
  }
  BOOLEAN available=s==EFI_SUCCESS && observation->Ready && pinned && GuProbePinSame(c,&pin);
  Text(c,"parent-baseline status=0x");Hex(c,s,16);Text(c," available=");Hex(c,available?1:0,1);
  Text(c," parents=0x");Hex(c,available?observation->Snapshot.ParentCount:0,2);End(c);
  if(available) {
    Device(c,&observation->Snapshot.Gpu,0);
    for(UINTN i=0;i<observation->Snapshot.ParentCount && c->Io==EFI_SUCCESS;i++)Device(c,&observation->Snapshot.Parents[i],i+1);
  }
  /* Logging may flush and run firmware callbacks too. An unavailable optional
   * observation never excuses calling changed capabilities in the deep probe. */
  BOOLEAN unchanged=pinned && GuProbePinSame(c,&pin);Active=FALSE;
  return unchanged?(c->Io==EFI_SUCCESS?s:c->Io):EFI_COMPROMISED_DATA;
}
