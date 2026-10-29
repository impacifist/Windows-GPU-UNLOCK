/* GPL-2.0-only. Host tool: run the production VBIOS parser on a PROM image and
 * print where FWSEC-FRTS lives. Reads one local file; no GPU access. */
/* EDK2's Base.h defines NULL unconditionally, so it must precede the CRT headers. */
#include "../../internal-boot/unlock/vendor/vbios_610.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/* Parser diagnostics go to stderr; stdout carries only the JSON result. */
VOID GuLog(const char *Format, ...) {
  va_list args;
  va_start(args, Format);
  vfprintf(stderr, Format, args);
  va_end(args);
}

int main(int argc, char **argv) {
  if (argc != 2) { fprintf(stderr, "usage: fwsec-locate PROM.BIN\n"); return 2; }
  FILE *f = fopen(argv[1], "rb");
  if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
  static u8 rom[1u << 20];
  size_t n = fread(rom, 1, sizeof(rom), f);
  int extra = fgetc(f) != EOF;
  fclose(f);
  if (n != sizeof(rom) || extra) { fprintf(stderr, "PROM must be exactly 1 MiB\n"); return 2; }
  struct gpu_unlock_fwsec fw;
  int ret = gpu_unlock_vbios_fwsec(rom, n, &fw);
  if (ret) { fprintf(stderr, "FWSEC-FRTS not found (%d)\n", ret); return 1; }
  printf("{\"image_off\":%u,\"image_size\":%u,\"dmem_size\":%u,\"data_off\":%u,\"pkc_off\":%u,"
         "\"ucode_id\":%u,\"engine_mask\":%u,\"sig_count\":%u,\"sig_versions\":%u,\"sig_bytes\":%u,"
         "\"imem_pa\":%u,\"imem_size\":%u,\"imem_va\":%u,\"dmem_pa\":%u,\"interface_off\":%u,\"sig_off\":%u}\n",
         fw.image_off, fw.image_size, fw.dmem_size, fw.data_off, fw.pkc_off,
         fw.ucode_id, fw.engine_mask, fw.sig_count, fw.sig_versions, fw.sig_bytes,
         fw.imem_pa, fw.imem_size, fw.imem_va, fw.dmem_pa, fw.interface_off, fw.sig_off);
  return 0;
}
