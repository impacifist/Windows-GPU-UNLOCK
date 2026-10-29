/* GPL-2.0-only. Separate, one-shot admission for Windows recovery only. */
static BOOLEAN FalconRefusalObserved,FalconRecoveryAttempted,FalconRecoveryEligible;
static const UINT32 FalconRecoveryOffsets[]={0,0x110100,0x1103c0,0x82381c,0x823820,0x625f04,0};
EFI_STATUS GuRefusalWitness(const char *event,EFI_STATUS status);
static BOOLEAN FalconExactRefusal(const FALCON_OBSERVATION *sample,const DECODE_READS *prior){
  if(sample->Attempted!=9 || FalconOffsets[8]!=0x111388 || sample->Values[8]!=0xbadf5620 ||
     sample->Values[0]!=0x194000a1 || sample->Values[1]!=0x10 || prior->Values[3]!=0x10 || sample->Values[7]!=0)return FALSE;
  for(UINTN i=0;i<9;i++)if(sample->Status[i]!=EFI_SUCCESS)return FALSE;
  return TRUE;
}
static EFI_STATUS FalconRefusalAdmission(CAPTURE *c,EFI_HANDLE gpu,EFI_PCI_IO_PROTOCOL *io,const PF_PIN *pins,const ED_STATE *parent,const DECODE_READS *prior,EFI_PCI_IO_PROTOCOL_IO_MEM read){
  if(!FalconRefusalObserved || FalconRecoveryAttempted || !prior || prior->Attempted!=22 || !read || io->Mem.Read!=read)return EFI_NOT_READY;
  FalconRecoveryAttempted=TRUE;
  Text(c,"ad104-falcon observation-complete=0 observation-result=EFI_NOT_READY recovery-only=1 execution-authority=0\r\n");
  EFI_STATUS s=EdFlush(c);if(s!=EFI_SUCCESS)return s;
  s=GuRefusalWitness("falcon-observation-refused",EFI_NOT_READY);if(s!=EFI_SUCCESS)return s;
  if(!PromFullState(c,gpu,io,pins,parent,read))return EFI_NOT_READY;
  Text(c,"ad104-falcon recovery-intent=two-seven-dword-required-state-batches no-optional-read-retry=1\r\n");
  s=EdFlush(c);if(s!=EFI_SUCCESS)return s;
  for(UINTN batch=0;batch<2;batch++){
    if(!PromFullState(c,gpu,io,pins,parent,read))return EFI_NOT_READY;
    UINT32 values[7]={0};EFI_STATUS status[7]={0};UINTN attempted=0;s=EFI_SUCCESS;
    const UINT32 expected[]={0x194000a1,prior->Values[3],0,prior->Values[12],prior->Values[13],prior->Values[15],0x194000a1};
    for(UINTN i=0;i<7;i++){
      if(io->Mem.Read!=read){s=EFI_NOT_READY;break;}
      status[i]=read(io,EfiPciIoWidthUint32,0,FalconRecoveryOffsets[i],1,&values[i]);attempted=i+1;
      if(status[i]!=EFI_SUCCESS || io->Mem.Read!=read || values[i]!=expected[i]){s=EFI_NOT_READY;break;}
    }
    for(UINTN i=0;i<attempted;i++){
      Text(c,"ad104-falcon recovery-batch=0x");Hex(c,batch+1,2);Text(c," offset=0x");Hex(c,FalconRecoveryOffsets[i],8);
      Text(c," status=0x");Hex(c,status[i],16);Text(c," value=0x");Hex(c,values[i],8);End(c);
    }
    EdStatus(c,"refusal-required-state",s);EFI_STATUS logged=EdFlush(c);if(logged!=EFI_SUCCESS)return logged;
    if(!PromFullState(c,gpu,io,pins,parent,read) || s!=EFI_SUCCESS)return EFI_NOT_READY;
  }
  Text(c,"ad104-falcon observation-complete=0 recovery-rechecks-complete=1 awaiting-fresh-V=1 execution-authority=0\r\n");
  s=EdFlush(c);if(s!=EFI_SUCCESS)return s;
  s=GuRefusalWitness("refusal-rechecks-complete",EFI_SUCCESS);if(s!=EFI_SUCCESS)return s;
  if(!PromFullState(c,gpu,io,pins,parent,read))return EFI_NOT_READY;
  FalconRecoveryEligible=TRUE;return EFI_SUCCESS;
}
