/* GPL-2.0-only. Parse CPU-owned bytes; no EFI services, MMIO or execution. */
#include "fwsec-audit.h"
#include "../unlock/sha256.h"
/* Suppress parser printf diagnostics in this translation unit only. This keeps
 * parsing independent of console/filesystem callbacks. Retain production logic. */
#define GPU_UNLOCK_DIAGNOSTIC_H
#define GU_NOTE(...) ((void)0)
#define GU_ERROR(...) ((void)0)
#define GU_DIAG(value) (value)
#include "../unlock/vendor/vbios_610.c"
static const struct gpu_unlock_fwsec Observed={
  0x45480,0x10300,0,0xf500,0,0,0xf500,0xd80,0xb24,0x1c,9,0x400,2,3,0x45180,768
};
_Static_assert(sizeof(struct gpu_unlock_fwsec)==64,"Descriptor layout must remain explicit");
static BOOLEAN AuditRange(UINTN bytes,UINT32 off,UINT32 n){return off<=bytes && n<=bytes-off;}
BOOLEAN GuAuditFwsecGeometry(const struct gpu_unlock_fwsec *f,UINTN bytes) {
  if(!f || bytes!=0x100000 || !AuditRange(bytes,f->image_off,f->image_size) ||
     !AuditRange(bytes,f->sig_off,f->sig_bytes) || !AuditRange(f->image_size,f->data_off,f->dmem_size) ||
     !AuditRange(f->dmem_size,f->pkc_off,384) || !AuditRange(f->dmem_size,f->interface_off,4))return FALSE;
  const UINT8 *a=(const UINT8 *)f,*b=(const UINT8 *)&Observed;
  for(UINTN i=0;i<sizeof(Observed);i++)if(a[i]!=b[i])return FALSE;
  return TRUE;
}
EFI_STATUS GuAuditCapturedFwsec(const UINT8 *rom,UINTN bytes,GU_FWSEC_AUDIT *out) {
  if(!out)return EFI_INVALID_PARAMETER;
  UINT8 *clear=(UINT8 *)out;for(UINTN i=0;i<sizeof(*out);i++)clear[i]=0;
  if(!rom || bytes!=0x100000)return EFI_INVALID_PARAMETER;
  GuSha256(rom,bytes,out->PromHash);
  out->PromMatch=GuHashMatches(rom,bytes,"accf2a054de0ec394078f933f74356ed7c4057ee0bd1ce6734cacda54ca9e1e1");
  if(!out->PromMatch)return EFI_CRC_ERROR;
  out->Parsed=gpu_unlock_vbios_fwsec(rom,bytes,&out->Descriptor)==0;
  if(!out->Parsed)return EFI_COMPROMISED_DATA;
  out->GeometryMatch=GuAuditFwsecGeometry(&out->Descriptor,bytes);
  if(!out->GeometryMatch)return EFI_COMPROMISED_DATA;
  const struct gpu_unlock_fwsec *f=&out->Descriptor;
  GuSha256(rom+f->image_off,f->image_size,out->ImageHash);
  GuSha256(rom+f->sig_off,f->sig_bytes,out->SignatureHash);
  out->ImageMatch=GuHashMatches(rom+f->image_off,f->image_size,"e604f9be77c4f45d14ccdd2e45caef480801342ec7833e8618c70bbf34c60ab9");
  out->SignatureMatch=GuHashMatches(rom+f->sig_off,f->sig_bytes,"df4953bd74c6c97736ba433a78db9cb27487c78e332445604379dabe7027a4f2");
  return out->ImageMatch && out->SignatureMatch?EFI_SUCCESS:EFI_CRC_ERROR;
}

