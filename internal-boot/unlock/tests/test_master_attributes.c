/* ASRock port changes: 2026-10-27; publication notice: 2026-10-28. GPL-2.0-only; see CHANGES.md. */
/* Modified 2026-10-19/20 for Windows GPU UNLOCK; changes documented in CHANGES.md. */ /* SPDX-License-Identifier: GPL-2.0-only */
/* Hardware Command and firmware attribute cache are deliberately independent. */
#define main transaction_fixture_main
#define GuLog FixtureLog
#define GuLogText FixtureLogText
#include "test_efi_transaction.c"
#undef main
#undef GuLog
#undef GuLogText

enum master_case { NORMAL, NO_API, SUPPORT_ERROR, UNSUPPORTED, GET_ERROR, CACHE_ALREADY_ON,
 ENABLE_ERROR, ENABLE_ERROR_AFTER_WRITE, ENABLE_NOOP, ENABLE_HW_ONLY, ENABLE_CACHE_ONLY,
 ENABLE_EXTRA_ATTRIBUTE, ENABLE_GET_ERROR, ENABLE_READ_ERROR, ENABLE_LOG_ERROR, ENABLE_POST_LOG_ERROR,
 DISABLE_ERROR, DISABLE_ERROR_AFTER_CLEAR, DISABLE_NOOP, DISABLE_HW_ONLY, DISABLE_CACHE_ONLY,
 DISABLE_GET_ERROR, DISABLE_RAW_ERROR, DISABLE_EXTRA_ATTRIBUTE, DISABLE_EXTRA_HARDWARE, MASTER_CASES };
static const char *names[]={"normal","no-api","support-error","unsupported","get-error","cache-already-on",
 "enable-error","enable-error-after-write","enable-noop","enable-hardware-only","enable-cache-only",
 "enable-extra-attribute","enable-get-error","enable-command-error","enable-log-error","enable-post-log-error",
 "disable-error","disable-error-after-clear","disable-noop","disable-hardware-only","disable-cache-only",
 "disable-get-error","disable-raw-error","disable-extra-attribute","disable-extra-hardware-bit"};
