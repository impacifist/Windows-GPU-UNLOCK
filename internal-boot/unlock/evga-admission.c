/* GPL-2.0-only. CPU-only admission; no PCI operations or GPU writes. */
#include "evga-admission.h"
#include "sha256.h"
BOOLEAN GuEvgaAdmit(const UINT32 config[64],UINTN segment,UINTN bus,UINTN device,UINTN function,
                    const GU_EVGA_COLD *cold,const VOID *prom,UINTN promBytes,GU_CAPS *caps) {
 GU_CAPS found={0};if(!caps)return FALSE;*caps=found;
 if(!config || !cold || !prom || promBytes!=GU_EVGA_PROM_BYTES)return FALSE;
 if(!GuCheckConfigIdentity(config,segment,bus,device,function,1,GU_EVGA_BOARD,GU_EVGA_SUBSYSTEM,&found))return FALSE;
 if((config[1]&0xffffU)!=2U || config[2]!=0x030000a1U || config[4]!=GU_EVGA_BAR_BASE || config[0xc8/4]!=0x00050011U ||
    (config[(found.pcie+8U)/4U]&0x00200000U))return FALSE;
 if(cold->chip!=0xb72000a1U || cold->gfw_plm!=0x8b8fU || cold->gfw_boot!=0x3ffU ||
    cold->gfw_cpuctl!=0x10U || cold->wpr_hi!=0 || cold->feature_plm!=0xffffff8fU ||
    cold->ss0==0xffffffffU || (cold->ss0&0xffff0000U)==0xbadf0000U ||
    cold->ss0==0x88888888U || cold->ss1!=7U || cold->memory!=0x6000U)return FALSE;
 if(!GuHashMatches(prom,promBytes,GU_EVGA_PROM_HASH))return FALSE;
 *caps=found;return TRUE;
}
