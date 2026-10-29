/* GPL-2.0-only. Five-stage engine with RAM services; branch gate tested separately. */
#include <Uefi.h>
static BOOLEAN PfEqual(const VOID *,const VOID *,UINTN);
#define ParentBranch RealParentBranch
#include "parent-branch.h"
#undef ParentBranch
static BOOLEAN ParentBranch(EFI_SYSTEM_TABLE *,EFI_HANDLE,const VOID *,UINTN);
#define GU_AD104_PARENT_TEST
#pragma push_macro("main")
#undef main
#define main OwnerMain
#include "test_owner_child.c"
#undef main
#pragma pop_macro("main")
static int BranchCalls;
static BOOLEAN ParentBranch(EFI_SYSTEM_TABLE *t,EFI_HANDLE gpu,const VOID *base,UINTN size) {
  (void)t;(void)gpu;(void)base;(void)size;BranchCalls++;
  return ParentFault!=17 && !(ParentFault==18 && BranchCalls==2);
}
static EFI_STATUS BranchStatus;static VOID *BranchPointer;
static EFI_STATUS EFIAPI BranchHandle(EFI_HANDLE h,EFI_GUID *g,VOID **out) {
  EFI_GUID want={0xc7735a2f,0x88f5,0x4882,{0xae,0x63,0xfa,0xac,0x8c,0x8b,0x86,0xb3}};
  Check(h==(VOID *)1 && PfEqual(g,&want,sizeof(want)),"exact alternate branch protocol");
  *out=BranchPointer;return BranchStatus;
}
static CAPTURE *ParentSetup(int fault) {
  CAPTURE *c=OwnerSetup(0);ParentAttempted=FALSE;ParentFault=fault;
  ParentCalls=ParentFlushes=ParentPostFlushes=BranchCalls=0;return c;
}
int main(void) {
  CAPTURE *c=ParentSetup(0);
  Check(EdRun(c,(VOID *)1,&Io,Pins)==EFI_SUCCESS && OwnerCalls==1 && ParentCalls==1 && Stage==5,"five-stage release qualifies quiet handoff");
  Check(!AfterFreeReads && CurrentAttributes==0 && !(Config[1]&7),"decode and BME off; no freed child access");
  Check(ParentStep(c,(VOID *)1,&Io,Pins,NULL)==EFI_ALREADY_STARTED && ParentCalls==1,"parent retry refused");
  for(int f=1;f<=21;f++) {
    c=ParentSetup(f);
    Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS,"parent partial errors/state/log/branch/input drift refuse handoff");
    Check(ParentCalls==(f==10 || f==11 || f==17 || f==18?0:1),"failed intent, binding or branch admission invokes no parent");
    Check(!AfterFreeReads,"no freed child access after any parent outcome");
  }
  c=ParentSetup(0);OwnerFault=1;
  Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !ParentCalls,"child callback failure prevents parent");
  c=ParentSetup(0);ConsumerFault=2;
  Check(EdRun(c,(VOID *)1,&Io,Pins)!=EFI_SUCCESS && !ParentCalls,"consumer callback failure prevents parent");
  /* Real branch gate: fixed data fingerprint and only defined NULL absence. */
  static UINT8 image[0x4000];EFI_GUID want={0xc7735a2f,0x88f5,0x4882,{0xae,0x63,0xfa,0xac,0x8c,0x8b,0x86,0xb3}};
  memcpy(image+0x39d0,&want,sizeof(want));Bs.HandleProtocol=BranchHandle;
  BranchPointer=NULL;BranchStatus=EFI_NOT_FOUND;
  Check(RealParentBranch(&T,(VOID *)1,image,sizeof(image)),"not-found protocol admitted");
  BranchStatus=EFI_UNSUPPORTED;Check(RealParentBranch(&T,(VOID *)1,image,sizeof(image)),"unsupported absence admitted");
  BranchStatus=EFI_SUCCESS;BranchPointer=(VOID *)1;
  Check(!RealParentBranch(&T,(VOID *)1,image,sizeof(image)),"alternate protocol present rejected");
  BranchPointer=NULL;BranchStatus=EFI_DEVICE_ERROR;
  Check(!RealParentBranch(&T,(VOID *)1,image,sizeof(image)),"query error is not absence");
  BranchStatus=EFI_NOT_FOUND;BranchPointer=(VOID *)1;
  Check(!RealParentBranch(&T,(VOID *)1,image,sizeof(image)),"non-null pointer with absence rejected");
  image[0x39d0]^=1;Check(!RealParentBranch(&T,(VOID *)1,image,sizeof(image)),"branch GUID drift rejected");
  Check(!RealParentBranch(&T,(VOID *)1,image,0x39df),"truncated GUID rejected");
  printf("PASS direct parent stop: %d checks, RAM-only mocks\n",Checks);return 0;
}
