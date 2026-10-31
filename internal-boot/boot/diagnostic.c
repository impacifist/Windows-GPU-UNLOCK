/* GPL-2.0-only. Read-only protocol/console ownership capture, 2026-10-21.
 * Writes only to the caller's new diagnostic file. No device state changes.
 * HandleProtocol observations can add passive firmware open records; the
 * snapshot is observational, not an atomic authorization for future detach.
 */
#include "diagnostic.h"
#include <Protocol/DevicePath.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/PciIo.h>
#include <Guid/GlobalVariable.h>
#include "../unlock/machine-profile.h"
#ifdef GU_DIAGNOSTIC_PROBE
#include "probe.h"
#endif

#define MAX_HANDLES 4096
#define MAX_PROTOCOLS 256
#define MAX_OPENS 512
#define MAX_BYTES (8U*1024U*1024U)
static EFI_GUID PathGuid=EFI_DEVICE_PATH_PROTOCOL_GUID;
static EFI_GUID DriverGuid=EFI_DRIVER_BINDING_PROTOCOL_GUID;
static EFI_GUID ImageGuid=EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID GopGuid=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
static EFI_GUID PciGuid=EFI_PCI_IO_PROTOCOL_GUID;
static EFI_GUID GlobalGuid=EFI_GLOBAL_VARIABLE;


static EFI_STATUS Flush(CAPTURE *c) {
  if(c->Io!=EFI_SUCCESS)return c->Io;
  if(c->Used) {
    UINTN n=c->Used;
    c->Io=c->File->Write(c->File,&n,c->Buffer);
    if(c->Io==EFI_SUCCESS && n!=c->Used)c->Io=EFI_DEVICE_ERROR;
    c->Used=0;
  }
  return c->Io;
}
VOID Text(CAPTURE *c,const char *s) {
  while(*s && c->Io==EFI_SUCCESS) {
    if(c->Total>=MAX_BYTES){c->Io=EFI_VOLUME_FULL;c->Complete=FALSE;return;}
    if(c->Used==sizeof(c->Buffer) && Flush(c)!=EFI_SUCCESS)return;
    c->Buffer[c->Used++]=*s++;c->Total++;
  }
}
VOID Hex(CAPTURE *c,UINT64 v,UINTN digits) {
  char s[17];
  if(digits>16)digits=16;
  for(UINTN i=0;i<digits;i++)s[i]="0123456789abcdef"[(v>>((digits-i-1)*4))&15];
  s[digits]=0;Text(c,s);
}
static VOID Field(CAPTURE *c,const char *s,UINT64 v) {Text(c,s);Hex(c,v,16);}
static VOID Guid(CAPTURE *c,const EFI_GUID *g) {
  Hex(c,g->Data1,8);Text(c,"-");Hex(c,g->Data2,4);Text(c,"-");Hex(c,g->Data3,4);Text(c,"-");
  for(UINTN i=0;i<8;i++){if(i==2)Text(c,"-");Hex(c,g->Data4[i],2);}
}
VOID End(CAPTURE *c) {Text(c,"\r\n");}
VOID Problem(CAPTURE *c,const char *where,EFI_STATUS s) {
  c->Complete=FALSE;Text(c,"incomplete reason=");Text(c,where);Field(c," status=0x",s);End(c);
}
static VOID FreeCapture(CAPTURE *c,VOID *data,const char *where) {
  if(data) {
    EFI_STATUS s=c->Table->BootServices->FreePool(data);
    if(s!=EFI_SUCCESS)Problem(c,where,s);
  }
}
/* Variable paths have a known byte size. Firmware interface paths do not;
 * for those, as in the existing wrapper, trust firmware pointers but bound
 * traversal. Log each node verbatim, including multi-instance terminators.
 */
