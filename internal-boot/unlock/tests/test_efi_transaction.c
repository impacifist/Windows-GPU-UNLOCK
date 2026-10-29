/* ASRock port changes: 2026-10-27; publication notice: 2026-10-28. GPL-2.0-only; see CHANGES.md. */
/* Modified 2026-10-19/20 for Windows GPU UNLOCK; changes documented in CHANGES.md. */ /* SPDX-License-Identifier: GPL-2.0-only */
/* Real retained core + compatibility layer + UEFI port; only EFI services are fake. */
#include "../port.h"
#include "../checks.h"
#ifdef GU_ENTRY_LATCHED
/* Test fixture baseline only, never a production fixed-selector definition. */
#ifndef GU_STOCK_PROFILE_SS0
#define GU_STOCK_PROFILE_SS0 0x03100062U
#endif
static UINT32 test_entry_ss0=0x03100062U,test_entry_ss1=6;
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <malloc.h>
#undef EPROTO
#undef EBADMSG
#undef EOVERFLOW
#undef EOPNOTSUPP
#undef ETIMEDOUT
#undef EALREADY
#include "../compat/compat.h"
#include "../vendor/unlock_610.h"
#include "../vendor/prep_610.h"
#include "../vendor/vbios_610.h"

#ifndef GU_UEFI_PORT
#error This integration test must execute the real UEFI compatibility path.
#endif
#define MAX_BUFFERS 24000
#define MAX_POOLS 26000
#define GSP 0x110000U
#define SEC2 0x840000U
#define SS0 0x82381cU
#define SS1 0x823820U
#define PLM 0x823804U
enum scenario {
  SUCCESS, FWSEC_TIMEOUT, POST_FLR_TIMEOUT, BME_READBACK_FAIL,
  MMIO_WRITE_FAIL, FLUSH_FAIL, FLR_WRITE_FAIL, UNMAP_FAIL,
  MAP_FAIL, MAP_PARTIAL, RESTORE_FAIL, DISPLAY_CHANGED, SELECTOR_LOST,
  FWSEC_NO_WPR, FWSEC_RUNNING, DIAG_READ_FAIL, POST_DIAG_READ_FAIL,
  PIO_UNAVAILABLE, PIO_UNAVAILABLE_NO_WPR, FWSEC_ERROR_CODE,
#ifdef GU_ENTRY_LATCHED
  CORE_ENTRY_SS0, CORE_ENTRY_SS1, BEFORE_FW_SS0, BEFORE_FW_SS1,
  SELECTOR_ENTRY_SS0, SELECTOR_ENTRY_SS1, PRE_FLR_SS1, LATE_FLR_SS1,
#endif
  SCENARIO_COUNT
};
static const char *case_names[]={"success","fwsec-timeout","post-flr-gfw-timeout",
  "bme-readback-failure","mmio-write-failure","flush-failure","flr-write-failure",
  "unmap-failure","map-failure","partial-map","pci-restore-failure","display-changed","selector-lost",
  "fwsec-halted-no-wpr","fwsec-running-then-halt",
  "fwsec-prestart-snapshot-failure","fwsec-posthalt-snapshot-failure",
  "pio-unavailable-fwsec-success","pio-unavailable-fwsec-no-wpr","fwsec-error-code"
#ifdef GU_ENTRY_LATCHED
  ,"core-entry-ss0","core-entry-ss1","before-fw-ss0","before-fw-ss1",
  "selector-entry-ss0","selector-entry-ss1","pre-flr-ss1","late-flr-ss1"
#endif
};
typedef struct {VOID *cpu;UINTN bytes;UINT64 dma;BOOLEAN mapped,freed;} TEST_BUFFER;
typedef struct {VOID *cpu;BOOLEAN freed;} TEST_POOL;
static TEST_BUFFER buffers[MAX_BUFFERS];
static TEST_POOL pools[MAX_POOLS];
static EFI_BOOT_SERVICES bs;
static EFI_PCI_IO_PROTOCOL pci;
static UINT8 config[256],prom[1024*1024];
static UINT32 core_captured_config[64];
static BOOLEAN core_use_capture;
static UINT32 registers[0x1000000/4];
static UINT8 gsp_dmem[0x800];
static struct {
  enum scenario mode;
  UINTN buffer_count,pool_count,pool_live,find_hint;
  UINTN maps,unmaps,frees,live_maps,live_buffers,mmio_writes,pci_writes;
  UINTN gsp_starts,sec2_starts,flrs,flushes,dma_reads,radix_bytes;
  UINTN dmem_reads,pio_reads,pio_writes,gsp_poll_reads,trace_count;
  UINTN master_enables,master_disables;
  UINT64 pci_attributes;
  UINT32 write_trace[8192][4];
  UINT64 next_dma,delay_since_flr,elapsed,meta_dma;
  BOOLEAN bme_read_failed,unmap_failed,write_failed,restore_failed;
  char log[65536];
} fake;

