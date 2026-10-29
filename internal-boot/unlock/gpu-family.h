/* GPL-2.0-only. Shared traits for the reviewed GA10x transaction implementation.
 * This catalog does not grant hardware support: the selected machine or captured
 * local profile still pins subsystem, PROM, VRAM, BAR, topology and driver code.
 * Do not infer console firmware or a register map from a marketing GPU name.
 */
#pragma once

#if defined(GU_GPU_FAMILY_GA102) || defined(GU_GPU_FAMILY_GA104) || defined(GU_GPU_FAMILY_GA106) || defined(GU_GPU_FAMILY_NAME) || defined(GU_FAMILY_CHIP_ID)
#error GPU family traits must come from gpu-family.h
#endif
#define GU_GPU_FAMILY_GA102 102U
#define GU_GPU_FAMILY_GA104 104U
#define GU_GPU_FAMILY_GA106 106U

/* Existing generated headers retain their captured literal GU_CHIP_ID. Derive
 * metadata from that exact revision, without changing generator admission. */
#if defined(GU_PROFILE_LOCAL) && !defined(GU_GPU_FAMILY)
#if GU_CHIP_ID == 0xb72000a1U
#define GU_GPU_FAMILY GU_GPU_FAMILY_GA102
#elif GU_CHIP_ID == 0xb74000a1U
#define GU_GPU_FAMILY GU_GPU_FAMILY_GA104
#elif GU_CHIP_ID == 0xb76000a1U
#define GU_GPU_FAMILY GU_GPU_FAMILY_GA106
#else
#error Captured chip has no reviewed GPU family implementation
#endif
#endif

#if !defined(GU_GPU_FAMILY) || !defined(GU_BOARD_ID) || !defined(GU_DEVICE_MEMORY_SIZE)
#error GPU family traits require an exact card/profile selection
#endif

#if GU_GPU_FAMILY == GU_GPU_FAMILY_GA102
#define GU_GPU_FAMILY_NAME "GA102"
#define GU_FAMILY_CHIP_ID 0xb72000a1U
/* Preserve the current LOCAL envelope's two-ID/two-size combinations. */
#if (GU_BOARD_ID != 0x220410deU && GU_BOARD_ID != 0x220310deU) || (GU_DEVICE_MEMORY_SIZE != 0x6000U && GU_DEVICE_MEMORY_SIZE != 0x5ff4U)
#error Card identity or memory is outside the reviewed GA102 family combinations
#endif
#elif GU_GPU_FAMILY == GU_GPU_FAMILY_GA104
#define GU_GPU_FAMILY_NAME "GA104"
#define GU_FAMILY_CHIP_ID 0xb74000a1U
/* These reviewed SKUs do not form a free identity/memory cross product.
 * LOCAL driver admission remains PCI-specific in proven.json. */
#if !(((GU_BOARD_ID == 0x248210deU || GU_BOARD_ID == 0x248410deU) && GU_DEVICE_MEMORY_SIZE == 0x2000U) || (GU_BOARD_ID == 0x248710deU && GU_DEVICE_MEMORY_SIZE == 0x3000U))
#error Card identity or memory is outside the reviewed GA104 family combinations
#endif
#elif GU_GPU_FAMILY == GU_GPU_FAMILY_GA106
#define GU_GPU_FAMILY_NAME "GA106"
#define GU_FAMILY_CHIP_ID 0xb76000a1U
#if (GU_BOARD_ID != 0x250410deU && !(defined(GU_PROFILE_ASROCK_B450M_3060) && GU_BOARD_ID == 0x250310deU && GU_BOARD_SUBSYSTEM == 0x40741458U)) || GU_DEVICE_MEMORY_SIZE != 0x3000U
#error Card identity or memory is outside the reviewed GA106 family combinations
#endif
#else
#error No reviewed implementation for the selected GPU family
#endif

#ifdef GU_CHIP_ID
#if GU_CHIP_ID != GU_FAMILY_CHIP_ID
#error Captured chip revision disagrees with the selected GPU family
#endif
#else
#define GU_CHIP_ID GU_FAMILY_CHIP_ID
#endif
