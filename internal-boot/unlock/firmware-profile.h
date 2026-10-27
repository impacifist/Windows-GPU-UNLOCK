/* GPL-2.0-only. Exact captured console/driver fingerprint templates.
 * Select independently of GPU family: GA102 machines use different nvgop and
 * console firmware. LOCAL always keeps its own captured values, never defaults.
 */
#pragma once
#if defined(GU_PROFILE_LOCAL) || defined(GU_PROFILE_PROBE_H610M_3060) || defined(GU_PROFILE_PROBE_B550_3060)
#error Captured local and identity-only probe profiles must not inherit firmware templates
#endif
#if defined(GU_FIRMWARE_AMI_B650M_3057) || defined(GU_FIRMWARE_AMI_B660M_4003) || defined(GU_FIRMWARE_AMI_X870_F13D) || defined(GU_FIRMWARE_AMI_H610M_FC) || defined(GU_FIRMWARE_AMI_B550_F13)
#error Firmware template selectors must come from firmware-profile.h
#endif
#define GU_FIRMWARE_AMI_B650M_3057 1U
#define GU_FIRMWARE_AMI_B660M_4003 2U
#define GU_FIRMWARE_AMI_X870_F13D 3U
#define GU_FIRMWARE_AMI_H610M_FC 4U
#define GU_FIRMWARE_AMI_B550_F13 5U
#ifndef GU_FIRMWARE_PROFILE
#error Select an exact reviewed firmware template
#endif

#if GU_FIRMWARE_PROFILE == GU_FIRMWARE_AMI_B650M_3057
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

#elif GU_FIRMWARE_PROFILE == GU_FIRMWARE_AMI_B660M_4003
/* Also captured byte-identically on B660-PLUS BIOS 3010. Sharing these
 * fingerprints does not share its display paths or child-count policy. */
#define GU_CONSOLE_TEXT_SIZE 0x6a40U
#define GU_CONSOLE_TEXT_STOP 0x104cU
#define GU_CONSOLE_TEXT_VERSION 0x10U
#define GU_CONSOLE_GFX_SIZE 0x3bc0U
#define GU_CONSOLE_GFX_STOP 0xb2cU
#define GU_CONSOLE_GFX_VERSION 0x10U
#define GU_COMBO_SIZE 0x2160U
#define GU_COMBO_STOP 0x88cU
#define GU_COMBO_VERSION 0x11U
#define GU_EDID_IMAGE_SIZE 0x9000U
#define GU_EDID_STOP 0x14d4U
#define GU_EDID_TEXT_BYTES 0x37daU
#define GU_EDID_TEXT_SHA256 "a015d52f55fa6441414458e2b699ba2edcb53f03b001ac6367e45bcd908c418e"

#elif GU_FIRMWARE_PROFILE == GU_FIRMWARE_AMI_X870_F13D
#define GU_CONSOLE_TEXT_SIZE 0xb000U
#define GU_CONSOLE_TEXT_STOP 0x20e0U
#define GU_CONSOLE_TEXT_VERSION 0x10U
#define GU_CONSOLE_GFX_SIZE 0x9000U
#define GU_CONSOLE_GFX_STOP 0x1cc0U
#define GU_CONSOLE_GFX_VERSION 0x10U
#define GU_EDID_IMAGE_SIZE 0x9000U
#define GU_EDID_STOP 0x14d0U
#define GU_EDID_TEXT_BYTES 0x37beU
#define GU_EDID_TEXT_SHA256 "9a08c48e3a09965d1dda48b05a400311d71160d727cc9250630976cf4057eca9"

#elif GU_FIRMWARE_PROFILE == GU_FIRMWARE_AMI_H610M_FC
#define GU_CONSOLE_TEXT_SIZE 0x5de0U
#define GU_CONSOLE_TEXT_STOP 0xf90U
#define GU_CONSOLE_TEXT_VERSION 0x10U
#define GU_CONSOLE_GFX_SIZE 0x3c20U
#define GU_CONSOLE_GFX_STOP 0xb60U
#define GU_CONSOLE_GFX_VERSION 0x10U
#define GU_EDID_IMAGE_SIZE 0x9000U
#define GU_EDID_STOP 0x14d4U
#define GU_EDID_TEXT_BYTES 0x3bb2U
#define GU_EDID_TEXT_SHA256 "49fe68461765963d0b63144b0a7863f3b2db5926f1c988cbacc1e85ebe2b96f4"
#elif GU_FIRMWARE_PROFILE == GU_FIRMWARE_AMI_B550_F13
/* Gigabyte B550 AORUS ELITE V2 BIOS F13: captured identically in three cold boots.
 * Its console text driver has three bindings from one image; the pinned stop is the
 * binding that holds the RTX GOP child BY_DRIVER. nvgop 0x6000a is unique to this
 * template and has never released on reviewed hardware. */
#define GU_CONSOLE_TEXT_SIZE 0x5b80U
#define GU_CONSOLE_TEXT_STOP 0xe6cU
#define GU_CONSOLE_TEXT_VERSION 0x10U
#define GU_CONSOLE_GFX_SIZE 0x3b20U
#define GU_CONSOLE_GFX_STOP 0xa40U
#define GU_CONSOLE_GFX_VERSION 0x10U
#define GU_EDID_IMAGE_SIZE 0x9000U
#define GU_EDID_STOP 0x149cU
#define GU_EDID_TEXT_BYTES 0x3526U
#define GU_EDID_TEXT_SHA256 "34c093b37d6c469aa6e24636de39f9568a0f6fe21368ad30c17d3d5f03b16d1b"
#define GU_NVGOP_VERSION 0x6000aU
#define GU_NVGOP_IMAGE_SIZE 0x2be40U
#define GU_NVGOP_STOP 0x698U
#define GU_NVGOP_TEXT_BYTES 0x24426U
#define GU_NVGOP_TEXT_SHA256 "8924ba488419e5d82b524d10e8dd63bfe886f077026fae1a9cc03e25b920b276"
#else
#error No reviewed firmware template for this selection
#endif

#if GU_FIRMWARE_PROFILE != GU_FIRMWARE_AMI_B650M_3057 && GU_FIRMWARE_PROFILE != GU_FIRMWARE_AMI_B550_F13
/* These five fixed profiles pin the same nvgop code; LOCAL does not inherit it. */
#define GU_NVGOP_VERSION 0x60010U
#define GU_NVGOP_IMAGE_SIZE 0x26ae0U
#define GU_NVGOP_STOP 0x678U
#define GU_NVGOP_TEXT_BYTES 0x20d33U
#define GU_NVGOP_TEXT_SHA256 "615b37a708b1894e18d7fa60cfcdacaa4d8b598863a10b8103d231a595847b2b"
#endif
