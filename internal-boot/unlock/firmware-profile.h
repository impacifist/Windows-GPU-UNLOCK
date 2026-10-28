/* GPL-2.0-only. Exact captured console/driver fingerprint templates.
 * Select independently of GPU family: GA102 machines use different nvgop and
 * console firmware. LOCAL always keeps its own captured values, never defaults.
 */
#pragma once
#if defined(GU_PROFILE_LOCAL) || defined(GU_PROFILE_PROBE_H610M_3060)
#error Captured local and identity-only probe profiles must not inherit firmware templates
#endif
#if defined(GU_FIRMWARE_AMI_B650M_3057) || defined(GU_FIRMWARE_AMI_B660M_4003) || defined(GU_FIRMWARE_AMI_X870_F13D) || defined(GU_FIRMWARE_AMI_H610M_FC) || defined(GU_FIRMWARE_AMI_B760MK_1663)
#error Firmware template selectors must come from firmware-profile.h
#endif
#define GU_FIRMWARE_AMI_B650M_3057 1U
#define GU_FIRMWARE_AMI_B660M_4003 2U
#define GU_FIRMWARE_AMI_X870_F13D 3U
#define GU_FIRMWARE_AMI_H610M_FC 4U
#define GU_FIRMWARE_AMI_B760MK_1663 5U
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

#elif GU_FIRMWARE_PROFILE == GU_FIRMWARE_AMI_B760MK_1663
/* DRAFT, unreviewed. ASUS PRIME B760M-K BIOS 1663, three identical iGPU-off
 * captures. EDID and nvgop .text hashes match B660M_4003; console_gfx matches
 * its size, stop and version. Stop offsets and versions of the other two match.
 * combo (+0x20): .reloc starts at 0x2160, the B660M image size, so the same
 * code plus a relocation table is plausible but unproven.
 * console_text (+0xa0): .xdata already ends at 0x6a4c, past the whole B660M
 * image (0x6a40), so it is a DIFFERENT build; its Stop() behaviour is unproven.
 * Captured .text SHA-256 prefixes (no relocations in .text):
 * combo 3f4c4ea26023ddf0 (0x1990 bytes), console_text 721b3a8839de8581 (0x5620). */
#define GU_CONSOLE_TEXT_SIZE 0x6ae0U
#define GU_CONSOLE_TEXT_STOP 0x104cU
#define GU_CONSOLE_TEXT_VERSION 0x10U
#define GU_CONSOLE_GFX_SIZE 0x3bc0U
#define GU_CONSOLE_GFX_STOP 0xb2cU
#define GU_CONSOLE_GFX_VERSION 0x10U
#define GU_COMBO_SIZE 0x2180U
#define GU_COMBO_STOP 0x88cU
#define GU_COMBO_VERSION 0x11U
#define GU_EDID_IMAGE_SIZE 0x9000U
#define GU_EDID_STOP 0x14d4U
#define GU_EDID_TEXT_BYTES 0x37daU
#define GU_EDID_TEXT_SHA256 "a015d52f55fa6441414458e2b699ba2edcb53f03b001ac6367e45bcd908c418e"
#else
#error No reviewed firmware template for this selection
#endif

#if GU_FIRMWARE_PROFILE != GU_FIRMWARE_AMI_B650M_3057
/* These five fixed profiles pin the same nvgop code; LOCAL does not inherit it. */
#define GU_NVGOP_VERSION 0x60010U
#define GU_NVGOP_IMAGE_SIZE 0x26ae0U
#define GU_NVGOP_STOP 0x678U
#define GU_NVGOP_TEXT_BYTES 0x20d33U
#define GU_NVGOP_TEXT_SHA256 "615b37a708b1894e18d7fa60cfcdacaa4d8b598863a10b8103d231a595847b2b"
#endif
