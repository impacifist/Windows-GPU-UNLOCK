/* SPDX-License-Identifier: GPL-2.0-only */
/* Reuse the upstream synthetic ROM generator, never a physical card image. */
#define main UpstreamParserMain
#include "test_vbios_610.c"
#undef main

int main(void) {
    u32 offset=0;u8 rom[4096];struct gpu_unlock_fwsec fw;unsigned checks=0;
#define VERIFY(x) do {checks++;if(!(x)){fprintf(stderr,"parser case failed: %s\n",#x);return 1;}}while(0)
    VERIFY(!gpu_unlock_fwsec_sig_offset(0x8000,15,384,&offset) && offset==0);
    VERIFY(!gpu_unlock_fwsec_sig_offset(0xffff,15,384,&offset) && offset==15*384);
    offset=0xfeed;
    VERIFY(gpu_unlock_fwsec_sig_offset(1,15,384,&offset)==-ERANGE && offset==0xfeed);
    VERIFY(gpu_unlock_fwsec_sig_offset(0xffff,16,384,&offset)==-EINVAL);
    VERIFY(gpu_unlock_fwsec_sig_offset(0x10000,0,384,&offset)==-EINVAL);
    VERIFY(gpu_unlock_fwsec_sig_offset(1,0,0,&offset)==-EINVAL);
    VERIFY(gpu_unlock_fwsec_sig_offset(1,0,384,NULL)==-EINVAL);
    VERIFY(gpu_unlock_fwsec_sig_offset(0xffff,15,0xffffffff,&offset)==-EOVERFLOW);
    vbios_fixture(rom,8);w32(rom+256,(812u<<16)|0x301);rom[295]=2;rom[296]=3;
    VERIFY(!gpu_unlock_vbios_fwsec(rom,sizeof(rom),&fw) && fw.sig_count==2 &&
           fw.sig_versions==3 && fw.image_off==1068 && fw.sig_bytes==768);
    /* Parsing another valid geometry does not satisfy the exact card profile. */
    VERIFY(fw.image_off!=0x45488 && fw.image_size!=0x10300);
    for(unsigned kind=0;kind<4;kind++) {
        vbios_fixture(rom,8);
        if(kind==0)w32(rom+260,0);
        if(kind==1)w32(rom+260,0xffffffff);
        if(kind==2)rom[295]=0;
        if(kind==3)rom[296]=3;
        VERIFY(gpu_unlock_vbios_fwsec(rom,sizeof(rom),&fw)!=0);
    }
    printf("PASS: %u extra synthetic signature/descriptor checks; no board ROM or firmware execution.\n",checks);
    return 0;
}
