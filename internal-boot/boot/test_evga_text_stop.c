/* GPL-2.0-only. Host mocks: never execute captured firmware or access a GPU. */
#include "evga-text-stop.c"
static EFI_BOOT_SERVICES Bs;
static EFI_SYSTEM_TABLE Table;
static EFI_SIMPLE_TEXT_INPUT_PROTOCOL Input;
static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL Output;
static EFI_GRAPHICS_OUTPUT_PROTOCOL Graphics[2];
static EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE Modes[2];
static EFI_DRIVER_BINDING_PROTOCOL Drivers[5];
static EFI_LOADED_IMAGE_PROTOCOL Images[5];
static EFI_PCI_IO_PROTOCOL PciIo;
static EFI_FILE_PROTOCOL Root,Marker;
#ifdef GU_EVGA_UNLOCK
GU_PLATFORM Gu;
static BOOLEAN FailTransaction,FailExclusiveClose,FailAssetCleanup,FailUnlockSafe,FailUnlockGraph;
static UINTN Transactions,ExclusiveCloses,AssetCleanups;
static BOOLEAN SlotRecorded;
static EFI_TPL EFIAPI Raise(EFI_TPL n){(void)n;return TPL_APPLICATION;}
static VOID EFIAPI Restore(EFI_TPL n){(void)n;}
static EFI_STATUS EFIAPI CloseOwner(EFI_HANDLE h,EFI_GUID *g,EFI_HANDLE image,EFI_HANDLE controller){
 (void)g;if(h!=(EFI_HANDLE)4 || image!=(EFI_HANDLE)80 || controller)return EFI_INVALID_PARAMETER;
 ExclusiveCloses++;return FailExclusiveClose?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
EFI_HANDLE GuEvgaResourcesImage(VOID){return (EFI_HANDLE)80;}
BOOLEAN GuEvgaResourcesLogReady(VOID){return TRUE;}
BOOLEAN GuSafeToReturn(VOID){return !FailUnlockSafe;}
EFI_STATUS GuEvgaResourcesRelease(EFI_SYSTEM_TABLE *t){(void)t;AssetCleanups++;return FailAssetCleanup?EFI_DEVICE_ERROR:EFI_SUCCESS;}
EFI_STATUS GuEvgaTransaction(EFI_SYSTEM_TABLE *t,EFI_HANDLE image,EFI_HANDLE parent,EFI_PCI_IO_PROTOCOL *pci,GU_EVGA_GRAPH_CHECK graph,GU_EVGA_LOG_READY ready){
 (void)t;(void)pci;Transactions++;if(FailTransaction)return EFI_DEVICE_ERROR;Gu.owned=TRUE;
 return !FailUnlockGraph && ready() && graph(parent,image,TRUE)?EFI_SUCCESS:EFI_ACCESS_DENIED;
}
#endif
static UINTN Keys,Calls,Records;
static BOOLEAN Removed,MarkerExists,BadHash,BadPost,FailRecord,FailStop,BadInventory,ExtraOwner;
static BOOLEAN Cancel,Timeout,ShortWrite,FlushFail;
#ifdef GU_EVGA_RELEASE_TEXT
static BOOLEAN RemovedVirtual,FailVirtual,BadVirtual;
#ifdef GU_EVGA_RELEASE_GRAPHICS
static BOOLEAN RemovedGraphics,FailGraphics,BadGraphics;
#endif
#endif
#ifdef GU_EVGA_RELEASE_EDID
static BOOLEAN RemovedEdid,FailEdid,BadEdid,BadEdidInventory,ExtraEdidOwner;
#ifdef GU_EVGA_RELEASE_EDID_SIBLINGS
static UINTN SiblingBits,FailSibling,BadSibling;
static BOOLEAN WrongEdidPath;
#endif
#endif
#ifdef GU_EVGA_AUDIT_NVIDIA
static UINT8 PrivateContext[0xa0];
static BOOLEAN BadNvidiaChildren,PrivateFailure,UnknownStored;
#ifdef GU_EVGA_STOP_NVIDIA_CHILDREN
static BOOLEAN NvidiaGone,FailNvidiaStop,BadNvidiaPost;
#ifdef GU_EVGA_STOP_NVIDIA_PARENT
static BOOLEAN ParentGone,FailParent,BadParentOwner,BadParentProtocol;
#endif
#endif
#endif
static UINTN InterfaceFailure;
static volatile UINTN GopConsoleQueries;
static EFI_HANDLE H(UINTN n){return (EFI_HANDLE)n;}
#ifdef GU_EVGA_PROBE_STATE
static BOOLEAN BadPromHash,BadState,FailProm;static UINTN StateReads,PromReads;
#endif
BOOLEAN GuHashMatches(const VOID *data,UINTN n,const char *hash){
#ifdef GU_EVGA_PROBE_STATE
 if(n==0x100000)return !BadPromHash;
#endif
 return data && n && hash && !BadHash;}
static EFI_STATUS EFIAPI Free(VOID *p){return p?EFI_SUCCESS:EFI_INVALID_PARAMETER;}
static EFI_STATUS EFIAPI Stall(UINTN n){(void)n;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI SayMock(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *p,CHAR16 *s){(void)p;(void)s;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI ReadKey(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *p,EFI_INPUT_KEY *k){
  (void)p;if(Keys++==0 || Timeout)return EFI_NOT_READY;k->UnicodeChar=Cancel?0:'G';k->ScanCode=Cancel?SCAN_ESC:0;return EFI_SUCCESS;
}
#ifdef GU_EVGA_PROBE_DECODE
static UINT64 DecodeAttributes;static UINTN ChipReads,AttributeWrites;static BOOLEAN BadChip,FailEnable,FailDisable;
static EFI_STATUS EFIAPI AttributesMock(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION op,UINT64 a,UINT64 *out){
 (void)p;if(op==EfiPciIoAttributeOperationGet){*out=DecodeAttributes;return EFI_SUCCESS;}
 AttributeWrites++;if(a!=EFI_PCI_IO_ATTRIBUTE_MEMORY)return EFI_INVALID_PARAMETER;
 if(op==EfiPciIoAttributeOperationEnable){if(FailEnable)return EFI_DEVICE_ERROR;DecodeAttributes=a;return EFI_SUCCESS;}
 if(op==EfiPciIoAttributeOperationDisable){if(FailDisable)return EFI_DEVICE_ERROR;DecodeAttributes=0;return EFI_SUCCESS;}
 return EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI MemReadMock(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT8 bar,UINT64 off,UINTN n,VOID *v){
 (void)p;if(w!=EfiPciIoWidthUint32 || bar || n!=1 || DecodeAttributes!=EFI_PCI_IO_ATTRIBUTE_MEMORY)return EFI_INVALID_PARAMETER;
#ifdef GU_EVGA_PROBE_STATE
 if(off>=0x300000 && off<=0x3ffffc && !(off&3)){PromReads++;if(FailProm)return EFI_DEVICE_ERROR;*(UINT32 *)v=0;return EFI_SUCCESS;}
 if(off){static const UINT32 a[]={0x118128,0x118234,0x110100,0x1fa828,0x823804,0x82381c,0x823820,0x1183a4};for(UINTN i=0;i<8;i++)if(off==a[i]){StateReads++;*(UINT32 *)v=BadState?0xbadf0000U:0;return EFI_SUCCESS;}return EFI_INVALID_PARAMETER;}
#else
 if(off)return EFI_INVALID_PARAMETER;
#endif
 ChipReads++;*(UINT32 *)v=BadChip?0xffffffffU:0xb72000a1U;return EFI_SUCCESS;
}
#endif
#ifdef GU_EVGA_PROBE_PLATFORM
static BOOLEAN BadPlatformConfig,BadBar;
static EFI_STATUS EFIAPI LocationMock(EFI_PCI_IO_PROTOCOL *p,UINTN *a,UINTN *b,UINTN *c,UINTN *d){(void)p;*a=*c=*d=0;*b=1;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI BarMock(EFI_PCI_IO_PROTOCOL *p,UINT8 bar,UINT64 *supports,VOID **out){
 (void)p;(void)supports;if(bar)return EFI_INVALID_PARAMETER;static EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR r;
 r=(EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR){0};r.Desc=ACPI_ADDRESS_SPACE_DESCRIPTOR;r.Len=sizeof(r)-3;r.AddrSpaceGranularity=32;r.AddrRangeMin=BadBar?0x85000000:0x84000000;r.AddrRangeMax=0x84ffffff;r.AddrLen=0x1000000;*out=&r;return EFI_SUCCESS;
}
#endif
static EFI_STATUS EFIAPI ReadPci(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 off,UINTN n,VOID *v){
  (void)p;(void)w;
#ifdef GU_EVGA_PROBE_PLATFORM
 if(off || (n!=16 && n!=64))return EFI_INVALID_PARAMETER;
#else
 if(off || n!=16)return EFI_INVALID_PARAMETER;
#endif
  UINT32 *cfg=v;for(UINTN i=0;i<n;i++)cfg[i]=0;
  #ifdef GU_EVGA_PROBE_DECODE
  cfg[1]=DecodeAttributes?2:0;
#endif
  #ifdef GU_EVGA_PROBE_PLATFORM
 if(n==64){cfg[1]|=0x00100000;cfg[0x34/4]=0x40;cfg[0x40/4]=0x00034801;cfg[0x48/4]=0x00020010;cfg[0x4c/4]=0x10000000;cfg[0x10/4]=0x84000000;cfg[0xc8/4]=0x00050011;if(BadPlatformConfig)cfg[0x40/4]=0;}
#endif
 cfg[0]=BadInventory?0x123410deU:0x220410deU;cfg[2]=0x03000000U;
#ifdef GU_EVGA_PROBE_PLATFORM
 cfg[2]=0x030000a1U;
#endif
 cfg[11]=0x33873842U;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Locate(EFI_LOCATE_SEARCH_TYPE type,EFI_GUID *guid,VOID *key,UINTN *n,EFI_HANDLE **out){
  (void)type;(void)key;static EFI_HANDLE g[2],p[1];g[0]=H(1);g[1]=H(2);p[0]=H(4);
#ifdef GU_EVGA_STOP_NVIDIA_CHILDREN
  static EFI_HANDLE remaining[4];remaining[0]=H(2);remaining[1]=H(3);remaining[2]=H(4);remaining[3]=H(14);
  if(type==AllHandles){*n=4;*out=remaining;return EFI_SUCCESS;}
  if(guid==&Gop && NvidiaGone&&!BadNvidiaPost){*n=1;*out=remaining;return EFI_SUCCESS;}
  if(guid==&Edid && NvidiaGone){*n=0;*out=NULL;return EFI_NOT_FOUND;}
#endif
  if(guid==&Gop){*n=2;*out=g;return EFI_SUCCESS;}
#ifdef GU_EVGA_RELEASE_EDID
  static EFI_HANDLE edids[3];edids[0]=H(1);edids[1]=H(5);edids[2]=H(6);
  if(guid==&Edid){*n=BadEdidInventory?2:3;*out=edids;return EFI_SUCCESS;}
#endif
  if(guid==&Pci){*n=1;*out=p;return EFI_SUCCESS;}return EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI Handle(EFI_HANDLE h,EFI_GUID *guid,VOID **out){
  *out=NULL;
  if(guid==&Text && ((InterfaceFailure==1 && h==H(2)) || (InterfaceFailure==2 && h==H(3))))return EFI_NOT_FOUND;
  if(guid==&Gop && h==H(3))GopConsoleQueries++;
  if(guid==&Gop && h==H(3) && InterfaceFailure==4){*out=&Graphics[0];return EFI_SUCCESS;}
  if(guid==&Gop && (h==H(1)||h==H(2))){*out=&Graphics[(UINTN)h-1];return EFI_SUCCESS;}
#ifdef GU_EVGA_RELEASE_EDID_SIBLINGS
  if(guid==&Path && (h==H(1)||h==H(5)||h==H(6))){
    static UINT8 paths[3][36];static const UINT8 raw[36]={0x02,0x01,0x0c,0,0xd0,0x41,3,0x0a,0,0,0,0,1,1,6,0,0,1,1,1,6,0,0,0,2,3,8,0,0x20,3,1,0x80,0x7f,0xff,4,0};
    UINTN k=h==H(1)?0:(h==H(5)?1:2);for(UINTN i=0;i<36;i++)paths[k][i]=raw[i];
    paths[k][28]=k==0?0x20:(k==1?0x40:0x70);if(WrongEdidPath && k==2)paths[k][23]=1;
    *out=paths[k];return EFI_SUCCESS;
  }
#endif
  if(guid==&Path && h==H(1)){*out=&Output;return EFI_SUCCESS;}
#ifdef GU_EVGA_RELEASE_TEXT
  if(guid==&Text && h==H(2) && RemovedVirtual)return EFI_UNSUPPORTED;
#endif
  if(guid==&Text && (h==H(2)||h==H(3))){*out=&Output;return EFI_SUCCESS;}
  if(guid==&Binding && (UINTN)h>=10 && (UINTN)h<=14){*out=&Drivers[(UINTN)h-10];return EFI_SUCCESS;}
  if(guid==&Loaded && (UINTN)h>=10 && (UINTN)h<=14){*out=&Images[(UINTN)h-10];return EFI_SUCCESS;}
#ifdef GU_EVGA_AUDIT_NVIDIA
  if(h==H(4) && guid->Data1==0xfe47349a){
    if(PrivateFailure)return EFI_UNSUPPORTED;
    *(EFI_HANDLE *)(PrivateContext+0x88)=UnknownStored?H(99):H(1);
#ifdef GU_EVGA_STOP_NVIDIA_CHILDREN
    if(NvidiaGone)*(EFI_HANDLE *)(PrivateContext+0x88)=NULL;
#endif
    *out=PrivateContext;return EFI_SUCCESS;
  }
#endif
  if(guid==&Pci && h==H(4)){*out=&PciIo;return EFI_SUCCESS;}return EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI ProtocolList(EFI_HANDLE h,EFI_GUID ***out,UINTN *n){
  static EFI_GUID *list[3];
#ifdef GU_EVGA_STOP_NVIDIA_PARENT
  static EFI_GUID privateGuid={0xfe47349a,0x7f0d,0x4641,{0x82,0x2b,0x34,0xba,0xa2,0x8e,0xcd,0xd0}};
  if(h==H(4) && ParentGone){list[0]=&Pci;list[1]=&privateGuid;*out=list;*n=BadParentProtocol?2:1;return EFI_SUCCESS;}
#endif
#ifdef GU_EVGA_RELEASE_TEXT
  static EFI_GUID extra={0xe1e4a857,0xc970,0x4075,{0xa4,0xda,0xe9,0xc4,0x1b,0x69,0xad,0xfc}};
  if(h==H(2)){list[0]=&Gop;list[1]=&Text;list[2]=&extra;*out=list;*n=RemovedVirtual&&!BadVirtual?1:3;return EFI_SUCCESS;}
#endif
  if(h!=H(3))return EFI_UNSUPPORTED;
  if(InterfaceFailure==3)return EFI_DEVICE_ERROR;
  list[0]=InterfaceFailure==4?&Gop:&Text;list[1]=&Text;
  *out=list;*n=InterfaceFailure==5?2:1;
  if(InterfaceFailure==6)list[0]=NULL;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Opens(EFI_HANDLE source,EFI_GUID *guid,EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **out,UINTN *n){
  static EFI_OPEN_PROTOCOL_INFORMATION_ENTRY e[3];*out=e;*n=0;EFI_HANDLE agent=NULL,child=NULL;
  if(source==H(1)&&guid==&Gop){
#ifdef GU_EVGA_RELEASE_GRAPHICS
    if(RemovedGraphics&&!BadGraphics)return EFI_SUCCESS;
#endif
    agent=H(10);child=H(2);
  }
  if(source==H(2)&&guid==&Gop){
#ifdef GU_EVGA_RELEASE_TEXT
    if(RemovedVirtual&&!BadVirtual)return EFI_SUCCESS;
#endif
    agent=H(11);
  }
  if(source==H(2)&&guid==&Text){if(Removed&&!BadPost)return EFI_SUCCESS;agent=H(12);child=H(3);}
#ifdef GU_EVGA_RELEASE_EDID
  if(guid==&Edid && (source==H(1)||source==H(5)||source==H(6))) {
    if(RemovedEdid&&!BadEdid&&source==H(1))return EFI_SUCCESS;
#ifdef GU_EVGA_RELEASE_EDID_SIBLINGS
    UINTN bit=source==H(5)?1:(source==H(6)?2:0);
    if(bit && (SiblingBits&bit) && BadSibling!=bit)return EFI_SUCCESS;
#endif
    agent=H(13);if(ExtraEdidOwner && source==H(5))agent=H(99);
  }
#endif
#ifdef GU_EVGA_AUDIT_NVIDIA
  if(source==H(4)&&guid==&Pci){
#ifdef GU_EVGA_STOP_NVIDIA_PARENT
    if(ParentGone&&!BadParentOwner){
#ifdef GU_EVGA_UNLOCK
      if(Gu.owned){e[0]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){H(80),NULL,EFI_OPEN_PROTOCOL_EXCLUSIVE,1};*n=1;return EFI_SUCCESS;}
#endif
      *n=0;return EFI_SUCCESS;
    }
#endif
    agent=H(14);
  }
  if(source==H(4)&&guid==&Path){
#ifdef GU_EVGA_STOP_NVIDIA_CHILDREN
    if(NvidiaGone){*n=0;return EFI_SUCCESS;}
#endif
    for(UINTN i=0;i<3;i++)e[i]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){H(14),H(i==0?1:(i==1?5:6)),8,1};
    *n=BadNvidiaChildren?2:3;return EFI_SUCCESS;
  }
#endif
  if(!agent)return EFI_UNSUPPORTED;
  e[0]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){agent,source,16,1};*n=1;
  if(child){e[1]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){agent,child,8,1};*n=2;}
  if(ExtraOwner){e[*n]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){H(99),source,16,1};(*n)++;}
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI MockStop(EFI_DRIVER_BINDING_PROTOCOL *d,EFI_HANDLE controller,UINTN n,EFI_HANDLE *children){
#ifdef GU_EVGA_STOP_NVIDIA_CHILDREN
  if(d==&Drivers[4]){
#ifdef GU_EVGA_STOP_NVIDIA_PARENT
    if(n==0){if(controller!=H(4)||children||!NvidiaGone)return EFI_INVALID_PARAMETER;Calls++;if(FailParent)return EFI_DEVICE_ERROR;ParentGone=TRUE;return EFI_SUCCESS;}
#endif
    if(controller!=H(4)||n!=3||!children||children[0]!=H(1)||children[1]!=H(5)||children[2]!=H(6)||SiblingBits!=3)return EFI_INVALID_PARAMETER;
    Calls++;if(FailNvidiaStop)return EFI_DEVICE_ERROR;NvidiaGone=TRUE;return EFI_SUCCESS;
  }
#endif
#ifdef GU_EVGA_RELEASE_EDID
#ifdef GU_EVGA_RELEASE_EDID_SIBLINGS
  if(d==&Drivers[3] && (controller==H(5)||controller==H(6))){
    if(n||children || !RemovedEdid)return EFI_INVALID_PARAMETER;
    Calls++;UINTN bit=controller==H(5)?1:2;if(FailSibling==bit)return EFI_DEVICE_ERROR;
    SiblingBits|=bit;return EFI_SUCCESS;
  }
#endif
  if(d==&Drivers[3]){if(controller!=H(1)||n||children)return EFI_INVALID_PARAMETER;Calls++;if(FailEdid)return EFI_DEVICE_ERROR;RemovedEdid=TRUE;return EFI_SUCCESS;}
#endif
#ifdef GU_EVGA_RELEASE_GRAPHICS
  if(d==&Drivers[0]){if(controller!=H(1)||n!=1||!children||children[0]!=H(2))return EFI_INVALID_PARAMETER;Calls++;if(FailGraphics)return EFI_DEVICE_ERROR;RemovedGraphics=TRUE;return EFI_SUCCESS;}
#endif
#ifdef GU_EVGA_RELEASE_TEXT
  if(d==&Drivers[1]){if(controller!=H(2)||n||children)return EFI_INVALID_PARAMETER;Calls++;if(FailVirtual)return EFI_DEVICE_ERROR;RemovedVirtual=TRUE;return EFI_SUCCESS;}
#endif
  if(d!=&Drivers[2] || controller!=H(2) || n!=1 || !children || children[0]!=H(3))return EFI_INVALID_PARAMETER;
  Calls++;if(FailStop)return EFI_DEVICE_ERROR;Removed=TRUE;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI FileClose(EFI_FILE_PROTOCOL *p){(void)p;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI FileFlush(EFI_FILE_PROTOCOL *p){(void)p;return FlushFail?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI FileWrite(EFI_FILE_PROTOCOL *p,UINTN *n,VOID *buffer){(void)p;(void)buffer;if(ShortWrite)(*n)--;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI FileOpen(EFI_FILE_PROTOCOL *p,EFI_FILE_PROTOCOL **out,CHAR16 *name,UINT64 mode,UINT64 attrs){
  (void)p;(void)name;(void)attrs;*out=NULL;
#if defined(GU_EVGA_UNLOCK) && GU_EVGA_ATTEMPT_SLOT == 5
  {const CHAR16 *expected=L"\\RTX3090-UNLOCK\\EVGA-UNLOCK5.ARMED";UINTN i=0;while(expected[i] && name[i]==expected[i])i++;if(name[i]!=expected[i])return EFI_INVALID_PARAMETER;}
#elif defined(GU_EVGA_UNLOCK) && GU_EVGA_ATTEMPT_SLOT == 10
  {const CHAR16 *expected=L"\\RTX3090-UNLOCK\\EVGA-UNLOCK10.ARMED";UINTN i=0;while(expected[i] && name[i]==expected[i])i++;if(name[i]!=expected[i])return EFI_INVALID_PARAMETER;}
#endif
  if(mode==EFI_FILE_MODE_READ && !MarkerExists)return EFI_NOT_FOUND;
  MarkerExists=TRUE;*out=&Marker;return EFI_SUCCESS;
}
static EFI_STATUS RecordMock(VOID *p,const char *name,EFI_STATUS s){
#ifdef GU_EVGA_UNLOCK
  const char *prefix="evga-unlock-slot-";UINTN i=0;while(prefix[i] && name[i]==prefix[i])i++;
  if(!prefix[i] && name[i]==(char)('0'+GU_EVGA_ATTEMPT_SLOT/10) && name[i+1]==(char)('0'+GU_EVGA_ATTEMPT_SLOT%10) && !name[i+2])SlotRecorded=TRUE;
#endif
  (void)p;(void)name;(void)s;Records++;return FailRecord?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static VOID Init(VOID){
#ifdef GU_EVGA_UNLOCK
 Gu=(GU_PLATFORM){0};FailTransaction=FailExclusiveClose=FailAssetCleanup=FailUnlockSafe=FailUnlockGraph=FALSE;
 Transactions=ExclusiveCloses=AssetCleanups=0;SlotRecorded=FALSE;
#endif
#ifdef GU_EVGA_PROBE_PLATFORM
 BadPlatformConfig=BadBar=FALSE;
#endif
#ifdef GU_EVGA_PROBE_STATE
 BadPromHash=BadState=FailProm=FALSE;StateReads=PromReads=0;
#endif
#ifdef GU_EVGA_PROBE_DECODE
 DecodeAttributes=ChipReads=AttributeWrites=0;BadChip=FailEnable=FailDisable=FALSE;
#endif
#ifdef GU_EVGA_STOP_NVIDIA_PARENT
  NvidiaParentStopped=ParentGone=FailParent=BadParentOwner=BadParentProtocol=FALSE;
#endif
#ifdef GU_EVGA_STOP_NVIDIA_CHILDREN
  NvidiaChildrenStopped=NvidiaGone=FailNvidiaStop=BadNvidiaPost=FALSE;NvidiaDriver=NULL;
#endif
#ifdef GU_EVGA_AUDIT_NVIDIA
  NvidiaParent=NvidiaAgent=NvidiaStoredChild=NULL;NvidiaSaved=FALSE;NvidiaChildClass=NvidiaFlag=0;
  BadNvidiaChildren=PrivateFailure=UnknownStored=FALSE;for(UINTN i=0;i<sizeof(PrivateContext);i++)PrivateContext[i]=0;
#endif
#ifdef GU_EVGA_RELEASE_EDID
#ifdef GU_EVGA_RELEASE_EDID_SIBLINGS
  WrongEdidPath=FALSE;EdidSiblingReleased[0]=EdidSiblingReleased[1]=NULL;SiblingBits=FailSibling=BadSibling=0;
#endif
  EdidAgent=NULL;EdidDriver=NULL;EdidReleased=RemovedEdid=FailEdid=BadEdid=BadEdidInventory=ExtraEdidOwner=FALSE;
  for(UINTN i=0;i<3;i++)EdidHandles[i]=NULL;
#endif
#ifdef GU_EVGA_RELEASE_TEXT
#ifdef GU_EVGA_RELEASE_GRAPHICS
  GraphicsDriver=NULL;GraphicsReleased=RemovedGraphics=FailGraphics=BadGraphics=FALSE;
#endif
  VirtualDriver=NULL;TextReleased=RemovedVirtual=FailVirtual=BadVirtual=FALSE;
#endif
  GopConsoleQueries=0;InterfaceFailure=0;Plan=(PLAN){0};Keys=Calls=Records=0;Removed=MarkerExists=BadHash=BadPost=FailRecord=FailStop=BadInventory=ExtraOwner=FALSE;
  Cancel=Timeout=ShortWrite=FlushFail=FALSE;
  Bs=(EFI_BOOT_SERVICES){0};Bs.FreePool=Free;Bs.Stall=Stall;Bs.LocateHandleBuffer=Locate;Bs.HandleProtocol=Handle;Bs.OpenProtocolInformation=Opens;Bs.ProtocolsPerHandle=ProtocolList;
#ifdef GU_EVGA_UNLOCK
 Bs.RaiseTPL=Raise;Bs.RestoreTPL=Restore;Bs.CloseProtocol=CloseOwner;
#endif
  Input=(EFI_SIMPLE_TEXT_INPUT_PROTOCOL){0};Input.ReadKeyStroke=ReadKey;
  Output=(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL){0};Output.OutputString=SayMock;
  Table=(EFI_SYSTEM_TABLE){0};Table.BootServices=&Bs;Table.ConIn=&Input;Table.ConOut=&Output;Table.ConsoleOutHandle=H(3);Table.StandardErrorHandle=H(3);
  for(UINTN i=0;i<2;i++){Modes[i]=(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE){0};Graphics[i]=(EFI_GRAPHICS_OUTPUT_PROTOCOL){0};Graphics[i].Mode=&Modes[i];}
  Modes[0].FrameBufferBase=0x100000;Modes[0].FrameBufferSize=0x200000;
  UINTN bytes[5]={0x2180,0x3bc0,0x6ae0,0x9000,0x2b700},stops[5]={0x88c,0xb2c,0x104c,0x14d4,0x698};
  for(UINTN i=0;i<5;i++){
    Drivers[i]=(EFI_DRIVER_BINDING_PROTOCOL){0};Drivers[i].DriverBindingHandle=Drivers[i].ImageHandle=H(10+i);Drivers[i].Version=i==4?0x60009:(i==3?1:(i?16:17));Drivers[i].Stop=MockStop;
    Images[i]=(EFI_LOADED_IMAGE_PROTOCOL){0};Images[i].ImageBase=(VOID *)((UINTN)MockStop-stops[i]);Images[i].ImageSize=bytes[i];
  }
  PciIo=(EFI_PCI_IO_PROTOCOL){0};PciIo.Pci.Read=ReadPci;
#ifdef GU_EVGA_PROBE_PLATFORM
 PciIo.GetLocation=LocationMock;PciIo.GetBarAttributes=BarMock;
#endif
#ifdef GU_EVGA_PROBE_DECODE
 PciIo.Attributes=AttributesMock;PciIo.Mem.Read=MemReadMock;
#endif
  Root=(EFI_FILE_PROTOCOL){0};Root.Open=FileOpen;Marker=(EFI_FILE_PROTOCOL){0};Marker.Close=FileClose;Marker.Write=FileWrite;Marker.Flush=FileFlush;
}
#if defined(GU_EVGA_PROBE_RESOURCES) && !defined(GU_EVGA_UNLOCK)
static BOOLEAN ResourceRefusal;
EFI_STATUS GuEvgaResourcesClaim(EFI_SYSTEM_TABLE *t,EFI_HANDLE h,EFI_PCI_IO_PROTOCOL *p,GU_EVGA_RECORD r){(void)t;(void)h;(void)p;(void)r;return ResourceRefusal?EFI_ACCESS_DENIED:EFI_SUCCESS;}
#endif
#ifdef GU_EVGA_UNLOCK
int main(VOID){BOOLEAN started=FALSE;EFI_STATUS s;
 Init();s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_SUCCESS)return 60+(int)(Transactions*10+ExclusiveCloses*2+AssetCleanups);if(!started)return 50;if(Calls!=8)return 51;if(Transactions!=1)return 52;if(ExclusiveCloses!=1)return 53;if(AssetCleanups!=1)return 54;if(Gu.owned)return 55;if(DecodeAttributes)return 56;if(!SlotRecorded)return 57;
 if(GuEvgaTextStopVerify(&Table)!=EFI_SUCCESS)return 2;
 Init();FailTransaction=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS || !started || Transactions!=1 || ExclusiveCloses || AssetCleanups || AttributeWrites!=1)return 3;
 Init();FailUnlockSafe=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS || ExclusiveCloses || AssetCleanups || AttributeWrites!=1)return 4;
 Init();FailExclusiveClose=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS || ExclusiveCloses!=1 || AssetCleanups || !Gu.owned)return 5;
 Init();FailAssetCleanup=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS || ExclusiveCloses!=1 || AssetCleanups!=1 || Gu.owned)return 6;
 Init();FailUnlockGraph=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS || ExclusiveCloses || AssetCleanups || AttributeWrites!=1)return 7;
 Init();FailDisable=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS || ExclusiveCloses || AssetCleanups)return 8;
 Init();s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_SUCCESS)return 9;Gu.fault=TRUE;if(GuEvgaTextStopVerify(&Table)==EFI_SUCCESS)return 10;
 return 0;
}
#elif defined(GU_EVGA_PROBE_PLATFORM)
int main(VOID){
 BOOLEAN started=FALSE;EFI_STATUS s;
 Init();s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_SUCCESS||Calls!=8||StateReads!=16||PromReads!=262144||AttributeWrites!=2||DecodeAttributes)return 1;
 UINTN saved=Records;if(GuEvgaTextStopVerify(&Table)!=EFI_SUCCESS||Records!=saved)return 2;
 Init();BadPlatformConfig=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||StateReads||PromReads||AttributeWrites!=1)return 3;
 Init();BadBar=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||StateReads||PromReads||AttributeWrites!=1)return 4;
 Init();FailEnable=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||StateReads||PromReads)return 5;
