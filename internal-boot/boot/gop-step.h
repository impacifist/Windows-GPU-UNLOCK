/* GPL-2.0-only. One graphics-console consumer stop after verified text stop. */
static BOOLEAN GopAttempted;
static EFI_STATUS GopProtocols(EFI_SYSTEM_TABLE *t,EFI_HANDLE child,BOOLEAN absent,VOID **identity) {
  EFI_GUID text={0x387477c2,0x69c7,0x11d2,{0x8e,0x39,0,0xa0,0xc9,0x69,0x72,0x3b}};
  EFI_GUID companion={0xe1e4a857,0xc970,0x4075,{0xa4,0xda,0xe9,0xc4,0x1b,0x69,0xad,0xfc}};
  VOID *a=NULL,*b=NULL;
  EFI_STATUS sa=t->BootServices->HandleProtocol(child,&text,&a);
  EFI_STATUS sb=t->BootServices->HandleProtocol(child,&companion,&b);
  if(identity)*identity=a;
  if(absent)return (sa==EFI_UNSUPPORTED || sa==EFI_NOT_FOUND) && !a &&
    (sb==EFI_UNSUPPORTED || sb==EFI_NOT_FOUND) && !b?EFI_SUCCESS:EFI_NOT_READY;
  return sa==EFI_SUCCESS && sb==EFI_SUCCESS && a && (UINTN)a<=MAX_UINTN-0x101 &&
    (UINTN)b==(UINTN)a+0x101?EFI_SUCCESS:EFI_NOT_READY;
}
static BOOLEAN GopPost(const ED_STATE *before,const ED_STATE *after) {
  ED_STATE expected=*before;expected.Drivers.MatchedEdges=5;
  return PfEqual(&expected,after,sizeof(expected));
}
static EFI_STATUS GopStep(CAPTURE *c,EFI_HANDLE gpu,EFI_PCI_IO_PROTOCOL *io,const PF_PIN *pins,
                          const ED_STATE *original,const ED_STATE *text,ED_STATE *result) {
  EFI_SYSTEM_TABLE *t=c->Table;static ED_STATE pre,first,second;
  VOID *published=NULL,*freshPublished=NULL;
  if(GopAttempted || !EdAttempted || !GuTextOutputDisabled)return EFI_ALREADY_STARTED;
  EFI_STATUS s=EdCollect(t,gpu,io,pins,TRUE,&pre);
  if(s==EFI_SUCCESS && (!EdPost(original,&pre) || !PfEqual(text,&pre,sizeof(pre))))s=EFI_NOT_READY;
  if(s==EFI_SUCCESS)s=GopProtocols(t,pre.Drivers.Child,FALSE,&published);
  EdStatus(c,"gop-precheck",s);if(s!=EFI_SUCCESS){(void)EdFlush(c);return s;}
  Text(c,"ad104-display intent=one-gop-consumer-disconnect agent=img17 pci-owner-retained=1 no-retry=1\r\n");
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  s=EdCollect(t,gpu,io,pins,TRUE,&pre);
  if(s!=EFI_SUCCESS || !EdPost(original,&pre) || !PfEqual(text,&pre,sizeof(pre)) ||
     GopProtocols(t,pre.Drivers.Child,FALSE,&freshPublished)!=EFI_SUCCESS || freshPublished!=published)return EFI_NOT_READY;
  GopAttempted=TRUE;
  EFI_STATUS callback=t->BootServices->DisconnectController(pre.Drivers.Child,pre.Drivers.Agent[2],NULL);
  EFI_STATUS one=EdCollectMask(t,gpu,io,pins,TRUE,0x1c,&first);
  EFI_STATUS two=EdCollectMask(t,gpu,io,pins,TRUE,0x1c,&second);
  EFI_STATUS gone=GopProtocols(t,pre.Drivers.Child,TRUE,NULL);
  EdStatus(c,"gop-disconnect-return",callback);EdStatus(c,"gop-post",one);
  EdStatus(c,"gop-post-repeat",two);EdStatus(c,"gop-protocols-absent",gone);
  EdDescribe(c,&first,"gop-post");EdDescribe(c,&second,"gop-post-repeat");
  BOOLEAN good=callback==EFI_SUCCESS && one==EFI_SUCCESS && two==EFI_SUCCESS && gone==EFI_SUCCESS &&
    GopPost(&pre,&first) && GopPost(&pre,&second);
  if(EdFlush(c)!=EFI_SUCCESS)return c->Io;
  if(!good)return EFI_NOT_READY;
  *result=second;return EFI_SUCCESS;
}
