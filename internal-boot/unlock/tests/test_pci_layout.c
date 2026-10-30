/* SPDX-License-Identifier: GPL-2.0-only */
/* Production save/FLR/restore on RAM-only PCI mocks, without firmware assets. */
#define main legacy_port_main
#include "test_port.c"
#undef main
#include "../checks.h"

static void layout_word(unsigned offset,UINT16 value) {memcpy(config+offset,&value,2);}

static void setup_layout(unsigned version,unsigned at,unsigned command) {
  setup();unsigned pm=at==0x40?0xa0:0x40;
  memset(config+0x40,0,0xc0);config[4]=(UINT8)command;config[6]=0x10;config[0x34]=(UINT8)pm;
  config[pm]=1;config[pm+1]=(UINT8)at;layout_word(pm+2,3);
  config[at]=0x10;layout_word(at+2,(UINT16)version);
  UINT32 support=1U<<28;memcpy(config+at+4,&support,4);
  layout_word(at+8,0x2930);layout_word(at+10,0x1040);
  if(version==2)layout_word(at+0x28,0x4321);
  GU_CAPS caps;UINT32 snapshot[64];memcpy(snapshot,config,sizeof(snapshot));
  assert(GuCheckConfig(snapshot,GU_TARGET_SEGMENT,GU_TARGET_BUS,0,0,&caps));
  Gu.pcie_cap=caps.pcie;Gu.pcie_version=caps.pcie_version;Gu.pm_cap=caps.pm;
}

int main(void) {
  unsigned cases=0;
  const unsigned layouts[][2]={{1,0xec},{1,0xd8},{1,0x48},{2,0xc4},{2,0x5c},{2,0x40}};
  for(unsigned i=0;i<sizeof(layouts)/sizeof(layouts[0]);i++)
  for(unsigned command=2;command<=3;command++) {
    unsigned version=layouts[i][0],at=layouts[i][1];
    setup_layout(version,at,command);
    UINT32 snapshot[64];memcpy(snapshot,config,sizeof(snapshot));
    assert(!EFI_ERROR(GuSavePci()));
    assert(!EFI_ERROR(GuFlr()) && fake.flr_seen && !fake.config_too_early && Gu.flrs==1);
    assert(!EFI_ERROR(GuRestorePci()));
    assert(!memcmp(config+0x10,(UINT8 *)snapshot+0x10,24));
    assert(config[4]==command && !(config[0x30]&1));
    UINT16 value;memcpy(&value,config+at+8,2);assert(value==0x2930);
    memcpy(&value,config+at+10,2);assert(value==0x1040);
    if(version==2){memcpy(&value,config+at+0x28,2);assert(value==0x4321);}
    assert(!memcmp(config+Gu.pm_cap,(UINT8 *)snapshot+Gu.pm_cap,8));
    assert(EFI_ERROR(GuFlr()) && Gu.flrs==1);
    cases++;
  }
  /* Exercise failure containment at both late boundaries as well as early layouts. */
  for(unsigned i=0;i<sizeof(layouts)/sizeof(layouts[0]);i++)
  for(unsigned failure=0;failure<8;failure++) {
    setup_layout(layouts[i][0],layouts[i][1],2);
    if(failure==0) {
      config[4]|=4;assert(EFI_ERROR(GuSavePci()) && !Gu.saved && !fake.pciwrites);
    } else if(failure==1) {
      fake.config_fail=1;assert(EFI_ERROR(GuSavePci()) && !Gu.saved && !fake.pciwrites);
    } else if(failure==2) {
      layout_word(Gu.pcie_cap+8,0xa930);
      assert(EFI_ERROR(GuSavePci()) && !Gu.saved && !fake.pciwrites);
    } else {
      assert(!EFI_ERROR(GuSavePci()));
      if(failure==3) {
        config[Gu.pcie_cap+10]|=0x20;
        assert(GuFlr()==EFI_TIMEOUT && !fake.flr_seen && !fake.pciwrites && fake.delay==1000000);
      } else if(failure==4) {
        fake.config_fail=1;assert(EFI_ERROR(GuFlr()) && !fake.flr_seen && !fake.pciwrites);
      } else if(failure==5 || failure==6) {
        fake.lost_after_flr=failure==5;fake.flr_write_error=failure==6;
        assert(EFI_ERROR(GuFlr()) && fake.flr_seen && fake.delay>=100000 && !fake.config_too_early);
        UINTN writes=fake.pciwrites;
        assert(EFI_ERROR(GuRestorePci()) && fake.pciwrites==writes);
      } else {
        assert(!EFI_ERROR(GuFlr()));fake.config_fail=1;
        UINTN writes=fake.pciwrites;
        assert(EFI_ERROR(GuRestorePci()) && fake.pciwrites==writes && !fake.config_too_early);
      }
    }
    cases++;
  }
  const unsigned invalid[][2]={{0,0x78},{3,0x78},{255,0x78},{1,0x3c},
    {1,0x41},{1,0xf0},{1,0xfc},{2,0xc8},{2,0xd4},{2,0xff},{2,0xffff}};
  for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
    setup();Gu.pcie_version=(UINT8)invalid[i][0];Gu.pcie_cap=(UINT16)invalid[i][1];
    assert(GuSavePci()==EFI_NOT_READY && !Gu.saved);
    assert(!fake.pcireads && !fake.pciwrites && !fake.reads && !fake.writes);
    cases++;
  }
  printf("PASS %u PCI layout boundary/FLR/restore cases; RAM-only mocks, no GPU access\n",cases);
  return 0;
}
