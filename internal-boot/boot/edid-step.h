/* GPL-2.0-only. One EDID stop after verified text and GOP consumer removal. */
static BOOLEAN ConsumerEdidAttempted;
static BOOLEAN ConsumerEdidPost(const ED_STATE *before,const ED_STATE *after) {
  ED_STATE expected=*before;
  expected.Drivers.MatchedEdges=4;expected.Drivers.SharedFlag=0;
  return PfEqual(&expected,after,sizeof(expected));
}
static EFI_STATUS ConsumerEdidStep(CAPTURE *c,EFI_HANDLE gpu,EFI_PCI_IO_PROTOCOL *io,
                                  const PF_PIN *pins,const ED_STATE *before,ED_STATE *result) {
  static ED_STATE pre,one,two;
  EFI_SYSTEM_TABLE *t=c->Table;
  if(ConsumerEdidAttempted || !GopAttempted || !EdAttempted || !GuTextOutputDisabled)return EFI_ALREADY_STARTED;
  EFI_STATUS s=EdCollectMask(t,gpu,io,pins,TRUE,0x1c,&pre);
  if(s==EFI_SUCCESS && (pre.Drivers.MatchedEdges!=5 || pre.Drivers.SharedFlag!=1 ||
     !PfEqual(before,&pre,sizeof(pre))))s=EFI_NOT_READY;
  if(s==EFI_SUCCESS)s=GopProtocols(t,pre.Drivers.Child,TRUE,NULL);
  EdStatus(c,"edid-precheck",s);
  if(s!=EFI_SUCCESS){(void)EdFlush(c);return s;}
  Text(c,"ad104-display intent=one-edid-disconnect agent=img08 pci-owner-retained=1 no-retry=1\r\n");
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  s=EdCollectMask(t,gpu,io,pins,TRUE,0x1c,&pre);
  if(s!=EFI_SUCCESS || !PfEqual(before,&pre,sizeof(pre)) ||
     GopProtocols(t,pre.Drivers.Child,TRUE,NULL)!=EFI_SUCCESS)return EFI_NOT_READY;
  ConsumerEdidAttempted=TRUE;
  EFI_STATUS callback=t->BootServices->DisconnectController(pre.Drivers.Child,pre.Drivers.Agent[0],NULL);
  EFI_STATUS a=EdCollectMask(t,gpu,io,pins,TRUE,0x3c,&one);
  EFI_STATUS b=EdCollectMask(t,gpu,io,pins,TRUE,0x3c,&two);
  EFI_STATUS absent=GopProtocols(t,pre.Drivers.Child,TRUE,NULL);
  EdStatus(c,"edid-disconnect-return",callback);EdStatus(c,"edid-post",a);
  EdStatus(c,"edid-post-repeat",b);EdStatus(c,"edid-protocols-absent",absent);
  EdDescribe(c,&one,"edid-post");EdDescribe(c,&two,"edid-post-repeat");
  BOOLEAN good=callback==EFI_SUCCESS && a==EFI_SUCCESS && b==EFI_SUCCESS && absent==EFI_SUCCESS &&
    ConsumerEdidPost(&pre,&one) && ConsumerEdidPost(&pre,&two);
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  if(!good)return EFI_NOT_READY;
  *result=two;return EFI_SUCCESS;
}
