/* GPL-2.0-only. Observation-only discovery; never a release permit. */
#include <Protocol/SimpleTextIn.h>
typedef struct {UINTN Size,Code,Bytes,Stop;UINT32 Version;EFI_MEMORY_TYPE CodeType;const char *Hash;} PF_PIN;
typedef struct {
  EFI_HANDLE Agent[4],Image[4],NullBinding,Child;
  VOID *Base[4];UINT8 SharedFlag;UINT64 PointerCounts[2];
  UINTN MatchedEdges,RejectionStep;
} PF_STATE;
static const PF_PIN PfPins[]={
 {36864,0x1000,12802,0x1464,1,EfiRuntimeServicesCode,"99e4b7aff51f40e07b8fbbfe62b89f55a5d6a678edb87de5ae598b4ffd0e932b"},
 {30912,0x2a0,23699,0x11e0,0x10,EfiBootServicesCode,"a45cc097c00d9af3619eba22e2fa0969e54475f307fc7631aab9737fc751dfae"},
 {22080,0x2a0,11997,0xa18,0x10,EfiBootServicesCode,"1d4e4f48e37b298ce8b50d6ba7aaa968d6d0ae6633cd4bc74c96bce94aa7098d"},
 {18432,0x2a0,13965,0x7c4,0x24,EfiBootServicesCode,"98316fea18ef529eb95c688985517b1e8bfe2071e4c105288106c152d79faa4c"}
};
static BOOLEAN PfEqual(const VOID *a,const VOID *b,UINTN n) {
  for(UINTN i=0;i<n;i++)if(((const UINT8 *)a)[i]!=((const UINT8 *)b)[i])return FALSE;
  return TRUE;
}
static BOOLEAN PfHash(const UINT8 *bytes,UINTN n,const char *expected) {
  UINT8 digest[32];GuSha256(bytes,n,digest);
  for(UINTN i=0;i<32;i++)if("0123456789abcdef"[digest[i]>>4]!=expected[i*2] ||
    "0123456789abcdef"[digest[i]&15]!=expected[i*2+1])return FALSE;
  return expected[64]==0;
}
static EFI_STATUS PfDrivers(EFI_SYSTEM_TABLE *t,const PF_PIN *pins,PF_STATE *out) {
  EFI_BOOT_SERVICES *bs=t->BootServices;EFI_HANDLE *hs=NULL;UINTN count=0;
  EFI_STATUS s=bs->LocateHandleBuffer(ByProtocol,&BindGuidL,NULL,&count,&hs);
  if(s!=EFI_SUCCESS)return s;
  if(!hs || count>256){s=EFI_BAD_BUFFER_SIZE;goto done;}
  for(UINTN i=0;i<count;i++) {
    EFI_DRIVER_BINDING_PROTOCOL *d=NULL;EFI_LOADED_IMAGE_PROTOCOL *im=NULL;
    if(bs->HandleProtocol(hs[i],&BindGuidL,(VOID **)&d)!=EFI_SUCCESS || !d){s=EFI_DEVICE_ERROR;goto done;}
    if(!d->ImageHandle)continue;
    if(bs->HandleProtocol(d->ImageHandle,&ImgGuidL,(VOID **)&im)!=EFI_SUCCESS || !im){s=EFI_DEVICE_ERROR;goto done;}
    for(UINTN role=0;role<4;role++) {
      const PF_PIN *p=&pins[role];
      if(im->ImageSize!=p->Size || d->Version!=p->Version)continue;
      if(!im->ImageBase || im->ImageCodeType!=p->CodeType || d->DriverBindingHandle!=hs[i] ||
         p->Code>p->Size || p->Bytes>p->Size-p->Code ||
         (UINTN)im->ImageBase>MAX_UINTN-p->Size || p->Stop>=p->Size ||
         (UINTN)d->Stop!=(UINTN)im->ImageBase+p->Stop ||
         !PfHash((UINT8 *)im->ImageBase+p->Code,p->Bytes,p->Hash))continue;
      if(out->Agent[role]){s=EFI_ALREADY_STARTED;goto done;}
      out->Agent[role]=hs[i];out->Image[role]=d->ImageHandle;out->Base[role]=im->ImageBase;
    }
  }
  for(UINTN r=0;r<4;r++)if(!out->Agent[r]){s=EFI_NOT_FOUND;goto done;}
  for(UINTN i=0;i<count;i++) {
    EFI_DRIVER_BINDING_PROTOCOL *d=NULL;
    if(bs->HandleProtocol(hs[i],&BindGuidL,(VOID **)&d)!=EFI_SUCCESS || !d){s=EFI_DEVICE_ERROR;goto done;}
    if(!d->ImageHandle && d->DriverBindingHandle==hs[i] && d->Version==0x10 &&
       (UINTN)d->Stop==(UINTN)out->Base[3]+0x2b38) {
      if(out->NullBinding){s=EFI_ALREADY_STARTED;goto done;}
      out->NullBinding=hs[i];
    }
  }
  if(!out->NullBinding){s=EFI_NOT_FOUND;goto done;}
  /* Loaded-image protocol buffers are trusted firmware allocations, as in the
   * existing image dumper. Bounds above must cover every sampled data field. */
  if(pins[0].Size<=0x5557){s=EFI_BAD_BUFFER_SIZE;goto done;}
  out->SharedFlag=*((volatile UINT8 *)out->Base[0]+0x546a);
  out->PointerCounts[0]=*(volatile UINT64 *)((UINT8 *)out->Base[0]+0x5530);
  out->PointerCounts[1]=*(volatile UINT64 *)((UINT8 *)out->Base[0]+0x5550);
done:
  if(hs && bs->FreePool(hs)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  return s;
}
typedef struct {EFI_HANDLE Handle,Agent,Controller;EFI_GUID Protocol;UINT32 Attributes;} PF_EDGE;
static EFI_STATUS PfGraphKnown(EFI_SYSTEM_TABLE *t,EFI_HANDLE gpu,PF_STATE *state,UINTN missingMask,EFI_HANDLE knownChild) {
  EFI_BOOT_SERVICES *bs=t->BootServices;
  EFI_GUID text={0x387477c2,0x69c7,0x11d2,{0x8e,0x39,0,0xa0,0xc9,0x69,0x72,0x3b}};
  EFI_GUID edid={0x1c0c34f6,0xd380,0x41fa,{0xa0,0x49,0x8a,0xd0,0x6c,0x1a,0x66,0xaa}};
  EFI_GUID pointer={0x31878c87,0x0b75,0x11d5,{0x9a,0x4f,0,0x90,0x27,0x3f,0xc1,0x4d}};
  EFI_GUID absolute={0x8d59d32b,0xc655,0x4ae9,{0x9b,0x15,0xf2,0x59,0x04,0x99,0x2a,0x43}};
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *entries=NULL;UINTN n=0,children=0;
  EFI_STATUS s=bs->OpenProtocolInformation(gpu,&PciGuidL,&entries,&n);
  if(s!=EFI_SUCCESS)return s;
  if(n>512 || (n&&!entries))s=EFI_BAD_BUFFER_SIZE;
  for(UINTN i=0;s==EFI_SUCCESS && i<n;i++)if(entries[i].Attributes==8 && entries[i].AgentHandle==state->Agent[3]) {
    state->Child=entries[i].ControllerHandle;children++;
  }
  if(entries && bs->FreePool(entries)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  if(s!=EFI_SUCCESS)return s;
  if(missingMask&2) {
    if(children || !knownChild)return EFI_NOT_READY;
    state->Child=knownChild;
  }
  if((!(missingMask&2) && children!=1) || !state->Child || state->Child==gpu || t->ConsoleOutHandle!=t->ConsoleInHandle ||
     t->ConsoleOutHandle!=t->StandardErrorHandle || t->ConsoleOutHandle==gpu ||
     t->ConsoleOutHandle==state->Child)return EFI_UNSUPPORTED;
  PF_EDGE expected[]={
    {gpu,state->Agent[3],gpu,PciGuidL,16},{gpu,state->Agent[3],state->Child,PciGuidL,8},
    {state->Child,state->Agent[1],state->Child,text,16},{state->Child,state->Agent[1],t->ConsoleOutHandle,text,8},
    {state->Child,state->Agent[2],state->Child,GopGuidL,16},{state->Child,state->Agent[0],state->Child,edid,16},
    {t->ConsoleOutHandle,state->Agent[0],t->ConsoleOutHandle,pointer,16},
    {t->ConsoleOutHandle,state->Agent[0],t->ConsoleOutHandle,absolute,16}};
  BOOLEAN found[8]={0};EFI_HANDLE *handles=NULL;UINTN hc=0;
  s=bs->LocateHandleBuffer(AllHandles,NULL,NULL,&hc,&handles);
  if(s==EFI_SUCCESS && (!handles || !hc || hc>1024))s=EFI_BAD_BUFFER_SIZE;
  if(s!=EFI_SUCCESS){if(handles)(void)bs->FreePool(handles);return s;}
  for(UINTN h=0;h<hc;h++) {
    EFI_GUID **ps=NULL;UINTN pc=0;s=bs->ProtocolsPerHandle(handles[h],&ps,&pc);
    if(s==EFI_SUCCESS && (!ps || !pc || pc>256))s=EFI_BAD_BUFFER_SIZE;
    for(UINTN p=0;s==EFI_SUCCESS && p<pc;p++) {
      entries=NULL;n=0;
      if(!ps[p]){s=EFI_COMPROMISED_DATA;break;}
      s=bs->OpenProtocolInformation(handles[h],ps[p],&entries,&n);
      if(s==EFI_SUCCESS && (n>512 || (n&&!entries)))s=EFI_BAD_BUFFER_SIZE;
      for(UINTN i=0;s==EFI_SUCCESS && i<n;i++) {
        EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *e=&entries[i];
        if(e->Attributes==1 || e->Attributes==2 || e->Attributes==4)continue;
        BOOLEAN relevant=handles[h]==gpu || handles[h]==state->Child || handles[h]==t->ConsoleOutHandle || e->AgentHandle==state->NullBinding;
        for(UINTN r=0;r<4;r++)if(e->AgentHandle==state->Agent[r])relevant=TRUE;
        if(!relevant)continue;
        UINTN match=8;
        for(UINTN k=0;k<8;k++)if(!(missingMask&(1U<<k)) && expected[k].Handle==handles[h] && expected[k].Agent==e->AgentHandle &&
           expected[k].Controller==e->ControllerHandle && expected[k].Attributes==e->Attributes &&
           PfEqual(&expected[k].Protocol,ps[p],sizeof(EFI_GUID)))match=k;
        if(match==8 || found[match] || e->OpenCount!=1){s=EFI_UNSUPPORTED;break;}
        found[match]=TRUE;
        state->MatchedEdges++;
      }
      if(entries && bs->FreePool(entries)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
    }
    if(ps && bs->FreePool(ps)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
    if(s!=EFI_SUCCESS)break;
  }
  if(bs->FreePool(handles)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  if(s!=EFI_SUCCESS)return s;
  for(UINTN k=0;k<8;k++)if(!(missingMask&(1U<<k)) && !found[k])return EFI_NOT_FOUND;
  return EFI_SUCCESS;
}
static EFI_STATUS PfGraphMissing(EFI_SYSTEM_TABLE *t,EFI_HANDLE gpu,PF_STATE *state,UINTN missingMask) {
  return PfGraphKnown(t,gpu,state,missingMask,NULL);
}
static EFI_STATUS PfGraphForState(EFI_SYSTEM_TABLE *t,EFI_HANDLE gpu,PF_STATE *state,BOOLEAN edidDetached) {
  return PfGraphMissing(t,gpu,state,edidDetached?(1U<<5):0);
}
static EFI_STATUS PfGraph(EFI_SYSTEM_TABLE *t,EFI_HANDLE gpu,PF_STATE *state) {
  return PfGraphForState(t,gpu,state,FALSE);
}
static EFI_STATUS PfCollect(EFI_SYSTEM_TABLE *t,EFI_HANDLE gpu,const PF_PIN *pins,PF_STATE *out) {
  UINT8 *b=(VOID *)out;for(UINTN i=0;i<sizeof(*out);i++)b[i]=0;
  EFI_STATUS s=PfDrivers(t,pins,out);
  if(s!=EFI_SUCCESS)out->RejectionStep=1;
  else {s=PfGraph(t,gpu,out);if(s!=EFI_SUCCESS)out->RejectionStep=2;}
  return s;
}