static enum master_case mode;
static UINTN attributeGets,enables,disables;
static BOOLEAN clearing;
static EFI_PCI_IO_PROTOCOL replacement;
VOID GuLog(const char *format,...) {
  CHECK(!clearing); /* GuClearMaster can run at TPL_NOTIFY: no file logging. */
  va_list args;size_t used=strlen(fake.log);
  va_start(args,format);vsnprintf(fake.log+used,sizeof(fake.log)-used,format,args);va_end(args);
  if(mode==ENABLE_LOG_ERROR && strstr(format,"Bus Master enable intent"))Gu.fault=TRUE;
  if(mode==ENABLE_POST_LOG_ERROR && strstr(format,"Bus Master enabled:"))Gu.fault=TRUE;
}
VOID GuLogText(const char *s){GuLog("%s",s);}
static EFI_STATUS EFIAPI attr(EFI_PCI_IO_PROTOCOL *p,
 EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION op,UINT64 mask,UINT64 *out) {
  CHECK(p==&pci);
  if(op==EfiPciIoAttributeOperationSupported) {
    CHECK(out && !mask);*out=EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY|
      (mode==UNSUPPORTED?0:EFI_PCI_IO_ATTRIBUTE_BUS_MASTER);
    return mode==SUPPORT_ERROR?EFI_DEVICE_ERROR:EFI_SUCCESS;
  }
  if(op==EfiPciIoAttributeOperationGet) {
    CHECK(out && !mask);attributeGets++;*out=fake.pci_attributes;
    return ((mode==GET_ERROR && attributeGets==1) || (mode==ENABLE_GET_ERROR && attributeGets==2) ||
      (mode==DISABLE_GET_ERROR && disables))?EFI_DEVICE_ERROR:EFI_SUCCESS;
  }
  CHECK(!out && mask==EFI_PCI_IO_ATTRIBUTE_BUS_MASTER);
  if(op==EfiPciIoAttributeOperationEnable) {
    enables++;
    if(mode==ENABLE_ERROR)return EFI_DEVICE_ERROR;
    if(mode==ENABLE_NOOP)return EFI_SUCCESS;
    if(mode!=ENABLE_CACHE_ONLY)config[4]|=4;
    if(mode!=ENABLE_HW_ONLY)fake.pci_attributes|=mask;
    if(mode==ENABLE_EXTRA_ATTRIBUTE)fake.pci_attributes|=EFI_PCI_IO_ATTRIBUTE_DUAL_ADDRESS_CYCLE;
    if(mode==ENABLE_READ_ERROR)fake.mode=BME_READBACK_FAIL;
    return mode==ENABLE_ERROR_AFTER_WRITE?EFI_DEVICE_ERROR:EFI_SUCCESS;
  }
  CHECK(op==EfiPciIoAttributeOperationDisable);disables++;
  if(mode==DISABLE_RAW_ERROR){fake.write_failed=TRUE;return EFI_DEVICE_ERROR;}
  if(mode==DISABLE_ERROR)return EFI_DEVICE_ERROR;
  if(mode==DISABLE_NOOP)return EFI_SUCCESS;
  if(mode!=DISABLE_CACHE_ONLY)config[4]&=(UINT8)~4U;
  if(mode!=DISABLE_HW_ONLY)fake.pci_attributes&=~mask;
  if(mode==DISABLE_EXTRA_ATTRIBUTE)fake.pci_attributes^=EFI_PCI_IO_ATTRIBUTE_MEMORY;
  if(mode==DISABLE_EXTRA_HARDWARE)config[4]&=(UINT8)~2U;
  return mode==DISABLE_ERROR_AFTER_CLEAR?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI raw_write(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,
 UINT32 offset,UINTN count,VOID *value) {
  if(mode==DISABLE_RAW_ERROR && fake.write_failed)return EFI_DEVICE_ERROR;
  return write_config(p,w,offset,count,value);
}
static EFI_STATUS EFIAPI replacement_read(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,
 UINT32 offset,UINTN count,VOID *value) {
  CHECK(p==&replacement);return read_config(&pci,w,offset,count,value);
}
static EFI_STATUS EFIAPI replacement_write(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,
 UINT32 offset,UINTN count,VOID *value) {
  CHECK(p==&replacement);return write_config(&pci,w,offset,count,value);
}
int main(void) {
  setvbuf(stdout,NULL,_IONBF,0);
  for(mode=NORMAL;mode<MASTER_CASES;mode++) {
    printf("BME attributes %s: ",names[mode]);setup(SUCCESS);attributeGets=enables=disables=0;
    pci.Attributes=mode==NO_API?NULL:attr;pci.Pci.Write=raw_write;
    if(mode==CACHE_ALREADY_ON)fake.pci_attributes|=EFI_PCI_IO_ATTRIBUTE_BUS_MASTER;
    struct pci_dev dev={0};pci_set_master(&dev);
    BOOLEAN enabled=mode==NORMAL || mode>=DISABLE_ERROR;
    if(enabled)CHECK(!Gu.fault && enables==1 && (config[4]&4) &&
      (fake.pci_attributes&EFI_PCI_IO_ATTRIBUTE_BUS_MASTER));
    else CHECK(Gu.fault && !fake.mmio_writes);
    if(enables)CHECK(!GuSafeToReturn());
    if(mode==NO_API || mode==SUPPORT_ERROR || mode==UNSUPPORTED || mode==GET_ERROR ||
       mode==CACHE_ALREADY_ON || mode==ENABLE_LOG_ERROR)CHECK(!enables && !(config[4]&4));
    /* Containment must still run despite the failure latch. */
    if(mode>=DISABLE_ERROR)Gu.started=TRUE;
    clearing=TRUE;EFI_STATUS status=GuClearMaster();clearing=FALSE;
    BOOLEAN cleanup_error=mode==DISABLE_ERROR || mode==DISABLE_ERROR_AFTER_CLEAR || mode==DISABLE_NOOP ||
      mode==DISABLE_HW_ONLY || mode==DISABLE_GET_ERROR || mode==DISABLE_RAW_ERROR ||
      mode==DISABLE_EXTRA_ATTRIBUTE || mode==DISABLE_EXTRA_HARDWARE || mode==ENABLE_EXTRA_ATTRIBUTE;
    CHECK(EFI_ERROR(status)==cleanup_error);
    CHECK((config[4]&4)==(mode==DISABLE_RAW_ERROR?4:0));
    if(cleanup_error)CHECK(Gu.fault && Gu.retained && !GuSafeToReturn());
    if(mode>=DISABLE_ERROR)CHECK(Gu.started && !Gu.quiesced && !GuSafeToReturn());
    if(enabled && !cleanup_error)CHECK(!(fake.pci_attributes&EFI_PCI_IO_ATTRIBUTE_BUS_MASTER));
    if(cleanup_error) {
      /* Repeated containment cannot erase a previous failed cleanup. */
      enum master_case original=mode;mode=NORMAL;
      clearing=TRUE;(void)GuClearMaster();clearing=FALSE;
      CHECK(Gu.fault && Gu.retained && !GuSafeToReturn());
      mode=original;
    }
    CHECK(!fake.mmio_writes && !fake.flrs);puts("PASS");
  }
  mode=DISABLE_EXTRA_HARDWARE;setup(SUCCESS);attributeGets=enables=disables=0;pci.Attributes=attr;
  CHECK(!EFI_ERROR(GuEnableMaster()) && !Gu.started);
  clearing=TRUE;CHECK(EFI_ERROR(GuClearMaster()));clearing=FALSE;
  CHECK(Gu.fault && Gu.retained && !GuSafeToReturn() && !(config[4]&4));
  CHECK((config[4]&3)==1 && Gu.master_attributes_pending && !Gu.started);
  CHECK(!EFI_ERROR(Gu.master_attribute_status) && EFI_ERROR(Gu.master_hardware_status));
  puts("BME attributes nonstarted extra hardware bit retains: PASS");
  mode=NORMAL;setup(SUCCESS);attributeGets=enables=disables=0;pci.Attributes=attr;
  VOID *cpu=NULL;UINT64 dma=0;CHECK(!EFI_ERROR(GuDmaAllocate(4096,&cpu,&dma)));
  CHECK(!EFI_ERROR(GuEnableMaster()) && !Gu.started && !GuSafeToReturn());
  CHECK(GuDmaFree(cpu,dma)==EFI_ACCESS_DENIED && !fake.unmaps && !fake.frees && Gu.retained);
  clearing=TRUE;CHECK(!EFI_ERROR(GuClearMaster()));clearing=FALSE;
  CHECK(!GuSafeToReturn() && GuDmaFree(cpu,dma)==EFI_ACCESS_DENIED);
  dispose_host_fixture();puts("BME attributes pending-DMA-free retention: PASS");
  setup(SUCCESS);attributeGets=enables=disables=0;pci.Attributes=attr;
  CHECK(!EFI_ERROR(GuEnableMaster()));
  replacement=pci;Gu.pci=&replacement;
  /* The fixture accepts only its original interface. A replacement must never
     receive the original interface's saved Attributes baseline. */
  replacement.Pci.Read=replacement_read;replacement.Pci.Write=replacement_write;
  clearing=TRUE;CHECK(EFI_ERROR(GuClearMaster()));clearing=FALSE;
  CHECK(!disables && Gu.retained && !GuSafeToReturn() && !(config[4]&4));
  CHECK(Gu.master_attributes_pending && Gu.master_attribute_status==EFI_COMPROMISED_DATA);
  puts("BME attributes replacement protocol raw containment only: PASS");
  setup(SUCCESS);attributeGets=enables=disables=0;pci.Attributes=attr;
  CHECK(!EFI_ERROR(GuEnableMaster()));
  /* Identity change is a separate rejection before any Attributes call. */
  *(UINT32 *)config=0x220610de; /* Old RTX3080 must now be refused. */
  clearing=TRUE;CHECK(EFI_ERROR(GuClearMaster()));clearing=FALSE;
  CHECK(!disables && Gu.retained && !GuSafeToReturn() && (config[4]&4));
  puts("BME attributes changed identity containment refusal: PASS");
  printf("%u BME protocol/cache/containment scenarios passed; EFI services simulated.\n",MASTER_CASES+4);
  return 0;
}
