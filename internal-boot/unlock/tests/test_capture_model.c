/* SPDX-License-Identifier: GPL-2.0-only */
/* Synthetic capture model regression. No firmware assets, GPU or EFI execution. */
#define main retained_transaction_main
#include "test_efi_transaction.c"
#undef main

static void put_word(UINT8 *at,UINT16 word) {memcpy(at,&word,sizeof(word));}

int main(void) {
  static const unsigned layouts[][3]={{0x60,0x78,0x68},{0x40,0x48,0},
    {0x40,0x5c,0x48},{0xa0,0x40,0x88},{0x40,0xec,0},{0x40,0xc4,0}};
  unsigned cases=0;
  for(unsigned layout=0;layout<sizeof(layouts)/sizeof(layouts[0]);layout++)
  for(unsigned version=1;version<=2;version++)
  for(unsigned command=2;command<=3;command++) {
    if(layouts[layout][1]==0xec && version==2)continue; /* v2 cannot fit here. */
    core_use_capture=FALSE;setup(SUCCESS);
    UINT8 *bytes=(UINT8 *)core_captured_config;
    memcpy(bytes,config,sizeof(config));memset(bytes+0x40,0,0xc0);
    unsigned pm=layouts[layout][0],pcie=layouts[layout][1],msi=layouts[layout][2];
    bytes[4]=(UINT8)command;bytes[0x34]=(UINT8)pm;
    bytes[pm]=1;bytes[pm+1]=(UINT8)(msi?msi:pcie);put_word(bytes+pm+2,3);
    if(msi){bytes[msi]=5;bytes[msi+1]=(UINT8)pcie;put_word(bytes+msi+2,0x80);}
    bytes[pcie]=0x10;put_word(bytes+pcie+2,(UINT16)version);
    UINT32 flr_support=1U<<28;memcpy(bytes+pcie+4,&flr_support,4);
    put_word(bytes+pcie+8,0x1234);
    if(version==2)put_word(bytes+pcie+0x28,0x4321);
    core_use_capture=TRUE;setup(SUCCESS);
    CHECK(Gu.pm_cap==pm && Gu.pcie_cap==pcie && Gu.msi_cap==msi && !Gu.msix_cap);
    CHECK(Gu.pcie_version==version && !memcmp(config,bytes,sizeof(config)));
    CHECK(fake.pci_attributes==(EFI_PCI_IO_ATTRIBUTE_MEMORY|
      (command&1?EFI_PCI_IO_ATTRIBUTE_IO:0)));
    CHECK(GuSavePci()==EFI_SUCCESS && Gu.saved_devctl==0x1234);
    CHECK(version!=2 || Gu.saved_devctl2==0x4321);
    CHECK(!fake.pci_writes && !fake.mmio_writes && !fake.flrs);
    dispose_host_fixture();cases++;
  }
  printf("PASS %u synthetic captured-config model cases; no firmware or hardware execution\n",cases);
  return 0;
}
