/* GPL-2.0-only. Observation-only parser results; never transaction authority. */
#pragma once
#include <Uefi.h>
#include "../unlock/vendor/vbios_610.h"
typedef struct {
  struct gpu_unlock_fwsec Descriptor;
  BOOLEAN PromMatch,Parsed,GeometryMatch,ImageMatch,SignatureMatch;
  UINT8 PromHash[32],ImageHash[32],SignatureHash[32];
} GU_FWSEC_AUDIT;
BOOLEAN GuAuditFwsecGeometry(const struct gpu_unlock_fwsec *f,UINTN bytes);
EFI_STATUS GuAuditCapturedFwsec(const UINT8 *rom,UINTN bytes,GU_FWSEC_AUDIT *out);

/* A synthetic command encoding experiment on disjoint scratch RAM only. */
typedef struct {
  BOOLEAN SourceUnchanged,PatchMatch,SelectorMatrix,ScratchIntact;
  UINT32 ChangedBytes;
  UINT8 PatchedHash[32];
} GU_FWSEC_REHEARSAL;
EFI_STATUS GuRehearseCapturedFwsec(const UINT8 *rom,UINTN bytes,UINT8 *scratch,UINTN scratchBytes,GU_FWSEC_REHEARSAL *out);

typedef struct {
  GU_FWSEC_REHEARSAL Base;
  UINT32 Version,SignatureOffset,ChangedBytes;
  BOOLEAN Supported,ImageMatch;
  UINT8 SignatureHash[32],AssembledHash[32];
} GU_FWSEC_SIGNATURE_AUDIT;
UINT32 GuAuditFuseVersion(UINT32 raw);
EFI_STATUS GuRehearseObservedSignature(const UINT8 *rom,UINTN bytes,UINT8 *scratch,UINTN scratchBytes,
  UINT32 sampleOffset,UINT32 sampleRaw,GU_FWSEC_SIGNATURE_AUDIT *out);
