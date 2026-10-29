/* GPL-2.0-only. Direct one-child Stop; never dispatch parent Stop. */
static BOOLEAN OwnerChildAttempted;
static EFI_STATUS OwnerGone(EFI_SYSTEM_TABLE *t,EFI_HANDLE child) {
  EFI_HANDLE *handles=NULL;UINTN n=0;
  EFI_STATUS s=t->BootServices->LocateHandleBuffer(AllHandles,NULL,NULL,&n,&handles);
  if(s==EFI_SUCCESS && (!handles || !n || n>1024))s=EFI_BAD_BUFFER_SIZE;
  if(s==EFI_SUCCESS)for(UINTN i=0;i<n;i++)if(handles[i]==child){s=EFI_NOT_READY;break;}
  if(handles && t->BootServices->FreePool(handles)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  return s;
}
static EFI_STATUS OwnerCollect(CAPTURE *c,EFI_HANDLE gpu,EFI_PCI_IO_PROTOCOL *io,
                               const PF_PIN *pins,EFI_HANDLE child,ED_STATE *out) {
  return EdCollectMode(c->Table,gpu,io,pins,TRUE,0x3e,child,TRUE,out);
}
static BOOLEAN OwnerPost(const ED_STATE *before,const ED_STATE *after) {
  ED_STATE expected=*before;expected.Drivers.MatchedEdges=3;
  /* Retain only independent PCD fields. GOP and its allocations may be freed. */
  TEXT_DISPLAY pcd={0};pcd.Pcd=before->Display.Pcd;pcd.GetBool=before->Display.GetBool;
  pcd.Next=before->Display.Next;pcd.Size=before->Display.Size;expected.Display=pcd;
  return PfEqual(&expected,after,sizeof(expected));
}
static EFI_STATUS OwnerInvoke(EFI_DRIVER_BINDING_PROTOCOL *binding,EFI_HANDLE gpu,EFI_HANDLE child) {
  /* A direct Stop is legal under DriverBinding.h's controller/child restrictions.
   * Unlike DisconnectController, this cannot append a zero-child parent Stop. */
  return binding->Stop(binding,gpu,1,&child);
}
#ifdef GU_AD104_PARENT_TEST
#include "owner-parent.h"
#endif
static EFI_STATUS OwnerChildStep(CAPTURE *c,EFI_HANDLE gpu,EFI_PCI_IO_PROTOCOL *io,
                                 const PF_PIN *pins,const ED_STATE *before) {
  EFI_SYSTEM_TABLE *t=c->Table;static ED_STATE pre,one,two,final;
  EFI_DRIVER_BINDING_PROTOCOL *binding=NULL,*again=NULL;
  if(OwnerChildAttempted || !ConsumerEdidAttempted || !GuTextOutputDisabled)return EFI_ALREADY_STARTED;
  EFI_STATUS s=EdCollectMask(t,gpu,io,pins,TRUE,0x3c,&pre);
  if(s!=EFI_SUCCESS || pre.Drivers.MatchedEdges!=4 || pre.Drivers.SharedFlag ||
     !PfEqual(before,&pre,sizeof(pre)) || GopProtocols(t,pre.Drivers.Child,TRUE,NULL)!=EFI_SUCCESS)return EFI_NOT_READY;
  s=t->BootServices->HandleProtocol(pre.Drivers.Agent[3],&BindGuidL,(VOID **)&binding);
  if(s!=EFI_SUCCESS || !binding || binding->DriverBindingHandle!=pre.Drivers.Agent[3] ||
     binding->ImageHandle!=pre.Drivers.Image[3] || binding->Version!=pins[3].Version ||
     (UINTN)binding->Stop!=(UINTN)pre.Drivers.Base[3]+pins[3].Stop)return EFI_NOT_READY;
  Text(c,"ad104-display intent=direct-one-child-stop agent=img29 parent-stage-separate=1 no-retry=1\r\n");
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  s=EdCollectMask(t,gpu,io,pins,TRUE,0x3c,&pre);
  if(s!=EFI_SUCCESS || !PfEqual(before,&pre,sizeof(pre)) ||
     GopProtocols(t,pre.Drivers.Child,TRUE,NULL)!=EFI_SUCCESS ||
     t->BootServices->HandleProtocol(pre.Drivers.Agent[3],&BindGuidL,(VOID **)&again)!=EFI_SUCCESS || again!=binding ||
     binding->DriverBindingHandle!=pre.Drivers.Agent[3] || binding->ImageHandle!=pre.Drivers.Image[3] ||
     binding->Version!=pins[3].Version || (UINTN)binding->Stop!=(UINTN)pre.Drivers.Base[3]+pins[3].Stop)return EFI_NOT_READY;
  OwnerChildAttempted=TRUE;
  EFI_STATUS callback=OwnerInvoke(binding,gpu,pre.Drivers.Child);
  EFI_STATUS a=OwnerCollect(c,gpu,io,pins,pre.Drivers.Child,&one);
  EFI_STATUS b=OwnerCollect(c,gpu,io,pins,pre.Drivers.Child,&two);
  EFI_STATUS gone=OwnerGone(t,pre.Drivers.Child);
  EdStatus(c,"owner-child-return",callback);EdStatus(c,"owner-child-post",a);
  EdStatus(c,"owner-child-repeat",b);EdStatus(c,"owner-child-absent",gone);
  EdDescribe(c,&one,"owner-child-post");EdDescribe(c,&two,"owner-child-repeat");
  BOOLEAN good=callback==EFI_SUCCESS && a==EFI_SUCCESS && b==EFI_SUCCESS && gone==EFI_SUCCESS &&
    OwnerPost(&pre,&one) && OwnerPost(&pre,&two);
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  if(!good)return EFI_NOT_READY;
#ifdef GU_AD104_PARENT_TEST
  return ParentStep(c,gpu,io,pins,&two);
#endif
  s=EdKey(t,'V',NULL);
  a=OwnerCollect(c,gpu,io,pins,pre.Drivers.Child,&final);gone=OwnerGone(t,pre.Drivers.Child);
  EdStatus(c,"owner-input-after",s);EdStatus(c,"owner-final-post",a);EdStatus(c,"owner-final-absent",gone);
  EdDescribe(c,&final,"owner-final-post");
  if(s!=EFI_SUCCESS || a!=EFI_SUCCESS || gone!=EFI_SUCCESS || !OwnerPost(&pre,&final))s=EFI_NOT_READY;
  EdStatus(c,"windows-eligible",s);
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  return s;
}
