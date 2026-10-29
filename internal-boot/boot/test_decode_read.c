/* GPL-2.0-only. Real combined lifecycle/decoder engine with RAM-only MMIO. */
#define GU_AD104_DECODE_READ_TEST
#define main ParentMain
#include "test_owner_parent.c"
#undef main
static int BatchFileCalls;
#ifdef GU_AD104_FALCON_OBSERVE_TEST
static int FalconFileCalls;
#ifdef GU_AD104_FALCON_REFUSAL_TEST
static int RecoveryWitnessRecords;
EFI_STATUS GuRefusalWitness(const char *event,EFI_STATUS status){
  RecoveryWitnessRecords++;
  if(!strcmp(event,"falcon-observation-refused")){Check(status==EFI_NOT_READY,"original observation failure preserved");if(FalconFault==640)Config[1]|=4;}
  else Check(status==EFI_SUCCESS,"recovery witness status");
  if((FalconFault==630 && RecoveryWitnessRecords==1) || (FalconFault==631 && RecoveryWitnessRecords==2) || (FalconFault==643 && RecoveryWitnessRecords==3))return EFI_DEVICE_ERROR;
  if(FalconFault==641 && RecoveryWitnessRecords==2)Io.Mem.Read=NULL;
  if(FalconFault==642 && RecoveryWitnessRecords==2)Missing=-1;
  if(FalconFault==644 && RecoveryWitnessRecords==3)Config[1]|=4;
  return EFI_SUCCESS;
}
#endif
#endif
#ifdef GU_AD104_PROM_HEADER_TEST
static int PromFileCalls;
#endif
static EFI_STATUS EFIAPI ReadBar(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH width,
                                UINT8 bar,UINT64 offset,UINTN count,VOID *out) {
  (void)io;Check(width==EfiPciIoWidthUint32 && !bar && count==1 && DecodeCalls==1 && Stage==6,
    "MMIO only after explicit decode Enable");
#ifdef GU_AD104_FALCON_OBSERVE_TEST
#ifdef GU_AD104_FALCON_REFUSAL_TEST
  if(FalconRecoveryAttempted && MmioCalls>=262329){
    UINTN n=MmioCalls-262329,j=n%7;Check(n<14 && offset==FalconRecoveryOffsets[j],"only bounded required recovery reads");
    if(!j)FalconFileCalls=Writes+Flushes;
    Check(Writes+Flushes==FalconFileCalls,"no file callbacks in recovery read batch");
    MmioCalls++;UINT32 value=j==0 || j==6?0x194000a1:j==1?0x10:0;
    if(FalconFault>=601 && FalconFault<=614 && n==(UINTN)(FalconFault-601))return EFI_DEVICE_ERROR;
    if(FalconFault==620 && j==0)value=0xbadf5620;
    if(FalconFault==621 && j==4)value^=1;
    if(FalconFault==622 && j==2)Io.Mem.Read=NULL;
    if(FalconFault==623 && n==6)Config[1]|=4;
    *(UINT32 *)out=value;return EFI_SUCCESS;
  }
#endif
  if(MmioCalls>=262320){
    UINTN n=MmioCalls-262320,j=n%FalconWords;Check(n<2*FalconWords && offset==FalconOffsets[j],"exact Falcon read order and bound");
    if(!j)FalconFileCalls=Writes+Flushes;
    Check(Writes+Flushes==FalconFileCalls,"no file callbacks inside Falcon batch");
#if defined(GU_AD104_FALCON_BCR_TEST) || defined(GU_AD104_ENTRY_INPUTS_TEST)
    Check(offset!=0x111388 && offset!=0x11010c,"subset excludes refused CPUCTL and DMACTL");
#ifdef GU_AD104_ENTRY_INPUTS_TEST
    Check(offset!=0x111668,"entry subset excludes BCR");
#endif
#endif
    MmioCalls++;UINT32 value=j==0 || j==FalconWords-1?0x194000a1:0;
#ifdef GU_AD104_ENTRY_INPUTS_TEST
    if(offset==0x1103c0 && FalconFault==800)value=1;
    if(offset==0x110100)value=0x10;
    if(offset==0x82381c)value=0x24020000;
    if(offset==0x823820)value=5;
    if(offset==0x625f04)value=0x10e09;
#endif
    if(FalconFault==(int)n+1 || FalconFault==100+(int)n)return EFI_DEVICE_ERROR;
    if(FalconFault==20 && j==2)value=MAX_UINT32;
    if(FalconFault==26 && j==2)value=0xbadf5620;
    if(FalconFault==21 && j==FalconWords-1)value=0;
    if(FalconFault==22 && offset==0x823820)value^=1;
    if(FalconFault==23 && j==4)Io.Mem.Read=NULL;
    if(FalconFault==24 && n==FalconWords-1)Config[1]|=4;
    if(FalconFault==40 && n==FalconWords+3)value=42;
#ifdef GU_AD104_FALCON_CAPS_TEST
    if(FalconFault>=200 && FalconFault<=202 && n>=FalconWords && j==(UINTN)(4+FalconFault-200))value=1;
    if(FalconFault>=300 && FalconFault<=305 && j==(UINTN)(4+(FalconFault-300)/2))value=FalconFault%2?0xbadf5620:MAX_UINT32;
#endif
#if defined(GU_AD104_FALCON_BCR_TEST)
    if(offset==0x111668 && FalconFault>=700 && FalconFault<=703)value=FalconFault==700?MAX_UINT32:FalconFault==701?0xbadf5620:FalconFault==702?0xbadf5621:0xbadf0000;
    if(offset==0x111668 && n>=FalconWords && FalconFault>=704 && FalconFault<=706)value=FalconFault==704?0x10:FalconFault==705?0x11:1;
    if(offset==0x1103c0 && FalconFault==707)value=1;
#elif defined(GU_AD104_FALCON_CORE_TEST)
    if(FalconFault>=400 && FalconFault<=402 && n>=FalconWords && j==(UINTN)(7+FalconFault-400))value=0x91;
    if(FalconFault>=500 && FalconFault<=505 && j==(UINTN)(7+(FalconFault-500)/2))value=FalconFault%2?0xbadf5620:MAX_UINT32;
#endif
#ifdef GU_AD104_FALCON_REFUSAL_TEST
    if(offset==0x110100)value=0x10;
    if(FalconFault>=600 && FalconFault<=655 && n==8){value=0xbadf5620;if(FalconFault==627)return EFI_DEVICE_ERROR;if(FalconFault==628)value=MAX_UINT32;if(FalconFault==629)value=0xbadf5621;}
    if(FalconFault==626 && n==7)value=1;
    if(FalconFault==656 && n==FalconWords+8)value=0xbadf5620;
#endif
    *(UINT32 *)out=value;return EFI_SUCCESS;
  }
#endif
#ifdef GU_AD104_PROM_FULL_TEST
  if(MmioCalls>=174) {
    UINTN n=MmioCalls-174;
    Check(n<262146,"bounded full PROM plus bookends");
    if(!n)PromFileCalls=Writes+Flushes;
    Check(Writes+Flushes==PromFileCalls,"no file callbacks within full PROM reads");
    Check(offset==(n<262144?0x300000+n*4:n==262144?0x300000:0),"contiguous exact 1 MiB window then signature/chip");
    MmioCalls++;UINT32 v=(UINT32)n;
    if(n<128 || n==262144)v=0;
    if(!n || n==262144)v=0xaa55;
    if(n==6)v=0x170;if(n==92)v=0x52494350;if(n==93)v=0x278210de;
    if(n==94)v=0x00180000;if(n==96)v=0x7e;
    if(n==200)v=MAX_UINT32;if(n==201)v=0xbadf5620;
    if(n==262145)v=0x194000a1;
    if(PromFault>=2000 && n==(UINTN)(PromFault-2000))return EFI_DEVICE_ERROR;
    if(PromFault==20 && !n)v=MAX_UINT32;
    if(PromFault==21 && n==20)v^=1;
    if(PromFault==22 && n==262144)v^=1;
    if(PromFault==23 && n==262145)v=0;
    if(PromFault==24 && n==1024)Io.Mem.Read=NULL;
    if(PromFault==25 && n==262145)Config[1]|=4;
    *(UINT32 *)out=v;return EFI_SUCCESS;
  }
#endif
#ifdef GU_AD104_PROM_HEADER_TEST
  if(MmioCalls>=44) {
    UINTN n=MmioCalls-44;Check(n<130,"bounded header and signature/chip bookends");
    if(!n)PromFileCalls=Writes+Flushes;
    Check(Writes+Flushes==PromFileCalls,"no file callbacks within PROM window reads");
    Check(offset==(n<128?0x300000+n*4:n==128?0x300000:0),"only admitted PROM prefix and bookends");
    MmioCalls++;UINT32 v=0;
    if(n==0 || n==128)v=0xaa55;
    UINTN pw=(PromFault==17 || PromFault==18)?122:92;
    if(n==6)v=(UINT32)pw*4;
    if(n==pw)v=0x52494350;
    if(n==pw+1)v=0x278210de;
    if(n==pw+2)v=PromFault==18?0x00190000:0x00180000;
    if(n==pw+4)v=0x7e;
    if(PromFault==16 && n==20)v=MAX_UINT32;
    if(PromFault==16 && n==21)v=0xbadf5620;
    if(n==129)v=0x194000a1;
    if(PromFault>=1000 && n==(UINTN)(PromFault-1000))return EFI_DEVICE_ERROR;
    if(PromFault==1 && !n)v=MAX_UINT32;
    if(PromFault==2 && n==6)v=0xfff0;
    if(PromFault==3 && n==93)v=0;
    if(PromFault==4 && n==94)v=0;
    if(PromFault==5 && n==96)v=0;
    if(PromFault==6 && n==128)v^=1;
    if(PromFault==7 && n==129)v=0;
    if(PromFault==8 && n==64)Io.Mem.Read=NULL;
    if(PromFault==9 && n==129)Config[1]|=4;
    *(UINT32 *)out=v;return EFI_SUCCESS;
  }
#endif
  UINTN index=(UINTN)MmioCalls%22;
  if(!index)BatchFileCalls=Writes+Flushes;
  Check(Writes+Flushes==BatchFileCalls,"no log service callbacks within an MMIO batch");
  Check(offset==DecodeOffsets[index] && offset<=0x1000000-4,"only bounded allowlisted registers");
  MmioCalls++;*(UINT32 *)out=(index==0 || index==21)?0x194000a1U:0;
#ifdef GU_AD104_ENTRY_INPUTS_TEST
  if(index==3)*(UINT32 *)out=0x10;
  if(index==12)*(UINT32 *)out=0x24020000;
  if(index==13)*(UINT32 *)out=5;
  if(index==14)*(UINT32 *)out=0x2ffa;
  if(index==15)*(UINT32 *)out=0x10e09;
  if(index==16)*(UINT32 *)out=0x45cb8f;
  if(index==17)*(UINT32 *)out=0x100000;
  if(index==18)*(UINT32 *)out=0x2ef9ff0;
#endif
#ifdef GU_AD104_FALCON_REFUSAL_TEST
  if(index==3)*(UINT32 *)out=0x10;
#endif
#ifdef GU_AD104_DELAYED_READ_TEST
  if(OffFault==18 && MmioCalls==36)*(UINT32 *)out=6;
  if(OffFault==19 && MmioCalls==38)*(UINT32 *)out=1;
#endif
  if((DecodeFault>=100 && DecodeFault<122 && MmioCalls==DecodeFault-99) ||
     (DecodeFault>=200 && DecodeFault<222 && MmioCalls==DecodeFault-177))return EFI_DEVICE_ERROR;
  if(DecodeFault==9 && MmioCalls==7)return EFI_DEVICE_ERROR;
  if(DecodeFault==10 && MmioCalls==7)*(UINT32 *)out=0xbadf5620;
  if(DecodeFault==11 && MmioCalls==44)*(UINT32 *)out=1;
  if(DecodeFault==12 && MmioCalls==1)*(UINT32 *)out=0;
  if(DecodeFault==13 && MmioCalls==22)Config[1]|=4;
  if(DecodeFault==18 && MmioCalls==5)Io.Mem.Read=NULL;
  if(DecodeFault==19 && MmioCalls==7)*(UINT32 *)out=MAX_UINT32;
  if(DecodeFault==20 && MmioCalls==23)*(UINT32 *)out=0;
  if(DecodeFault==21 && MmioCalls==44)Config[1]|=4;
  if(DecodeFault==22 && MmioCalls==44)Missing=-1;
  return EFI_SUCCESS;
}
static CAPTURE *DecodeSetup(int fault) {
  CAPTURE *c=ParentSetup(0);DecodeFault=fault;DecodeCalls=MmioCalls=DecodeFlushes=0;
  DecodeAttempted=FALSE;Io.Mem.Read=ReadBar;
#ifdef GU_AD104_DECODE_OFF_TEST
  OffIntentSeen=0;
#ifdef GU_AD104_PROM_HEADER_TEST
  PromIntentSeen=PromOutputSeen=0;
#ifdef GU_AD104_FWSEC_AUDIT_TEST
  AuditSeen=0;
#endif
#endif
#endif
  return c;
}
#ifdef GU_AD104_PROM_FULL_TEST
static EFI_FILE_PROTOCOL FullRoot,FullFile,FullWitness;
static UINT8 SavedProm[0x100000];static UINTN FilePosition;
static VOID *FullAllocation;static EFI_ALLOCATE_POOL SavedAllocate;static EFI_FREE_POOL SavedFree;
static EFI_STATUS EFIAPI FullAllocate(EFI_MEMORY_TYPE type,UINTN n,VOID **out) {
  if(n==0x200000){if(PromFault==52){*out=NULL;return EFI_SUCCESS;}if(PromFault==26)return EFI_OUT_OF_RESOURCES;EFI_STATUS s=EFI_SUCCESS;*out=calloc(1,n);if(!*out)s=EFI_OUT_OF_RESOURCES;FullAllocation=*out;if(PromFault==38)Config[1]|=4;return s;}
  return SavedAllocate?SavedAllocate(type,n,out):EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI FullFree(VOID *p) {
  BOOLEAN full=p==FullAllocation;EFI_STATUS s=SavedFree(p);
  if(full){FullAllocation=NULL;if(PromFault==41)Config[1]|=4;if(PromFault==36)return EFI_DEVICE_ERROR;}return s;
}
static EFI_STATUS EFIAPI FullWrite(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *p) {
  Check(f==&FullFile && *n==0x100000,"one exact PROM file write");if(PromFault==54)((UINT8 *)p)[512]^=1;memcpy(SavedProm,p,*n);
  if(PromFault==29)(*n)--;if(PromFault==42)FullFile.Read=NULL;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI FullRead(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *p) {
  Check(f==&FullFile,"read back same PROM file");
  if(*n==1){*n=PromFault==33?1:0;return EFI_SUCCESS;}
  Check(*n==0x100000 && !FilePosition,"exact readback after seek");memcpy(p,SavedProm,*n);FilePosition=*n;
  if(PromFault==32)(*n)--;if(PromFault==34)((UINT8 *)p)[512]^=1;if(PromFault==40)Config[1]|=4;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI FullSeek(EFI_FILE_PROTOCOL *f,UINT64 n){Check(f==&FullFile && !n,"seek to zero only");FilePosition=0;return PromFault==31?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI FullFlush(EFI_FILE_PROTOCOL *f){Check(f==&FullFile,"flush PROM only");return PromFault==30?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI FullClose(EFI_FILE_PROTOCOL *f){Check(f==&FullFile,"never close root/log/witness");return PromFault==35?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI FullOpen(EFI_FILE_PROTOCOL *f,EFI_FILE_PROTOCOL **out,CHAR16 *name,UINT64 mode,UINT64 attr) {
  Check(f==&FullRoot && name && !attr,"only borrowed capture root");*out=NULL;
  if(mode==EFI_FILE_MODE_READ){if(PromFault==27){*out=&FullFile;return EFI_SUCCESS;}return EFI_NOT_FOUND;}
  Check(mode==(EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE),"only new capture file");
  *out=PromFault==28?&FullRoot:&FullFile;
  if(PromFault==43)*out=&File;if(PromFault==44)*out=&FullWitness;
  if(PromFault==47)return EFI_DEVICE_ERROR;
  if(PromFault==48)FullFile.Read=NULL;if(PromFault==49)FullFile.Flush=NULL;if(PromFault==50)FullFile.Close=NULL;
  if(PromFault==39)Config[1]|=4;return EFI_SUCCESS;
}
static CAPTURE *FullSetup(int fault) {
  CAPTURE *c=DecodeSetup(0);OffCalls=OffFault=0;DecodeOffAttempted=PromHeaderAttempted=PromFullAttempted=FALSE;PromFault=fault;
  PromPrefixGpu=NULL;PromPrefixIo=NULL;FullAllocation=NULL;FilePosition=0;
#ifdef GU_AD104_FALCON_OBSERVE_TEST
  FalconAttempted=FALSE;FalconFault=FalconIntentSeen=FalconSummarySeen=0;
#ifdef GU_AD104_ENTRY_INPUTS_TEST
  EntryAuditSeen=0;
#endif
#ifdef GU_AD104_FALCON_REFUSAL_TEST
  FalconRefusalObserved=FalconRecoveryAttempted=FalconRecoveryEligible=FALSE;
  RecoveryIntentSeen=RecoverySummarySeen=RecoveryWitnessRecords=0;
#endif
#endif
  memset(&FullRoot,0,sizeof(FullRoot));memset(&FullFile,0,sizeof(FullFile));memset(&FullWitness,0,sizeof(FullWitness));
  FullRoot.Open=FullOpen;FullFile.Write=FullWrite;FullFile.Read=FullRead;FullFile.SetPosition=FullSeek;FullFile.Flush=FullFlush;FullFile.Close=FullClose;
  c->DumpRoot=&FullRoot;c->Witness=&FullWitness;SavedAllocate=Bs.AllocatePool;SavedFree=Bs.FreePool;Bs.AllocatePool=FullAllocate;Bs.FreePool=FullFree;return c;
}
int main(void) {
  CAPTURE *c=FullSetup(0);Check(EdRun(c,(VOID *)1,&Io,Pins)==EFI_SUCCESS && OffCalls==1 && MmioCalls==
#ifdef GU_AD104_FALCON_OBSERVE_TEST
  262320+2*FalconWords
#else
  262320
#endif
  && !FullAllocation,"full capture/file readback then recovery");
  for(int f=20;f<=
#ifdef GU_AD104_FWSEC_AUDIT_TEST
  57
#else
  54
#endif
  ;f++) {
    if(f==45 || f==46 || f==51 || f==53)continue;
    c=FullSetup(f);Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !OffCalls && !FullAllocation,"full read/file/cleanup/state faults block handoff");
  }
#ifdef GU_AD104_FALCON_OBSERVE_TEST
  for(int fault=1;fault<=39;fault++){
    if((fault<=18 && fault>2*FalconWords) || fault==19 || (fault==25 || (fault>=27 && fault<=29)))continue;
    c=FullSetup(0);FalconAttempted=FALSE;FalconFault=fault;
    Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !OffCalls && !FullAllocation,"Falcon transport/sentinel/state/method faults block handoff");
    if(fault>=30 && fault<=33)Check(MmioCalls==262320,"intent log drift prevents all Falcon reads");
    if(fault>=34 && fault<=37)Check(MmioCalls==262320+FalconWords,"batch log drift prevents second batch");
    if(fault<=18)Check(MmioCalls==262320+fault,"Falcon failure stops remaining reads");
  }
#ifdef GU_AD104_FALCON_CAPS_TEST
  for(int fault=200;fault<=202;fault++){
    c=FullSetup(0);FalconFault=fault;
    Check(EdRun(c,(VOID *)1,&Io,Pins)==EFI_SUCCESS && OffCalls==1,"new capability field drift remains observational");
  }
  for(int fault=300;fault<=305;fault++){
    c=FullSetup(0);FalconFault=fault;
    Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !OffCalls && MmioCalls==262325+(fault-300)/2,"every capability register sentinel blocks remaining reads");
  }
  for(int position=18;position<2*FalconWords;position++){
    c=FullSetup(0);FalconFault=100+position;
    Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !OffCalls && MmioCalls==262321+position,"capability tail read errors stop remaining reads");
  }
#endif
#ifdef GU_AD104_ENTRY_INPUTS_TEST
  Check(FalconWords==12,"entry subset exactly twelve words");
  for(int fault=800;fault<=804;fault++){
    c=FullSetup(0);FalconFault=fault;
    Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !OffCalls,"engine and audit log/state/method faults block Windows");
    Check(MmioCalls==(fault==800?262328:262344),"audit adds no MMIO");
  }
#elif defined(GU_AD104_FALCON_BCR_TEST)
  Check(FalconWords==13,"BCR subset exactly thirteen words");
  for(int fault=700;fault<=703;fault++){
    c=FullSetup(0);FalconFault=fault;
    Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !OffCalls && MmioCalls==262329,"every BCR error pattern holds without recovery");
  }
  for(int fault=704;fault<=706;fault++){
    c=FullSetup(0);FalconFault=fault;
    Check(EdRun(c,(VOID *)1,&Io,Pins)==EFI_SUCCESS && OffCalls==1 && MmioCalls==262346,"BCR selection/valid changes remain observational");
  }
  c=FullSetup(0);FalconFault=707;
  Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !OffCalls && MmioCalls==262328,"ENGINE reset mismatch holds before BCR");
