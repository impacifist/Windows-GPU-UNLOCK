/* SPDX-License-Identifier: MIT */
/* Characterize observation semantics using the pinned MIT synthetic graph. */
#define main UpstreamPlatformMain
#include "platform_tests.c"
#undef main

static unsigned Variant;
static EFI_STATUS EFIAPI ResearchHandle(EFI_HANDLE h,EFI_GUID *guid,VOID **out) {
    if((Variant==2 && BytesEqual(guid,&DriverGuid,16)) ||
       (Variant==3 && BytesEqual(guid,&LoadedGuid,16))) {
        *out=NULL;return EFI_UNSUPPORTED;
    }
    return Handle(h,guid,out);
}
static EFI_STATUS EFIAPI ResearchLocate(EFI_LOCATE_SEARCH_TYPE kind,EFI_GUID *guid,
    VOID *key,UINTN *count,EFI_HANDLE **out) {
    EFI_STATUS result=Locate(kind,guid,key,count,out);
    if(Variant==1)(*out)[0]=H(99); /* target path exists but target is not enumerated */
    return result;
}
static EFI_STATUS EFIAPI ResearchOpens(EFI_HANDLE h,EFI_GUID *guid,
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **out,UINTN *count) {
    EFI_STATUS result=Opens(h,guid,out,count);
    if(Variant==5)for(UINTN i=0;i<*count;i++)(*out)[i].AgentHandle=H(99);
    return result;
}
int main(void) {
    static CHAR8 data[65536];TEXT t;EFI_BOOT_SERVICES bs;EFI_SYSTEM_TABLE st;
    const char *names[]={"unused","graph-target-absent","graph-binding-missing",
      "graph-image-missing","graph-code-malformed","graph-agent-absent"};
    for(Variant=1;Variant<=5;Variant++) {
        Mode=0;SetupPe();memset(&Binding,0,sizeof(Binding));memset(&Image,0,sizeof(Image));
        Binding.DriverBindingHandle=Binding.ImageHandle=H(3);Binding.Version=0x6000b;
        Binding.Stop=(EFI_DRIVER_BINDING_STOP)(UINTN)(Pe+0x208);
        Image.ImageBase=Pe;Image.ImageSize=sizeof(Pe);Image.ImageCodeType=EfiBootServicesCode;
        Image.DeviceHandle=H(1);Image.FilePath=(EFI_DEVICE_PATH_PROTOCOL *)Parent;
        memset(&bs,0,sizeof(bs));memset(&st,0,sizeof(st));st.BootServices=&bs;
        bs.LocateHandleBuffer=ResearchLocate;bs.HandleProtocol=ResearchHandle;
        bs.FreePool=Free;bs.ProtocolsPerHandle=Protocols;bs.OpenProtocolInformation=ResearchOpens;
        if(Variant==4)Put16(68,0x14c);
        TextInit(&t,data,sizeof(data));SamplePlatform(&t,&st,H(1));
        CHECK(!t.failed && strstr(data,"\"complete\":true"));
        CHECK(strstr(data,"\"atomic_snapshot\":false") && strstr(data,"\"release_authorized\":false"));
        if(Variant==1)CHECK(strstr(data,"\"target\":null"));
        if(Variant==2 || Variant==3)CHECK(!strstr(data,"\"code\":"));
        if(Variant==4)CHECK(strstr(data,"\"code\":null"));
        if(Variant==5)CHECK(strstr(data,"\"agent\":null"));
        char path[100];snprintf(path,sizeof(path),"build/%s.json",names[Variant]);
        FILE *f=fopen(path,"wb");CHECK(f);CHECK(fwrite(data,1,t.len,f)==t.len);CHECK(!fclose(f));
    }
    CODE_VIEW view;
    SetupPe();Put16(68,0x14c);CHECK(!LoadedCodeView(Pe,sizeof(Pe),&view));
    SetupPe();Put16(88,0x10b);CHECK(!LoadedCodeView(Pe,sizeof(Pe),&view));
    SetupPe();Put32(328+8,0);CHECK(!LoadedCodeView(Pe,sizeof(Pe),&view));
    SetupPe();Put32(328+8,512*1024+1);CHECK(!LoadedCodeView(Pe,sizeof(Pe),&view));
    SetupPe();Put32(88+112+5*8,0x300);CHECK(!LoadedCodeView(Pe,sizeof(Pe),&view));
    SetupPe();Put32(88+116+5*8,12);CHECK(!LoadedCodeView(Pe,sizeof(Pe),&view));
    for(UINT32 block=7;block<=14;block++) {
        SetupPe();Put32(88+112+5*8,0x300);Put32(88+116+5*8,12);Put32(0x304,block);
        if(block!=12)CHECK(!LoadedCodeView(Pe,sizeof(Pe),&view));
    }
    /* DIR64 at 0x1fc spans eight bytes across code start 0x200. The current
       function counts starting addresses in code, so it reports zero. */
    SetupPe();Put32(88+112+5*8,0x300);Put32(88+116+5*8,12);
    Put32(0x304,12);Put16(0x308,0xa1fc);
    CHECK(LoadedCodeView(Pe,sizeof(Pe),&view) && view.relocations==0);
    printf("PASS: 5 graph-observation cases, 13 malformed PE cases, 1 relocation-overlap characterization; %u assertions.\n",Checks);
    return 0;
}
