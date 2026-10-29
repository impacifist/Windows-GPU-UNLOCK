/* SPDX-License-Identifier: MIT */
/* Include the pinned MIT host fixture; every protocol callback is synthetic. */
#define main UpstreamHostMain
#include "host_tests.c"
#undef main

static unsigned Fault,ExtraMemoryReads,ExtraConfigReads;
static EFI_STATUS EFIAPI ResearchConfig(EFI_PCI_IO_PROTOCOL *self,
    EFI_PCI_IO_PROTOCOL_WIDTH width,UINT32 offset,UINTN count,VOID *out) {
    ExtraConfigReads++;
    if(ExtraConfigReads==2 && Fault==1)return EFI_DEVICE_ERROR;
    EFI_STATUS result=ConfigRead(self,width,offset,count,out);
    if(ExtraConfigReads==2 && Fault==2)((UINT8 *)out)[8]^=1;
    return result;
}
static EFI_STATUS EFIAPI ResearchRead(EFI_PCI_IO_PROTOCOL *self,
    EFI_PCI_IO_PROTOCOL_WIDTH width,UINT8 bar,UINT64 offset,UINTN count,VOID *out) {
    ExtraMemoryReads++;
    if(offset==0 && ((Fault==3 && ExtraMemoryReads==259) ||
                     (Fault==9 && ExtraMemoryReads==23)))return EFI_DEVICE_ERROR;
    EFI_STATUS result=MemoryRead(self,width,bar,offset,count,out);
    if(result!=EFI_SUCCESS)return result;
    if((Fault==4 && ExtraMemoryReads==259) ||
       (Fault==10 && ExtraMemoryReads==23))*(UINT32 *)out^=1;
    if(Fault==5 && offset==0x300000 && count==1024)((UINT32 *)out)[0]^=1;
    if(ExtraMemoryReads==1 && Fault==6)*(UINT32 *)out=0;
    if(ExtraMemoryReads==1 && Fault==7)*(UINT32 *)out=0xffffffff;
    if(ExtraMemoryReads==2 && Fault==8)*(UINT32 *)out=0xbadf0001;
    return result;
}
int main(void) {
    static CHAR8 data[3*1024*1024];
    static const UINTN expected[]={0,258,258,259,259,259,1,1,2,23,23};
    static const char *names[]={"unused","prom-final-pci-failure","prom-final-pci-drift",
      "prom-final-chip-failure","prom-final-chip-drift","prom-signature-change",
      "register-chip-zero","register-chip-all-ones","register-badf-sentinel",
      "register-final-chip-failure","register-final-chip-drift"};
    for(Fault=1;Fault<=10;Fault++) {
        TEXT t;Setup(0);ExtraMemoryReads=ExtraConfigReads=0;
        MockPci[0].Pci.Read=ResearchConfig;MockPci[0].Mem.Read=ResearchRead;
        TextInit(&t,data,sizeof(data));
        UINTN attempts=Fault<=5?SampleProm(&t,&MockPci[0],&Bs):SampleRegisters(&t,&MockPci[0],&Bs);
        CHECK(!t.failed && attempts==expected[Fault] && ExtraMemoryReads==attempts);
        CHECK(strstr(data,"\"complete\":false") && strstr(data,"\"unlock_eligibility\":false"));
        if(Fault<=5)CHECK(strstr(data,"\"sha256\":null") && strstr(data,"\"hex\":null"));
        char path[100];snprintf(path,sizeof(path),"build/%s.json",names[Fault]);
        FILE *f=fopen(path,"wb");CHECK(f);CHECK(fwrite(data,1,t.len,f)==t.len);CHECK(!fclose(f));
    }
    printf("PASS: 10 new synthetic capture failure scenarios; %u assertions; no device access.\n",Checks);
    return 0;
}
