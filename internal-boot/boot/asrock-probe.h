/* ASRock port changes: 2026-10-27; publication notice: 2026-10-28. GPL-2.0-only; see CHANGES.md. */
/* GPL-2.0-only. Experimental read-only capture identity, not an unlock profile. */
#pragma once
#if !defined(GU_DIAGNOSTIC_ONLY) || !defined(GU_DIAGNOSTIC_PROBE) || !defined(GU_PROBE_GENERIC) || !defined(GU_PROBE_DUMP_PROM)
#error ASRock capture requires the generic read-only diagnostic with PROM preservation
#endif
#if defined(GU_FULL_UNLOCK) || defined(GU_EXPERIMENTAL_3090) || defined(GU_UEFI_PORT) || defined(GU_INTERNAL_BOOT) || defined(GU_CONSOLE_PREPARE) || defined(GU_AUTOMATIC_BOOT) || defined(GU_HEADLESS_RESEARCH)
#error ASRock capture must never be combined with unlock or driver release
#endif
#if !defined(GU_PROBE_BUS) || GU_PROBE_BUS != 7
#error ASRock capture requires the observed PCI bus 7
#endif
static BOOLEAN AsrockProbeIdentity(const UINT32 *cfg) {
  /* DIAG00 from this machine: device, subsystem, revision and raw BAR0.
   * Resource size, MEM/BME state and post-callback identity remain checked
   * by ProbeEnvelope/ProbeUnchanged before any GPU register read. */
  return cfg[0]==0x250310deU && cfg[11]==0x40741458U &&
         cfg[2]==0x030000a1U && cfg[4]==0xfb000000U;
}