#elif defined(GU_AD104_FALCON_CORE_TEST)
  for(int fault=400;fault<=402;fault++){
    c=FullSetup(0);FalconFault=fault;
    Check(EdRun(c,(VOID *)1,&Io,Pins)==EFI_SUCCESS && OffCalls==1,"new core-state field drift remains observational");
  }
  for(int fault=500;fault<=505;fault++){
#ifdef GU_AD104_FALCON_REFUSAL_TEST
    if(fault==503)continue;
#endif
    c=FullSetup(0);FalconFault=fault;
    Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !OffCalls && MmioCalls==262328+(fault-500)/2,"every new core register sentinel blocks remaining reads");
  }
#endif
  c=FullSetup(0);FalconAttempted=FALSE;FalconFault=40;
  Check(EdRun(c,(VOID *)1,&Io,Pins)==EFI_SUCCESS && OffCalls==1,"mailbox drift remains observational");
  FalconFault=0;
#endif
#ifdef GU_AD104_FALCON_REFUSAL_TEST
  c=FullSetup(0);FalconFault=600;
  Check(EdRun(c,(VOID *)1,&Io,Pins)==EFI_SUCCESS && FalconRefusalObserved && FalconRecoveryEligible && OffCalls==1 && MmioCalls==262343 && RecoveryWitnessRecords==3,"known refused observation reaches only guarded recovery");
  Check(FalconRefusalAdmission(c,(VOID *)1,&Io,Pins,NULL,NULL,NULL)!=EFI_SUCCESS && MmioCalls==262343,"no recovery retry");
  c=FullSetup(0);FalconFault=656;
  Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !FalconRecoveryAttempted && !OffCalls && !RecoveryWitnessRecords && MmioCalls==262343,"second optional batch refusal cannot enter recovery");
  {int faults[]={1,3,7,10,12,13};for(UINTN i=0;i<sizeof(faults)/sizeof(faults[0]);i++){
    c=FullSetup(0);FalconFault=600;OffFault=faults[i];
    Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && OffCalls==((faults[i]==10 || faults[i]==12 || faults[i]==13)?0:1),"fresh V and decode-off guards remain mandatory after refusal");
  }}
  for(int fault=601;fault<=655;fault++){
    if((fault>=615 && fault<=619) || (fault>=632 && fault<=639) || (fault>=645 && fault<=649) || fault==653)continue;
    c=FullSetup(0);FalconFault=fault;
    Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && OffCalls==((fault==643 || fault==644)?1:0),"recovery read/log/method/state/witness faults block handoff");
    if(fault>=601 && fault<=614)Check(MmioCalls==262329+fault-600,"recovery read failure stops remaining MMIO");
    if(fault==626 || fault==627 || fault==628 || fault==629 || fault==655)Check(!FalconRecoveryAttempted && MmioCalls==262329,"other engine/transport/sentinel states never enter recovery");
  }
