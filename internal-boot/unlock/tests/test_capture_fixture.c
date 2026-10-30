/* GPL-2.0-only. Exercise the real app fixture setup and port in host RAM.
 * No efi_main, firmware asset loading, vendor transaction or device access. */
#define GU_EMPTY_APP
#define GU_PREFLIGHT_FIXTURE
#include "test_app.c"
#undef main

static const UINT64 decode_bits[]={0,0x100,0x200,0x300,0x400,0x500,0x600,0x700};
static void capture_command(UINT16 command) {
  setup(SUCCESS);
  memcpy(captured_config,config,sizeof(captured_config));
  captured_config[1]=(captured_config[1]&0xffff0000U)|command;
}
static void no_transaction(void) {
  CHECK(!fake.mmio_writes && !fake.flrs && !fake.maps && !fake.live_maps && !fake.live_buffers);
  CHECK(!fake.pio_reads && !fake.pio_writes && !fake.dmem_reads);
}
int main(void) {
  UINTN scenarios=0;
  for(UINT16 command=0;command<8;command++) {
    capture_command((UINT16)(command|0x400));
    app_setup(APP_SUCCESS);
    CHECK(fake.pci_attributes==decode_bits[command]);
    CHECK(!memcmp(config,captured_config,sizeof(config)));
    CHECK(!fake.master_enables && !fake.master_disables && !fake.pci_writes);
    no_transaction();dispose_host_fixture();scenarios++;
  }
  for(UINT16 command=2;command<=3;command++) {
    for(UINTN mode=0;mode<APP_COUNT;mode++) {
      capture_command(command);app_setup((enum app_case)mode);
      CHECK(fake.pci_attributes==decode_bits[command]);
      CHECK(!fake.master_enables && !fake.master_disables && !fake.pci_writes);
      no_transaction();dispose_host_fixture();scenarios++;
    }
    capture_command((UINT16)(command|0x400));app_setup(APP_SUCCESS);
    /* Model the post-release, exclusively owned phase, without calling EFI. */
    disconnect_calls=1;exclusive=TRUE;
    CHECK(GuEnableMaster()==EFI_SUCCESS && !Gu.fault);
    CHECK(fake.master_enables==1 && fake.pci_attributes==decode_bits[command|4]);
    CHECK(config[4]==(command|4) && config[5]==4);
    CHECK(GuClearMaster()==EFI_SUCCESS && !Gu.master_attributes_pending);
    CHECK(fake.master_disables==1 && fake.pci_attributes==decode_bits[command]);
    CHECK(!memcmp(config,captured_config,sizeof(config)));
    no_transaction();dispose_host_fixture();scenarios++;
  }
  const struct { UINT16 command;UINT64 attributes; } invalid[]={
    {2,0x300},{3,0x200},{2,0x600},{3,0x700},{0,0},{1,0x100},{6,0x600},{7,0x700}
  };
  for(UINTN i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
    capture_command(invalid[i].command);app_setup(APP_SUCCESS);
    fake.pci_attributes=invalid[i].attributes;
    disconnect_calls=1;exclusive=TRUE;
    CHECK(GuEnableMaster()!=EFI_SUCCESS && Gu.fault);
    CHECK(!fake.master_enables && !fake.master_disables && !fake.pci_writes);
    CHECK(fake.pci_attributes==invalid[i].attributes);
    CHECK(!memcmp(config,captured_config,sizeof(config)));
    CHECK(!Gu.master_attributes_pending);
    no_transaction();dispose_host_fixture();scenarios++;
  }
  printf("PASS %llu capture-setup/production-port scenarios; host RAM only, no app or vendor transaction\n",
    (unsigned long long)scenarios);
  return 0;
}
