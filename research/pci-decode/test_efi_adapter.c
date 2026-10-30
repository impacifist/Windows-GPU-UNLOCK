/* GPL-2.0-only. Real EFIAPI signatures backed exclusively by RAM. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef NULL
#include "efi-adapter.h"

static unsigned Checks;
static void Check(int ok,const char *s) {Checks++;if(!ok){fprintf(stderr,"FAIL %s\n",s);exit(1);}}
typedef struct {UINTN Tag;EFI_PCI_IO_PROTOCOL Io;GU_DS_DEVICE State;UINT8 Path[GU_DS_EFI_MAX_PATH];UINTN Bytes;} DEVICE;
typedef struct {
  EFI_BOOT_SERVICES Boot;
  GU_DS_EFI_CONTEXT Adapter;
  GU_DS_TRANSACTION Transaction;
  GU_DS_EFI_BINDING Bindings[16];
  GU_DS_SNAPSHOT Expected;
  DEVICE Devices[16];
  unsigned Count,Calls,Writes,DisableAt,Logs,Reads,FaultAt,DropAt,Partial,LocateMode,Alias,Mutation,MutateAt;
  unsigned LogFault,LogDrift,Reenter,Freeze,WriteExtra,MutationDevice;
  UINTN OwnerTag;
  unsigned OwnerMode,OwnerDriftAt,OwnerQueries,Frees,PoolLive,LastOwnerQuery,LastFree,OwnerLogDrift;
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *Pool;
  UINT8 NoOutput[4096];
  EFI_STATUS FaultStatus,WriteStatus;
} MODEL;
static MODEL *M;
static EFI_GUID PciGuid=EFI_PCI_IO_PROTOCOL_GUID,PathGuid=EFI_DEVICE_PATH_PROTOCOL_GUID;
static DEVICE *Device(EFI_PCI_IO_PROTOCOL *io) {
  for(unsigned i=0;i<M->Count;i++)if(io==&M->Devices[i].Io)return &M->Devices[i];
  Check(0,"only bound interfaces used");return NULL;
}
static EFI_STATUS Tick(void) {
  M->Calls++;
  if(M->Reenter) {
    GU_DS_TRANSACTION fresh={0};
    Check(GuDsEfiRun(&M->Adapter,&fresh)==GU_DS_REUSED,"same adapter refuses reentry through a fresh transaction");
    Check(GuDsEfiInitialize(&M->Adapter,&M->Boot,(EFI_HANDLE)&M->OwnerTag,M->Bindings,M->Count,&M->Expected,NULL,NULL)==EFI_ALREADY_STARTED,
      "Init cannot reset a running adapter");
  }
  if(M->MutateAt==M->Calls) {
    DEVICE *d=&M->Devices[M->MutationDevice?M->MutationDevice:M->Count-1];
    if(M->Mutation==1)d->State.Config[63]^=1;
    if(M->Mutation==2)d->State.Segment^=1;
    if(M->Mutation==3)d->Path[8]^=1;
    if(M->Mutation==4)d->State.Attributes^=0x80000000ULL;
    if(M->Mutation==5)d->State.Supported^=0x80000000ULL;
    if(M->Mutation==6)d->Io.GetLocation=NULL;
    if(M->Mutation==7)M->Boot.HandleProtocol=NULL;
    if(M->Mutation==8)d->Io.Pci.Read=NULL;
    if(M->Mutation==9)d->Io.Attributes=NULL;
    if(M->Mutation==10)M->Boot.LocateDevicePath=NULL;
    if(M->Mutation==11)M->Boot.OpenProtocolInformation=NULL;
    if(M->Mutation==12)M->Boot.FreePool=NULL;
  }
  if(M->OwnerDriftAt==M->Calls)M->OwnerMode=2;
  return M->FaultAt==M->Calls?M->FaultStatus:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Owners(EFI_HANDLE handle,EFI_GUID *guid,EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **out,UINTN *count) {
  Check(handle==(EFI_HANDLE)&M->Devices[0].Tag && !memcmp(guid,&PciGuid,sizeof(*guid)),"ownership queries only the pinned GPU PCI protocol");
  M->OwnerQueries++;M->LastOwnerQuery=M->Calls+1;
  EFI_STATUS s=Tick();if(M->DropAt==M->Calls)return s;
  unsigned mode=M->OwnerMode;
  if(mode==1){*out=NULL;*count=0;return s;}
  if(mode==13){*out=NULL;*count=1;return s;}
  if(mode==33 || mode==34) {
    *out=(VOID *)(mode==33?(UINTN)&M->Adapter+1:~(UINTN)0-7);
    *count=GU_DS_EFI_MAX_OPENERS;return s;
  }
  if(mode>=21 && mode<=27) {
    VOID *borrowed[]={&M->Adapter,&M->Transaction,M->Bindings,&M->Expected,&M->Boot,&M->Devices[0].Io,&M->OwnerTag};
    *out=borrowed[mode-21];*count=1;return s;
  }
  Check(!M->PoolLive && !M->Pool,"only one query allocation outstanding");
  unsigned n=mode==9 || mode==10 || mode>=28?2:mode==11?4:mode==20?64:1;
  M->Pool=calloc(n,sizeof(*M->Pool));Check(M->Pool!=NULL,"mock query pool allocation");M->PoolLive++;
  *out=M->Pool;
  if(mode!=14)*count=mode==15?65:mode==16?0:n;
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *e=M->Pool;
  e[0]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){(EFI_HANDLE)&M->OwnerTag,NULL,EFI_OPEN_PROTOCOL_EXCLUSIVE,1};
  if(mode==2)e[0].AgentHandle=(EFI_HANDLE)&M->Devices[1].Tag;
  if(mode==3)e[0].ControllerHandle=handle;
  if(mode==4)e[0].Attributes=EFI_OPEN_PROTOCOL_BY_DRIVER;
  if(mode==5)e[0].Attributes=EFI_OPEN_PROTOCOL_BY_DRIVER|EFI_OPEN_PROTOCOL_EXCLUSIVE;
  if(mode==6)e[0].Attributes=EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER;
  if(mode==7)e[0].OpenCount=0;
  if(mode==8)e[0].OpenCount=2;
  if(mode==9 || mode==10){e[1]=e[0];if(mode==10)e[1].AgentHandle=handle;}
  if(mode==11)for(unsigned i=1;i<4;i++)e[i]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){handle,NULL,1U<<(i-1),i};
  if(mode==12)e[0].Attributes=0x40;
  if(mode==17)e[0]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){handle,NULL,EFI_OPEN_PROTOCOL_GET_PROTOCOL,0};
  if(mode==18)e[0].Attributes=EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL|EFI_OPEN_PROTOCOL_GET_PROTOCOL;
  if(mode==19)e[0].Attributes=EFI_OPEN_PROTOCOL_GET_PROTOCOL;
  if(mode==20)for(unsigned i=1;i<n;i++)e[i]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){handle,NULL,EFI_OPEN_PROTOCOL_GET_PROTOCOL,1};
  if(mode>=28) {
    e[1]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){handle,NULL,
      mode==28?EFI_OPEN_PROTOCOL_BY_DRIVER:mode==29?EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER:
      mode==30?(EFI_OPEN_PROTOCOL_BY_DRIVER|EFI_OPEN_PROTOCOL_EXCLUSIVE):mode==31?0x40:EFI_OPEN_PROTOCOL_GET_PROTOCOL,
      mode==32?0:1};
  }
  return s;
}
static EFI_STATUS EFIAPI Free(VOID *p) {
  Check(p && p==M->Pool && M->PoolLive==1,"only a query-owned allocation is freed exactly once");
  free(p);M->Pool=NULL;M->PoolLive=0;M->Frees++;M->LastFree=M->Calls+1;
  Check(M->Calls+1<sizeof(M->NoOutput),"mock callback trace is bounded");M->NoOutput[M->Calls+1]=1;
  return Tick();
}
static EFI_STATUS EFIAPI Handle(EFI_HANDLE h,EFI_GUID *guid,VOID **out) {
  DEVICE *d=NULL;
  for(unsigned i=0;i<M->Count;i++)if(h==(EFI_HANDLE)&M->Devices[i].Tag)d=&M->Devices[i];
  Check(d!=NULL,"known handle queried");
  EFI_STATUS s=Tick();if(M->DropAt==M->Calls)return s;
  if(!memcmp(guid,&PciGuid,sizeof(*guid)))*out=M->Alias?&M->Devices[(d==&M->Devices[0])?1:0].Io:&d->Io;
  else {Check(!memcmp(guid,&PathGuid,sizeof(*guid)),"only PCI and path protocols requested");*out=d->Path;}
  return s;
}
static EFI_STATUS EFIAPI Locate(EFI_GUID *guid,EFI_DEVICE_PATH_PROTOCOL **rest,EFI_HANDLE *h) {
  Check(!memcmp(guid,&PciGuid,sizeof(*guid)),"Locate requests PCI only");
  DEVICE *d=NULL;
  for(unsigned i=0;i<M->Count;i++)if(!memcmp(*rest,M->Devices[i].Path,M->Devices[i].Bytes))d=&M->Devices[i];
  Check(d!=NULL,"Locate receives exact pinned prefix");
  EFI_STATUS s=Tick();if(M->DropAt==M->Calls)return s;
  UINTN offset=d->Bytes-4;
  if(M->LocateMode==1)offset=0;
  if(M->LocateMode==2)offset-=6;
  if(M->LocateMode==3)offset=d->Bytes+16;
  if(M->LocateMode==4)((UINT8 *)*rest)[8]^=1;
  *h=(EFI_HANDLE)&d->Tag;*rest=(VOID *)((UINTN)*rest+offset);return s;
}
static EFI_STATUS EFIAPI Location(EFI_PCI_IO_PROTOCOL *io,UINTN *seg,UINTN *bus,UINTN *dev,UINTN *fn) {
  DEVICE *d=Device(io);EFI_STATUS s=Tick();if(M->DropAt==M->Calls)return s;
  *seg=d->State.Segment;*bus=d->State.Bus;*dev=d->State.Device;*fn=d->State.Function;return s;
}
static EFI_STATUS EFIAPI Read(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH width,UINT32 offset,UINTN count,VOID *out) {
  DEVICE *d=Device(io);Check(width==EfiPciIoWidthUint32 && !offset && count==64,"read full config only");
  M->Reads++;EFI_STATUS s=Tick();if(M->DropAt==M->Calls)return s;
  memcpy(out,d->State.Config,(M->Partial?63:64)*sizeof(UINT32));return s;
}
static EFI_STATUS EFIAPI Attributes(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION op,UINT64 mask,UINT64 *out) {
  DEVICE *d=Device(io);
  if(op==EfiPciIoAttributeOperationDisable) {
    Check(d==&M->Devices[0] && mask==GU_DS_IO && !out,"sole write is Disable(IO) on pinned GPU");
    M->Writes++;M->DisableAt=M->Calls+1;Check(M->Transaction.Attempted,"core attempt marked before firmware write");
    Check(M->PoolLive==1 && M->LastOwnerQuery==M->Calls && M->LastFree<M->LastOwnerQuery,
      "final ownership buffer is held; no FreePool callback between final query and Disable");
    d->State.Attributes&=~GU_DS_IO;
    if(M->WriteExtra)d->State.Config[1]|=4;
    EFI_STATUS s=Tick();return s==EFI_SUCCESS?M->WriteStatus:s;
  }
  Check(!mask && out && (op==EfiPciIoAttributeOperationGet || op==EfiPciIoAttributeOperationSupported),"no enable, config or BME writes");
  EFI_STATUS s=Tick();if(M->DropAt==M->Calls)return s;
  *out=op==EfiPciIoAttributeOperationGet?d->State.Attributes:d->State.Supported;return s;
}
static BOOLEAN Log(VOID *context,const char *event) {
  Check(context==M && event,"logger has frozen context");M->Logs++;
  if(M->Reenter) {
    GU_DS_TRANSACTION fresh={0};Check(GuDsEfiRun(&M->Adapter,&fresh)==GU_DS_REUSED,"log reentry blocked");
    Check(GuDsEfiInitialize(&M->Adapter,&M->Boot,(EFI_HANDLE)&M->OwnerTag,M->Bindings,M->Count,&M->Expected,Log,M)==EFI_ALREADY_STARTED,"log Init reentry blocked");
  }
  if(M->Freeze) {
    M->Adapter.Expected.Gpu.Config[20]^=1;M->Adapter.Devices[0].Pin.Path[8]^=1;
    M->Bindings[0].Path[8]^=1;M->Expected.Gpu.Config[20]^=1;
    M->Adapter.OwnerImage=NULL;
  }
  if(M->LogDrift==M->Logs)M->Devices[M->Count-1].State.Config[63]^=1;
  if(M->OwnerLogDrift==M->Logs)M->OwnerMode=2;
  return M->LogFault!=M->Logs;
}
static void Setup(MODEL *m,unsigned parents,UINTN segment) {
  memset(m,0,sizeof(*m));M=m;m->Count=parents+1;m->Boot.HandleProtocol=Handle;m->Boot.LocateDevicePath=Locate;
  m->OwnerTag=0xface;m->Boot.OpenProtocolInformation=Owners;m->Boot.FreePool=Free;
  m->Expected.Complete=TRUE;m->Expected.ParentCount=parents;
  UINT8 gpuPath[GU_DS_EFI_MAX_PATH]={2,1,12,0,0xd0,0x41,3,0x0a,42,0,0,0};
  for(unsigned i=0;i<=parents;i++) {
    UINTN at=12+6*i;gpuPath[at]=1;gpuPath[at+1]=1;gpuPath[at+2]=6;
    gpuPath[at+4]=i==parents?0:i%8;gpuPath[at+5]=i==parents?0:3+i;
  }
  static const UINT8 end[]={0x7f,0xff,4,0};
  for(unsigned i=0;i<m->Count;i++) {
    DEVICE *d=&m->Devices[i];GU_DS_DEVICE *s=&d->State;
    d->Tag=i+1;d->Io.GetLocation=Location;d->Io.Pci.Read=Read;d->Io.Attributes=Attributes;
    s->Handle=(UINTN)&d->Tag;s->Interface=(UINTN)&d->Io;s->Segment=segment;s->Bus=i?i-1:parents;
    s->Device=i?2+i:0;s->Function=i?(i-1)%8:0;
    s->Supported=0x700;s->Attributes=i?0x700:0x300;s->Config[0]=i?0x14831022:0x220410de;
    s->Config[1]=i?7:0x00100002;s->Config[2]=i?0x06040000:0x030000a1;s->Config[3]=i?0x00010000:0;
    s->Config[4]=i?0:0xf5000000;s->Config[11]=i?0:0x403a1458;
    if(i)s->Config[6]=(i-1)|(i<<8)|(parents<<16);
    UINTN prefix=12+6*(i?i:parents+1);d->Bytes=prefix+4;memcpy(d->Path,gpuPath,prefix);memcpy(d->Path+prefix,end,4);
    GU_DS_EFI_BINDING *pin=&m->Bindings[i];pin->Handle=(EFI_HANDLE)&d->Tag;pin->Io=&d->Io;
    pin->PathBytes=d->Bytes;memcpy(pin->Path,d->Path,d->Bytes);
    if(i)m->Expected.Parents[i-1]=*s;else m->Expected.Gpu=*s;
  }
}
static EFI_STATUS Init(MODEL *m) {return GuDsEfiInitialize(&m->Adapter,&m->Boot,(EFI_HANDLE)&m->OwnerTag,m->Bindings,m->Count,&m->Expected,Log,m);}
static GU_DS_RESULT Run(MODEL *m) {return GuDsEfiRun(&m->Adapter,&m->Transaction);}
static void Refuse(MODEL *m) {
  GU_DS_RESULT r=Run(m);Check(r!=GU_DS_FIXED && r!=GU_DS_ALREADY_CONSISTENT,"fault refuses successful correction");
  Check(m->Writes<=1,"at most one physical Disable callback");
  Check(!m->PoolLive && !m->Pool,"failure paths dispose every owned mock pool allocation without retry");
  unsigned n=m->Calls,w=m->Writes,l=m->Logs;GU_DS_TRANSACTION fresh={0};
  Check(GuDsEfiRun(&m->Adapter,&fresh)==GU_DS_REUSED,"new transaction cannot reuse adapter");
  Check(m->Calls==n && m->Writes==w && m->Logs==l,"reused adapter cannot call firmware");
}
int main(void) {
  MODEL m;
  for(unsigned parents=1;parents<=GU_DS_EFI_MAX_PARENTS;parents++)for(unsigned seg=0;seg<3;seg++) {
    Setup(&m,parents,seg==2?0xffff:seg);m.Reenter=1;
    if(seg==2)for(unsigned i=0;i<m.Count;i++) {
      m.Devices[i].Path[6]=m.Bindings[i].Path[6]=8;
      for(unsigned j=8;j<12;j++)m.Devices[i].Path[j]=m.Bindings[i].Path[j]=0xff;
    }
    Check(Init(&m)==EFI_SUCCESS && !m.Calls,"Init validates without firmware calls");
    Check(Run(&m)==GU_DS_FIXED && m.Writes==1,"complete immediate ancestry synchronizes through actual protocol callbacks");
    Check(m.Adapter.WriteSpent && m.Adapter.RunUsed,"adapter permanently spends run and write");
  }
  Setup(&m,1,0);Check(Init(&m)==EFI_SUCCESS,"baseline Init");Check(Run(&m)==GU_DS_FIXED,"baseline run");
  unsigned calls=m.Calls,disableAt=m.DisableAt;UINT8 outputless[4096];memcpy(outputless,m.NoOutput,sizeof(outputless));
  for(unsigned at=1;at<=calls;at++)for(unsigned warning=0;warning<2;warning++) {
    Setup(&m,1,0);Check(Init(&m)==EFI_SUCCESS,"fault fixture Init");m.FaultAt=at;
    m.FaultStatus=warning?EFI_WARN_UNKNOWN_GLYPH:EFI_DEVICE_ERROR;Refuse(&m);
    if(at<disableAt)Check(!m.Writes,"pre-write callback fault has no mutation");
  }
  for(unsigned at=1;at<=calls;at++)if(at!=disableAt && !outputless[at]) {
    Setup(&m,1,0);Check(Init(&m)==EFI_SUCCESS,"missing output fixture Init");m.DropAt=at;Refuse(&m);
    if(at<disableAt)Check(!m.Writes,"success without required output cannot authorize a write");
  }
  for(unsigned mode=1;mode<=4;mode++) {Setup(&m,2,0);Check(Init(&m)==EFI_SUCCESS,"Locate fixture Init");m.LocateMode=mode;Refuse(&m);Check(!m.Writes,"wrong consumption/path copy fails before writes");}
  Setup(&m,2,0);Check(Init(&m)==EFI_SUCCESS,"alias fixture Init");m.Alias=1;Refuse(&m);Check(!m.Writes,"interface replacement rejected");
  Setup(&m,1,0);
  for(unsigned i=0;i<m.Count;i++){m.Devices[i].State.Config[63]=0xa5a5a5a5;m.Expected.Gpu.Config[63]=0xa5a5a5a5;m.Expected.Parents[0].Config[63]=0xa5a5a5a5;}
  Check(Init(&m)==EFI_SUCCESS,"partial sentinel fixture Init");m.Partial=1;Refuse(&m);
  Check(!m.Writes && m.Reads>=3,"different scan sentinels detect partial success that matched the first pattern");
  for(unsigned mutation=1;mutation<=12;mutation++) {
    Setup(&m,2,0);Check(Init(&m)==EFI_SUCCESS,"drift fixture Init");m.Mutation=mutation;m.MutateAt=1;Refuse(&m);Check(!m.Writes,"early parent, method or boot capability drift prevents mutation");
  }
  for(unsigned at=1;at<=5;at++) {
    Setup(&m,2,0);Check(Init(&m)==EFI_SUCCESS,"log fixture Init");m.LogFault=at;Refuse(&m);
    Setup(&m,2,0);Check(Init(&m)==EFI_SUCCESS,"late parent fixture Init");m.LogDrift=at;Refuse(&m);
  }
  Setup(&m,2,0);Check(Init(&m)==EFI_SUCCESS,"frozen policy fixture Init");m.Freeze=1;
  Check(Run(&m)==GU_DS_FIXED && m.Writes==1,"caller-owned binding and baseline edits cannot alter active policy");
  Setup(&m,2,0);Check(Init(&m)==EFI_SUCCESS,"failed write fixture Init");m.WriteStatus=EFI_DEVICE_ERROR;Refuse(&m);
  Check(m.Writes==1 && m.Transaction.PostObserved && m.Devices[0].State.Attributes==0x200,"changed state after failing write observed without retry");
  Setup(&m,2,0);Check(Init(&m)==EFI_SUCCESS,"extra write fixture Init");m.WriteExtra=1;Refuse(&m);
  Setup(&m,2,0);m.Devices[0].State.Attributes=m.Expected.Gpu.Attributes=0x200;
  Check(Init(&m)==EFI_SUCCESS && Run(&m)==GU_DS_ALREADY_CONSISTENT && !m.Writes,"consistent firmware state is read-only");
  Setup(&m,2,0);Check(Init(&m)==EFI_SUCCESS && Run(&m)==GU_DS_FIXED,"late capability baseline");
  unsigned late=m.DisableAt-1;
  for(unsigned method=0;method<3;method++) {
    Setup(&m,2,0);Check(Init(&m)==EFI_SUCCESS,"late earlier-parent fixture Init");
    m.Mutation=method?7+method:6;m.MutateAt=late;m.MutationDevice=1;Refuse(&m);
    Check(!m.Writes,"last parent callback cannot alter an earlier parent's capability before write");
  }
  for(unsigned mode=1;mode<=34;mode++) {
    if(mode==11 || mode==20)continue;
    Setup(&m,2,0);m.OwnerMode=mode;Check(Init(&m)==EFI_SUCCESS,"ownership fixture Init");
    Refuse(&m);Check(!m.Writes,"missing, malformed, foreign, driver and borrowed ownership records cannot authorize writes");
    if((mode>=21 && mode<=27) || mode>=33)Check(m.Adapter.OwnershipFault && !m.Frees,
      "borrowed, unaligned and overflowing memory is never freed or dereferenced as pool entries");
  }
  for(unsigned mode=11;mode<=20;mode+=9) {
    Setup(&m,2,0);m.OwnerMode=mode;Check(Init(&m)==EFI_SUCCESS,"passive reader Init");
    Check(Run(&m)==GU_DS_FIXED && m.Writes==1 && !m.PoolLive,"exact passive flags and bounded opener count preserve an exclusive self claim");
  }
  Setup(&m,2,0);Check(Init(&m)==EFI_SUCCESS && Run(&m)==GU_DS_FIXED,"ownership callback baseline");
  unsigned ownershipWrite=m.DisableAt;
  Setup(&m,2,0);m.FaultAt=ownershipWrite-1;m.FaultStatus=EFI_NOT_STARTED;
  Check(Init(&m)==EFI_SUCCESS,"actual NOT_STARTED ownership error Init");Refuse(&m);
  Check(!m.Writes && m.Adapter.OwnershipObserved && !m.Adapter.DisableInvoked && m.Adapter.OwnershipStatus==EFI_NOT_STARTED,
    "a real NOT_STARTED error is retained across subsequent successful ownership observations");
  for(unsigned at=1;at<ownershipWrite;at++) {
    Setup(&m,2,0);m.OwnerDriftAt=at;Check(Init(&m)==EFI_SUCCESS,"claim loss fixture Init");
    Refuse(&m);Check(!m.Writes,"claim loss in a pre-write snapshot, query or FreePool callback prevents Disable");
  }
  for(unsigned log=1;log<=5;log++) {
    Setup(&m,2,0);m.OwnerLogDrift=log;Check(Init(&m)==EFI_SUCCESS,"claim loss in log Init");Refuse(&m);
    if(log==1)Check(!m.Writes,"intent log cannot drop ownership and authorize correction");
  }
  Setup(&m,2,0);m.FaultAt=2;m.FaultStatus=EFI_WARN_UNKNOWN_GLYPH;
  Check(Init(&m)==EFI_SUCCESS,"pre-write cleanup warning Init");Refuse(&m);
  Check(!m.Writes && m.Adapter.OwnershipFault && m.Adapter.CleanupStatus==EFI_WARN_UNKNOWN_GLYPH,
    "pre-write FreePool warning is retained and cannot authorize a write");
  for(unsigned error=0;error<2;error++) {
    Setup(&m,2,0);Check(Init(&m)==EFI_SUCCESS,"post-write cleanup error Init");
    m.FaultAt=ownershipWrite+1;m.FaultStatus=EFI_OUT_OF_RESOURCES;
    m.WriteStatus=error?EFI_DEVICE_ERROR:EFI_SUCCESS;Refuse(&m);
    Check(m.Writes==1 && m.Transaction.Attempted && m.Adapter.WriteSpent && m.Adapter.DisableInvoked && m.Adapter.OwnershipFault,
      "cleanup failure after a write cannot imply no write or authorize a retry");
    Check(m.Adapter.DisableIoStatus==m.WriteStatus && m.Adapter.CleanupStatus==EFI_OUT_OF_RESOURCES,
      "Disable and cleanup failures retain separate original statuses");
  }
  for(unsigned bad=0;bad<4;bad++) {
    Setup(&m,2,0);EFI_HANDLE owner=(EFI_HANDLE)&m.OwnerTag;
    if(bad==0)owner=NULL;
    if(bad==1)owner=m.Bindings[0].Handle;
    if(bad==2)m.Boot.OpenProtocolInformation=NULL;
    if(bad==3)m.Boot.FreePool=NULL;
    Check(GuDsEfiInitialize(&m.Adapter,&m.Boot,owner,m.Bindings,m.Count,&m.Expected,Log,&m)!=EFI_SUCCESS && !m.Calls,
      "missing or aliased ownership capability refuses during pure Init");
  }
  for(unsigned bad=0;bad<8;bad++) {
    Setup(&m,2,0);
    switch(bad) {
      case 0:m.Bindings[1].Path[8]^=1;break;
      case 1:m.Bindings[1].PathBytes-=6;break;
      case 2:m.Bindings[2]=m.Bindings[1];break;
      case 3:m.Bindings[0].Path[6]=9;break;
      case 4:m.Bindings[0].PathBytes=113;break;
      case 5:m.Expected.Parents[0].Device^=1;break;
      case 6:m.Bindings[1].Io=&m.Devices[0].Io;break;
      case 7:m.Boot.LocateDevicePath=NULL;break;
    }
    Check(Init(&m)!=EFI_SUCCESS && !m.Calls && !m.Writes,"malformed ancestry/capability rejected before firmware");
  }
  printf("PASS %u UEFI adapter assertions; all EFIAPI services are RAM mocks, no firmware execution\n",Checks);
  return 0;
}
