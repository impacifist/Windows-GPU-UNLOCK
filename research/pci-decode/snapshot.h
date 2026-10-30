/* GPL-2.0-only. Read-only PCI snapshot data and predicates, no actuator API. */
#pragma once
#include <Uefi.h>

#define GU_DS_MAX_PARENTS 16U
#define GU_DS_IO 0x100ULL
#define GU_DS_MEMORY 0x200ULL
#define GU_DS_MASTER 0x400ULL

typedef struct {
  UINTN Handle, Interface, Segment, Bus, Device, Function;
  UINT32 Config[64];
  UINT64 Attributes, Supported;
} GU_DS_DEVICE;

/* Parents are the caller-verified immediate ancestry, ordered root to leaf.
 * Complete must be set only after every location/config/attribute query succeeds.
 * Tokens identify handles/interfaces, not ownership or authenticated provenance. */
typedef struct {
  BOOLEAN Complete;
  UINTN ParentCount;
  GU_DS_DEVICE Gpu, Parents[GU_DS_MAX_PARENTS];
} GU_DS_SNAPSHOT;

/* Supplied values only: no ownership, provenance or physical qualification. */
BOOLEAN GuDecodeSnapshotValid(const GU_DS_SNAPSHOT *Snapshot);
BOOLEAN GuDecodeSnapshotMatches(const GU_DS_SNAPSHOT *Observed,const GU_DS_SNAPSHOT *Expected,BOOLEAN IoCleared);