static VOID Path(CAPTURE *c,const EFI_DEVICE_PATH_PROTOCOL *p,UINTN bound,BOOLEAN exact) {
  if(!p){Text(c,"path absent\r\n");return;}
  UINTN at=0;
  for(UINTN nodes=0;nodes<256 && at+4<=bound;nodes++) {
    const UINT8 *b=(const VOID *)p;
    UINTN n=b[2]+((UINTN)b[3]<<8);
    if(n<4 || n>bound-at){Problem(c,"path-node-length",EFI_COMPROMISED_DATA);return;}
    Text(c,"path-node raw=");for(UINTN j=0;j<n;j++)Hex(c,b[j],2);End(c);
    at+=n;
    if(b[0]==END_DEVICE_PATH_TYPE) {
      if(n!=4 || (b[1]!=END_ENTIRE_DEVICE_PATH_SUBTYPE && b[1]!=END_INSTANCE_DEVICE_PATH_SUBTYPE)) {
        Problem(c,"path-end",EFI_COMPROMISED_DATA);return;
      }
      if(b[1]==END_ENTIRE_DEVICE_PATH_SUBTYPE) {
        if(exact && at!=bound)Problem(c,"path-trailing-data",EFI_COMPROMISED_DATA);
        return;
      }
    }
    p=(const VOID *)(b+n);
  }
  Problem(c,"path-limit-or-missing-end",EFI_BAD_BUFFER_SIZE);
}
static VOID Variables(CAPTURE *c) {
  CHAR16 *names[]={L"ConOut",L"ConOutDev",L"ErrOut",L"ErrOutDev",L"ConIn",L"ConInDev"};
  const char *ascii[]={"ConOut","ConOutDev","ErrOut","ErrOutDev","ConIn","ConInDev"};
  for(UINTN i=0;i<6 && c->Io==EFI_SUCCESS;i++) {
    UINTN n=0;UINT32 attrs=0;
    EFI_STATUS s=c->Table->RuntimeServices->GetVariable(names[i],&GlobalGuid,&attrs,&n,NULL);
    Text(c,"variable name=");Text(c,ascii[i]);Field(c," initial-status=0x",s);Field(c," bytes=0x",n);End(c);
    if(s==EFI_NOT_FOUND)continue;
    if(s!=EFI_BUFFER_TOO_SMALL || !n || n>16384){Problem(c,"variable-size",s);continue;}
    UINT8 *data=NULL;UINTN capacity=n;
    s=c->Table->BootServices->AllocatePool(EfiBootServicesData,n,(VOID **)&data);
    if(s!=EFI_SUCCESS || !data){Problem(c,"variable-allocate",EFI_OUT_OF_RESOURCES);continue;}
    s=c->Table->RuntimeServices->GetVariable(names[i],&GlobalGuid,&attrs,&n,data);
    Field(c,"variable-status=0x",s);Field(c," attrs=0x",attrs);Field(c," bytes=0x",n);End(c);
    if(s==EFI_SUCCESS && n && n<=capacity)Path(c,(VOID *)data,n,TRUE);
    else Problem(c,"variable-read",s==EFI_SUCCESS?EFI_BAD_BUFFER_SIZE:s);
    FreeCapture(c,data,"variable-free");
  }
}
static VOID Details(CAPTURE *c,EFI_HANDLE h,EFI_GUID *g) {
  EFI_BOOT_SERVICES *bs=c->Table->BootServices;VOID *p=NULL;
  BOOLEAN path=TRUE,driver=TRUE,image=TRUE,gop=TRUE,pci=TRUE;
  const UINT8 *v=(VOID *)g;
  for(UINTN j=0;j<sizeof(EFI_GUID);j++) {
    if(v[j]!=((UINT8 *)&PathGuid)[j])path=FALSE;
    if(v[j]!=((UINT8 *)&DriverGuid)[j])driver=FALSE;
    if(v[j]!=((UINT8 *)&ImageGuid)[j])image=FALSE;
    if(v[j]!=((UINT8 *)&GopGuid)[j])gop=FALSE;
    if(v[j]!=((UINT8 *)&PciGuid)[j])pci=FALSE;
  }
  if(!path && !driver && !image && !gop && !pci)return;
  EFI_STATUS s=bs->HandleProtocol(h,g,&p);
  if(s!=EFI_SUCCESS || !p){Problem(c,"interface-read",s==EFI_SUCCESS?EFI_NOT_FOUND:s);return;}
  if(path)Path(c,p,4096,FALSE);
  if(driver) {
    EFI_DRIVER_BINDING_PROTOCOL *d=p;
    Field(c,"driver image=0x",(UINTN)d->ImageHandle);Field(c," binding=0x",(UINTN)d->DriverBindingHandle);
    Field(c," version=0x",d->Version);Field(c," stop-address=0x",(UINTN)d->Stop);End(c);
  }
  if(image) {
    EFI_LOADED_IMAGE_PROTOCOL *im=p;
    Field(c,"image parent=0x",(UINTN)im->ParentHandle);Field(c," device=0x",(UINTN)im->DeviceHandle);
    Field(c," base=0x",(UINTN)im->ImageBase);Field(c," size=0x",im->ImageSize);
    Field(c," code-type=0x",im->ImageCodeType);End(c);
    Text(c,"image-file-");Path(c,im->FilePath,4096,FALSE);
  }
  if(gop) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gp=p;
    if(!gp->Mode){Problem(c,"gop-null-mode",EFI_NOT_READY);return;}
    Field(c,"gop framebuffer=0x",gp->Mode->FrameBufferBase);Field(c," bytes=0x",gp->Mode->FrameBufferSize);
    Field(c," mode=0x",gp->Mode->Mode);Field(c," max-mode=0x",gp->Mode->MaxMode);End(c);
    if(gp->Mode->Info && gp->Mode->SizeOfInfo>=sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION)) {
      Field(c,"gop width=0x",gp->Mode->Info->HorizontalResolution);
      Field(c," height=0x",gp->Mode->Info->VerticalResolution);
      Field(c," stride=0x",gp->Mode->Info->PixelsPerScanLine);End(c);
      Field(c,"gop pixel-format=0x",gp->Mode->Info->PixelFormat);
      Field(c," info-bytes=0x",gp->Mode->SizeOfInfo);End(c);
    } else {
#if GU_CONSOLE_AGGREGATE || defined(GU_PROBE_GENERIC)
      /* Aggregate/pathless GOPs legitimately lack Mode->Info on this firmware;
         record it as an observation, not a capture failure (generic probe: topology unknown). */
      Field(c,"gop info-bytes=0x",gp->Mode->SizeOfInfo);End(c);
#elif defined(GU_EVGA_STOP_ONLY)
      /* The exact captured virtual GOP has no mode-info block. Do not widen
       * the exception to physical GOPs, nonempty metadata or path errors. */
      VOID *devicePath=NULL;
      EFI_STATUS ps=bs->HandleProtocol(h,&PathGuid,&devicePath);
      if(!gp->Mode->SizeOfInfo &&
         !gp->Mode->FrameBufferBase && !gp->Mode->FrameBufferSize && ps==EFI_UNSUPPORTED) {
        Field(c,"gop info-bytes=0x",gp->Mode->SizeOfInfo);End(c);
      } else Problem(c,"gop-info",EFI_NOT_READY);
#else
      Problem(c,"gop-info",EFI_NOT_READY);
#endif
    }
  }
  if(pci) {
    EFI_PCI_IO_PROTOCOL *io=p;UINTN segment=0,bus=0,dev=0,fun=0;UINT32 config[16];
    s=io->GetLocation(io,&segment,&bus,&dev,&fun);
    Field(c,"pci location-status=0x",s);Field(c," segment=0x",segment);Field(c," bus=0x",bus);
    Field(c," device=0x",dev);Field(c," function=0x",fun);End(c);
    if(s!=EFI_SUCCESS)Problem(c,"pci-location",s);
    s=io->Pci.Read(io,EfiPciIoWidthUint32,0,16,config);
    Field(c,"pci config-status=0x",s);End(c);
    if(s==EFI_SUCCESS) {
      Text(c,"pci config-dwords=");for(UINTN j=0;j<16;j++){Hex(c,config[j],8);Text(c," ");}End(c);
    } else Problem(c,"pci-read",s);
  }
}
EFI_STATUS GuCaptureGraphOnVolume(EFI_SYSTEM_TABLE *t,EFI_FILE_PROTOCOL *file,EFI_FILE_PROTOCOL *root) {
  if(!t || !t->BootServices || !t->RuntimeServices || !file || !file->Write || !file->Flush)return EFI_INVALID_PARAMETER;
  /* Single-threaded EFI application; keep the file buffer off the EFI stack. */
  static CAPTURE c;
  c.Table=t;c.File=file;c.DumpRoot=root;c.Complete=TRUE;c.Io=EFI_SUCCESS;c.Used=0;c.Total=0;
  Text(&c,"gpu-console-graph-v1 observational-only; no-detach no-unlock no-nvram-write\r\n");
  Field(&c,"console out=0x",(UINTN)t->ConsoleOutHandle);Field(&c," error=0x",(UINTN)t->StandardErrorHandle);
  Field(&c," input=0x",(UINTN)t->ConsoleInHandle);End(&c);
  Variables(&c);
  EFI_BOOT_SERVICES *bs=t->BootServices;EFI_HANDLE *handles=NULL;UINTN count=0;
#ifdef GU_DIAGNOSTIC_PROBE
  EFI_FREE_POOL probeRelease=bs->FreePool;
#endif
  EFI_STATUS s=bs->LocateHandleBuffer(AllHandles,NULL,NULL,&count,&handles);
#ifdef GU_DIAGNOSTIC_PROBE
    /* Read-only profile probe: config space, caps, BAR/PROM hash, image dumps.
     * The probe records its own failures through Problem, which clears
     * Complete, so CAPTURE.Complete stays the authoritative signal for the
     * log and the return status. Record the probe status as evidence; an
     * absent reviewed device (EFI_NOT_FOUND) is not a capture failure, and a
     * probe error that did not clear Complete still fails closed here. */
    EFI_STATUS probe=GuProbeGpu(t,&c);
    Field(&c,"probe status=0x",probe);End(&c);
    if(probe==EFI_COMPROMISED_DATA) {
      Problem(&c,"probe-capability-snapshot",EFI_NOT_READY);
      if(handles && probeRelease)probeRelease(handles);
      Text(&c,"capture-complete=0\r\n");
      EFI_STATUS written=Flush(&c);if(written!=EFI_SUCCESS)return written;
      written=file->Flush(file);return written==EFI_SUCCESS?EFI_NOT_READY:written;
    }
    if(probe!=EFI_SUCCESS && probe!=EFI_NOT_FOUND && c.Complete)Problem(&c,"probe",probe);
#endif
  Field(&c,"handles status=0x",s);Field(&c," count=0x",count);End(&c);
  if(s!=EFI_SUCCESS || !handles || !count || count>MAX_HANDLES)Problem(&c,"handle-snapshot",s==EFI_SUCCESS?EFI_BAD_BUFFER_SIZE:s);
  else for(UINTN i=0;i<count && c.Io==EFI_SUCCESS;i++) {
    EFI_GUID **protocols=NULL;UINTN pc=0;
    Field(&c,"handle id=0x",(UINTN)handles[i]);End(&c);
    s=bs->ProtocolsPerHandle(handles[i],&protocols,&pc);
    if(s!=EFI_SUCCESS || !protocols || !pc || pc>MAX_PROTOCOLS)Problem(&c,"protocol-snapshot",s==EFI_SUCCESS?EFI_BAD_BUFFER_SIZE:s);
    else for(UINTN j=0;j<pc && c.Io==EFI_SUCCESS;j++) {
      if(!protocols[j]){Problem(&c,"protocol-null-guid",EFI_COMPROMISED_DATA);continue;}
      Text(&c,"protocol guid=");Guid(&c,protocols[j]);End(&c);
      EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *opens=NULL;UINTN oc=0;
      s=bs->OpenProtocolInformation(handles[i],protocols[j],&opens,&oc);
      Field(&c,"opens status=0x",s);Field(&c," count=0x",oc);End(&c);
      if(s!=EFI_SUCCESS || oc>MAX_OPENS || (oc && !opens))Problem(&c,"open-snapshot",s==EFI_SUCCESS?EFI_BAD_BUFFER_SIZE:s);
      else for(UINTN k=0;k<oc;k++) {
        Field(&c,"open agent=0x",(UINTN)opens[k].AgentHandle);
        Field(&c," controller=0x",(UINTN)opens[k].ControllerHandle);
        Field(&c," attributes=0x",opens[k].Attributes);Field(&c," count=0x",opens[k].OpenCount);End(&c);
      }
      FreeCapture(&c,opens,"opens-free");
      Details(&c,handles[i],protocols[j]);
    }
    FreeCapture(&c,protocols,"protocols-free");
  }
  /* Re-enumerate handle identities: detect additions/removals during capture.
   * Equal lists still do not prove atomic open records. Never authorize detach
   * from this log without a fresh live snapshot in the future prepare stage.
   */
  EFI_HANDLE *after=NULL;UINTN ac=0;
  s=bs->LocateHandleBuffer(AllHandles,NULL,NULL,&ac,&after);
  if(s!=EFI_SUCCESS || !after || ac!=count || ac>MAX_HANDLES)Problem(&c,"handle-snapshot-changed",EFI_NOT_READY);
  else if(handles && count<=MAX_HANDLES) {
    for(UINTN i=0;i<count;i++) {
      BOOLEAN found=FALSE;
      for(UINTN j=0;j<ac;j++)if(handles[i]==after[j]){found=TRUE;break;}
      if(!found){Problem(&c,"handle-identity-changed",EFI_NOT_READY);break;}
    }
  }
  FreeCapture(&c,after,"after-free");
  FreeCapture(&c,handles,"handles-free");
  Text(&c,c.Complete?"capture-complete=1\r\n":"capture-complete=0\r\n");
  s=Flush(&c);if(s!=EFI_SUCCESS)return s;
  s=file->Flush(file);if(s!=EFI_SUCCESS)return s;
  return c.Complete?EFI_SUCCESS:EFI_NOT_READY;
}
EFI_STATUS GuCaptureGraph(EFI_SYSTEM_TABLE *t,EFI_FILE_PROTOCOL *file) {
  return GuCaptureGraphOnVolume(t,file,NULL);
}