#endif
  int points[]={0,1,127,128,1023,1024,131072,262142,262143,262144,262145};
  for(UINTN j=0;j<sizeof(points)/sizeof(points[0]);j++) {
    c=FullSetup(2000+points[j]);Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !OffCalls && MmioCalls==175+points[j] && !FullAllocation,"selected boundary transport errors stop without retry");
  }
  printf("PASS full PROM capture: %d checks, RAM-only mocks\n",Checks);return 0;
}
#elif defined(GU_AD104_PROM_HEADER_TEST)
int main(void) {
  CAPTURE *c=DecodeSetup(0);DecodeOffAttempted=PromHeaderAttempted=FALSE;OffFault=OffCalls=PromFault=0;
  Check(EdRun(c,(VOID *)1,&Io,Pins)==EFI_SUCCESS && MmioCalls==174 && OffCalls==1,"bounded PROM header then verified recovery");
  Check(PromHeader(c,(VOID *)1,&Io,Pins,NULL)==EFI_ALREADY_STARTED,"no header retry");
  for(int f=1;f<=18;f++) {
    c=DecodeSetup(0);DecodeOffAttempted=PromHeaderAttempted=FALSE;OffCalls=0;PromFault=f;
    EFI_STATUS s=EdRun(c,(VOID *)1,&Io,Pins);
    if(f==16 || f==17)Check(s==EFI_SUCCESS && OffCalls==1,"literal payload sentinels and max legal PCIR header are valid");
    else Check(s!=EFI_SUCCESS && !OffCalls,"header/admission/output faults block handoff");
    if(f>=10 && f<=13)Check(MmioCalls==44,"intent flush admission drift prevents every PROM read");
  }
  for(int n=0;n<130;n++) {
    c=DecodeSetup(0);DecodeOffAttempted=PromHeaderAttempted=FALSE;OffCalls=0;PromFault=1000+n;
    Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !OffCalls && MmioCalls==45+n,"every PROM/bookend transport error stops remaining reads");
  }
  printf("PASS bounded PROM header: %d checks, RAM-only mocks\n",Checks);return 0;
}
#elif defined(GU_AD104_DECODE_OFF_TEST)
int main(void) {
  CAPTURE *c=DecodeSetup(0);DecodeOffAttempted=FALSE;OffFault=OffCalls=0;
  EFI_STATUS run=EdRun(c,(VOID *)1,&Io,Pins);
  Check(run==EFI_SUCCESS && OffCalls==1 && MmioCalls==44,"capture then one decode off qualifies Windows");
  Check(CurrentAttributes==0 && !(Config[1]&7),"exact post-parent decode/BME state restored");
  Check(DecodeOff(c,(VOID *)1,&Io,Pins,NULL,NULL)==EFI_ALREADY_STARTED && OffCalls==1,"no disable retry");
  for(int f=1;f<=
#ifdef GU_AD104_DELAYED_READ_TEST
  19
#else
  13
#endif
  ;f++) {
    c=DecodeSetup(0);DecodeOffAttempted=FALSE;OffFault=f;OffCalls=0;
    Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && OffCalls==((f==6 || f==9 || f==10 || f>=11)?0:1) && MmioCalls==((f>=14 && f<=17)?22:44),"disable error/partial/config/log drift blocks Windows without more MMIO");
  }
  c=DecodeSetup(10);DecodeOffAttempted=FALSE;OffFault=OffCalls=0;
  Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !OffCalls,"failed capture prevents disable");
  printf("PASS capture then decode off: %d checks, RAM-only mocks\n",Checks);return 0;
}
#else
int main(void) {
  CAPTURE *c=DecodeSetup(0);
  Check(EdRun(c,(VOID *)1,&Io,Pins)==EFI_SUCCESS && DecodeCalls==1 && MmioCalls==44,
    "all release stages plus limited decode and two register batches qualify handoff");
  Check(CurrentAttributes==0x300 && (Config[1]&7)==3,"BME/VGA remain off");
  Check(DecodeStep(c,(VOID *)1,&Io,Pins,NULL)==EFI_ALREADY_STARTED && DecodeCalls==1,"no second Enable");
  for(int f=1;f<=25;f++) {
    c=DecodeSetup(f);
    Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS,"decode/read/log/state/input faults block handoff");
    Check(DecodeCalls==(f==14?0:1),"failed decode intent prevents Enable");
    if(f==16 || f==23 || f==24 || f==25)Check(MmioCalls==22,"batch flush failure or state drift prevents second MMIO batch");
    if(f<=8 || f==14)Check(!MmioCalls,"failed or mismatching Enable performs zero MMIO");
  }
  for(int batch=0;batch<2;batch++)for(int i=0;i<22;i++) {
    c=DecodeSetup((batch?200:100)+i);
    Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS,"each individual read failure blocks handoff");
    Check(MmioCalls==batch*22+i+1,"read failure prevents remaining reads");
  }
  c=DecodeSetup(0);ParentFault=2;
  Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !DecodeCalls && !MmioCalls,"failed parent prevents decode and MMIO");
  printf("PASS limited decode and registers: %d checks, RAM-only mocks\n",Checks);return 0;
}

#endif
