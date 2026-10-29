/* SPDX-License-Identifier: GPL-2.0-only */
/* CPU virtual memory only. No device handles, driver, or firmware execution. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "vbios_610.h"
#include "vbios_fixture.h"

static unsigned Calls,Accepted,Rejected,Exceptions;
static int ParseGuarded(const u8 *input,size_t length,int *accepted) {
    struct {uint64_t before;struct gpu_unlock_fwsec fw;uint64_t after;} output;
    output.before=output.after=UINT64_C(0x3456789abcdef012);
    memset(&output.fw,0x5a,sizeof(output.fw));
    int result=0;Calls++;
    __try {result=gpu_unlock_vbios_fwsec(input,length,&output.fw);}
    __except(EXCEPTION_EXECUTE_HANDLER) {Exceptions++;return 0;}
    if(output.before!=UINT64_C(0x3456789abcdef012) || output.after!=output.before)return 0;
    *accepted=!result;
    if(result){Rejected++;return 1;}
    Accepted++;
    const struct gpu_unlock_fwsec *f=&output.fw;
    return (uint64_t)f->image_off+f->image_size<=length &&
           (uint64_t)f->sig_off+f->sig_bytes<=length &&
           (uint64_t)f->data_off+f->dmem_size<=f->image_size &&
           (uint64_t)f->pkc_off+384<=f->dmem_size &&
           (uint64_t)f->interface_off+4<=f->dmem_size;
}
static int Check(u8 *page,SIZE_T page_size,const u8 *source,size_t length,int expect) {
    DWORD old;int accepted=0;
    if(!VirtualProtect(page,page_size,PAGE_READWRITE,&old))return 0;
    u8 *input=page+page_size-length;
    if(length)memcpy(input,source,length);
    if(!VirtualProtect(page,page_size,PAGE_READONLY,&old))return 0;
    if(!ParseGuarded(input,length,&accepted))return 0;
    return expect<0 || accepted==expect;
}
static void BitChecksum(u8 *rom) {
    unsigned sum=0;rom[75]=0;
    if(rom[72]<12)return;
    for(unsigned i=64;i<64u+rom[72];i++)sum+=rom[i];
    rom[75]=(u8)(0u-sum);
}
int main(void) {
    SYSTEM_INFO info;GetSystemInfo(&info);SIZE_T page_size=info.dwPageSize;
    if(page_size<4096)return 2;
    u8 *allocation=VirtualAlloc(NULL,page_size*3,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);
    if(!allocation)return 2;
    u8 *page=allocation+page_size;u8 seed[4096],mutated[4096];
    vbios_fixture(seed,8);
#define CASE(src,len,expect) do {if(!Check(page,page_size,src,len,expect)){fprintf(stderr,"FAIL guarded case %u line %d\n",Calls,__LINE__);VirtualFree(allocation,0,MEM_RELEASE);return 1;}}while(0)
    for(size_t length=0;length<=sizeof(seed);length++)CASE(seed,length,length==sizeof(seed));
    for(unsigned byte=0;byte<sizeof(seed);byte++)for(unsigned bit=0;bit<8;bit++) {
        memcpy(mutated,seed,sizeof(seed));mutated[byte]^=(u8)(1u<<bit);
        CASE(mutated,sizeof(mutated),-1);
    }
    /* Named edge families; some deliberately remain structurally valid. */
    static const struct {unsigned offset;u32 value;} fields[]={
      {80,4092},{80,4093},{80,0xffffffff},{100,4090},{100,4091},{100,0xffffffff},
      {142,4096-44},{142,4096-43},{142,0xffffffff},
      {256,(43u<<16)|0x301},{256,(44u<<16)|0x301},
      {256,(428u<<16)|0x301},{256,(65535u<<16)|0x301},
      {260,0},{260,1},{260,255},{260,256},{260,0xffffff00},{260,0xffffffff},
      {264,640},{264,641},{268,1020},{268,1021},{276,256},{276,257},{288,1024},{288,1025}};
    for(unsigned i=0;i<sizeof(fields)/sizeof(fields[0]);i++) {
        memcpy(mutated,seed,sizeof(seed));vbios_w32(mutated+fields[i].offset,fields[i].value);
        CASE(mutated,sizeof(mutated),-1);
    }
    for(unsigned field=0;field<3;field++)for(unsigned value=0;value<4;value++) {
        static const u8 sizes[]={11,12,13,255},strides[]={5,6,8,255},counts[]={0,1,2,255};
        memcpy(mutated,seed,sizeof(seed));
        mutated[72+field]=field==0?sizes[value]:field==1?strides[value]:counts[value];
        BitChecksum(mutated);CASE(mutated,sizeof(mutated),-1);
    }
    if(!VirtualFree(allocation,0,MEM_RELEASE))return 2;
    printf("PASS: %u guard-backed ROM calls (%u accepted, %u rejected); %u exceptions; read-only inputs and output canaries.\n",
           Calls,Accepted,Rejected,Exceptions);
    return 0;
}