EFI_STATUS GuRehearseCapturedFwsec(const UINT8 *rom,UINTN bytes,UINT8 *scratch,UINTN scratchBytes,GU_FWSEC_REHEARSAL *out) {
  if(!out)return EFI_INVALID_PARAMETER;
  UINT8 *clear=(UINT8 *)out;for(UINTN i=0;i<sizeof(*out);i++)clear[i]=0;
  /* Reject overlap before writes, including wrapping pointer intervals. */
  UINTN r=(UINTN)rom,s=(UINTN)scratch;
  if(!rom || !scratch || bytes!=0x100000 || scratchBytes<0x10320 ||
     r>MAX_UINTN-bytes || s>MAX_UINTN-scratchBytes || (r<s+scratchBytes && s<r+bytes))return EFI_INVALID_PARAMETER;
  out->ScratchIntact=TRUE;
  GU_FWSEC_AUDIT audit;EFI_STATUS status=GuAuditCapturedFwsec(rom,bytes,&audit);
  if(status!=EFI_SUCCESS)return status;
  const struct gpu_unlock_fwsec *f=&audit.Descriptor;
  UINT8 *allocation=scratch;for(UINTN i=0;i<16;i++){allocation[i]=0xa5;allocation[16+f->image_size+i]=0x5a;}
  scratch+=16;
  for(UINTN i=0;i<f->image_size;i++)scratch[i]=rom[f->image_off+i];
  /* This constant is a test vector, not an allocated or admitted GPU address. */
  int patched=gpu_unlock_fwsec_patch_frts(scratch,f,0x100000000ULL);
#ifdef GU_FWSEC_AUDIT_TEST_FAULTS
  extern int GuFwsecPatchTestFault;
  if(GuFwsecPatchTestFault){patched=-EINVAL;if(GuFwsecPatchTestFault==1)allocation[0]^=1;}
#endif
  /* Independently pin the observed DMAP/command locations and expected bytes.
   * Compare every image byte, including bytes the production patch may not touch. */
  static const UINT32 command[11]={1,24,0,0,0,2,1,20,0x100000,0x100,2};
  out->PatchMatch=patched==0;
  for(UINTN i=0;i<f->image_size;i++) {
    UINT8 original=rom[f->image_off+i],expected=original;
    if(i>=0xf500+0xd40 && i<0xf500+0xd40+44){UINTN j=i-(0xf500+0xd40);expected=(UINT8)(command[j/4]>>(8*(j%4)));}
    if(i>=0xf500+0xae0+44 && i<0xf500+0xae0+48)expected=i==0xf500+0xae0+44?0x15:0;
    if(scratch[i]!=expected)out->PatchMatch=FALSE;
    if(scratch[i]!=original)out->ChangedBytes++;
  }
  for(UINTN i=0;i<16;i++)if(allocation[i]!=0xa5 || allocation[16+f->image_size+i]!=0x5a)out->ScratchIntact=FALSE;
  GuSha256(scratch,f->image_size,out->PatchedHash);
  out->SelectorMatrix=TRUE;
  for(UINT32 fuse=0;fuse<16;fuse++) {
    UINT32 off=0xdeadbeef;int result=gpu_unlock_fwsec_sig_offset(f->sig_versions,fuse,384,&off);
    if(fuse<2){if(result || off!=fuse*384 || off>f->sig_bytes || 384>f->sig_bytes-off)out->SelectorMatrix=FALSE;}
    else if(result!=-ERANGE || off!=0xdeadbeef)out->SelectorMatrix=FALSE;
  }
  out->SourceUnchanged=GuHashMatches(rom,bytes,"accf2a054de0ec394078f933f74356ed7c4057ee0bd1ce6734cacda54ca9e1e1");
  return out->ScratchIntact && out->SourceUnchanged && out->PatchMatch && out->SelectorMatrix?EFI_SUCCESS:EFI_COMPROMISED_DATA;
}

/* Same low16/highest-set-bit+1 conversion as vendor/prep_610.c. No MMIO. */
UINT32 GuAuditFuseVersion(UINT32 raw){UINT32 value=raw&0xffffU,version=0;while(value){version++;value>>=1;}return version;}
EFI_STATUS GuRehearseObservedSignature(const UINT8 *rom,UINTN bytes,UINT8 *scratch,UINTN scratchBytes,
  UINT32 sampleOffset,UINT32 sampleRaw,GU_FWSEC_SIGNATURE_AUDIT *out) {
  if(!out)return EFI_INVALID_PARAMETER;
  UINT8 *clear=(UINT8 *)out;for(UINTN i=0;i<sizeof(*out);i++)clear[i]=0;
  EFI_STATUS status=GuRehearseCapturedFwsec(rom,bytes,scratch,scratchBytes,&out->Base);
  if(status!=EFI_SUCCESS)return status;
  /* The descriptor and ROM were exact-pinned by the base rehearsal. Bind only
   * the known, successful pre-PROM register sample; never claim a fresh fuse. */
  if(sampleOffset!=0x8241c0+(Observed.ucode_id-1)*4 || sampleRaw!=1)return EFI_UNSUPPORTED;
  out->Version=GuAuditFuseVersion(sampleRaw);
  if(gpu_unlock_fwsec_sig_offset(Observed.sig_versions,out->Version,384,&out->SignatureOffset) ||
     out->SignatureOffset>Observed.sig_bytes || 384>Observed.sig_bytes-out->SignatureOffset ||
     !AuditRange(Observed.image_size,Observed.data_off+Observed.pkc_off,384))return EFI_COMPROMISED_DATA;
  out->Supported=TRUE;
  const UINT8 *signature=rom+Observed.sig_off+out->SignatureOffset;
  UINT8 *image=scratch+16;
  for(UINTN i=0;i<384;i++)image[Observed.data_off+Observed.pkc_off+i]=signature[i];
  static const UINT32 command[11]={1,24,0,0,0,2,1,20,0x100000,0x100,2};
  out->ImageMatch=TRUE;
  for(UINTN i=0;i<Observed.image_size;i++){
    UINT8 original=rom[Observed.image_off+i],expected=original;
    if(i>=0x10240 && i<0x1026c){UINTN j=i-0x10240;expected=(UINT8)(command[j/4]>>(8*(j%4)));}
    if(i>=0x1000c && i<0x10010)expected=i==0x1000c?0x15:0;
    if(i>=0x10024 && i<0x101a4)expected=signature[i-0x10024];
    if(image[i]!=expected)out->ImageMatch=FALSE;
    if(image[i]!=original)out->ChangedBytes++;
  }
  for(UINTN i=0;i<16;i++)if(scratch[i]!=0xa5 || scratch[16+Observed.image_size+i]!=0x5a)out->Base.ScratchIntact=FALSE;
  out->Base.SourceUnchanged=GuHashMatches(rom,bytes,"accf2a054de0ec394078f933f74356ed7c4057ee0bd1ce6734cacda54ca9e1e1");
  GuSha256(signature,384,out->SignatureHash);GuSha256(image,Observed.image_size,out->AssembledHash);
  return out->ImageMatch && out->Base.ScratchIntact && out->Base.SourceUnchanged?EFI_SUCCESS:EFI_COMPROMISED_DATA;
}
