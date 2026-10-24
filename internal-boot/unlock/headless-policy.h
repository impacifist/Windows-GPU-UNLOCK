/* GPL-2.0-only. Research-only, observational policy; never calls console methods. */
#pragma once
#include "port.h" /* pulls machine-profile.h: GU_CONSOLE_AGGREGATE */
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextOut.h>
#if !defined(GU_AUTOMATIC_BOOT) || !defined(GU_INTERNAL_BOOT) || !defined(GU_EXPERIMENTAL_3090) || !defined(GU_ENTRY_LATCHED_SS1_FULL)
#error Headless research requires the exact automatic internal full profile
#endif
typedef struct {EFI_HANDLE handle,agent,controller;EFI_GUID protocol;UINT32 attributes,count;} GU_HEADLESS_EDGE;
typedef struct {UINTN count;GU_HEADLESS_EDGE edges[4096];} GU_HEADLESS_GRAPH;
static struct {
  BOOLEAN ready;EFI_SYSTEM_TABLE *table;EFI_HANDLE out,in,err;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *output,*error;
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL *input;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL outputCopy,errorCopy;
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL inputCopy;
  GU_HEADLESS_GRAPH graph;
} GuHeadless;
static BOOLEAN GuHeadlessBytes(const VOID *a,const VOID *b,UINTN n) {
  const UINT8 *x=a,*y=b;for(UINTN i=0;i<n;i++)if(x[i]!=y[i])return FALSE;return TRUE;
}
static BOOLEAN GuHeadlessGuid(const EFI_GUID *a,const EFI_GUID *b){return GuHeadlessBytes(a,b,sizeof(*a));}
static EFI_STATUS GuHeadlessSnapshot(GU_HEADLESS_GRAPH *g) {
  EFI_GUID pci=EFI_PCI_IO_PROTOCOL_GUID;EFI_BOOT_SERVICES *bs=Gu.bs;
  EFI_HANDLE *handles=NULL;UINTN n=0;g->count=0;
  EFI_STATUS s=bs->LocateHandleBuffer(AllHandles,NULL,NULL,&n,&handles);
  if(s==EFI_SUCCESS && (!handles || !n || n>1024))s=EFI_BAD_BUFFER_SIZE;
  for(UINTN i=0;s==EFI_SUCCESS && i<n;i++) {
    EFI_GUID **protocols=NULL;UINTN np=0;
    s=bs->ProtocolsPerHandle(handles[i],&protocols,&np);
    if(s==EFI_SUCCESS && (!protocols || !np || np>256))s=EFI_BAD_BUFFER_SIZE;
    for(UINTN j=0;s==EFI_SUCCESS && j<np;j++) {
      EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *entries=NULL;UINTN ne=0;
      if(!protocols[j]){s=EFI_COMPROMISED_DATA;break;}
      s=bs->OpenProtocolInformation(handles[i],protocols[j],&entries,&ne);
      if(s==EFI_SUCCESS && (ne>512 || (ne && !entries)))s=EFI_BAD_BUFFER_SIZE;
      for(UINTN k=0;s==EFI_SUCCESS && k<ne;k++) {
        EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *e=&entries[k];
        if(e->Attributes==1 || e->Attributes==2 || e->Attributes==4)continue;
        if(handles[i]==Gu.handle && GuHeadlessGuid(protocols[j],&pci) && Gu.owned &&
           e->AgentHandle==Gu.image && !e->ControllerHandle && e->Attributes==EFI_OPEN_PROTOCOL_EXCLUSIVE && e->OpenCount==1) {
          /* Preserve this record too: comparison normalizes exactly one own claim. */
        }else if(!e->AgentHandle || !e->ControllerHandle || !e->OpenCount){s=EFI_COMPROMISED_DATA;break;}
        if(g->count==4096){s=EFI_BAD_BUFFER_SIZE;break;}
        g->edges[g->count++]=(GU_HEADLESS_EDGE){handles[i],e->AgentHandle,e->ControllerHandle,*protocols[j],e->Attributes,e->OpenCount};
      }
      if(entries && bs->FreePool(entries)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
    }
    if(protocols && bs->FreePool(protocols)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  }
  if(handles && bs->FreePool(handles)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  return s;
}
static BOOLEAN GuHeadlessOwn(const GU_HEADLESS_EDGE *e) {
  EFI_GUID pci=EFI_PCI_IO_PROTOCOL_GUID;
  return e->handle==Gu.handle && e->agent==Gu.image && !e->controller && e->attributes==EFI_OPEN_PROTOCOL_EXCLUSIVE &&
    e->count==1 && GuHeadlessGuid(&e->protocol,&pci);
}
static BOOLEAN GuHeadlessSame(const GU_HEADLESS_EDGE *a,const GU_HEADLESS_EDGE *b) {
  return a->handle==b->handle && a->agent==b->agent && a->controller==b->controller && a->attributes==b->attributes &&
    a->count==b->count && GuHeadlessGuid(&a->protocol,&b->protocol);
}
static VOID GuHeadlessReset(EFI_SYSTEM_TABLE *t) {
  GuHeadless.ready=FALSE;GuHeadless.table=t;GuHeadless.graph.count=0;
}
/* Caller holds TPL_NOTIFY across every observation and baseline commit. */
static BOOLEAN GuHeadlessCheckInner(VOID) {
  EFI_SYSTEM_TABLE *t=GuHeadless.table;EFI_BOOT_SERVICES *bs=Gu.bs;
  EFI_GUID text=EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID,input=EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID;
  EFI_GUID gop=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID,path=EFI_DEVICE_PATH_PROTOCOL_GUID;
  if(!t || !bs || !bs->RaiseTPL || !bs->RestoreTPL || !bs->LocateHandleBuffer || !bs->HandleProtocol ||
     !bs->ProtocolsPerHandle || !bs->OpenProtocolInformation || !bs->FreePool || !t->ConOut || !t->StdErr || !t->ConIn ||
     !t->ConsoleOutHandle || !t->StandardErrorHandle || !t->ConsoleInHandle || !t->ConIn->Reset || !t->ConIn->ReadKeyStroke || !t->ConIn->WaitForKey)return FALSE;
  if(GuHeadless.ready && (t->ConsoleOutHandle!=GuHeadless.out || t->ConsoleInHandle!=GuHeadless.in || t->StandardErrorHandle!=GuHeadless.err ||
     t->ConOut!=GuHeadless.output || t->StdErr!=GuHeadless.error || t->ConIn!=GuHeadless.input ||
     !GuHeadlessBytes(t->ConOut,&GuHeadless.outputCopy,sizeof(*t->ConOut)) || !GuHeadlessBytes(t->StdErr,&GuHeadless.errorCopy,sizeof(*t->StdErr)) ||
     !GuHeadlessBytes(t->ConIn,&GuHeadless.inputCopy,sizeof(*t->ConIn))))return FALSE;
  VOID *p=NULL;
  if(bs->HandleProtocol(t->ConsoleOutHandle,&text,&p)!=EFI_SUCCESS || p!=t->ConOut)return FALSE;
  if(bs->HandleProtocol(t->StandardErrorHandle,&text,&p)!=EFI_SUCCESS || p!=t->StdErr)return FALSE;
  if(bs->HandleProtocol(t->ConsoleInHandle,&input,&p)!=EFI_SUCCESS || p!=t->ConIn)return FALSE;
  EFI_HANDLE *handles=NULL;UINTN n=0;
  EFI_STATUS s=bs->LocateHandleBuffer(ByProtocol,&gop,NULL,&n,&handles);
  if(s==EFI_NOT_FOUND && !n && !handles)s=EFI_SUCCESS;
  else if(s==EFI_SUCCESS) {
    if(!handles || !n || n>2)s=EFI_UNSUPPORTED;
    for(UINTN i=0;s==EFI_SUCCESS && i<n;i++) {
      EFI_DEVICE_PATH_PROTOCOL *dp=NULL;EFI_GRAPHICS_OUTPUT_PROTOCOL *g=NULL;
      EFI_STATUS ps=bs->HandleProtocol(handles[i],&path,(VOID **)&dp);
#if !GU_CONSOLE_AGGREGATE
      if((ps!=EFI_UNSUPPORTED && ps!=EFI_NOT_FOUND) || dp ||
         (handles[i]!=t->ConsoleOutHandle && handles[i]!=t->StandardErrorHandle) ||
         bs->HandleProtocol(handles[i],&gop,(VOID **)&g)!=EFI_SUCCESS || !g || !g->Mode || g->Mode->FrameBufferBase || g->Mode->FrameBufferSize)s=EFI_UNSUPPORTED;
#else
      /* Aggregate topology: the AMI combo aggregate GOP is pathless and is
         not a console handle; any pathless zero-FB GOP is tolerated. */
      if((ps!=EFI_UNSUPPORTED && ps!=EFI_NOT_FOUND) || dp ||
         bs->HandleProtocol(handles[i],&gop,(VOID **)&g)!=EFI_SUCCESS || !g || !g->Mode || g->Mode->FrameBufferBase || g->Mode->FrameBufferSize)s=EFI_UNSUPPORTED;
#endif
      for(UINTN j=0;j<i;j++)if(handles[j]==handles[i])s=EFI_UNSUPPORTED;
    }
  }
  if(handles && bs->FreePool(handles)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  if(s!=EFI_SUCCESS)return FALSE;
  static GU_HEADLESS_GRAPH current;
  s=GuHeadlessSnapshot(&current);
  if(s!=EFI_SUCCESS)return FALSE;
  UINTN own=0;
  for(UINTN i=0;i<current.count;i++) {
    const GU_HEADLESS_EDGE *e=&current.edges[i];
    if(GuHeadlessOwn(e)){own++;continue;}
    if(e->handle==Gu.handle)return FALSE; /* No driver/child may remain on GPU. */
    if(GuHeadless.ready) {
      UINTN matches=0;for(UINTN j=0;j<GuHeadless.graph.count;j++)if(GuHeadlessSame(e,&GuHeadless.graph.edges[j]))matches++;
      if(matches!=1)return FALSE;
    }
  }
  if(own!=(Gu.owned?1U:0U))return FALSE;
  if(GuHeadless.ready) {
    if(current.count!=GuHeadless.graph.count+own)return FALSE;
    for(UINTN i=0;i<GuHeadless.graph.count;i++) {
      UINTN matches=0;for(UINTN j=0;j<current.count;j++)if(GuHeadlessSame(&GuHeadless.graph.edges[i],&current.edges[j]))matches++;
      if(matches!=1)return FALSE;
    }
    return TRUE;
  }
  if(own)return FALSE;
  GuHeadless.graph=current;GuHeadless.out=t->ConsoleOutHandle;GuHeadless.in=t->ConsoleInHandle;GuHeadless.err=t->StandardErrorHandle;
  GuHeadless.output=t->ConOut;GuHeadless.error=t->StdErr;GuHeadless.input=t->ConIn;
  GuHeadless.outputCopy=*t->ConOut;GuHeadless.errorCopy=*t->StdErr;GuHeadless.inputCopy=*t->ConIn;GuHeadless.ready=TRUE;
  return TRUE;
}
static BOOLEAN GuHeadlessCheck(VOID) {
  EFI_BOOT_SERVICES *bs=Gu.bs;
  if(!bs || !bs->RaiseTPL || !bs->RestoreTPL)return FALSE;
  EFI_TPL old=bs->RaiseTPL(TPL_NOTIFY);
  BOOLEAN valid=GuHeadlessCheckInner();
  bs->RestoreTPL(old);
  return valid;
}
