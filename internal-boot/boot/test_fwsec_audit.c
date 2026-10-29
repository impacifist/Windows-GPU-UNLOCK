/* GPL-2.0-only. RAM-only geometry/error tests; optional private capture input. */
#include "fwsec-audit.h"
#include "../unlock/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int Checks;
#ifdef GU_FWSEC_AUDIT_TEST_FAULTS
int GuFwsecPatchTestFault;
#endif
#define CHECK(x) do{Checks++;if(!(x)){fprintf(stderr,"audit check failed line%d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv) {
  struct gpu_unlock_fwsec f={0x45480,0x10300,0,0xf500,0,0,0xf500,0xd80,0xb24,0x1c,9,0x400,2,3,0x45180,768};
  CHECK(GuAuditFwsecGeometry(&f,0x100000));
  for(UINTN i=0;i<16;i++){struct gpu_unlock_fwsec bad=f;((UINT32 *)&bad)[i]^=1;CHECK(!GuAuditFwsecGeometry(&bad,0x100000));}
  CHECK(!GuAuditFwsecGeometry(NULL,0x100000));CHECK(!GuAuditFwsecGeometry(&f,0xfffff));
  struct gpu_unlock_fwsec bad=f;bad.image_off=MAX_UINT32;CHECK(!GuAuditFwsecGeometry(&bad,0x100000));
  bad=f;bad.sig_bytes=MAX_UINT32;CHECK(!GuAuditFwsecGeometry(&bad,0x100000));
  UINT8 *rom=calloc(1,0x100000);CHECK(rom!=NULL);GU_FWSEC_AUDIT out;
  CHECK(GuAuditCapturedFwsec(NULL,0x100000,&out)==EFI_INVALID_PARAMETER);
  CHECK(GuAuditCapturedFwsec(rom,512,&out)==EFI_INVALID_PARAMETER);
  CHECK(GuAuditCapturedFwsec(rom,0x100000,NULL)==EFI_INVALID_PARAMETER);
  CHECK(GuAuditCapturedFwsec(rom,0x100000,&out)==EFI_CRC_ERROR && !out.PromMatch && !out.Parsed);
  UINT8 *scratch=malloc(0x10320);CHECK(scratch!=NULL);GU_FWSEC_REHEARSAL rehearsal;
  CHECK(GuRehearseCapturedFwsec(rom,0x100000,rom,0x10300,&rehearsal)==EFI_INVALID_PARAMETER);
  CHECK(GuRehearseCapturedFwsec(rom,0x100000,scratch,1,&rehearsal)==EFI_INVALID_PARAMETER);
  CHECK(GuRehearseCapturedFwsec(rom,0x100000,scratch,0x10320,&rehearsal)==EFI_CRC_ERROR);
  CHECK(GuRehearseCapturedFwsec(rom,0x100000,scratch,0x1031f,&rehearsal)==EFI_INVALID_PARAMETER);
  CHECK(GuRehearseCapturedFwsec(rom,0x100000,rom+1,0x10320,&rehearsal)==EFI_INVALID_PARAMETER);
  CHECK(GuRehearseCapturedFwsec(rom+1,0x100000,rom,0x10320,&rehearsal)==EFI_INVALID_PARAMETER);
  CHECK(GuRehearseCapturedFwsec(rom,0x100000,(UINT8 *)(MAX_UINTN-15),0x10320,&rehearsal)==EFI_INVALID_PARAMETER);
  for(UINT32 raw=0;raw<65536;raw++){UINT32 expected=0,v=raw;while(v){expected++;v/=2;}CHECK(GuAuditFuseVersion(raw)==expected && GuAuditFuseVersion(raw|0xabcd0000)==expected);}
  UINT32 offset=0xdeadbeef;
  CHECK(gpu_unlock_fwsec_sig_offset(3,16,384,&offset)!=0 && offset==0xdeadbeef);
  CHECK(gpu_unlock_fwsec_sig_offset(0x10000,0,384,&offset)!=0);
  CHECK(gpu_unlock_fwsec_sig_offset(3,0,0,&offset)!=0);
  CHECK(gpu_unlock_fwsec_sig_offset(3,0,384,NULL)!=0);
  CHECK(gpu_unlock_fwsec_sig_offset(0xffff,15,MAX_UINT32,&offset)!=0);
  CHECK(GuRehearseCapturedFwsec(NULL,0x100000,scratch,0x10320,&rehearsal)==EFI_INVALID_PARAMETER);
  CHECK(GuRehearseCapturedFwsec(rom,0x100000,scratch,0x10320,NULL)==EFI_INVALID_PARAMETER);
  if(argc==2){
    FILE *file=fopen(argv[1],"rb");CHECK(file!=NULL);CHECK(fread(rom,1,0x100000,file)==0x100000);CHECK(fgetc(file)==EOF);fclose(file);
    UINT8 before[32],after[32];GuSha256(rom,0x100000,before);
    CHECK(GuAuditCapturedFwsec(rom,0x100000,&out)==EFI_SUCCESS && out.PromMatch && out.Parsed && out.GeometryMatch && out.ImageMatch && out.SignatureMatch);
    GuSha256(rom,0x100000,after);CHECK(!memcmp(before,after,32));
    memcpy(scratch,rom+0x45480,0x10300);
    CHECK(gpu_unlock_fwsec_patch_frts(scratch,&f,0)!=0);
    CHECK(gpu_unlock_fwsec_patch_frts(scratch,&f,1)!=0);
    CHECK(gpu_unlock_fwsec_patch_frts(scratch,&f,0x100000000000ULL)!=0);
    CHECK(GuHashMatches(scratch,0x10300,"e604f9be77c4f45d14ccdd2e45caef480801342ec7833e8618c70bbf34c60ab9"));
    memset(scratch,0xa5,0x10320);
    CHECK(GuRehearseCapturedFwsec(rom,0x100000,scratch,0x10320,&rehearsal)==EFI_SUCCESS);
    CHECK(rehearsal.ScratchIntact && rehearsal.SourceUnchanged && rehearsal.PatchMatch && rehearsal.SelectorMatrix && rehearsal.ChangedBytes==9);
    CHECK(GuHashMatches(scratch+16,0x10300,"0bca65c5d5f41cec855acd8009340a86a1e08d117cb9c1a716cb444f3d0a2d52"));
    GuSha256(rom,0x100000,after);CHECK(!memcmp(before,after,32));
    printf("patched RAM hash=");for(int i=0;i<32;i++)printf("%02x",rehearsal.PatchedHash[i]);printf(" changed=%u\n",rehearsal.ChangedBytes);
#ifdef GU_FWSEC_AUDIT_TEST_FAULTS
    GuFwsecPatchTestFault=1;
    CHECK(GuRehearseCapturedFwsec(rom,0x100000,scratch,0x10320,&rehearsal)==EFI_COMPROMISED_DATA && !rehearsal.ScratchIntact && rehearsal.SourceUnchanged);
    GuFwsecPatchTestFault=2;
    CHECK(GuRehearseCapturedFwsec(rom,0x100000,scratch,0x10320,&rehearsal)==EFI_COMPROMISED_DATA && rehearsal.ScratchIntact && rehearsal.SourceUnchanged && !rehearsal.PatchMatch);
    GuFwsecPatchTestFault=0;
#endif
    GU_FWSEC_SIGNATURE_AUDIT sig;
    CHECK(GuRehearseObservedSignature(rom,0x100000,scratch,0x10320,0x8241e0,1,&sig)==EFI_SUCCESS);
    CHECK(sig.Supported && sig.ImageMatch && sig.Version==1 && sig.SignatureOffset==384 && sig.Base.SourceUnchanged && sig.Base.ScratchIntact);
    CHECK(sig.ChangedBytes==393);
    CHECK(GuHashMatches(scratch+16,0x10300,"5004460ab7fea25b83cc95b6468456d830b98700926ed0c8696e49505eb19d07"));
    CHECK(GuHashMatches(rom+0x45300,384,"7806dc110f1bbf58f3b41f4414c4e0426000b0d985a66013d035a6ea4d0e6dd5"));
    printf("assembled hash=");for(int i=0;i<32;i++)printf("%02x",sig.AssembledHash[i]);printf(" changed=%u\n",sig.ChangedBytes);
    CHECK(GuRehearseObservedSignature(rom,0x100000,scratch,0x10320,0x8241dc,1,&sig)==EFI_UNSUPPORTED);
    CHECK(GuRehearseObservedSignature(rom,0x100000,scratch,0x10320,0x8241e0,3,&sig)==EFI_UNSUPPORTED);
    const UINT32 invalidRaw[]={0,2,3,MAX_UINT32,0xbadf5620,0x10001};
    for(UINTN i=0;i<sizeof(invalidRaw)/sizeof(invalidRaw[0]);i++){
      CHECK(GuRehearseObservedSignature(rom,0x100000,scratch,0x10320,0x8241e0,invalidRaw[i],&sig)==EFI_UNSUPPORTED && !sig.Supported && sig.Base.ScratchIntact);
      CHECK(GuHashMatches(scratch+16,0x10300,"0bca65c5d5f41cec855acd8009340a86a1e08d117cb9c1a716cb444f3d0a2d52"));
    }
    rom[0x45480]^=1;CHECK(GuAuditCapturedFwsec(rom,0x100000,&out)==EFI_CRC_ERROR);
  }
  free(scratch);free(rom);printf("PASS FWSEC RAM audit: %d checks; execution authority absent\n",Checks);return 0;
}
