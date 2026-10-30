/* Modified 2026-10-19/20 for Windows GPU UNLOCK; changes documented in CHANGES.md. */ #include "../port.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

static EFI_BOOT_SERVICES bs;
static EFI_PCI_IO_PROTOCOL pci;
static UINT8 config[256];
static UINT32 registers[0x1000000/4];
static struct {
  UINTN allocs, frees, maps, unmaps, writes, pciwrites, pcireads, reads, delay;
  int alloc_fail, map_fail, short_map, high_map, unaligned_map, unmap_fail, free_fail;
  int mem_fail, config_fail, lost_after_flr, flr_seen, config_too_early, flr_write_error;
  UINT64 next_dma;
} fake;
static EFI_STATUS EFIAPI pool(EFI_MEMORY_TYPE T, UINTN N, VOID **P) {
  (void)T; *P=calloc(1,N); return *P?EFI_SUCCESS:EFI_OUT_OF_RESOURCES;
}
static EFI_STATUS EFIAPI freepool(VOID *P) {free(P);return EFI_SUCCESS;}
static EFI_STATUS EFIAPI allocate(EFI_PCI_IO_PROTOCOL *P, EFI_ALLOCATE_TYPE T,
 EFI_MEMORY_TYPE M, UINTN N, VOID **C, UINT64 A) {
  (void)P;(void)T;(void)M;assert(!A); fake.allocs++;
  if(fake.alloc_fail)return EFI_OUT_OF_RESOURCES;
  *C=_aligned_malloc(N*4096,4096);return *C?EFI_SUCCESS:EFI_OUT_OF_RESOURCES;
}
static EFI_STATUS EFIAPI freebuffer(EFI_PCI_IO_PROTOCOL *P, UINTN N, VOID *C) {
  (void)P;assert(N);fake.frees++;if(fake.free_fail)return EFI_DEVICE_ERROR;
  _aligned_free(C);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI map(EFI_PCI_IO_PROTOCOL *P, EFI_PCI_IO_PROTOCOL_OPERATION O,
 VOID *C, UINTN *N, EFI_PHYSICAL_ADDRESS *D, VOID **M) {
  (void)P;assert(O==EfiPciIoOperationBusMasterCommonBuffer);assert(C && *N);
  fake.maps++; if(fake.map_fail)return EFI_UNSUPPORTED;
  *D=fake.next_dma;fake.next_dma+=0x100000;
  if(fake.high_map)*D=0x100000000ULL;
  if(fake.unaligned_map)(*D)++;
  if(fake.short_map)*N-=4096;
  *M=(VOID *)(UINTN)(0x100+fake.maps);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI unmap(EFI_PCI_IO_PROTOCOL *P, VOID *M) {
  (void)P;assert(M);fake.unmaps++;return fake.unmap_fail?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI readconfig(EFI_PCI_IO_PROTOCOL *P,
 EFI_PCI_IO_PROTOCOL_WIDTH W, UINT32 O, UINTN N, VOID *V) {
  (void)P;UINTN bytes=((UINTN)1<<W)*N;assert(O+bytes<=sizeof(config));
  fake.pcireads++;
  if(fake.flr_seen && fake.delay<100000)fake.config_too_early=1;
  if(fake.config_fail)return EFI_DEVICE_ERROR;
  if(fake.flr_seen && fake.lost_after_flr && O==0){memset(V,255,bytes);return EFI_SUCCESS;}
  memcpy(V,config+O,bytes);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI writeconfig(EFI_PCI_IO_PROTOCOL *P,
 EFI_PCI_IO_PROTOCOL_WIDTH W, UINT32 O, UINTN N, VOID *V) {
  (void)P;UINTN bytes=((UINTN)1<<W)*N;assert(O+bytes<=sizeof(config));
  /* Status and W1C capability registers must never be replayed. */
  assert(!(O<=6 && O+bytes>6));
  assert(!(O<Gu.pcie_cap+12U && O+bytes>Gu.pcie_cap+10U));
  assert(!(O<Gu.pm_cap+6U && O+bytes>Gu.pm_cap+4U));
  fake.pciwrites++;if(fake.config_fail)return EFI_DEVICE_ERROR;
  memcpy(config+O,V,bytes);
  if(O==Gu.pcie_cap+8U && W==EfiPciIoWidthUint16 && (*(UINT16 *)V&0x8000)) {
    fake.flr_seen=1;fake.delay=0;
    memset(config+0x10,0,24);config[4]=0;
    config[Gu.pcie_cap+8]=config[Gu.pcie_cap+9]=0;
    if(Gu.pcie_version==2)config[Gu.pcie_cap+0x28]=config[Gu.pcie_cap+0x29]=0;
    if(fake.flr_write_error)return EFI_DEVICE_ERROR;
  }
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI readmem(EFI_PCI_IO_PROTOCOL *P,
 EFI_PCI_IO_PROTOCOL_WIDTH W, UINT8 B, UINT64 O, UINTN N, VOID *V) {
  (void)P;assert(!B && W==EfiPciIoWidthUint32 && N==1);assert(O+4<=sizeof(registers));
  fake.reads++;if(fake.mem_fail)return EFI_DEVICE_ERROR;*(UINT32 *)V=registers[O/4];return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI writemem(EFI_PCI_IO_PROTOCOL *P,
 EFI_PCI_IO_PROTOCOL_WIDTH W, UINT8 B, UINT64 O, UINTN N, VOID *V) {
  (void)P;assert(!B && W==EfiPciIoWidthUint32 && N==1);assert(O+4<=sizeof(registers));
  fake.writes++;if(fake.mem_fail)return EFI_DEVICE_ERROR;registers[O/4]=*(UINT32 *)V;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI stall(UINTN N) {fake.delay+=N;return EFI_SUCCESS;}
VOID GuLog(const char *F, ...) {(void)F;}
static VOID setup(VOID) {
  memset(&fake,0,sizeof(fake));memset(&bs,0,sizeof(bs));memset(&pci,0,sizeof(pci));
  memset(config,0,sizeof(config));memset(registers,0,sizeof(registers));
  fake.next_dma=0x12000000;
  bs.AllocatePool=pool;bs.FreePool=freepool;bs.Stall=stall;
  pci.AllocateBuffer=allocate;pci.FreeBuffer=freebuffer;pci.Map=map;pci.Unmap=unmap;
  pci.Mem.Read=readmem;pci.Mem.Write=writemem;pci.Pci.Read=readconfig;pci.Pci.Write=writeconfig;
  *(UINT32 *)(config+0)=GU_BOARD_ID;*(UINT32 *)(config+0x2c)=GU_BOARD_SUBSYSTEM;
  config[4]=3;config[9]=0;config[10]=0;config[11]=3;
  config[0x34]=0x60;config[0x60]=1;config[0x61]=0x78;
  config[0x78]=0x10;config[0x7a]=2;
  *(UINT32 *)(config+0x7c)=0x112c8de1;*(UINT16 *)(config+0x80)=0x2930;
  *(UINT32 *)(config+0x10)=0x84000000;
  *(UINT32 *)(config+0x14)=0xc;*(UINT32 *)(config+0x18)=0x60;
  *(UINT32 *)(config+0x1c)=0xc;*(UINT32 *)(config+0x20)=0x64;
  *(UINT32 *)(config+0x24)=0x5001;*(UINT32 *)(config+0x30)=0x85000000;
  GuInit(&bs,&pci);Gu.bar_base=0x84000000;Gu.bar_size=sizeof(registers);
  Gu.pcie_cap=0x78;Gu.pcie_version=2;Gu.pm_cap=0x60;Gu.armed=TRUE;Gu.owned=TRUE;Gu.display_ok=TRUE;Gu.profile_verified=TRUE;
}
int main(void) {
  setup();
  assert(GuTargetConfig((UINT32 *)config,GU_TARGET_SEGMENT,GU_TARGET_BUS,0,0));
  assert(!GuTargetConfig((UINT32 *)config,GU_TARGET_SEGMENT^1U,GU_TARGET_BUS,0,0));
  assert(!GuTargetConfig((UINT32 *)config,GU_TARGET_SEGMENT+0x10000U,GU_TARGET_BUS,0,0));
  assert(!GuTargetConfig((UINT32 *)config,GU_TARGET_SEGMENT,GU_TARGET_BUS+1U,0,0));
  *(UINT32 *)(config+0x2c)^=1;assert(!GuTargetConfig((UINT32 *)config,GU_TARGET_SEGMENT,GU_TARGET_BUS,0,0));
  setup(); VOID *cpu=NULL;UINT64 dma=0;
  assert(!EFI_ERROR(GuDmaAllocate(59136,&cpu,&dma)));
  assert(cpu && dma==0x12000000 && dma!=(UINTN)cpu && fake.maps==1);
  for(UINTN i=0;i<15*4096;i++)assert(((UINT8 *)cpu)[i]==0);
  assert(!EFI_ERROR(GuDmaFree(cpu,dma)) && fake.unmaps==1 && fake.frees==1);
  for(int mode=0;mode<5;mode++) {
    setup();fake.alloc_fail=mode==0;fake.map_fail=mode==1;fake.short_map=mode==2;
    fake.high_map=mode==3;fake.unaligned_map=mode==4;cpu=NULL;dma=0;
    assert(EFI_ERROR(GuDmaAllocate(8192,&cpu,&dma)) && !cpu && !dma);
    assert(!fake.writes && !fake.pciwrites && !Gu.allocations);
    assert(fake.frees==(mode==0?0u:1u));
    assert(fake.unmaps==(mode>=2?1u:0u));
  }
  setup();assert(!EFI_ERROR(GuDmaAllocate(4096,&cpu,&dma)));
  Gu.started=TRUE;assert(!GuSafeToReturn());assert(EFI_ERROR(GuDmaFree(cpu,dma)));
  assert(!fake.frees && !fake.unmaps && Gu.retained);
  Gu.quiesced=TRUE;Gu.retained=FALSE;assert(!EFI_ERROR(GuDmaFree(cpu,dma)));
  setup();assert(!EFI_ERROR(GuDmaAllocate(4096,&cpu,&dma)));fake.unmap_fail=1;
  assert(EFI_ERROR(GuDmaFree(cpu,dma)) && !fake.frees && Gu.retained);
  fake.unmap_fail=0;
  assert(EFI_ERROR(GuDmaFree(cpu,dma)) && fake.unmaps==1 && !fake.frees);
  /* Host-only disposal: production never clears a retained state to retry. */
  Gu.retained=FALSE;assert(!EFI_ERROR(GuDmaFree(cpu,dma)));
  setup();assert(!EFI_ERROR(GuDmaAllocate(4096,&cpu,&dma)));fake.free_fail=1;
  assert(EFI_ERROR(GuDmaFree(cpu,dma)) && Gu.retained && Gu.allocations && !Gu.allocations->mapped);
  fake.free_fail=0;assert(EFI_ERROR(GuDmaFree(cpu,dma)) && fake.frees==1);
  Gu.retained=FALSE;assert(!EFI_ERROR(GuDmaFree(cpu,dma)));
  for(int mode=0;mode<2;mode++) {
    setup();fake.short_map=1;fake.unmap_fail=!mode;fake.free_fail=mode;
    assert(EFI_ERROR(GuDmaAllocate(8192,&cpu,&dma)) && !cpu && !dma);
    assert(Gu.retained && Gu.allocations && !GuSafeToReturn());
    assert(Gu.allocations->mapped==!mode);
    assert(fake.unmaps==1 && fake.frees==(UINTN)mode);
    cpu=Gu.allocations->cpu;dma=Gu.allocations->dma;
    fake.unmap_fail=fake.free_fail=0;Gu.retained=FALSE;
    assert(!EFI_ERROR(GuDmaFree(cpu,dma)));
  }
  setup();GuWrite32(0x82381c,0x88888888);assert(registers[0x82381c/4]==0x88888888);
  setup();assert(!EFI_ERROR(GuDmaAllocate(4096,&cpu,&dma)));
  Gu.started=Gu.quiesced=Gu.fault=TRUE;
  assert(EFI_ERROR(GuDmaFree(cpu,dma)) && Gu.retained && !fake.unmaps && !fake.frees);
  Gu.fault=Gu.retained=FALSE;assert(!EFI_ERROR(GuDmaFree(cpu,dma)));
  setup();GuWrite32(0x82381c,0x88888888);
  fake.mem_fail=1;GuWrite32(0x823820,8);assert(Gu.fault);
  fake.mem_fail=0;GuWrite32(0x823820,8);assert(fake.writes==2 && registers[0x823820/4]==0);
  setup();GuWrite32(0x1000000,1);assert(Gu.fault && !fake.writes);
  setup();Gu.armed=FALSE;GuWrite32(0,1);assert(!fake.writes && Gu.fault);
  setup();fake.mem_fail=1;assert(GuRead32(0)==0xffffffffU && Gu.fault);
  setup();assert(!EFI_ERROR(GuSavePci()));UINT32 saved[64];memcpy(saved,config,sizeof(saved));
  assert(!EFI_ERROR(GuFlr()) && fake.flr_seen && !fake.config_too_early && Gu.flrs==1);
  assert(!EFI_ERROR(GuRestorePci()));
  assert(!memcmp(config+0x10,(UINT8 *)saved+0x10,24) && !(config[4]&4) && (config[4]&2));
  assert(*(UINT16 *)(config+0x80)==0x2930);
  assert(EFI_ERROR(GuFlr()) && Gu.flrs==1);
  setup();assert(!EFI_ERROR(GuSavePci()));fake.lost_after_flr=1;
  assert(EFI_ERROR(GuFlr()) && fake.delay>=100000 && !fake.config_too_early);
  setup();assert(!EFI_ERROR(GuSavePci()));fake.flr_write_error=1;
  assert(EFI_ERROR(GuFlr()) && fake.flr_seen && fake.delay>=100000);
  assert(!GuSafeToReturn());assert(!EFI_ERROR(GuClearMaster()) && !fake.config_too_early);
  setup();assert(!EFI_ERROR(GuSavePci()));config[4]|=4;assert(EFI_ERROR(GuFlr()) && !fake.flr_seen);
  setup();assert(!EFI_ERROR(GuSavePci()));config[0x82]|=0x20;assert(EFI_ERROR(GuFlr()) && !fake.flr_seen && fake.delay==1000000);
  setup();config[4]|=4;Gu.fault=TRUE;
  assert(!EFI_ERROR(GuClearMaster()) && !(config[4]&4));
  puts("UEFI adapter targeting, DMA ownership, I/O faults and FLR containment: PASS");
  return 0;
}
