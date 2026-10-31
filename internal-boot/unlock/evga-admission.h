/* GPL-2.0-only. Exact captured EVGA 3090 admission; does not access hardware. */
#pragma once
#include "checks.h"
#define GU_EVGA_BOARD 0x220410deU
#define GU_EVGA_SUBSYSTEM 0x33873842U
#define GU_EVGA_BAR_BASE 0x84000000ULL
#define GU_EVGA_BAR_BYTES 0x1000000ULL
#define GU_EVGA_PROM_BYTES 0x100000U
#define GU_EVGA_PROM_HASH "f1c7c6b205d9a5c0626af4f82e28cc377aa16cde8c25c61c77b4b5160b26e71f"
typedef struct { UINT32 chip,gfw_plm,gfw_boot,gfw_cpuctl,wpr_hi,feature_plm,ss0,ss1,memory; } GU_EVGA_COLD;
BOOLEAN GuEvgaAdmit(const UINT32 config[64],UINTN segment,UINTN bus,UINTN device,UINTN function,
                    const GU_EVGA_COLD *cold,const VOID *prom,UINTN promBytes,GU_CAPS *caps);
