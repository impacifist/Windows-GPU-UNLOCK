/* ASRock port changes: 2026-10-27; publication notice: 2026-10-28. GPL-2.0-only; see CHANGES.md. */
/* GPL-2.0-only. No CRT, firmware, filesystem or GPU access. */
#include <Uefi.h>
#include "probe.c"
#include "diagnostic.c"
#include "../unlock/sha256.c"
static UINT32 Config[64] = {
  [0]=0x250310deU, [1]=3, [2]=0x030000a1U, [4]=0xfb000000U,
  [11]=0x40741458U
};
static struct {
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR Descriptor;
  UINT8 End[2];
} Resource;
static int FreeFails;
static EFI_STATUS EFIAPI TestFree(VOID *p) {
  return p==&Resource && !FreeFails?EFI_SUCCESS:EFI_DEVICE_ERROR;
}
static EFI_STATUS EFIAPI TestBar(EFI_PCI_IO_PROTOCOL *io,UINT8 bar,UINT64 *supports,VOID **out) {
  (void)io;(void)supports;
  if(bar)return EFI_UNSUPPORTED;
  *out=&Resource;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI NoRead(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_WIDTH width,UINT8 bar,UINT64 offset,UINTN count,VOID *data) {
  (void)io;(void)width;(void)bar;(void)offset;(void)count;(void)data;
  return EFI_DEVICE_ERROR;
}
static EFI_BOOT_SERVICES BootServices={.FreePool=TestFree};
static EFI_SYSTEM_TABLE SystemTable={.BootServices=&BootServices};
static CAPTURE Capture={.Table=&SystemTable};
static EFI_PCI_IO_PROTOCOL Io={.GetBarAttributes=TestBar,.Mem={.Read=NoRead}};
int main(void) {
  if(!AsrockProbeIdentity(Config))return 1;
  const unsigned offsets[]={0,2,4,11};
  for(unsigned i=0;i<4;i++) {
    Config[offsets[i]]^=1;
    if(AsrockProbeIdentity(Config))return 2+(int)i;
    Config[offsets[i]]^=1;
  }
  Config[0]=0x250410deU;
  if(AsrockProbeIdentity(Config))return 6;
  Config[0]=0x248710deU;
  if(AsrockProbeIdentity(Config))return 7;
  Config[0]=0x250310deU;
  Resource.Descriptor.Desc=ACPI_ADDRESS_SPACE_DESCRIPTOR;
  Resource.Descriptor.Len=sizeof(Resource.Descriptor)-3;
  Resource.Descriptor.AddrSpaceGranularity=32;
  Resource.Descriptor.AddrRangeMin=0xfb000000U;
  Resource.Descriptor.AddrRangeMax=0xfbffffffU;
  Resource.Descriptor.AddrLen=0x1000000U;
  Resource.End[0]=ACPI_END_TAG_DESCRIPTOR;
  if(!ProbeEnvelope(&Capture,&Io,Config))return 8;
  Config[1]=7;
  if(ProbeEnvelope(&Capture,&Io,Config))return 9;
  Config[1]=1;
  if(ProbeEnvelope(&Capture,&Io,Config))return 10;
  Config[1]=3;
  Resource.Descriptor.AddrLen--;
  if(ProbeEnvelope(&Capture,&Io,Config))return 11;
  Resource.Descriptor.AddrLen++;
  Resource.Descriptor.AddrRangeMin++;
  if(ProbeEnvelope(&Capture,&Io,Config))return 12;
  Resource.Descriptor.AddrRangeMin--;
  Resource.End[0]=0;
  if(ProbeEnvelope(&Capture,&Io,Config))return 13;
  Resource.End[0]=ACPI_END_TAG_DESCRIPTOR;
  Resource.End[1]=1;
  if(ProbeEnvelope(&Capture,&Io,Config))return 14;
  Resource.End[1]=0;
  FreeFails=1;
  if(ProbeEnvelope(&Capture,&Io,Config))return 15;
  return 0;
}
