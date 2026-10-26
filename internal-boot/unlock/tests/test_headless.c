/* Production child/core in host RAM; all console methods are traps. */
#include "test_entry_latched.c"
#undef main
#include "../../boot/media.h"
static UINT8 Media[46];static int Case,Reads,Outputs;
#ifdef GU_USB_MEDIA
static BOOLEAN UseInternalMedia;
#endif
static BOOLEAN LatePending,LateDelivered,LatePhysical;
static EFI_GUID TextGuid=EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID,InputGuid=EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID;
static EFI_STATUS EFIAPI HeadOutput(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *p,CHAR16 *s){(void)p;(void)s;Outputs++;return EFI_DEVICE_ERROR;}
static EFI_STATUS EFIAPI HeadHandle(EFI_HANDLE h,EFI_GUID *g,VOID **p) {
  if(h==H(2) && guid(g,&dg)){*p=Media;return EFI_SUCCESS;}
  if(h==H(6) && guid(g,&TextGuid)){CHECK(current_tpl==TPL_NOTIFY);*p=&conout;return EFI_SUCCESS;}
  if(h==H(6) && guid(g,&InputGuid)){CHECK(current_tpl==TPL_NOTIFY);*p=&conin;return EFI_SUCCESS;}
  return preflight_handle(h,g,p);
}
static EFI_STATUS EFIAPI HeadLocate(EFI_LOCATE_SEARCH_TYPE t,EFI_GUID *g,VOID *key,UINTN *n,EFI_HANDLE **h) {
  if(t==AllHandles){*n=1;CHECK(allocate_pool(EfiLoaderData,sizeof(**h),(VOID **)h)==EFI_SUCCESS);(*h)[0]=H(3);return EFI_SUCCESS;}
  if(guid(g,&gg)) {
    CHECK(current_tpl==TPL_NOTIFY);
    *n=0;*h=NULL;if(Case!=1 && Case!=2 && Case!=3 && !LatePhysical)return EFI_NOT_FOUND;
    *n=1;CHECK(allocate_pool(EfiLoaderData,sizeof(**h),(VOID **)h)==EFI_SUCCESS);
    (*h)[0]=Case==1 || LatePhysical?H(5):H(6);return EFI_SUCCESS;
  }
  return preflight_locate(t,g,key,n,h);
}
static EFI_STATUS EFIAPI HeadFree(VOID *memory) {
  if((Case==14 || Case==15) && Reads>=5 && GuHeadless.ready && !LateDelivered) {
    /* A protocol notification scheduled by pool cleanup must not execute in
     * the middle of the observation/baseline commit. */
    CHECK(current_tpl==TPL_NOTIFY);LatePending=TRUE;
  }
  return app_free_pool(memory);
}
static VOID EFIAPI HeadRestore(EFI_TPL old) {
  restore_tpl(old);
  if(LatePending && current_tpl<TPL_NOTIFY) {
    LatePending=FALSE;LateDelivered=TRUE;
    if(Case==14)LatePhysical=TRUE;else conin.WaitForKey=(VOID *)46;
  }
}
static EFI_STATUS EFIAPI HeadOpens(EFI_HANDLE h,EFI_GUID *g,EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **e,UINTN *n) {
  if(Case==8)return EFI_WARN_UNKNOWN_GLYPH;
  EFI_STATUS s=open_info(h,g,e,n);
  if(s==EFI_SUCCESS && !*n && guid(g,&pg) && exclusive) {
    *n=Case==11 && Reads>=5?2:1;CHECK(allocate_pool(EfiLoaderData,*n*sizeof(**e),(VOID **)e)==EFI_SUCCESS);
    **e=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){Gu.image,NULL,EFI_OPEN_PROTOCOL_EXCLUSIVE,1};
    if(*n==2)(*e)[1]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){H(8),H(3),EFI_OPEN_PROTOCOL_BY_DRIVER,1};
  }
  return s;
}
static EFI_STATUS EFIAPI HeadKey(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *p,EFI_INPUT_KEY *k) {
  CHECK(p==&conin && key_reset);Reads++;
  if(Case==4 && Reads==5)st.ConsoleInHandle=H(15);
  if(Case==5 && Reads==5)conin.WaitForKey=(VOID *)44;
  if(Case==6){k->ScanCode=0x17;return EFI_SUCCESS;}
  return EFI_NOT_READY;
}
static EFI_STATUS EFIAPI HeadWrite(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *data) {
  if(Case==9)return EFI_WARN_UNKNOWN_GLYPH;
  if(Case==10 && !strncmp(data,"ARMED: one transaction",22))conin.WaitForKey=(VOID *)45;
  if(Case==12 && !strncmp(data,"ARMED: one transaction",22))config[16]^=1;
  if(Case==13 && !strncmp(data,"ARMED: one transaction",22))registers[SS0/4]^=1;
  return file_write(f,n,data);
}
static VOID HeadSetup(int mode) {
  entry_setup(mode==7?APP_FLUSH:APP_SUCCESS,0x03000062,6);Case=mode;Reads=Outputs=0;
  LatePending=LateDelivered=LatePhysical=FALSE;
  memset(Media,0,sizeof(Media));Media[0]=4;Media[1]=1;Media[2]=42;Media[4]=1;Media[16]=1;
  memcpy(Media+24,&GuInternalPartition,16);Media[40]=2;Media[41]=2;memcpy(Media+42,(UINT8[]){127,255,4,0},4);
#ifdef GU_USB_MEDIA
  /* USB build: the child volume is a USB device path (UsbPath requires one USB node). */
  if(!UseInternalMedia){memset(Media,0,sizeof(Media));memcpy(Media,(UINT8[]){3,5,6,0,1,0,127,255,4,0},10);}
#endif
  st.ConsoleInHandle=st.StandardErrorHandle=st.ConsoleOutHandle=H(6);st.StdErr=&conout;conin.WaitForKey=(VOID *)43;
  conout.OutputString=HeadOutput;conin.ReadKeyStroke=HeadKey;
  bs.HandleProtocol=HeadHandle;bs.LocateHandleBuffer=HeadLocate;bs.OpenProtocolInformation=HeadOpens;
  bs.FreePool=HeadFree;bs.RestoreTPL=HeadRestore;
  files[3].protocol.Write=HeadWrite;
  if(mode==3)modes[1].FrameBufferBase=0x123000;
}
int main(int argc,char **argv) {
  CHECK(argc==4);exact_load(argv[1],argv[2]);load_fixture_assets(argv[1],argv[3]);memcpy(originals,GuAssets,sizeof(originals));
  for(int mode=0;mode<=15;mode++) {
    HeadSetup(mode);EFI_STATUS s=run_app();
    if(mode==0 || mode==2) {
      CHECK(s==EFI_SUCCESS && Reads==30 && fake.flrs==1 && fake.gsp_starts==1 && fake.sec2_starts==1);
      CHECK(registers[SS0/4]==0x88888888 && registers[SS1/4]==8 && Gu.quiesced && !halted && !fake.pool_live);
    }else{CHECK(s!=EFI_SUCCESS && !Gu.started && !fake.mmio_writes && !fake.master_enables && !fake.flrs);}
    if(mode==14 || mode==15)CHECK(LateDelivered && !LatePending && !Gu.dma_count);
    CHECK(current_tpl==TPL_APPLICATION);
    CHECK(!Outputs && !disconnect_calls);dispose_host_fixture();
  }
#ifdef GU_USB_MEDIA
  /* A USB build loaded from an internal partition must refuse before any hardware effect. */
  UseInternalMedia=TRUE;HeadSetup(0);EFI_STATUS internal=run_app();UseInternalMedia=FALSE;
  CHECK(internal!=EFI_SUCCESS && !Gu.started && !fake.mmio_writes && !fake.master_enables && !fake.flrs);dispose_host_fixture();
  puts("PASS USB media: USB child volume accepted; internal-partition child volume refused");
#else
  HeadSetup(0);Media[24]^=1;EFI_STATUS wrong=run_app();
  CHECK(wrong!=EFI_SUCCESS && !Gu.started && !fake.mmio_writes && !fake.master_enables && !fake.flrs);dispose_host_fixture();
  HeadSetup(0);memset(Media,0,sizeof(Media));memcpy(Media,(UINT8[]){3,5,6,0,1,0,127,255,4,0},10);
  EFI_STATUS usb=run_app();
  CHECK(usb!=EFI_SUCCESS && !Gu.started && !fake.mmio_writes && !fake.master_enables && !fake.flrs);dispose_host_fixture();
  puts("PASS internal media: pinned partition accepted; wrong GUID and USB refused before hardware effects");
#endif
  puts("PASS headless real app/core: zero-GOP and virtual-only success; physical/invalid GOP, late input/log mutation, Esc, file warning/failure and owner warning blocked; console calls zero");return 0;
}
