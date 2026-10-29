/* GPL-2.0-only. Actual AD104 capture with RAM-only firmware callbacks. */
#define GU_PROBE_TEST_REUSE
#include "test_probe_generic.c"

enum {A_OK=100,A_SUBSYSTEM,A_REVISION,A_BAD_CHIP,A_REG_ERROR,A_SENTINEL,A_PROM_ERROR,
      A_PROM_SIGNATURE,A_FINAL_CHIP,A_FINAL_CFG,A_BDF_DRIFT,A_BAD_BUS,A_BAD_SEGMENT,A_BAD_DEVICE,A_BAD_FUNCTION,
      A_PROM_COPY_SIGNATURE,A_FINAL_BDF,A_DEVICE};
static int AdMode,FaultIndex;
static EFI_STATUS EFIAPI AdRead(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 at,UINTN n,VOID *data) {
  EFI_STATUS s=GenericRead(io,w,at,n,data);
  if(s==EFI_SUCCESS && at==0 && w==EfiPciIoWidthUint32) {
    UINT32 *v=data;v[0]=AdMode==A_DEVICE?0x278310deU:0x278210deU;
    if(n==64) {
      v[11]=AdMode==A_SUBSYSTEM?0x51331462U:0x51321462U;
      if(AdMode==A_REVISION)v[2]^=1;
      if(AdMode==A_FINAL_CFG && S.PromReads)v[11]^=1;
    }
  }
  return s;
}
static EFI_STATUS EFIAPI AdLocation(EFI_PCI_IO_PROTOCOL *io,UINTN *seg,UINTN *bus,UINTN *dev,UINTN *fun) {
  (void)io;*seg=0;*bus=11;*dev=0;*fun=0;
  if(AdMode==A_BDF_DRIFT && S.PromAllocs)*bus=12;
  if(AdMode==A_BAD_BUS)*bus=256;
  if(AdMode==A_BAD_SEGMENT)*seg=65536;
  if(AdMode==A_BAD_DEVICE)*dev=32;
  if(AdMode==A_BAD_FUNCTION)*fun=8;
  if(AdMode==A_FINAL_BDF && S.PromReads)*bus=12;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI AdMem(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT8 bar,UINT64 at,UINTN n,VOID *data) {
  EFI_STATUS s=MemRead(io,w,bar,at,n,data);
  if(at>=0x300000 && at<0x400000) {
    if(AdMode==A_PROM_ERROR && S.PromReads==123)return EFI_DEVICE_ERROR;
    *(UINT32 *)data=at==0x300000?(AdMode==A_PROM_SIGNATURE?0U:0xaa55U):(UINT32)at;
    if(AdMode==A_PROM_COPY_SIGNATURE && at==0x300000 && S.PromReads>1)*(UINT32 *)data=0xbb55U;
  } else {
    if(at==0)*(UINT32 *)data=(AdMode==A_BAD_CHIP || (AdMode==A_FINAL_CHIP && S.PromReads))?0x194000a2U:0x194000a1U;
    if(AdMode==A_REG_ERROR && S.RegReads==FaultIndex)return EFI_DEVICE_ERROR;
    if(AdMode==A_SENTINEL && S.RegReads==FaultIndex)*(UINT32 *)data=0xbadf0001;
  }
  return s;
}
static EFI_STATUS AdRun(int mode) {
  GenericSetup(mode);AdMode=mode;
  Pci.Pci.Read=AdRead;Pci.GetLocation=AdLocation;Pci.Mem.Read=AdMem;
  return GuCaptureGraphOnVolume(&Table,&LogFile,&RootFile);
}
#ifndef GU_AD104_TEST_REUSE
int main(void) {
#ifdef GU_PROBE_AD104_METADATA
  for(int mode=0;mode<3;mode++) {
    GenericSetup(G_BME);AdMode=G_BME;
    Pci.Pci.Read=AdRead;Pci.GetLocation=AdLocation;Pci.Mem.Read=AdMem;
    Pci.RomImage=mode?(VOID *)(UINTN)1:NULL;
    Pci.RomSize=mode==2?MAX_UINT64:(mode?0x100000:0);
    Check(GuCaptureGraphOnVolume(&Table,&LogFile,&RootFile)==EFI_NOT_READY,"metadata is never a complete GPU capture");
    Check(!S.RegReads && !S.PromReads && !S.PromAllocs,"metadata never reads MMIO, PROM or allocates a PROM buffer");
    Check(Logged("ad104-metadata-only=1") && Logged("ad104-metadata-only-no-mmio"),"metadata mode is explicit");
    Check(Logged("bus-master-enabled=1"),"active BME is observed, never changed");
    Check(!Logged("prom bytes=") && !Logged("capture-complete=1"),"no ROM bytes or complete marker");
  }
  const int identities[]={A_DEVICE,A_SUBSYSTEM,A_REVISION};
  for(UINTN i=0;i<3;i++) {
    Check(AdRun(identities[i])==EFI_NOT_READY,"metadata wrong identity refused");
    Check(Logged("ad104-metadata-identity") && !Logged("ad104-metadata-only=1"),"wrong identity never labeled AD104 metadata");
    Check(!S.RegReads && !S.PromReads,"wrong identity performs no MMIO");
  }
  printf("PASS AD104 metadata: %d checks, invalid ROM pointers never dereferenced\n",Checks);
  return 0;
#endif
  Check(GenericRun(G_UNKNOWN,TRUE)==EFI_NOT_READY,"legacy unknown target stays refused");
  Check(AdRun(A_OK)==EFI_SUCCESS,"AD104 bus 0B capture succeeds in mock");
  Check(S.RegReads==23 && S.PromReads==PROM_DWORDS+1,"22 registers, final chip and bounded PROM");
  Check(Logged("capture-complete=1") && Logged("unlock-eligibility=0"),"complete observation never unlock eligibility");
  Check(Logged("ad104-admission command=0x0002 memory-enabled=1 bus-master-enabled=0"),"admission reports the sampled command bits");
  Check(Dumps==2 && RootOpens==2,"only caller launch root receives PROM and image");
  const int early[]={A_SUBSYSTEM,A_REVISION,A_BDF_DRIFT,A_BAD_BUS,A_BAD_SEGMENT,A_BAD_DEVICE,A_BAD_FUNCTION,
    G_BME,G_MEM_OFF,G_BAD_BAR,G_BAD_RESOURCE,G_MULTI,G_BAD_CHECKSUM,G_PROTOCOL_CHANGED};
  for(UINTN i=0;i<sizeof(early)/sizeof(early[0]);i++) {
    Check(AdRun(early[i])==EFI_NOT_READY,"invalid admission refuses capture");
    Check(!S.RegReads && !S.PromReads,"refused envelope performs zero MMIO");
  }
  Check(AdRun(A_BAD_CHIP)==EFI_NOT_READY && S.RegReads==1 && !S.PromReads,"chip mismatch stops all further reads");
  Check(AdRun(G_BME)==EFI_NOT_READY && !S.RegReads && !S.PromReads,"active bus mastering still refuses all MMIO");
  Check(Logged("bus-master-enabled=1") && Logged("ad104-envelope"),"BME refusal records command evidence");
  for(int index=1;index<=22;index++)for(int mode=A_REG_ERROR;mode<=A_SENTINEL;mode++) {
    FaultIndex=index;Check(AdRun(mode)==EFI_NOT_READY,"register error or sentinel invalidates capture");
    Check(!S.PromReads && !Logged("capture-complete=1"),"register failure precedes PROM");
  }
  const int late[]={A_PROM_ERROR,A_PROM_SIGNATURE,A_FINAL_CHIP,A_FINAL_CFG,G_SHORT_WRITE,G_FLUSH,G_CLOSE,
                    G_EXISTING_PROM,G_EXISTING_IMG,G_DUMP_READ_ERROR,A_PROM_COPY_SIGNATURE,A_FINAL_BDF};
  for(UINTN i=0;i<sizeof(late)/sizeof(late[0]);i++) {
    Check(AdRun(late[i])==EFI_NOT_READY,"partial or changed capture fails");
    Check(!Logged("capture-complete=1"),"partial capture has no complete marker");
  }
  printf("PASS AD104 capture: %d checks, RAM-only callbacks\n",Checks);
  return 0;
}
#endif
