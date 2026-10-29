/* GPL-2.0-only. One pinned zero-child Stop after verified child removal. */
#include "parent-branch.h"
static BOOLEAN ParentAttempted;
static EFI_STATUS ParentCollect(CAPTURE *c,EFI_HANDLE gpu,EFI_PCI_IO_PROTOCOL *io,
                                const PF_PIN *pins,EFI_HANDLE oldChild,ED_STATE *out) {
  return EdCollectMode(c->Table,gpu,io,pins,TRUE,0x3f,oldChild,TRUE,out);
}
static BOOLEAN ParentPost(const ED_STATE *before,const ED_STATE *after) {
  ED_STATE expected=*before;
  expected.Drivers.MatchedEdges=2;expected.Config[1]&=~7U;expected.Attributes=0;
  return PfEqual(&expected,after,sizeof(expected));
}
static BOOLEAN ParentBinding(EFI_SYSTEM_TABLE *t,const PF_PIN *pins,const ED_STATE *s,
                             EFI_DRIVER_BINDING_PROTOCOL **binding) {
  return t->BootServices->HandleProtocol(s->Drivers.Agent[3],&BindGuidL,(VOID **)binding)==EFI_SUCCESS &&
    *binding && (*binding)->DriverBindingHandle==s->Drivers.Agent[3] &&
    (*binding)->ImageHandle==s->Drivers.Image[3] && (*binding)->Version==pins[3].Version &&
    (UINTN)(*binding)->Stop==(UINTN)s->Drivers.Base[3]+pins[3].Stop;
}
#ifdef GU_AD104_DECODE_READ_TEST
#include "decode-read.h"
#endif
static EFI_STATUS ParentStep(CAPTURE *c,EFI_HANDLE gpu,EFI_PCI_IO_PROTOCOL *io,
                             const PF_PIN *pins,const ED_STATE *before) {
  EFI_SYSTEM_TABLE *t=c->Table;static ED_STATE pre,one,two,final;
  EFI_DRIVER_BINDING_PROTOCOL *binding=NULL,*again=NULL;
  if(ParentAttempted || !OwnerChildAttempted || !GuTextOutputDisabled)return EFI_ALREADY_STARTED;
  EFI_STATUS s=OwnerCollect(c,gpu,io,pins,before->Drivers.Child,&pre);
  if(s!=EFI_SUCCESS || pre.Drivers.MatchedEdges!=3 || pre.Drivers.SharedFlag ||
     (pre.Config[1]&0xffff)!=7 || pre.Attributes!=0x718 || pre.Supported!=0x1b77b ||
     !PfEqual(before,&pre,sizeof(pre)) || OwnerGone(t,pre.Drivers.Child)!=EFI_SUCCESS ||
     !ParentBinding(t,pins,&pre,&binding) || !ParentBranch(t,gpu,pre.Drivers.Base[3],pins[3].Size))return EFI_NOT_READY;
  Text(c,"ad104-display intent=direct-parent-stop agent=img29 children=0 driver-may-disable-0x718 no-retry=1 no-decode-restore=1\r\n");
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  s=OwnerCollect(c,gpu,io,pins,before->Drivers.Child,&pre);
  if(s!=EFI_SUCCESS || !PfEqual(before,&pre,sizeof(pre)) || OwnerGone(t,pre.Drivers.Child)!=EFI_SUCCESS ||
     !ParentBinding(t,pins,&pre,&again) || again!=binding ||
     !ParentBranch(t,gpu,pre.Drivers.Base[3],pins[3].Size))return EFI_NOT_READY;
  ParentAttempted=TRUE;
  EFI_STATUS callback=binding->Stop(binding,gpu,0,NULL);
  EFI_STATUS a=ParentCollect(c,gpu,io,pins,pre.Drivers.Child,&one);
  EFI_STATUS b=ParentCollect(c,gpu,io,pins,pre.Drivers.Child,&two);
  EFI_STATUS gone=OwnerGone(t,pre.Drivers.Child);
  EdStatus(c,"parent-return",callback);EdStatus(c,"parent-post",a);
  EdStatus(c,"parent-repeat",b);EdStatus(c,"parent-child-absent",gone);
  EdDescribe(c,&one,"parent-post");EdDescribe(c,&two,"parent-repeat");
  BOOLEAN good=callback==EFI_SUCCESS && a==EFI_SUCCESS && b==EFI_SUCCESS && gone==EFI_SUCCESS &&
    ParentPost(&pre,&one) && ParentPost(&pre,&two);
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  if(!good)return EFI_NOT_READY;
#ifdef GU_AD104_DECODE_READ_TEST
  return DecodeStep(c,gpu,io,pins,&two);
#endif
  s=EdKey(t,'V',NULL);
  a=ParentCollect(c,gpu,io,pins,pre.Drivers.Child,&final);gone=OwnerGone(t,pre.Drivers.Child);
  EdStatus(c,"parent-input-after",s);EdStatus(c,"parent-final-post",a);EdStatus(c,"parent-final-absent",gone);
  EdDescribe(c,&final,"parent-final-post");
  if(s!=EFI_SUCCESS || a!=EFI_SUCCESS || gone!=EFI_SUCCESS || !ParentPost(&pre,&final))s=EFI_NOT_READY;
  EdStatus(c,"windows-eligible",s);
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  return s;
}
