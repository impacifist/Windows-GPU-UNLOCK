// Copyright (C) Windows GPU UNLOCK contributors. GPL-2.0-only.
/*
 * Reviewed single-machine profiles. Every write-enabled build compiles for
 * exactly one profile: all hardware identities, display topology and driver
 * fingerprints below are immutable, build-time selected and never read from
 * a user-editable file. Select with -DGU_PROFILE_B650M_3090FE or
 * -DGU_PROFILE_B660M_3070TI; the default is the originally verified machine.
 * Adding a profile here is never sufficient for support: each entry needs
 * its own hardware evidence (see docs/PORTABILITY.en.md stage 4).
 */
#pragma once

#if defined(GU_PROFILE_B650M_3090FE) && defined(GU_PROFILE_B660M_3070TI)
#error Select exactly one machine profile
#endif
#if !defined(GU_PROFILE_B650M_3090FE) && !defined(GU_PROFILE_B660M_3070TI)
#define GU_PROFILE_B650M_3090FE 1
#endif

/* 1 = the AMI combo splitter owns the console on top of the target GPU
 * (aggregate GOP + text-out driver chain; verified on AMI boards without a
 * second display GPU). 0 = nvgop owns the console GOP on the target directly
 * (original verified B650M headless topology). */
#if defined(GU_PROFILE_B660M_3070TI)
#define GU_CONSOLE_AGGREGATE 1
#else
#define GU_CONSOLE_AGGREGATE 0
#endif

#if defined(GU_PROFILE_B660M_3070TI)

/* ASUSTeK TUF GAMING B660M-E D4 BIOS 4003, i7-12700F (no iGPU),
 * MSI RTX 3070 Ti (GA104): hardware-verified headless port. */
#define GU_PROFILE_NAME "ASUS-TUF-GAMING-B660M-E-D4-4003-RTX3070TI"
#define GU_BOARD_ID 0x248210deU
#define GU_BOARD_SUBSYSTEM 0x50521462U
#define GU_BAR0_BASE 0x82000000ULL
#define GU_BAR0_END 0x82ffffffULL
#define GU_DEVICE_MEMORY_SIZE 0x2000U
#define GU_CHIP_ID 0xb74000a1U
#define GU_PROM_SHA256 "f429aa1d5f0f18d11ed9a19503683a674fe3541ae77e959df8e732652c2b4df0"
#define GU_ROOT_PORT_DEVICE 1U
#define GU_ROOT_PORT_FUNCTION 0U
/* 36-byte display device path: ACPI root, PciRoot(0), Pci(1,0), endpoint. */
#define GU_RTX_PATH_BYTES \
  2,1,12,0,0xd0,0x41,3,10,0,0,0,0,1,1,6,0,0,1,1,1,6,0,0,0,2,3,8,0,32,3,1,128,127,255,4,0
/* AMI console splitter text-out driver (BIOS 4003 image fingerprint). */
#define GU_CONSOLE_TEXT_SIZE 0x6a40U
#define GU_CONSOLE_TEXT_STOP 0x104cU
#define GU_CONSOLE_TEXT_VERSION 0x10U
/* AMI GOP combo splitter C1 GOP (BIOS 4003 image fingerprint). */
#define GU_CONSOLE_GFX_SIZE 0x3bc0U
#define GU_CONSOLE_GFX_STOP 0xb2cU
#define GU_CONSOLE_GFX_VERSION 0x10U
/* AMI GOP combo aggregate driver present only in this topology. */
#define GU_COMBO_SIZE 0x2160U
#define GU_COMBO_STOP 0x88cU
#define GU_COMBO_VERSION 0x11U
/* AMI EDID/mouse runtime driver (BIOS 4003 image fingerprint). */
#define GU_EDID_IMAGE_SIZE 0x9000U
#define GU_EDID_STOP 0x14d4U
#define GU_EDID_TEXT_BYTES 0x37daU
#define GU_EDID_TEXT_SHA256 "a015d52f55fa6441414458e2b699ba2edcb53f03b001ac6367e45bcd908c418e"
/* nvgop-ga1xx driver shipped for GA104 (binding 0x60010). */
#define GU_NVGOP_VERSION 0x60010U
#define GU_NVGOP_IMAGE_SIZE 0x26ae0U
#define GU_NVGOP_STOP 0x678U
#define GU_NVGOP_TEXT_BYTES 0x20d33U
#define GU_NVGOP_TEXT_SHA256 "615b37a708b1894e18d7fa60cfcdacaa4d8b598863a10b8103d231a595847b2b"

#else

/* ASUSTeK TUF GAMING B650M-PLUS WIFI BIOS 3057, Ryzen (Raphael iGPU
 * present but not the console in the headless profile),
 * RTX 3090 FE (GA102): originally verified machine. */
#define GU_PROFILE_NAME "ASUS-TUF-B650M-PLUS-WIFI-3057-RTX3090-FE"
#define GU_BOARD_ID 0x220410deU
#define GU_BOARD_SUBSYSTEM 0x145410deU
#define GU_BAR0_BASE 0xf5000000ULL
#define GU_BAR0_END 0xf5ffffffULL
#define GU_DEVICE_MEMORY_SIZE 0x6000U
#define GU_CHIP_ID 0xb72000a1U
#define GU_PROM_SHA256 "8ea297373e9e5d3600ff41ad9fae88f2d14c98c017d100669f7333981da673b4"
#define GU_ROOT_PORT_DEVICE 1U
#define GU_ROOT_PORT_FUNCTION 1U
#define GU_RTX_PATH_BYTES \
  2,1,12,0,0xd0,0x41,3,10,0,0,0,0,1,1,6,0,1,1,1,1,6,0,0,0,2,3,8,0,112,3,1,128,127,255,4,0
#define GU_CONSOLE_TEXT_SIZE 0x7820U
#define GU_CONSOLE_TEXT_STOP 0x1494U
#define GU_CONSOLE_TEXT_VERSION 0x10U
#define GU_CONSOLE_GFX_SIZE 0x48a0U
#define GU_CONSOLE_GFX_STOP 0xf04U
#define GU_CONSOLE_GFX_VERSION 0x10U
#define GU_EDID_IMAGE_SIZE 0x9000U
#define GU_EDID_STOP 0x157cU
#define GU_EDID_TEXT_BYTES 0x38f8U
#define GU_EDID_TEXT_SHA256 "53ef1716fa7a8cf866c986e85dd8412185a9fd58dd89e90202f97cb0997c96a6"
#define GU_NVGOP_VERSION 0x60008U
#define GU_NVGOP_IMAGE_SIZE 0x2b6e0U
#define GU_NVGOP_STOP 0x698U
#define GU_NVGOP_TEXT_BYTES 0x23d6aU
#define GU_NVGOP_TEXT_SHA256 "6aad26e6909c9791dd0caba92d6658e9dfd8009aee80676002bf24ce2ad24d08"

#endif
