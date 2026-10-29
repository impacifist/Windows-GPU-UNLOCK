/* Run the real app with the existing firmware/file mocks, no pinned assets.
   Unknown cold registers are deliberately given arbitrary/old-looking values. */
#define GU_PREFLIGHT_FIXTURE
#include "test_app.c"
#undef main

static EFI_PCI_IO_PROTOCOL amd;
static unsigned negative;
static EFI_STATUS EFIAPI amd_location(EFI_PCI_IO_PROTOCOL *p,UINTN *s,UINTN *b,UINTN *d,UINTN *f) {
  CHECK(p==&amd);*s=0;*b=negative==4?11:12;*d=*f=0;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI amd_read(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 o,UINTN n,VOID *v) {
  CHECK(p==&amd && w==EfiPciIoWidthUint32 && n==1 && (o==0 || o==0x2c));
  if(negative==5)return EFI_DEVICE_ERROR;
  *(UINT32 *)v=o==0?(negative==2?0x46808086:GU_AMD_ID):(negative==3?0x88771042:GU_AMD_SUBSYSTEM);
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI resolve(EFI_GUID *g,EFI_DEVICE_PATH_PROTOCOL **p,EFI_HANDLE *h) {
  CHECK(guid(g,&pg));if(negative==6)return EFI_NOT_FOUND;
  *p=(EFI_DEVICE_PATH_PROTOCOL *)((UINT8 *)*p+(negative==7?18:24));*h=H(12);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI preflight_handle(EFI_HANDLE h,EFI_GUID *g,VOID **p) {
  static const UINT8 path[]={2,1,12,0,0xd0,0x41,3,0xa,0,0,0,0,
    1,1,6,0,1,8,1,1,6,0,0,0,2,3,8,0,0,3,1,0x80,0x7f,0xff,4,0};
  if(h==H(12) && guid(g,&pg)){*p=&amd;return EFI_SUCCESS;}
  if(h==H(5) && guid(g,&dg)){*p=(VOID *)(negative==1?intel_path:path);return EFI_SUCCESS;}
  return handle(h,g,p);
}
static EFI_STATUS EFIAPI preflight_locate(EFI_LOCATE_SEARCH_TYPE t,EFI_GUID *g,VOID *key,UINTN *n,EFI_HANDLE **h) {
  if(!guid(g,&gg))return locate(t,g,key,n,h);
  *n=1;CHECK(!EFI_ERROR(allocate_pool(EfiLoaderData,sizeof(**h),(VOID **)h)));(*h)[0]=H(5);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI preflight_bar(EFI_PCI_IO_PROTOCOL *p,UINT8 b,UINT64 *s,VOID **out) {
  EFI_STATUS status=bar_info(p,b,s,out);EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *r=*out;
  /* Case 10 moves BAR0 off the profile's pin; a fixed address could equal a real pin. */
  r->AddrRangeMin=negative==10?GU_BAR0_BASE+0x1000000:GU_BAR0_BASE;r->AddrRangeMax=GU_BAR0_END;
  if(negative==11)r->AddrLen=0x2000000;
  return status;
}
#ifdef GU_EXACT_FIXTURE
#define main preflight_fixture_main
#endif
int main(int argc,char **argv) {
  CHECK(argc==3);size_t n;UINT8 *data=read_file(argv[1],&n);CHECK(n==sizeof(prom));memcpy(prom,data,n);free(data);
  FILE *capture=fopen(argv[2],"r");CHECK(capture);char line[256];unsigned count=0;unsigned long long o,v;
  while(count<64 && fgets(line,sizeof(line),capture))if(sscanf(line,"config_offset=0x%llx",&o)==1) {
    CHECK(o==count*4 && fgets(line,sizeof(line),capture) && sscanf(line,"value=0x%llx",&v)==1);
    captured_config[count++]=(UINT32)v;
  }
  fclose(capture);CHECK(count==64 && captured_config[0]==GU_BOARD_ID);
  for(negative=0;negative<17;negative++) {
    app_setup(APP_SUCCESS);memset(&amd,0,sizeof(amd));amd.GetLocation=amd_location;amd.Pci.Read=amd_read;
    bs.HandleProtocol=preflight_handle;bs.LocateDevicePath=resolve;bs.LocateHandleBuffer=preflight_locate;pci.GetBarAttributes=preflight_bar;
    infos[0].HorizontalResolution=infos[0].PixelsPerScanLine=2560;infos[0].VerticalResolution=1440;
    modes[0].Mode=0;modes[0].MaxMode=13;modes[0].FrameBufferBase=0xfce0000000ULL;modes[0].FrameBufferSize=0xe10000;
    registers[0x1183a4/4]=GU_DEVICE_MEMORY_SIZE;
    if(negative==8)*(UINT32 *)config=0x220610de;
    if(negative==9)*(UINT32 *)(config+0x2c)=0x145510de;
    if(negative==12)registers[0x300000/4]^=1;
    if(negative==13){modes[0].SizeOfInfo=0;modes[0].Info=(VOID *)(UINTN)1;}
    if(negative==14)config[4]|=4;
    if(negative==15)registers[0]=registers[SS0/4]=0xffffffff;
    if(negative==16)registers[0x1183a4/4]=0x2800;
    EFI_STATUS result=efi_main(H(1),&st);
    CHECK(EFI_ERROR(result) && !Gu.armed && !Gu.owned && !Gu.started && !Gu.allocations);
    CHECK(!fake.mmio_writes && !fake.pci_writes && !fake.flrs && !fake.maps && !fake.master_enables && !fake.master_disables);
    CHECK(!disconnect_calls && !exclusive_calls && !key_calls && !attribute_enables && !prompt);
    if(negative==0 || negative>=15)CHECK(result==EFI_NOT_READY && strstr(report_text,"baseline=UNKNOWN") && strstr(report_text,"cannot arm"));
    else CHECK(!strstr(report_text,"baseline=UNKNOWN"));
    CHECK(!halted);dispose_host_fixture();
  }
  puts("PASS 17 real-config RTX3090/AMD preflight scenarios; no GPU writes, DMA, ownership, driver release or arm");return 0;
}
