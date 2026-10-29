/* GPL-2.0-only. Pin the alternate-branch GUID and require protocol absence. */
#ifndef GU_PARENT_BRANCH_H
#define GU_PARENT_BRANCH_H
static BOOLEAN ParentBranch(EFI_SYSTEM_TABLE *t,EFI_HANDLE gpu,const VOID *base,UINTN size) {
  EFI_GUID alternate={0xc7735a2f,0x88f5,0x4882,{0xae,0x63,0xfa,0xac,0x8c,0x8b,0x86,0xb3}};
  if(!base || size<0x39e0 || (UINTN)base>MAX_UINTN-size ||
     !PfEqual((const UINT8 *)base+0x39d0,&alternate,sizeof(alternate)))return FALSE;
  VOID *p=NULL;
  EFI_STATUS s=t->BootServices->HandleProtocol(gpu,&alternate,&p);
  return (s==EFI_UNSUPPORTED || s==EFI_NOT_FOUND) && !p;
}
#endif