static void fail(const char *expression,const char *file,int line) {
  size_t length=strlen(fake.log);
  fprintf(stderr,"ASSERTION FAILED [%s] %s:%d: %s\n",case_names[fake.mode],file,line,expression);
  fprintf(stderr,"started=%u quiesced=%u fault=%u retained=%u GSP=%zu SEC2=%zu FLR=%zu mappings=%zu buffers=%zu\n",
    Gu.started,Gu.quiesced,Gu.fault,Gu.retained,fake.gsp_starts,fake.sec2_starts,
    fake.flrs,fake.live_maps,fake.live_buffers);
  fprintf(stderr,"%s",fake.log+(length>5000?length-5000:0));
  exit(1);
}
#define CHECK(x) ((x)?(void)0:fail(#x,__FILE__,__LINE__))
static VOID trace_write(UINT32 kind,UINT32 width,UINT32 offset,UINT32 value) {
  CHECK(fake.trace_count<8192);UINT32 *r=fake.write_trace[fake.trace_count++];
  r[0]=kind;r[1]=width;r[2]=offset;r[3]=value;
}
VOID GuLog(const char *format,...) {
  va_list args;size_t used=strlen(fake.log);
  va_start(args,format);vsnprintf(fake.log+used,sizeof(fake.log)-used,format,args);va_end(args);
}
VOID GuLogText(const char *text) {GuLog("%s",text);}
static UINT64 le64(const VOID *pointer) {
  const UINT8 *p=pointer;UINT64 value=0;for(int i=7;i>=0;i--)value=(value<<8)|p[i];return value;
}
static UINT32 le32(const VOID *pointer) {
  const UINT8 *p=pointer;return p[0]|((UINT32)p[1]<<8)|((UINT32)p[2]<<16)|((UINT32)p[3]<<24);
}
static VOID quiet_check(void) {CHECK(!fake.flrs || fake.delay_since_flr>=100000);}
static TEST_BUFFER *cpu_buffer(VOID *cpu) {
  if(fake.buffer_count && buffers[fake.buffer_count-1].cpu==cpu)return &buffers[fake.buffer_count-1];
  for(UINTN step=0;step<fake.buffer_count;step++) {
    UINTN index=(fake.find_hint+step)%fake.buffer_count;
    if(buffers[index].cpu==cpu){fake.find_hint=(index+1)%fake.buffer_count;return &buffers[index];}
  }
  CHECK(0);return NULL;
}
static UINT8 *dma_pointer(UINT64 address,UINTN bytes) {
  UINTN low=0,high=fake.buffer_count;
  while(low<high) {
    UINTN mid=low+(high-low)/2;
    if(buffers[mid].dma<=address)low=mid+1;else high=mid;
  }
  CHECK(low>0);TEST_BUFFER *b=&buffers[low-1];
  CHECK(b->mapped && !b->freed && address>=b->dma && address-b->dma<=b->bytes);
  CHECK(bytes<=b->bytes-(UINTN)(address-b->dma));
  CHECK(address!=(UINT64)(UINTN)b->cpu+(address-b->dma));
  fake.dma_reads++;return (UINT8 *)b->cpu+(UINTN)(address-b->dma);
}
static VOID check_fwsec_image(void) {
#ifdef GU_EXPERIMENTAL_3090
  struct gpu_unlock_fwsec fw;UINT32 sig_at;
  CHECK(!gpu_unlock_vbios_fwsec(prom,sizeof(prom),&fw));
  /* FWSEC-FRTS location pinned per profile PROM. */
#if defined(GU_PROFILE_B650M_3090FE)
  CHECK(fw.image_off==0x4239c && fw.image_size==0xde00 && fw.dmem_size==sizeof(gsp_dmem));
#elif defined(GU_PROFILE_X870_3090TI)
  CHECK(fw.image_off==0x41758 && fw.image_size==0xea00 && fw.dmem_size==sizeof(gsp_dmem)); /* VBIOS 94.02.a0.00.2c */
#elif defined(GU_PROFILE_H610M_3060)
  CHECK(fw.image_off==0x416bc && fw.image_size==0xea00 && fw.dmem_size==sizeof(gsp_dmem)); /* Gigabyte RTX 3060 12GB PROM 727dd833 */
#elif defined(GU_PROFILE_B660PLUS_3060)
  CHECK(fw.image_off==0x416b4 && fw.image_size==0xea00 && fw.dmem_size==sizeof(gsp_dmem)); /* VBIOS 94.04.71.00.87 */
#elif defined(GU_PROFILE_LOCAL) || defined(GU_PROFILE_ASROCK_B450M_3060)
  /* Pinned by make-profile.py from fwsec-locate on this machine's PROM.BIN. */
  CHECK(fw.image_off==GU_LOCAL_FWSEC_IMAGE_OFF && fw.image_size==GU_LOCAL_FWSEC_IMAGE_SIZE && fw.dmem_size==sizeof(gsp_dmem));
#else
#error Pin this profile's FWSEC-FRTS image offset and size from its PROM
#endif
  CHECK(!gpu_unlock_fwsec_sig_offset(fw.sig_versions,2,384,&sig_at));
  UINT64 last_source=((UINT64)registers[(GSP+0x110)/4]<<8)|((UINT64)registers[(GSP+0x128)/4]<<40);
  UINT8 *image=dma_pointer(last_source-fw.data_off,fw.image_size);
  CHECK(!memcmp(gsp_dmem,image+fw.data_off,sizeof(gsp_dmem)));
  CHECK(!memcmp(gsp_dmem+fw.pkc_off,prom+fw.sig_off+sig_at,384));
  static UINT8 expected[0x10000];CHECK(fw.image_size<=sizeof(expected));memcpy(expected,prom+fw.image_off,fw.image_size);
  /* With the mocked VGA/MMU registers FRTS sits 2 MiB below the reported FB top (B650M: 0x5ffe00000). */
  CHECK(!gpu_unlock_fwsec_patch_frts(expected,&fw,((UINT64)GU_DEVICE_MEMORY_SIZE<<20)-0x200000ULL));
  memcpy(expected+fw.data_off+fw.pkc_off,prom+fw.sig_off+sig_at,384);
  CHECK(!memcmp(gsp_dmem,expected+fw.data_off,sizeof(gsp_dmem)));
#else
  UINT64 last_source=((UINT64)registers[(GSP+0x110)/4]<<8)|((UINT64)registers[(GSP+0x128)/4]<<40);
  CHECK(last_source>=0xdf00);UINT8 *image=dma_pointer(last_source-0xdf00,0xe700);
  CHECK(!memcmp(gsp_dmem,image+0xdf00,sizeof(gsp_dmem)));
  CHECK(!memcmp(gsp_dmem+0x5a4,prom+0x43060+0x300,384));
  CHECK(le32(gsp_dmem+0x7c0+32)==0x27fe00);
  CHECK(le32(gsp_dmem+0x7c0+36)==0x100);
#endif
}
static VOID check_booter_publication(void) {
  UINT64 meta_address=(UINT64)registers[(SEC2+0x40)/4]|((UINT64)registers[(SEC2+0x44)/4]<<32);
  UINT8 *meta=dma_pointer(meta_address,256);fake.meta_dma=meta_address;
  CHECK(le64(meta)==GU_WPR_META_MAGIC && le64(meta+8)==1);
  CHECK(le64(meta+24)==0x5053000 && le64(meta+80)==0xfa00);
  UINT64 delta=0;
#ifdef GU_EXPERIMENTAL_3090
  /* Layout is FB-top relative: 10 GiB fixture top 0x280000000 -> profile top (B650M delta 0x380000000). */
  delta=((UINT64)GU_DEVICE_MEMORY_SIZE<<20)-0x280000000ULL;
#endif
  CHECK(le64(meta+112)==0x275400000ULL+delta && le64(meta+136)==0x27ada0000ULL+delta);
  CHECK(le64(meta+144)==0x27fdfa000ULL+delta && le64(meta+152)==0x27fe00000ULL+delta);
  CHECK(le64(meta+168)==0x27ff00000ULL+delta && le64(meta+176)==0x280000000ULL+delta);
  CHECK(!memcmp(dma_pointer(le64(meta+72),64000),GuAssets[1].firmware.data,64000));
  CHECK(!memcmp(dma_pointer(le64(meta+32),24576),GuAssets[3].firmware.data,24576));
  /* Walk published DMA addresses as firmware would; expected ELF bytes are pinned. */
  for(UINT64 offset=0,page=0;offset<0x5053000;offset+=4096,page++) {
    UINT8 *l0=dma_pointer(le64(meta+16),4096);
    UINT8 *l1=dma_pointer(le64(l0+(page>>18)*8),4096);
    UINT8 *l2=dma_pointer(le64(l1+((page>>9)&511)*8),4096);
    UINT8 *leaf=dma_pointer(le64(l2+(page&511)*8),4096);
    UINTN count=(UINTN)(0x5053000-offset);if(count>4096)count=4096;
    CHECK(!memcmp(leaf,GuAssets[2].firmware.data+0x40+(size_t)offset,count));
    for(UINTN i=count;i<4096;i++)CHECK(!leaf[i]);
    fake.radix_bytes+=count;
  }
}
static VOID check_stock_signature(void) {
  UINT8 *meta=dma_pointer(fake.meta_dma,256);
  CHECK(le64(meta+80)==4096);
  struct gpu_unlock_span signature;
  CHECK(!gpu_unlock_elf_section(GuAssets[2].firmware.data,GuAssets[2].firmware.size,
                               ".fwsignature_ga10x",&signature));
  CHECK(signature.size==4096);
  CHECK(!memcmp(dma_pointer(le64(meta+72),4096),GuAssets[2].firmware.data+signature.offset,4096));
}
static EFI_STATUS EFIAPI allocate_pool(EFI_MEMORY_TYPE type,UINTN size,VOID **memory) {
  CHECK(type==EfiLoaderData && fake.pool_count<MAX_POOLS);
  *memory=calloc(1,size);if(!*memory)return EFI_OUT_OF_RESOURCES;
  pools[fake.pool_count++]=(TEST_POOL){*memory,FALSE};fake.pool_live++;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI free_pool(VOID *memory) {
  for(UINTN i=fake.pool_count;i>0;i--)if(pools[i-1].cpu==memory && !pools[i-1].freed) {
    pools[i-1].freed=TRUE;fake.pool_live--;free(memory);return EFI_SUCCESS;
  }
  CHECK(0);return EFI_NOT_FOUND;
}
static EFI_STATUS EFIAPI allocate_buffer(EFI_PCI_IO_PROTOCOL *p,EFI_ALLOCATE_TYPE type,
  EFI_MEMORY_TYPE memory_type,UINTN pages,VOID **cpu,UINT64 attributes) {
  CHECK(p==&pci && type==AllocateAnyPages && memory_type==EfiBootServicesData && !attributes);
  CHECK(pages && fake.buffer_count<MAX_BUFFERS);
  *cpu=_aligned_malloc(pages*4096,4096);if(!*cpu)return EFI_OUT_OF_RESOURCES;
  buffers[fake.buffer_count++]=(TEST_BUFFER){*cpu,pages*4096,0,FALSE,FALSE};
  fake.live_buffers++;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI free_buffer(EFI_PCI_IO_PROTOCOL *p,UINTN pages,VOID *cpu) {
  CHECK(p==&pci);TEST_BUFFER *b=cpu_buffer(cpu);
  CHECK(!b->freed && !b->mapped && pages*4096==b->bytes);
  CHECK(!Gu.started || Gu.quiesced);
  b->freed=TRUE;fake.frees++;fake.live_buffers--;_aligned_free(cpu);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI map_buffer(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_OPERATION operation,
  VOID *cpu,UINTN *bytes,EFI_PHYSICAL_ADDRESS *device_address,VOID **mapping) {
  CHECK(p==&pci && operation==EfiPciIoOperationBusMasterCommonBuffer);
  TEST_BUFFER *b=cpu_buffer(cpu);CHECK(*bytes==b->bytes && !b->mapped && !b->freed);
  fake.maps++;if(fake.mode==MAP_FAIL && fake.maps==1)return EFI_UNSUPPORTED;
  b->dma=fake.next_dma;fake.next_dma+=b->bytes;CHECK(fake.next_dma<=0xffffffffULL);
  CHECK(b->dma!=(UINT64)(UINTN)cpu);b->mapped=TRUE;fake.live_maps++;
  *device_address=b->dma;*mapping=b;
  if(fake.mode==MAP_PARTIAL && fake.maps==48)*bytes/=2;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI unmap_buffer(EFI_PCI_IO_PROTOCOL *p,VOID *mapping) {
  CHECK(p==&pci);TEST_BUFFER *b=mapping;
  CHECK(b>=buffers && b<buffers+fake.buffer_count && b->mapped && !b->freed);
  CHECK(!Gu.started || Gu.quiesced);fake.unmaps++;
  if(fake.mode==UNMAP_FAIL && !fake.unmap_failed){fake.unmap_failed=TRUE;return EFI_DEVICE_ERROR;}
  b->mapped=FALSE;fake.live_maps--;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI read_config(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH width,
  UINT32 offset,UINTN count,VOID *value) {
  CHECK(p==&pci && width<=EfiPciIoWidthUint32 && count==1);quiet_check();
  UINTN bytes=(UINTN)1<<width;CHECK(offset+bytes<=sizeof(config));
  if(fake.mode==BME_READBACK_FAIL && offset==4 && (config[4]&4) && !fake.bme_read_failed) {
    fake.bme_read_failed=TRUE;return EFI_DEVICE_ERROR;
  }
  memcpy(value,config+offset,bytes);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI write_config(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH width,
  UINT32 offset,UINTN count,VOID *value) {
  CHECK(p==&pci && width<=EfiPciIoWidthUint32 && count==1);quiet_check();
  UINTN bytes=(UINTN)1<<width;CHECK(offset+bytes<=sizeof(config));
  CHECK(!(offset<=6 && offset+bytes>6) &&
    !(offset<=Gu.pcie_cap+10U && offset+bytes>Gu.pcie_cap+10U) &&
    !(offset<=Gu.pm_cap+4U && offset+bytes>Gu.pm_cap+4U));
  UINT32 trace_value=0;memcpy(&trace_value,value,bytes);trace_write(1,(UINT32)width,offset,trace_value);
  fake.pci_writes++;memcpy(config+offset,value,bytes);
  if(offset==Gu.pcie_cap+8U && width==EfiPciIoWidthUint16 && (*(UINT16 *)value&0x8000)) {
    CHECK(!(config[4]&4) && fake.gsp_starts==1 && fake.sec2_starts==1);
    CHECK(!fake.unmaps && !fake.frees);check_stock_signature();
    fake.flrs++;CHECK(fake.flrs==1);fake.delay_since_flr=0;
    memset(config+0x10,0,24);config[4]=0;config[Gu.pcie_cap+8]=config[Gu.pcie_cap+9]=0;
    registers[(GSP+0x3c0)/4]=registers[(SEC2+0x3c0)/4]=0;
    registers[0x1fa824/4]=registers[0x1fa828/4]=0;
    registers[0x118128/4]=1;registers[0x118234/4]=0xff;registers[0x110100/4]=0x10;
    if(fake.mode==POST_FLR_TIMEOUT)registers[0x110100/4]=registers[0x118234/4]=0;
    if(fake.mode==SELECTOR_LOST)registers[SS1/4]=3;
    if(fake.mode==FLR_WRITE_FAIL)return EFI_DEVICE_ERROR;
  }
  if(fake.mode==RESTORE_FAIL && fake.flrs && offset==0x10 && !fake.restore_failed) {
    fake.restore_failed=TRUE;return EFI_DEVICE_ERROR;
  }
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI read_memory(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH width,
  UINT8 bar,UINT64 offset,UINTN count,VOID *value) {
  CHECK(p==&pci && !bar);quiet_check();
  CHECK(width==EfiPciIoWidthUint8 || (width==EfiPciIoWidthUint32 && count==1));
  UINTN bytes=((UINTN)1<<width)*count;CHECK(offset+bytes<=sizeof(registers));
#ifdef GU_ENTRY_LATCHED
  if(offset==0x300000 && width==EfiPciIoWidthUint8 && fake.maps && !fake.gsp_starts) {
    if(fake.mode==BEFORE_FW_SS0)registers[SS0/4]^=1;
    if(fake.mode==BEFORE_FW_SS1)registers[SS1/4]^=1;
  }
#endif
  if(offset==GSP+0x1c0 || offset==GSP+0x1c4)fake.pio_reads++;
  if(offset==GSP+0x1c4) {
    UINT32 at=registers[(GSP+0x1c0)/4];
    CHECK(!fake.gsp_starts && !(at&0xff000003U) && at+4<=sizeof(gsp_dmem));
    fake.dmem_reads++;
    if(fake.mode==PIO_UNAVAILABLE || fake.mode==PIO_UNAVAILABLE_NO_WPR){*(UINT32 *)value=0xffffffffU;return EFI_SUCCESS;}
    memcpy(value,gsp_dmem+at,4);return EFI_SUCCESS;
  }
  if(offset==GSP+0x94 &&
      ((fake.mode==DIAG_READ_FAIL && !fake.gsp_starts) ||
       (fake.mode==POST_DIAG_READ_FAIL && fake.gsp_starts)))return EFI_DEVICE_ERROR;
  if(offset==GSP+0x100 && fake.gsp_starts && !fake.sec2_starts) {
    fake.gsp_poll_reads++;
    if(fake.mode==FWSEC_RUNNING)
      registers[offset/4]=fake.gsp_poll_reads<=2?0:0x10;
  }
  memcpy(value,(UINT8 *)registers+(UINTN)offset,bytes);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI write_memory(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH width,
  UINT8 bar,UINT64 offset,UINTN count,VOID *value) {
  CHECK(p==&pci && !bar && width==EfiPciIoWidthUint32 && count==1);quiet_check();
  CHECK(offset+4<=sizeof(registers) && Gu.started);
  UINT32 v=*(UINT32 *)value;registers[offset/4]=v;fake.mmio_writes++;
#ifdef GU_ENTRY_LATCHED_SS1_PRESERVE
  CHECK(offset!=SS1);
#endif
#ifdef GU_ENTRY_LATCHED
  if(offset==SS0 && fake.mode==PRE_FLR_SS1)registers[SS1/4]^=1;
#endif
  trace_write(0,(UINT32)width,(UINT32)offset,v);
  CHECK(offset!=GSP+0x1c4); /* Diagnostics must never write firmware data. */
  if(offset==GSP+0x1c0) {
    CHECK(!fake.gsp_starts);fake.pio_writes++;
  }
  if(fake.mode==MMIO_WRITE_FAIL && !fake.write_failed){fake.write_failed=TRUE;return EFI_DEVICE_ERROR;}
  if(offset==GSP+0x118 || offset==SEC2+0x118) {
    UINT32 base=offset==GSP+0x118?GSP:SEC2;
    UINT64 source=((UINT64)registers[(base+0x110)/4]<<8)|((UINT64)registers[(base+0x128)/4]<<40);
    UINT8 *source_cpu=dma_pointer(source+registers[(base+0x11c)/4],256);
    if(base==GSP && !(v&0x10)) {
      UINT32 dest=registers[(base+0x114)/4];CHECK(dest+256<=sizeof(gsp_dmem));
      memcpy(gsp_dmem+dest,source_cpu,256);
    }
    registers[offset/4]=2;
  }
  if(offset==0x111668 || offset==0x841668)registers[offset/4]=1;
  if(offset==GSP+0x100 || offset==GSP+0x130) {
    CHECK(fake.pci_attributes&EFI_PCI_IO_ATTRIBUTE_BUS_MASTER);
    fake.gsp_starts++;CHECK(fake.gsp_starts==1 && (config[4]&4));check_fwsec_image();
    if(fake.mode!=FWSEC_NO_WPR && fake.mode!=PIO_UNAVAILABLE_NO_WPR) {
      registers[0x1fa824/4]=0x27fe000;registers[0x1fa828/4]=0x27feff0;
#ifdef GU_EXPERIMENTAL_3090
      /* Simulated FWSEC-FRTS: WPR2 covers the 1 MiB FRTS region (B650M: 0x5ffe000/0x5ffeff0). */
      registers[0x1fa824/4]=(UINT32)(((((UINT64)GU_DEVICE_MEMORY_SIZE<<20)-0x200000ULL)>>12)<<4);
      registers[0x1fa828/4]=registers[0x1fa824/4]+0xff0;
#endif
    }
    if(fake.mode==FWSEC_ERROR_CODE)registers[0x1438/4]=0x10000;
    registers[GSP/4+0x100/4]=fake.mode==FWSEC_TIMEOUT?2:0x10;
  }
  if(offset==SEC2+0x100 || offset==SEC2+0x130) {
    fake.sec2_starts++;CHECK(fake.sec2_starts==1 && (config[4]&4));
    check_booter_publication();registers[PLM/4]=~0U;
#ifdef GU_ENTRY_LATCHED
    if(fake.mode==SELECTOR_ENTRY_SS0)registers[SS0/4]^=1;
    if(fake.mode==SELECTOR_ENTRY_SS1)registers[SS1/4]^=1;
#endif
  }
  if(offset==SS1)registers[offset/4]=v&15;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI flush(EFI_PCI_IO_PROTOCOL *p) {
  CHECK(p==&pci);fake.flushes++;
  return fake.mode==FLUSH_FAIL && fake.flushes==1?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI master_attributes(EFI_PCI_IO_PROTOCOL *p,
 EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION op,UINT64 mask,UINT64 *out) {
  CHECK(p==&pci);
  if(op==EfiPciIoAttributeOperationSupported) {
    CHECK(out && !mask);*out=EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY|EFI_PCI_IO_ATTRIBUTE_BUS_MASTER;
    return EFI_SUCCESS;
  }
  if(op==EfiPciIoAttributeOperationGet) {
    CHECK(out && !mask);*out=fake.pci_attributes;return EFI_SUCCESS;
  }
  CHECK(!out && mask==EFI_PCI_IO_ATTRIBUTE_BUS_MASTER);
  UINT16 command=*(UINT16 *)(config+4);
  if(op==EfiPciIoAttributeOperationEnable) {
    fake.master_enables++;command|=4;fake.pci_attributes|=mask;
  } else {
    CHECK(op==EfiPciIoAttributeOperationDisable);
    fake.master_disables++;command&=(UINT16)~4U;fake.pci_attributes&=~mask;
#ifdef GU_ENTRY_LATCHED
    if(fake.mode==LATE_FLR_SS1)registers[SS1/4]^=1;
#endif
  }
  return write_config(p,EfiPciIoWidthUint16,4,1,&command);
}
static EFI_STATUS EFIAPI stall(UINTN microseconds) {
  fake.elapsed+=microseconds;if(fake.flrs)fake.delay_since_flr+=microseconds;return EFI_SUCCESS;
}
static BOOLEAN display_check(void) {return fake.mode!=DISPLAY_CHANGED;}
static VOID setup(enum scenario mode) {
  memset(&fake,0,sizeof(fake));memset(buffers,0,sizeof(buffers));memset(pools,0,sizeof(pools));
  memset(&bs,0,sizeof(bs));memset(&pci,0,sizeof(pci));memset(config,0,sizeof(config));
  memset(registers,0,sizeof(registers));memset(gsp_dmem,0,sizeof(gsp_dmem));
  fake.mode=mode;fake.next_dma=0x12000000;
  bs.AllocatePool=allocate_pool;bs.FreePool=free_pool;bs.Stall=stall;
  pci.AllocateBuffer=allocate_buffer;pci.FreeBuffer=free_buffer;pci.Map=map_buffer;pci.Unmap=unmap_buffer;
  pci.Pci.Read=read_config;pci.Pci.Write=write_config;pci.Mem.Read=read_memory;pci.Mem.Write=write_memory;pci.Flush=flush;
  pci.Attributes=master_attributes;fake.pci_attributes=EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY;
  *(UINT32 *)(config+0)=GU_BOARD_ID;*(UINT32 *)(config+0x2c)=GU_BOARD_SUBSYSTEM;
  *(UINT32 *)(config+4)=0x00100003;*(UINT32 *)(config+8)=0x030000a1;*(UINT32 *)(config+12)=0x00800010;
  *(UINT32 *)(config+0x10)=0x84000000;*(UINT32 *)(config+0x14)=0xc;*(UINT32 *)(config+0x18)=0x60;
  *(UINT32 *)(config+0x1c)=0xc;*(UINT32 *)(config+0x20)=0x64;*(UINT32 *)(config+0x24)=0x5001;
  *(UINT32 *)(config+0x30)=0x85000000;config[0x34]=0x60;config[0x3c]=0xff;
  *(UINT32 *)(config+0x60)=0x48036801;*(UINT32 *)(config+0x64)=8;
  *(UINT32 *)(config+0x68)=0x00807805;*(UINT32 *)(config+0x78)=0x0012b410;
  *(UINT32 *)(config+0x7c)=0x112c8de1;*(UINT32 *)(config+0x80)=0x00092930;
  *(UINT32 *)(config+0xc8)=0x00050011;
  registers[0]=GU_CHIP_ID;registers[0x118128/4]=0x8b8f;registers[0x118234/4]=0x03ff;
  registers[GSP/4+0x100/4]=0x10;registers[0x1fa824/4]=0x1ffffe00;
  registers[PLM/4]=0xffffff8f;registers[SS0/4]=0x46254040;registers[SS1/4]=3;
  registers[0x1183a4/4]=0x2800;registers[0x625f04/4]=1;registers[0x1fa7c8/4]=0x45cb8f;
#ifdef GU_EXPERIMENTAL_3090
  registers[SS0/4]=GU_STOCK_PROFILE_SS0;registers[SS1/4]=6;registers[0x1183a4/4]=GU_DEVICE_MEMORY_SIZE;
#endif
  registers[0x1fa82c/4]=0x100000;registers[0x1fa830/4]=0x26ffff0;
  registers[0x824148/4]=1;registers[0x8241e0/4]=3;
  registers[(GSP+0xf4)/4]=registers[(SEC2+0xf4)/4]=1U<<31;
  registers[(GSP+0x118)/4]=registers[(SEC2+0x118)/4]=2;
  registers[(GSP+0x1c0)/4]=0x40;
  registers[(GSP+0x94)/4]=0x12345678;
  memcpy((UINT8 *)registers+0x300000,prom,sizeof(prom));
  GuInit(&bs,&pci);Gu.bar_base=0x84000000;Gu.bar_size=sizeof(registers);
#ifdef GU_EXPERIMENTAL_3090
  if(core_use_capture)memcpy(config,core_captured_config,sizeof(config));
  Gu.bar_base=GU_BAR0_BASE;
#endif
  Gu.pcie_cap=0x78;Gu.pcie_version=2;Gu.pm_cap=0x60;Gu.msi_cap=0x68;Gu.msix_cap=0;
#ifdef GU_EXPERIMENTAL_3090
  if(core_use_capture) {
    UINT32 words[64];GU_CAPS caps;memcpy(words,config,sizeof(words));
    CHECK(GuCheckConfig(words,GU_TARGET_SEGMENT,GU_TARGET_BUS,0,0,&caps));
    Gu.pcie_cap=caps.pcie;Gu.pcie_version=caps.pcie_version;Gu.pm_cap=caps.pm;
    Gu.msi_cap=caps.msi;Gu.msix_cap=caps.msix;
    fake.pci_attributes=(config[4]&1?EFI_PCI_IO_ATTRIBUTE_IO:0)|
      (config[4]&2?EFI_PCI_IO_ATTRIBUTE_MEMORY:0);
  }
#endif
  Gu.owned=Gu.armed=Gu.display_ok=Gu.profile_verified=TRUE;Gu.display_check=display_check;
#ifdef GU_ENTRY_LATCHED
  registers[SS0/4]=test_entry_ss0;registers[SS1/4]=test_entry_ss1;
  Gu.prom_verified=TRUE;CHECK(GuLatchEntrySelectors(test_entry_ss0,test_entry_ss1));
  if(mode==CORE_ENTRY_SS0)registers[SS0/4]^=1;
  if(mode==CORE_ENTRY_SS1)registers[SS1/4]^=1;
#endif
}
static VOID dispose_host_fixture(void) {
  /* Test process disposal only, after retention assertions. This is not a recovery path. */
  for(UINTN i=0;i<fake.buffer_count;i++)if(!buffers[i].freed)_aligned_free(buffers[i].cpu);
  for(UINTN i=0;i<fake.pool_count;i++)if(!pools[i].freed)free(pools[i].cpu);
}
static UINT8 *read_file(const char *path,size_t *length) {
  FILE *file=fopen(path,"rb");CHECK(file);CHECK(!fseek(file,0,SEEK_END));
  long size=ftell(file);CHECK(size>0 && !fseek(file,0,SEEK_SET));
  UINT8 *data=malloc((size_t)size);CHECK(data);
  CHECK(fread(data,1,(size_t)size,file)==(size_t)size && fgetc(file)==EOF);fclose(file);
  *length=(size_t)size;return data;
}
static VOID load_fixture_assets(const char *prom_path,const char *assets_path) {
  static const char *names[]={"booter_load_ga102.bin","v67-ga102-610.43.03.bin",
    "gsp_ga10x.bin","gsp_boot_ga102.bin","gsp_boot_desc_ga102.bin"};
  static const char *full_names[]={"gpu-unlock/610.43.03/booter_load_ga102.bin",
    "gpu-unlock/610.43.03/v67-ga102-610.43.03.bin","gpu-unlock/610.43.03/gsp_ga10x.bin",
    "gpu-unlock/610.43.03/gsp_boot_ga102.bin","gpu-unlock/610.43.03/gsp_boot_desc_ga102.bin"};
  size_t length;UINT8 *rom=read_file(prom_path,&length);CHECK(length==sizeof(prom));memcpy(prom,rom,length);free(rom);
  for(UINTN i=0;i<5;i++) {
    char path[4096];int count=snprintf(path,sizeof(path),"%s/%s",assets_path,names[i]);
    CHECK(count>0 && (size_t)count<sizeof(path));GuAssets[i].name=full_names[i];
    GuAssets[i].firmware.data=read_file(path,&GuAssets[i].firmware.size);
  }
}
int main(int argc,char **argv) {
  setvbuf(stdout,NULL,_IONBF,0);CHECK(argc==3 || argc==4);load_fixture_assets(argv[1],argv[2]);
  if(argc==4) {
    FILE *capture=fopen(argv[3],"r");CHECK(capture);char line[512];unsigned count=0,o,a,b,c,d;
    while(fgets(line,sizeof(line),capture))if(sscanf(line,"config[%x]=%x %x %x %x",&o,&a,&b,&c,&d)==5) {
      CHECK(o==count*4 && count<=60);core_captured_config[count++]=a;
      core_captured_config[count++]=b;core_captured_config[count++]=c;core_captured_config[count++]=d;
    }
    fclose(capture);CHECK(count==64);core_use_capture=TRUE;
  }
#ifdef GU_ENTRY_LATCHED
  for(unsigned permutation=0;permutation<4;permutation++) {
    test_entry_ss0=permutation&1?0x03000062U:0x03100062U;test_entry_ss1=6+(permutation>>1);
#endif
  for(int mode=SUCCESS;mode<SCENARIO_COUNT;mode++) {
    setup((enum scenario)mode);UINT64 mask=0xffffffffffffULL;
    struct pci_dev device={.dev={.dma_mask=&mask,.coherent_dma_mask=0xfffffffffULL},.pm_cap=Gu.pm_cap,.refs=1};
    struct gpu_unlock_610_shared shared={0};
    int result=gpu_unlock_610_load_shared(&device,&shared);CHECK(!result);
    result=gpu_unlock_610_unlock(&device,&shared);gpu_unlock_610_release_shared(&shared);
    BOOLEAN safe=GuSafeToReturn();
    if(mode==SUCCESS || mode==FWSEC_RUNNING || mode==PIO_UNAVAILABLE) {
      CHECK(!result && safe && Gu.started && Gu.quiesced && !Gu.fault && !Gu.retained);
      CHECK(fake.gsp_starts==1 && fake.sec2_starts==1 && fake.flrs==1 && fake.radix_bytes==0x5053000);
      CHECK(!fake.live_maps && !fake.live_buffers && !Gu.allocations && !fake.pool_live);
      CHECK(fake.unmaps==fake.maps && fake.frees==fake.buffer_count);
      CHECK(registers[SS0/4]==0x88888888);
#ifdef GU_ENTRY_LATCHED_SS1_PRESERVE
      CHECK(registers[SS1/4]==test_entry_ss1);
#else
      CHECK(registers[SS1/4]==8);
#endif
      CHECK(registers[0x110100/4]&0x10);CHECK(!(registers[0x1fa828/4]>>4));
      CHECK(mask==0xffffffffffffULL && device.dev.coherent_dma_mask==0xfffffffffULL && device.refs==1);
      if(mode==FWSEC_RUNNING)CHECK(strstr(fake.log,"observed_nonhalt=1") && fake.gsp_poll_reads>=3);
    } else if(mode==MAP_FAIL || mode==MAP_PARTIAL || mode==BME_READBACK_FAIL
#ifdef GU_ENTRY_LATCHED
      || mode==CORE_ENTRY_SS0 || mode==CORE_ENTRY_SS1 || mode==BEFORE_FW_SS0 || mode==BEFORE_FW_SS1
#endif
    ) {
      CHECK(result && !Gu.started && safe && !Gu.retained);
      CHECK(!fake.gsp_starts && !fake.sec2_starts && !fake.flrs && !fake.mmio_writes);
      CHECK(!fake.live_maps && !fake.live_buffers && !Gu.allocations && !fake.pool_live);
      if(mode==BME_READBACK_FAIL)CHECK(fake.bme_read_failed && Gu.fault && !(config[4]&4));
      else CHECK(!fake.pci_writes);
    } else if(mode==UNMAP_FAIL) {
      CHECK(fake.unmap_failed && !safe && Gu.retained && Gu.quiesced);
      CHECK(fake.live_maps==fake.maps && fake.live_buffers==fake.buffer_count && Gu.allocations);
      CHECK(fake.unmaps==1 && !fake.frees && fake.pool_live);
      CHECK(fake.gsp_starts==1 && fake.sec2_starts==1 && fake.flrs==1);
    } else {
      CHECK(result && !safe && Gu.started && !Gu.quiesced && Gu.retained);
      CHECK(fake.live_maps==fake.maps && fake.live_buffers==fake.buffer_count && Gu.allocations);
      CHECK(!fake.unmaps && !fake.frees);
      if(mode==FWSEC_TIMEOUT)CHECK(fake.gsp_starts==1 && !fake.sec2_starts && !fake.flrs && fake.elapsed>=5000000);
      if(mode==MMIO_WRITE_FAIL || mode==FLUSH_FAIL)CHECK(!fake.gsp_starts && !fake.sec2_starts && !fake.flrs);
      if(mode==POST_FLR_TIMEOUT || mode==FLR_WRITE_FAIL || mode==RESTORE_FAIL)CHECK(fake.flrs==1 && fake.delay_since_flr>=100000);
      if(mode==DISPLAY_CHANGED)CHECK(fake.gsp_starts==1 && fake.sec2_starts==1 && !fake.flrs);
      if(mode==FWSEC_NO_WPR || mode==PIO_UNAVAILABLE_NO_WPR) {
        CHECK(fake.gsp_starts==1 && !fake.sec2_starts && !fake.flrs);
#ifdef GU_EXPERIMENTAL_3090
#ifdef GU_ENTRY_LATCHED
        CHECK(registers[SS0/4]==test_entry_ss0 && registers[SS1/4]==test_entry_ss1);
#else
        CHECK(registers[SS0/4]==GU_STOCK_PROFILE_SS0 && registers[SS1/4]==6);
#endif
#else
        CHECK(registers[SS0/4]==0x46254040 && registers[SS1/4]==3);
#endif
        CHECK(strstr(fake.log,"FWSEC halt observation:") && strstr(fake.log,"observed_nonhalt=0"));
        CHECK(strstr(fake.log,"FWSEC snapshot: phase=post-halt") && strstr(fake.log,"value=0x12345678"));
      }
      if(mode==DIAG_READ_FAIL)CHECK(!fake.gsp_starts && !fake.sec2_starts && !fake.flrs && Gu.fault);
      if(mode==POST_DIAG_READ_FAIL)CHECK(Gu.fault && fake.gsp_starts==1 && !fake.sec2_starts && !fake.flrs);
      if(mode==FWSEC_ERROR_CODE) {
        CHECK(result==-EIO && fake.gsp_starts==1 && !fake.sec2_starts && !fake.flrs);
        CHECK(strstr(fake.log,"FWSEC result: scratch=0x10000"));
      }
    }
    /* App-level containment must work even when the normal I/O path is fault-latched. */
    CHECK(!EFI_ERROR(GuClearMaster()) && !(config[4]&4));
    if(!safe)CHECK(!GuSafeToReturn());
    CHECK(fake.gsp_starts<=1 && fake.sec2_starts<=1 && fake.flrs<=1);
    CHECK(!fake.pio_reads && !fake.pio_writes && !fake.dmem_reads);
#ifdef GU_ENTRY_LATCHED_SS1_PRESERVE
    for(UINTN i=0;i<fake.trace_count;i++)CHECK(fake.write_trace[i][0] || fake.write_trace[i][2]!=SS1);
#else
    if(mode==SUCCESS || mode==FWSEC_RUNNING || mode==PIO_UNAVAILABLE) {
      UINTN ss1_writes=0;
      for(UINTN i=0;i<fake.trace_count;i++)if(!fake.write_trace[i][0] && fake.write_trace[i][2]==SS1) {
        CHECK(fake.write_trace[i][3]==8);ss1_writes++;
      }
      CHECK(ss1_writes==1);
    }
#endif
#ifdef GU_ENTRY_LATCHED
    if(mode==SELECTOR_ENTRY_SS0 || mode==SELECTOR_ENTRY_SS1) {
      CHECK(registers[SS0/4]!=0x88888888 && !fake.flrs);
    }
    if(mode==PRE_FLR_SS1 || mode==LATE_FLR_SS1)CHECK(!fake.flrs);
    if(mode==SELECTOR_LOST)CHECK(fake.flrs==1 && result && !Gu.quiesced);
#endif
    printf("EFI integration %s: result=%d safe=%u retained=%u live_maps=%zu starts=%zu/%zu flr=%zu PASS\n",
      case_names[mode],result,safe,Gu.retained,fake.live_maps,fake.gsp_starts,fake.sec2_starts,fake.flrs);
    dispose_host_fixture();
  }
#ifdef GU_ENTRY_LATCHED
  }
#endif
  for(UINTN i=0;i<5;i++)free((VOID *)GuAssets[i].firmware.data);
  printf("%u real core + compat + UEFI port scenarios passed; all PCI/MMIO/DMA/firmware execution simulated.\n",
#ifdef GU_ENTRY_LATCHED
    SCENARIO_COUNT*4
#else
    SCENARIO_COUNT
#endif
  );
  return 0;
}
