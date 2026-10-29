/* GPL-2.0-only. RAM fixtures; deliberately invalid pointers must never be read. */
#define GU_AD104_TEST_REUSE
#include "test_probe_ad104.c"
static UINT8 RomBytes[0x24a00];
static int RomFault,Maps;
static EFI_STATUS EFIAPI RomOpen(EFI_FILE_PROTOCOL *f,EFI_FILE_PROTOCOL **out,CHAR16 *name,UINT64 mode,UINT64 attrs) {
  const CHAR16 target[]=L"\\RTX3090-UNLOCK\\FIRMROM.BIN";
  UINTN at=0;while(target[at] && target[at]==name[at])at++;
  if(target[at]==name[at] && (mode&EFI_FILE_MODE_CREATE) && (RomFault==22 || RomFault==23)) {
    *out=RomFault==22?NULL:f;return EFI_SUCCESS;
  }
  return DumpOpen(f,out,name,mode,attrs);
}
static EFI_STATUS EFIAPI RomMap(UINTN *size,EFI_MEMORY_DESCRIPTOR *map,UINTN *key,UINTN *stride,UINT32 *version) {
  (void)key;Maps++;
  *stride=sizeof(*map);*version=EFI_MEMORY_DESCRIPTOR_VERSION;*size=sizeof(*map);
  memset(map,0,sizeof(*map));map->Type=EfiBootServicesData;
  map->PhysicalStart=(UINTN)RomBytes&~4095ULL;
  map->NumberOfPages=(((UINTN)RomBytes&4095)+sizeof(RomBytes)+4095)/4096;
  if(RomFault==1)map->Type=EfiMemoryMappedIO;
  if(RomFault==2)map->Attribute=EFI_MEMORY_RP;
  if(RomFault==3)map->Attribute=EFI_MEMORY_RUNTIME;
  if(RomFault==4)map->NumberOfPages=1;
  if(RomFault==5)*stride=1;
  if(RomFault==6)*version=99;
  if(RomFault==7)return EFI_BUFFER_TOO_SMALL;
  if(RomFault==8)*size=65537;
  if(RomFault==9)*size=sizeof(*map)-1;
  if(RomFault==10){map->PhysicalStart=MAX_UINT64-4095;map->NumberOfPages=2;}
  if(RomFault==11 && Maps==2)RomBytes[500]^=1;
  if(RomFault==12 && Maps==2)Pci.RomImage=NULL;
  return EFI_SUCCESS;
}
static EFI_STATUS RomRun(int fault,int mode) {
  GenericSetup(mode);AdMode=mode;RomFault=fault;Maps=0;
  memset(RomBytes,0,sizeof(RomBytes));
  RomBytes[0]=0x55;RomBytes[1]=0xaa;RomBytes[0x18]=0x20;
  memcpy(RomBytes+0x20,"PCIR",4);RomBytes[0x24]=0xde;RomBytes[0x25]=0x10;
  RomBytes[0x26]=0x82;RomBytes[0x27]=0x27;
  RomBytes[0x2a]=0x18;RomBytes[0x30]=1;
  Pci.Pci.Read=AdRead;Pci.GetLocation=AdLocation;Pci.Mem.Read=AdMem;
  Pci.RomImage=RomBytes;Pci.RomSize=sizeof(RomBytes);Bs.GetMemoryMap=RomMap;
  RootFile.Open=RomOpen;
  if(fault==13)Pci.RomImage=(VOID *)(UINTN)1;
  if(fault==14)Pci.RomSize=MAX_UINT64;
  if(fault==15)RomBytes[0]=0;
  if(fault==16)RomBytes[0x19]=0xff;
  if(fault==17)RomBytes[0x24]=0;
  if(fault==18)Pci.RomImage=(VOID *)(MAX_UINTN-20);
  if(fault==19)RomBytes[0x2a]=0;
  if(fault==20)RomBytes[0x30]=0;
  if(fault==21)RomBytes[0x31]=0xff;
  return GuCaptureGraphOnVolume(&Table,&LogFile,&RootFile);
}
int main(void) {
  Check(RomRun(0,G_BME)==EFI_NOT_READY,"buffer capture never claims deep capture complete");
  Check(Logged("firmware-rom-buffer dump=FIRMROM.BIN") && Logged("unlock-eligibility=0"),"stable bounded RAM ROM saved");
  Check(!S.RegReads && !S.PromReads && !S.PromAllocs && Maps==2,"zero MMIO and two map observations");
  for(int fault=1;fault<=23;fault++) {
    Check(RomRun(fault,G_BME)==EFI_NOT_READY,"invalid buffer stays incomplete");
    Check(!Logged("firmware-rom-buffer dump="),"invalid buffer never published complete");
    Check(!S.RegReads && !S.PromReads,"no MMIO on failure");
    Check(!RootCloses,"capture root is never closed by a malformed dump handle");
  }
  const int modes[]={A_DEVICE,A_SUBSYSTEM,A_REVISION,G_MULTI,G_SHORT_WRITE,G_FLUSH,G_CLOSE,G_EXISTING_IMG,G_DUMP_READ_ERROR};
  for(UINTN i=0;i<sizeof(modes)/sizeof(modes[0]);i++) {
    (void)RomRun(0,modes[i]);
    Check(!Logged("firmware-rom-buffer dump="),"identity, inventory or output failure refuses ROM publication");
    Check(!S.RegReads && !S.PromReads,"output failure never falls into GPU reads");
  }
  printf("PASS firmware ROM buffer: %d checks, RAM-only\n",Checks);
  return 0;
}
