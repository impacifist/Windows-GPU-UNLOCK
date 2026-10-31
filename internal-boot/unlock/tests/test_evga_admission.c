/* Captured PCI configuration, synthetic cold mutations, no hardware access. */
#include "evga-admission.h"
static UINT32 Config[64]={0x220410deU,0x00100002U,0x030000a1U,0x00800010U,0x84000000U,0x0000000cU,0x00000040U,0x0000000cU,0x00000048U,0x00005001U,0x00000000U,0x33873842U,0x85000000U,0x00000060U,0x00000000U,0x000001ffU,0x33873842U,0x00000000U,0x00000000U,0x00000000U,0x00000000U,0x00000001U,0x0023d6ceU,0x00000000U,0x48036801U,0x00000008U,0x00807805U,0x00000000U,0x00000000U,0x00000000U,0x0012b410U,0x112c8de1U,0x00092930U,0x00454d04U,0x11040140U,0x00000000U,0x00000000U,0x00000000U,0x00000000U,0x00070813U,0x00000400U,0x0180001eU,0x001e0004U,0x00000000U,0x00000000U,0x01140009U,0x00000000U,0x00000000U,0x00000000U,0x00000000U,0x00050011U,0x00b90000U,0x00ba0000U,0x00000000U,0x00000000U,0x00000000U,0x00000000U,0x00000000U,0x00000000U,0x00000000U,0x00000000U,0x00000000U,0x00000000U,0x00000000U};
static UINT8 Prom;
static BOOLEAN BadHash;static UINTN HashCalls;
static BOOLEAN Eq(const char *a,const char *b){while(*a && *a==*b){a++;b++;}return *a==*b;}
BOOLEAN GuHashMatches(const VOID *p,UINTN n,const char *hash){HashCalls++;return !BadHash && p==&Prom && n==GU_EVGA_PROM_BYTES && Eq(hash,GU_EVGA_PROM_HASH);}
int main(VOID){
 GU_CAPS caps;GU_EVGA_COLD c={0xb72000a1,0x8b8f,0x3ff,0x10,0,0xffffff8f,0x30053014,7,0x6000};
 if(!GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps) || caps.pm!=0x60 || caps.msi!=0x68 || caps.pcie!=0x78 || HashCalls!=1 || !GuCheckConfig(Config,0,1,0,0,&caps))return 1;
 if(GuEvgaAdmit(Config,0,2,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 2;
 UINT32 original=Config[0];Config[0]^=1;if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 3;Config[0]=original;
 original=Config[11];Config[11]^=1;if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 4;Config[11]=original;
 original=Config[1];Config[1]|=4;if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 5;Config[1]=original;
 original=Config[4];Config[4]^=0x1000000;if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 6;Config[4]=original;
 original=Config[0x34/4];Config[0x34/4]=0x41;if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 7;Config[0x34/4]=original;
 original=Config[0xc8/4];Config[0xc8/4]^=1;if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 8;Config[0xc8/4]=original;
 c.chip^=1;if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 9;c.chip^=1;
 c.feature_plm^=1;if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 10;c.feature_plm^=1;
 c.ss0=0x30053015U;if(!GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 11;
 c.ss0=0x12053015U;if(!GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 26;
 c.ss0=0xbadf0000U;if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 27;
 c.ss0=0xffffffffU;if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 28;
 c.ss0=0x88888888U;if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 29;c.ss0=0x30053014U;
 c.ss1^=1;if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 12;c.ss1^=1;
 c.memory^=1;if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 13;c.memory^=1;
 BadHash=TRUE;if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 14;
 BadHash=FALSE;if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES-1,&caps))return 15;
 original=Config[1];Config[1]|=1;if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 16;Config[1]=original;
 original=Config[(0x78+8)/4];Config[(0x78+8)/4]|=0x00200000;if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 17;Config[(0x78+8)/4]=original;
 c.gfw_plm^=1;if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 18;c.gfw_plm^=1;
 c.gfw_boot^=1;if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 19;c.gfw_boot^=1;
 c.gfw_cpuctl^=1;if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 20;c.gfw_cpuctl^=1;
 c.wpr_hi=1;if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps))return 21;c.wpr_hi=0;
 caps.pm=1;if(GuEvgaAdmit(NULL,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,&caps) || caps.pm)return 22;
 if(GuEvgaAdmit(Config,0,1,0,0,NULL,&Prom,GU_EVGA_PROM_BYTES,&caps))return 23;
 if(GuEvgaAdmit(Config,0,1,0,0,&c,NULL,GU_EVGA_PROM_BYTES,&caps))return 24;
 if(GuEvgaAdmit(Config,0,1,0,0,&c,&Prom,GU_EVGA_PROM_BYTES,NULL))return 25;
 return 0;
}
