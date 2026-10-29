/* GPL-2.0-only. RAM-only measured-input arithmetic and rejection tests. */
#include "entry-inputs.h"
#include <stdio.h>
#include <string.h>
static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"entry input check failed line %d\n",__LINE__);return 1;}}while(0)
int main(void){
  UINT32 offsets[]={0,0x118128,0x118234,0x110100,0x1fa824,0x1fa828,0x1fa7cc,0x1fa7c4,0x9a0148,0x823804,0x823814,0x823818,0x82381c,0x823820,0x1183a4,0x625f04,0x1fa7c8,0x1fa82c,0x1fa830,0x824148,0x8241e0,0};
  UINT32 v[22]={0};EFI_STATUS s[22]={0};GU_ENTRY_INPUTS a={0};
  v[0]=v[21]=0x194000a1;v[13]=5;v[14]=0x2ffa;v[15]=0x10e09;v[16]=0x45cb8f;v[17]=0x100000;v[18]=0x2ef9ff0;
  CHECK(GuAuditEntryInputs(offsets,v,s,22,&a));
  CHECK(a.Fb==0x2ffa00000ULL && a.RawVga==0x10e0000 && a.PolicyVga==0x2ff9e0000ULL);
  CHECK(a.MmuLow==0x10000000 && a.MmuHigh==0x2ef9ff000ULL && a.MmuTop==0x2efa00000ULL);
  CHECK(a.VgaValid && a.MmuRangeValid && !a.TopLock && !a.ReferenceSs1 && !a.ReferenceVga);
  CHECK(a.Fb-a.MmuTop==0x10000000);
  GU_ENTRY_INPUTS saved=a;
  v[0]=0;CHECK(!GuAuditEntryInputs(offsets,v,s,22,&a));v[0]=0x194000a1;
  v[21]=0;CHECK(!GuAuditEntryInputs(offsets,v,s,22,&a));v[21]=0x194000a1;
  v[15]=(UINT32)((a.Fb>>16)<<8)|8;CHECK(!GuAuditEntryInputs(offsets,v,s,22,&a));v[15]=0x10e09;
  for(UINTN i=0;i<22;i++){
    offsets[i]^=4;CHECK(!GuAuditEntryInputs(offsets,v,s,22,&a));offsets[i]^=4;
    s[i]=EFI_DEVICE_ERROR;CHECK(!GuAuditEntryInputs(offsets,v,s,22,&a) && !memcmp(&saved,&a,sizeof(a)));s[i]=0;
    UINT32 old=v[i];v[i]=MAX_UINT32;CHECK(!GuAuditEntryInputs(offsets,v,s,22,&a));v[i]=0xbadf5620;CHECK(!GuAuditEntryInputs(offsets,v,s,22,&a));v[i]=old;
  }
  CHECK(!GuAuditEntryInputs(offsets,NULL,s,22,&a) && !GuAuditEntryInputs(offsets,v,NULL,22,&a) && !GuAuditEntryInputs(offsets,v,s,21,&a) && !GuAuditEntryInputs(offsets,v,s,22,NULL));
  v[14]=255;CHECK(!GuAuditEntryInputs(offsets,v,s,22,&a));v[14]=0x2ffa;
  v[16]=0;CHECK(!GuAuditEntryInputs(offsets,v,s,22,&a));v[16]=1;
  v[13]=1;v[15]=1;CHECK(GuAuditEntryInputs(offsets,v,s,22,&a) && a.ReferenceSs1 && a.ReferenceVga && !a.VgaValid && a.PolicyVga==a.Fb-0x100000);
  v[18]=0x2ff9ff0;CHECK(GuAuditEntryInputs(offsets,v,s,22,&a) && a.TopLock);
  v[17]=0x3000000;CHECK(GuAuditEntryInputs(offsets,v,s,22,&a) && !a.MmuRangeValid && !a.TopLock);
  v[15]=0xffffffffU;CHECK(!GuAuditEntryInputs(offsets,v,s,22,&a));
  printf("PASS entry input audit: %d checks, CPU-only; no layout/admission authority\n",checks);return 0;
}
