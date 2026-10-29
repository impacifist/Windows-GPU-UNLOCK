/* GPL-2.0-only. CPU-only explanation of sampled inputs; no admission authority. */
#pragma once
#include <Uefi.h>
typedef struct {
  UINT64 Fb,RawVga,PolicyVga,MmuLow,MmuHigh,MmuTop;
  BOOLEAN VgaValid,MmuRangeValid,TopLock,ReferenceSs1,ReferenceVga;
} GU_ENTRY_INPUTS;
static BOOLEAN GuAuditEntryInputs(const UINT32 *offsets,const UINT32 *values,const EFI_STATUS *status,UINTN attempted,GU_ENTRY_INPUTS *out){
  static const UINT32 expected[]={0,0x118128,0x118234,0x110100,0x1fa824,0x1fa828,0x1fa7cc,0x1fa7c4,0x9a0148,0x823804,0x823814,0x823818,0x82381c,0x823820,0x1183a4,0x625f04,0x1fa7c8,0x1fa82c,0x1fa830,0x824148,0x8241e0,0};
  if(!offsets || !values || !status || !out || attempted!=22)return FALSE;
  for(UINTN i=0;i<22;i++)if(offsets[i]!=expected[i])return FALSE;
  for(UINTN i=0;i<22;i++)if(status[i]!=EFI_SUCCESS || values[i]==MAX_UINT32 || (values[i]&0xffff0000U)==0xbadf0000U)return FALSE;
  if(values[0]!=0x194000a1 || values[21]!=0x194000a1 || values[14]<256 || !(values[16]&1))return FALSE;
  GU_ENTRY_INPUTS a={0};
  a.Fb=(UINT64)values[14]<<20;a.VgaValid=(values[15]&8)!=0;
  a.RawVga=(UINT64)(values[15]>>8)<<16;a.PolicyVga=a.Fb-0x100000;
  if(a.VgaValid){a.PolicyVga=a.RawVga;if(a.PolicyVga<a.Fb-0x100000)a.PolicyVga=a.Fb-0x20000;}
  if(a.PolicyVga>=a.Fb)return FALSE;
  a.MmuLow=(UINT64)(values[17]>>4)<<12;a.MmuHigh=(UINT64)(values[18]>>4)<<12;
  a.MmuTop=(a.MmuHigh+0xffffULL)&~0xffffULL;
  a.MmuRangeValid=a.MmuHigh>=a.MmuLow;
  a.TopLock=a.MmuRangeValid && (a.MmuTop>>20)==(a.Fb>>20);
  a.ReferenceSs1=values[13]==1;a.ReferenceVga=values[15]==1;
  *out=a;return TRUE;
}