#ifdef GU_EVGA_PROBE_RESOURCES
 Init();ResourceRefusal=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||StateReads||PromReads||AttributeWrites!=1)return 6;
#endif
 return 0;
}
#elif defined(GU_EVGA_PROBE_STATE)
int main(VOID){
 BOOLEAN started=FALSE;EFI_STATUS s;
 Init();s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_SUCCESS||ChipReads!=2||StateReads!=16||PromReads!=262144||AttributeWrites!=2||DecodeAttributes)return 1;
 UINTN saved=Records;if(GuEvgaTextStopVerify(&Table)!=EFI_SUCCESS||Records!=saved||StateReads!=16||PromReads!=262144)return 2;
 Init();BadState=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||StateReads!=1||PromReads||AttributeWrites!=1)return 3;
 Init();FailProm=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||StateReads!=16||PromReads!=1||AttributeWrites!=1)return 4;
 Init();BadPromHash=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||PromReads!=262144||AttributeWrites!=1)return 5;
 Init();FailEnable=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||StateReads||PromReads)return 6;
 return 0;
}
#elif defined(GU_EVGA_PROBE_DECODE)
int main(VOID){
 BOOLEAN started=FALSE;EFI_STATUS s;
 Init();s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_SUCCESS||Calls!=8||ChipReads!=2||AttributeWrites!=2||DecodeAttributes)return 1;
 UINTN saved=Records;if(GuEvgaTextStopVerify(&Table)!=EFI_SUCCESS||Records!=saved||ChipReads!=2||AttributeWrites!=2)return 2;
 Init();FailEnable=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||ChipReads||AttributeWrites!=1)return 3;
 Init();BadChip=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||ChipReads!=1||AttributeWrites!=1)return 4;
 Init();FailDisable=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||ChipReads!=2||AttributeWrites!=2)return 5;
 Init();s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_SUCCESS)return 6;DecodeAttributes=EFI_PCI_IO_ATTRIBUTE_MEMORY;if(GuEvgaTextStopVerify(&Table)==EFI_SUCCESS)return 7;
 Init();FailParent=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||AttributeWrites||ChipReads)return 8;
 return 0;
}
#elif defined(GU_EVGA_STOP_NVIDIA_PARENT)
int main(VOID){
  BOOLEAN started=FALSE;EFI_STATUS s;
  Init();s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_SUCCESS||Calls!=8||!ParentGone)return 1;
  UINTN saved=Records;if(GuEvgaTextStopVerify(&Table)!=EFI_SUCCESS||saved!=Records)return 2;
  Init();FailParent=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=8||ParentGone)return 3;
  Init();BadParentOwner=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=8)return 4;
  Init();BadParentProtocol=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=8)return 5;
  Init();FailNvidiaStop=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=7||ParentGone)return 6;
  Init();BadNvidiaPost=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=7||ParentGone)return 7;
  Init();MarkerExists=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_ALREADY_STARTED||Calls)return 8;
  Init();BadHash=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls)return 9;
  Init();s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_SUCCESS)return 10;
  BadInventory=TRUE;if(GuEvgaTextStopVerify(&Table)==EFI_SUCCESS)return 11;
  return 0;
}
#elif defined(GU_EVGA_STOP_NVIDIA_CHILDREN)
int main(VOID){
  BOOLEAN started=FALSE;EFI_STATUS s;
  Init();s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_SUCCESS||Calls!=7||!NvidiaGone)return 1;
  UINTN saved=Records;if(GuEvgaTextStopVerify(&Table)!=EFI_SUCCESS||saved!=Records)return 2;
  Init();FailNvidiaStop=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=7||NvidiaGone)return 3;
  Init();BadNvidiaPost=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=7)return 4;
  Init();FailSibling=2;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=6||NvidiaGone)return 5;
  Init();PrivateContext[0x98]=1;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls)return 6;
  Init();BadNvidiaChildren=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls)return 7;
  Init();MarkerExists=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_ALREADY_STARTED||Calls)return 8;
  Init();BadHash=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls)return 9;
  Init();s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_SUCCESS)return 10;
  BadNvidiaPost=TRUE;if(GuEvgaTextStopVerify(&Table)==EFI_SUCCESS)return 11;
  return 0;
}
#elif defined(GU_EVGA_AUDIT_NVIDIA)
int main(VOID){
  BOOLEAN started=FALSE;EFI_STATUS s;
  Init();s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_SUCCESS||Calls!=6||!NvidiaSaved||NvidiaChildClass!=1)return 1;
  UINTN saved=Records;if(GuEvgaTextStopVerify(&Table)!=EFI_SUCCESS||saved!=Records)return 2;
  Init();BadNvidiaChildren=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls)return 3;
  Init();PrivateFailure=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls)return 4;
  Init();UnknownStored=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls)return 5;
  Init();Drivers[4].Version++;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls)return 6;
  Init();PrivateContext[0x98]=2;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls)return 7;
  Init();s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_SUCCESS)return 8;
  PrivateContext[0x98]=1;if(GuEvgaTextStopVerify(&Table)==EFI_SUCCESS)return 9;
  Init();MarkerExists=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_ALREADY_STARTED||Calls)return 10;
  Init();FailSibling=1;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=5)return 11;
  return 0;
}
#elif defined(GU_EVGA_RELEASE_EDID_SIBLINGS)
int main(VOID){
  BOOLEAN started=FALSE;EFI_STATUS s;
  Init();s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_SUCCESS||Calls!=6||SiblingBits!=3)return 1;
  UINTN saved=Records;if(GuEvgaTextStopVerify(&Table)!=EFI_SUCCESS||saved!=Records)return 2;
  Init();FailSibling=1;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=5||SiblingBits)return 3;
  Init();FailSibling=2;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=6||SiblingBits!=1)return 4;
  Init();BadSibling=1;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=5||SiblingBits!=1)return 5;
  Init();BadSibling=2;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=6||SiblingBits!=3)return 6;
  Init();FailEdid=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=4||SiblingBits)return 7;
  Init();BadEdidInventory=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls)return 8;
  Init();MarkerExists=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_ALREADY_STARTED||Calls)return 9;
  Init();BadHash=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls)return 10;
  Init();s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_SUCCESS)return 11;
  BadSibling=2;if(GuEvgaTextStopVerify(&Table)==EFI_SUCCESS)return 12;
  Init();WrongEdidPath=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls)return 13;
  return 0;
}
#elif defined(GU_EVGA_RELEASE_EDID)
int main(VOID){
  BOOLEAN started=FALSE;EFI_STATUS s;
  Init();s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_SUCCESS||Calls!=4||!RemovedEdid)return 1;
  UINTN saved=Records;if(GuEvgaTextStopVerify(&Table)!=EFI_SUCCESS||saved!=Records)return 2;
  Init();FailEdid=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=4)return 3;
  Init();BadEdid=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=4)return 4;
  Init();BadEdidInventory=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls)return 5;
  Init();ExtraEdidOwner=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls)return 6;
  Init();FailGraphics=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=3||RemovedEdid)return 7;
  Init();Drivers[3].Version=2;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls)return 8;
  Init();MarkerExists=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_ALREADY_STARTED||Calls)return 9;
  Init();BadHash=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls)return 10;
  Init();s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_SUCCESS)return 11;
  ExtraEdidOwner=TRUE;if(GuEvgaTextStopVerify(&Table)==EFI_SUCCESS)return 12;
  return 0;
}
#elif defined(GU_EVGA_RELEASE_GRAPHICS)
int main(VOID){
  BOOLEAN started=FALSE;EFI_STATUS s;
  Init();s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_SUCCESS||Calls!=3||!RemovedGraphics)return 1;
  UINTN saved=Records;if(GuEvgaTextStopVerify(&Table)!=EFI_SUCCESS||saved!=Records)return 2;
  Init();FailGraphics=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=3)return 3;
  Init();BadGraphics=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=3)return 4;
  Init();FailVirtual=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=2||RemovedGraphics)return 5;
  Init();FailStop=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=1||RemovedGraphics)return 6;
  Init();MarkerExists=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_ALREADY_STARTED||Calls)return 7;
  Init();BadHash=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls)return 8;
  return 0;
}
#elif defined(GU_EVGA_RELEASE_TEXT)
int main(VOID){
  BOOLEAN started=FALSE;EFI_STATUS s;
  Init();s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);
  if(s!=EFI_SUCCESS||!started||Calls!=2||!RemovedVirtual)return 1;
  UINTN saved=Records;if(GuEvgaTextStopVerify(&Table)!=EFI_SUCCESS||saved!=Records)return 2;
  Init();FailVirtual=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=2||!started)return 3;
  Init();BadVirtual=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=2)return 4;
  Init();FailStop=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=1||RemovedVirtual)return 5;
  Init();MarkerExists=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_ALREADY_STARTED||Calls)return 6;
  Init();BadHash=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls)return 7;
  return 0;
}
#else
int main(VOID){
  BOOLEAN started=FALSE;EFI_STATUS s;
  Init();s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_SUCCESS||!started||Calls!=1||!Removed||Records<3)return 1;
  UINTN savedRecords=Records;
  if(GuEvgaTextStopVerify(&Table)!=EFI_SUCCESS || Records!=savedRecords)return 2;
  Init();BadHash=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls||started)return 3;
  Init();MarkerExists=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_ALREADY_STARTED||Calls||started)return 4;
  Init();FailRecord=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls||started)return 5;
  Init();FailStop=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=1||!started)return 6;
  Init();BadPost=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls!=1||!started)return 7;
  Init();BadInventory=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls||started)return 8;
  Init();ExtraOwner=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls||started)return 9;
  Init();Table.StandardErrorHandle=H(99);s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls||started)return 10;
  Init();Drivers[2].Version++;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls||started)return 11;
  Init();Drivers[2].Stop=NULL;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls||started)return 12;
  Init();Cancel=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_ABORTED||Calls||started||MarkerExists)return 13;
  Init();Timeout=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_TIMEOUT||Calls||started||MarkerExists)return 14;
  Init();ShortWrite=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls||started)return 15;
  Init();FlushFail=TRUE;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls||started)return 16;
  Init();s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_SUCCESS)return 17;
  ExtraOwner=TRUE;if(GuEvgaTextStopVerify(&Table)==EFI_SUCCESS)return 18;
  for(UINTN i=1;i<=6;i++){Init();InterfaceFailure=i;s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s==EFI_SUCCESS||Calls||started||MarkerExists)return 18+(int)i;}
  Init();s=GuEvgaTextStop(&Table,&Root,RecordMock,&started);if(s!=EFI_SUCCESS||GopConsoleQueries)return 25;
  return 0;
}

#endif
