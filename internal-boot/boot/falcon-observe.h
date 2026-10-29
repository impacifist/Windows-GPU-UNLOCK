/* GPL-2.0-only. Fixed read-only GSP Falcon baseline; no control writes. */
static BOOLEAN FalconAttempted;
#if defined(GU_AD104_ENTRY_INPUTS_TEST) && !defined(GU_AD104_FALCON_CORE_TEST)
#error Entry input audit requires core observation dependencies
#endif
#if defined(GU_AD104_ENTRY_INPUTS_TEST) && (defined(GU_AD104_FALCON_REFUSAL_TEST) || defined(GU_AD104_FALCON_BCR_TEST))
#error Entry input audit excludes refused register modes
#endif
#if defined(GU_AD104_FALCON_BCR_TEST) && defined(GU_AD104_FALCON_REFUSAL_TEST)
#error BCR observation cannot use the CPUCTL refusal recovery exception
#endif
#ifdef GU_AD104_ENTRY_INPUTS_TEST
static const UINT32 FalconOffsets[]={0,0x110100,0x110040,0x110044,0x110008,0x1100f4,0x110108,0x1103c0,0x82381c,0x823820,0x625f04,0};
#elif defined(GU_AD104_FALCON_BCR_TEST)
static const UINT32 FalconOffsets[]={0,0x110100,0x110040,0x110044,0x110008,0x1100f4,0x110108,0x1103c0,0x111668,0x82381c,0x823820,0x625f04,0};
#elif defined(GU_AD104_FALCON_CORE_TEST)
static const UINT32 FalconOffsets[]={0,0x110100,0x110040,0x110044,0x110008,0x1100f4,0x110108,0x1103c0,0x111388,0x111668,0x82381c,0x823820,0x625f04,0};
#elif defined(GU_AD104_FALCON_CAPS_TEST)
static const UINT32 FalconOffsets[]={0,0x110100,0x110040,0x110044,0x110008,0x1100f4,0x110108,0x82381c,0x823820,0x625f04,0};
#elif defined(GU_AD104_FALCON_MAILBOX_TEST)
static const UINT32 FalconOffsets[]={0,0x110100,0x110040,0x110044,0x82381c,0x823820,0x625f04,0};
#else
static const UINT32 FalconOffsets[]={0,0x110100,0x110040,0x110044,0x11010c,0x82381c,0x823820,0x625f04,0};
#endif
enum {FalconWords=sizeof(FalconOffsets)/sizeof(FalconOffsets[0])};
typedef struct {UINT32 Values[FalconWords];EFI_STATUS Status[FalconWords];UINTN Attempted;} FALCON_OBSERVATION;
static EFI_STATUS FalconBatch(EFI_PCI_IO_PROTOCOL *io,EFI_PCI_IO_PROTOCOL_IO_MEM read,FALCON_OBSERVATION *out,const DECODE_READS *prior){
  for(UINTN i=0;i<FalconWords;i++){
    if(io->Mem.Read!=read)return EFI_NOT_READY;
    UINT32 value=MAX_UINT32;EFI_STATUS s=read(io,EfiPciIoWidthUint32,0,FalconOffsets[i],1,&value);
    out->Values[i]=value;out->Status[i]=s;out->Attempted=i+1;
    if(s!=EFI_SUCCESS || io->Mem.Read!=read || value==MAX_UINT32 || (value&0xffff0000U)==0xbadf0000U)return EFI_NOT_READY;
    if((i==0 || i==FalconWords-1) && value!=0x194000a1)return EFI_NOT_READY;
#if defined(GU_AD104_FALCON_BCR_TEST) || defined(GU_AD104_ENTRY_INPUTS_TEST)
    if(FalconOffsets[i]==0x1103c0 && value!=0)return EFI_NOT_READY;
#endif
    if((FalconOffsets[i]==0x110100 && value!=prior->Values[3]) || (FalconOffsets[i]==0x82381c && value!=prior->Values[12]) ||
       (FalconOffsets[i]==0x823820 && value!=prior->Values[13]) || (FalconOffsets[i]==0x625f04 && value!=prior->Values[15]))return EFI_NOT_READY;
  }
  return EFI_SUCCESS;
}
#ifdef GU_AD104_FALCON_REFUSAL_TEST
#include "falcon-refusal.h"
#endif
static EFI_STATUS FalconObserve(CAPTURE *c,EFI_HANDLE gpu,EFI_PCI_IO_PROTOCOL *io,const PF_PIN *pins,const ED_STATE *parent,const DECODE_READS *prior){
  EFI_PCI_IO_PROTOCOL_IO_MEM read=io->Mem.Read;FALCON_OBSERVATION batches[2]={0};
  if(FalconAttempted || !read || !prior || prior->Attempted!=22)return EFI_NOT_READY;
  for(UINTN i=0;i<22;i++)if(prior->Status[i]!=EFI_SUCCESS)return EFI_NOT_READY;
  FalconAttempted=TRUE;
#ifdef GU_AD104_ENTRY_INPUTS_TEST
  Text(c,"ad104-falcon intent=two-twelve-dword-batches dmactl-riscv-cpuctl-bcr-excluded=1 no-writes=1 no-execution=1\r\n");
#elif defined(GU_AD104_FALCON_BCR_TEST)
  Text(c,"ad104-falcon intent=two-thirteen-dword-batches bcr-only=1 riscv-cpuctl-excluded=1 dmactl-excluded=1 no-writes=1 no-execution=1\r\n");
#elif defined(GU_AD104_FALCON_CORE_TEST)
  Text(c,"ad104-falcon intent=two-fourteen-dword-batches new-addresses=1103c0,111388,111668 dmactl-excluded=1 no-writes=1 no-execution=1\r\n");
#elif defined(GU_AD104_FALCON_CAPS_TEST)
  Text(c,"ad104-falcon intent=two-eleven-dword-batches new-addresses=110008,1100f4,110108 dmactl-excluded=1 no-writes=1 no-execution=1\r\n");
#elif defined(GU_AD104_FALCON_MAILBOX_TEST)
  Text(c,"ad104-falcon intent=two-eight-dword-batches addresses=110040,110044 dmactl-excluded=1 no-writes=1 no-execution=1\r\n");
#else
  Text(c,"ad104-falcon intent=two-nine-dword-batches new-addresses=110040,110044,11010c no-writes=1 no-execution=1\r\n");
#endif
  EFI_STATUS s=EdFlush(c);if(s!=EFI_SUCCESS)return s;
  for(UINTN batch=0;batch<2;batch++){
    if(!PromFullState(c,gpu,io,pins,parent,read))return EFI_NOT_READY;
    s=FalconBatch(io,read,&batches[batch],prior);
    for(UINTN i=0;i<batches[batch].Attempted;i++){
      Text(c,"ad104-falcon batch=0x");Hex(c,batch+1,2);Text(c," offset=0x");Hex(c,FalconOffsets[i],8);
      Text(c," status=0x");Hex(c,batches[batch].Status[i],16);Text(c," value=0x");Hex(c,batches[batch].Values[i],8);End(c);
    }
    EdStatus(c,"falcon-batch",s);EFI_STATUS log=EdFlush(c);
    if(log!=EFI_SUCCESS)return log;
    if(!PromFullState(c,gpu,io,pins,parent,read))return EFI_NOT_READY;
    if(s!=EFI_SUCCESS){
#ifdef GU_AD104_FALCON_REFUSAL_TEST
      if(batch==0 && FalconExactRefusal(&batches[0],prior))FalconRefusalObserved=TRUE;
#endif
      return EFI_NOT_READY;
    }
  }
#ifdef GU_AD104_FALCON_MAILBOX_TEST
  Text(c,"ad104-falcon mailbox-only-stable=");Hex(c,PfEqual(batches[0].Values+2,batches[1].Values+2,2*sizeof(UINT32)),1);
#else
  Text(c,"ad104-falcon mailbox-dma-stable=");Hex(c,PfEqual(batches[0].Values+2,batches[1].Values+2,3*sizeof(UINT32)),1);
#endif
  Text(c," readiness=unreviewed execution-authority=0\r\n");
#ifdef GU_AD104_FALCON_CAPS_TEST
  Text(c,"ad104-falcon capabilities irqstat=0x");Hex(c,batches[1].Values[4],8);
  Text(c," hwcfg2=0x");Hex(c,batches[1].Values[5],8);Text(c," hwcfg=0x");Hex(c,batches[1].Values[6],8);
  Text(c," riscv-field=");Hex(c,(batches[1].Values[5]>>10)&1,1);
  Text(c," scrubbing-field=");Hex(c,(batches[1].Values[5]>>12)&1,1);
  Text(c," imem-size-field=0x");Hex(c,batches[1].Values[6]&0x1ff,3);
  Text(c," fields-only=1 execution-authority=0\r\n");
#endif
#ifdef GU_AD104_ENTRY_INPUTS_TEST
  Text(c,"ad104-falcon engine=0x");Hex(c,batches[1].Values[7],8);Text(c," refused-registers-not-read=1 execution-authority=0\r\n");
#elif defined(GU_AD104_FALCON_BCR_TEST)
  Text(c,"ad104-falcon bcr-state engine=0x");Hex(c,batches[1].Values[7],8);
  Text(c," bcr-ctrl=0x");Hex(c,batches[1].Values[8],8);
  Text(c," bcr-valid-field=");Hex(c,batches[1].Values[8]&1,1);
  Text(c," configured-core-field=");Hex(c,(batches[1].Values[8]>>4)&1,1);
  Text(c," configured-core-qualified-by-valid=");Hex(c,batches[1].Values[8]&1,1);
  Text(c," current-execution-unknown=1 fields-only=1 execution-authority=0\r\n");
#elif defined(GU_AD104_FALCON_CORE_TEST)
  Text(c,"ad104-falcon core-state engine=0x");Hex(c,batches[1].Values[7],8);
  Text(c," riscv-cpuctl=0x");Hex(c,batches[1].Values[8],8);Text(c," bcr-ctrl=0x");Hex(c,batches[1].Values[9],8);
  Text(c," reset-field=");Hex(c,batches[1].Values[7]&1,1);
  Text(c," active-field=");Hex(c,(batches[1].Values[8]>>7)&1,1);
  Text(c," halted-field=");Hex(c,(batches[1].Values[8]>>4)&1,1);
  Text(c," bcr-valid-field=");Hex(c,batches[1].Values[9]&1,1);
  Text(c," configured-core-field=");Hex(c,(batches[1].Values[9]>>4)&1,1);
  Text(c," fields-only=1 execution-authority=0\r\n");
#endif
  s=EdFlush(c);if(s!=EFI_SUCCESS)return s;
  return PromFullState(c,gpu,io,pins,parent,read)?EFI_SUCCESS:EFI_NOT_READY;
}
