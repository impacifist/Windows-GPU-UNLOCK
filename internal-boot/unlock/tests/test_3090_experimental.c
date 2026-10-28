/* Actual captured PROM/PCI + production app/stock core; all EFI I/O simulated. */
#define GU_EXACT_FIXTURE
#include "test_3090_preflight.c"
#undef main
#include "../sha256.h"
VOID GuLog(const char *,...);
VOID GuLogText(const char *);
/* Include production source to exercise the static manual UI without bypassing
   its checks or needing to weaken production asset authentication. */
#include "../app.c"

/* Authenticated cold line of each profile's own capture (app.c ColdCheck format). */
#if defined(GU_PROFILE_B650M_3090FE)
#define GU_TEST_COLD_LINE "cold state: chip=0xb72000a1 plm=0xffffff8f gfw=0x8b8f/0x3ff/0x10 WPR2_HI=0 SS0=0x3100062 SS1=0x6"
#elif defined(GU_PROFILE_H610M_3060)
#define GU_TEST_COLD_LINE "cold state: chip=0xb76000a1 plm=0xffffff8f gfw=0x8b8f/0x3ff/0x10 WPR2_HI=0 SS0=0x6204001 SS1=0x6" /* probe run 1 */
#elif defined(GU_PROFILE_B760MK_3060)
#define GU_TEST_COLD_LINE "cold state: chip=0xb76000a1 plm=0xffffff8f gfw=0x8b8f/0x3ff/0x10 WPR2_HI=0 SS0=0x5020024 SS1=0x5" /* capture 1 */
#elif defined(GU_PROFILE_X870_3090TI)
#define GU_TEST_COLD_LINE "cold state: chip=0xb72000a1 plm=0xffffff8f gfw=0x8b8f/0x3ff/0x10 WPR2_HI=0 SS0=0x40123070 SS1=0x7"
#elif defined(GU_PROFILE_B660PLUS_3060)
#define GU_TEST_COLD_LINE "cold state: chip=0xb74000a1 plm=0xffffff8f gfw=0x8b8f/0x3ff/0x10 WPR2_HI=0 SS0=0x60110006 SS1=0x7"
#elif defined(GU_PROFILE_LOCAL)
#define GU_TEST_COLD_LINE GU_LOCAL_TEST_COLD_LINE /* make-profile.py, from this machine's capture */
#else
#error Add this profile's captured cold-state line (app.c ColdCheck format)
#endif
static void exact_load(const char *prom_path,const char *capture_path) {
  size_t n;UINT8 *data=read_file(prom_path,&n);CHECK(n==sizeof(prom));
  CHECK(GuHashMatches(data,n,GU_PROM_SHA256));
  char memory_text[32];snprintf(memory_text,sizeof(memory_text),"memory=%#x",GU_DEVICE_MEMORY_SIZE);
  memcpy(prom,data,n);free(data);
  FILE *file=fopen(capture_path,"r");CHECK(file);char line[512];unsigned count=0,offset,a,b,c,d;
  BOOLEAN cold=FALSE,memory=FALSE;
  while(fgets(line,sizeof(line),file)) {
    if(sscanf(line,"config[%x]=%x %x %x %x",&offset,&a,&b,&c,&d)==5) {
      CHECK(offset==count*4 && count<=60);
      captured_config[count++]=a;captured_config[count++]=b;captured_config[count++]=c;captured_config[count++]=d;
    }
     /* Authenticate the original capture. The selected SS0 in exact_setup is
        simulated input, not a claim that this capture contains the other SS0. */
     if(strstr(line,GU_TEST_COLD_LINE))cold=TRUE;
    if(strstr(line,memory_text))memory=TRUE;
  }
  fclose(file);CHECK(count==64 && cold && memory);
}
static void exact_setup(enum app_case mode) {
  negative=0;app_setup(mode);memset(&amd,0,sizeof(amd));amd.GetLocation=amd_location;amd.Pci.Read=amd_read;
  bs.HandleProtocol=preflight_handle;bs.LocateDevicePath=resolve;bs.LocateHandleBuffer=preflight_locate;pci.GetBarAttributes=preflight_bar;
  infos[0].HorizontalResolution=infos[0].PixelsPerScanLine=2560;infos[0].VerticalResolution=1440;
  modes[0].Mode=0;modes[0].MaxMode=13;modes[0].FrameBufferBase=0xfce0000000ULL;modes[0].FrameBufferSize=0xe10000;
  registers[0]=GU_CHIP_ID;registers[SS0/4]=GU_STOCK_PROFILE_SS0;registers[SS1/4]=6;
  registers[0x1183a4/4]=GU_DEVICE_MEMORY_SIZE;
}
static EFI_STATUS EFIAPI changed_cold_disconnect(EFI_HANDLE h,EFI_HANDLE a,EFI_HANDLE c) {
  EFI_STATUS result=disconnect(h,a,c);registers[SS0/4]^=1;return result;
}
static UINT32 transition_ss0;
static EFI_STATUS EFIAPI transition_key(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *p,EFI_INPUT_KEY *key) {
  EFI_STATUS result=read_key(p,key);
  if(!EFI_ERROR(result))registers[SS0/4]=transition_ss0;
  return result;
}
static EFI_STATUS EFIAPI transition_disconnect(EFI_HANDLE h,EFI_HANDLE a,EFI_HANDLE c) {
  EFI_STATUS result=disconnect(h,a,c);registers[SS0/4]=transition_ss0;return result;
}
static void no_effects(void) {
  CHECK(!Gu.started && !Gu.armed && !Gu.allocations && !Gu.owned);
  CHECK(!fake.mmio_writes && !fake.pci_writes && !fake.maps && !fake.flrs && !fake.master_enables);
  CHECK(!disconnect_calls && !attribute_enables && !halted && !exclusive && !fake.pool_live);
}
#ifdef GU_ENTRY_LATCHED
#define main historical_experimental_fixture_main
#endif
int main(int argc,char **argv) {
  CHECK(argc==3 || argc==4);exact_load(argv[1],argv[2]);
  struct gpu_unlock_fwsec fw;CHECK(!gpu_unlock_vbios_fwsec(prom,sizeof(prom),&fw));
  CHECK(fw.image_off==0x4239c && fw.image_size==0xde00 && fw.imem_size==0xd600 &&
    fw.dmem_size==0x800 && fw.engine_mask==0x400 && fw.ucode_id==9 && fw.sig_versions==7);
  UINT8 image[0xde00];memcpy(image,prom+fw.image_off,sizeof(image));
  CHECK(!gpu_unlock_fwsec_patch_frts(image,&fw,0x5ffe00000ULL));
  UINT32 sig_at;CHECK(!gpu_unlock_fwsec_sig_offset(fw.sig_versions,2,384,&sig_at));
  CHECK(gpu_unlock_fwsec_sig_offset(fw.sig_versions,32,384,&sig_at));
  image[fw.data_off+fw.interface_off]=0xff;
  CHECK(gpu_unlock_fwsec_patch_frts(image,&fw,0x5ffe00000ULL));
  puts("PASS actual RTX3090 PROM FWSEC parser, 24GB FRTS patch, unknown signature/layout rejection");
  const UINT32 offsets[]={SS0,SS1,0,0x1183a4,0x118128,0x118234,0x110100,0x1fa828,PLM};
  for(unsigned i=0;i<sizeof(offsets)/sizeof(offsets[0]);i++) {
    exact_setup(APP_SUCCESS);registers[offsets[i]/4]^=1;
    CHECK(EFI_ERROR(efi_main(H(1),&st)));no_effects();
    CHECK(!Gu.profile_verified && !key_calls && !exclusive_calls);dispose_host_fixture();
  }
  exact_setup(APP_SUCCESS);registers[SS0/4]=GU_STOCK_PROFILE_SS0==0x03100062U?0x03000062U:0x03100062U;
  CHECK(EFI_ERROR(efi_main(H(1),&st)));no_effects();CHECK(!Gu.profile_verified && !key_calls && !exclusive_calls);dispose_host_fixture();
  puts("PASS opposite compile-time SS0 profile rejected before side effects");
  const UINT32 rejected_ss0[]={0,0xffffffffU,0x88888888U,0x46254040U,0x02000062U,0x13100062U};
  for(unsigned i=0;i<sizeof(rejected_ss0)/sizeof(rejected_ss0[0]);i++) {
    exact_setup(APP_SUCCESS);registers[SS0/4]=rejected_ss0[i];
    CHECK(EFI_ERROR(efi_main(H(1),&st)));no_effects();
    CHECK(!Gu.profile_verified && !key_calls && !exclusive_calls);dispose_host_fixture();
  }
  exact_setup(APP_SUCCESS);registers[0x300000/4]^=1;
  CHECK(efi_main(H(1),&st)==EFI_CRC_ERROR);no_effects();CHECK(!Gu.profile_verified);dispose_host_fixture();
  /* Valid PROM/cold profile reaches asset loading. No assets => cannot prompt. */
  exact_setup(APP_SUCCESS);CHECK(EFI_ERROR(efi_main(H(1),&st)));
  no_effects();CHECK(Gu.profile_verified && exclusive_calls==1 && !key_calls);dispose_host_fixture();
  puts("PASS exact captured PROM/PCI, nine cold profile mutations and asset gate");
  for(unsigned i=0;i<3;i++) {
    exact_setup(i==0?APP_SUCCESS:i==1?APP_ESCAPE:APP_TIMEOUT);
    Table=&st;GuIoOpen(H(1),&st);Gu.prom_verified=TRUE;Gu.armed=FALSE;
    CHECK(!EFI_ERROR(ColdCheck()));
    EFI_STATUS armed=Arm();
    CHECK(armed==(i==0?EFI_SUCCESS:i==1?EFI_ABORTED:EFI_TIMEOUT));
    CHECK(key_reset && prompt && key_calls==(i==2?300U:1U));
    CHECK(!Gu.armed && !fake.mmio_writes && !fake.maps && !disconnect_calls);
    CHECK(!EFI_ERROR(GuIoClose()));dispose_host_fixture();
  }
  puts("PASS production manual U, Esc and 30-second timeout UI");
  exact_setup(APP_SUCCESS);Table=&st;Gu.profile_verified=FALSE;Gu.armed=FALSE;
  CHECK(Arm()==EFI_ACCESS_DENIED && !key_calls && !key_reset);dispose_host_fixture();
  if(argc!=4) {
    puts("PARTIAL: stock app transaction and manual-input mocks require -Assets fixtures");return 0;
  }
  load_fixture_assets(argv[1],argv[3]);
  memcpy(originals,GuAssets,sizeof(originals));
  const UINT32 transitions[]={GU_STOCK_PROFILE_SS0==0x03100062U?0x03000062U:0x03100062U,0,0xffffffffU,0x88888888U};
  for(unsigned handoff=0;handoff<2;handoff++)for(unsigned i=0;i<sizeof(transitions)/sizeof(transitions[0]);i++) {
    exact_setup(handoff?APP_HANDOFF:APP_SUCCESS);transition_ss0=transitions[i];
    if(handoff)bs.DisconnectController=transition_disconnect;else conin.ReadKeyStroke=transition_key;
    EFI_STATUS result=EFI_ABORTED;if(!setjmp(escape_halt))result=efi_main(H(1),&st);
    CHECK(EFI_ERROR(result) && !Gu.profile_verified && key_calls==1);
    CHECK(!Gu.started && !fake.mmio_writes && !fake.pci_writes && !fake.maps && !fake.flrs && !fake.master_enables && !fake.master_disables && !attribute_enables);
    CHECK(strstr(report_text,GU_STOCK_PROFILE_NAME) && strstr(report_text,"cold expected:") && strstr(report_text,"cold observed:"));
    if(handoff)CHECK(halted && Gu.retained && disconnect_calls==1 && strstr(report_text,"stage=cold-after-release") && !Gu.allocations);
    else {no_effects();CHECK(strstr(report_text,"stage=armed-cold-state"));}
    dispose_host_fixture();
  }
  puts("PASS SS0 opposite/arbitrary transitions at U and handoff rejected with no GPU writes/DMA/FLR");
  const enum app_case modes_to_test[]={APP_SUCCESS,APP_ESCAPE,APP_TIMEOUT,APP_HANDOFF,APP_HANDOFF_ESCAPE,
    APP_HANDOFF_TIMEOUT,APP_HANDOFF_PROM,APP_HANDOFF_CONFIG,APP_DIRTY,APP_UNMAP,APP_HANDOFF};
  for(unsigned i=0;i<sizeof(modes_to_test)/sizeof(modes_to_test[0]);i++) {
    enum app_case mode=modes_to_test[i];exact_setup(mode);
    BOOLEAN changed_cold=i==10;if(changed_cold)bs.DisconnectController=changed_cold_disconnect;
    EFI_STATUS result=EFI_ABORTED;if(!setjmp(escape_halt))result=efi_main(H(1),&st);
    if(!changed_cold && (mode==APP_SUCCESS || mode==APP_HANDOFF || mode==APP_HANDOFF_CONFIG)) {
      CHECK(result==EFI_SUCCESS && !halted && Gu.started && Gu.quiesced && !Gu.allocations);
      CHECK(fake.gsp_starts==1 && fake.sec2_starts==1 && fake.flrs==1);
      CHECK(registers[SS0/4]==0x88888888 && registers[SS1/4]==8 && !(config[4]&4));
      CHECK(key_calls==1 && key_reset && prompt && !Gu.armed);
      if(mode!=APP_SUCCESS)CHECK(disconnect_calls==1 && strstr(report_text,"stage=prom-after-release status=0"));
    } else if(mode==APP_ESCAPE || mode==APP_TIMEOUT || mode==APP_HANDOFF_ESCAPE || mode==APP_HANDOFF_TIMEOUT) {
      CHECK(result==(mode==APP_ESCAPE || mode==APP_HANDOFF_ESCAPE?EFI_ABORTED:EFI_TIMEOUT));
      no_effects();CHECK(prompt && key_reset && key_calls==(UINTN)(mode==APP_TIMEOUT || mode==APP_HANDOFF_TIMEOUT?300:1));
    } else {
      CHECK(halted && Gu.retained && !(config[4]&4));
      if(mode==APP_HANDOFF_PROM || changed_cold)CHECK(!Gu.started && !fake.maps && !Gu.profile_verified);
      else CHECK(Gu.started && Gu.allocations);
    }
    printf("Exact-card app %s: PASS (simulated)\n",app_names[mode]);dispose_host_fixture();
  }
  for(int i=0;i<5;i++)free((VOID *)originals[i].firmware.data);
  puts("PASS exact-card manual input and stock app transactions; hardware untested");return 0;
}
