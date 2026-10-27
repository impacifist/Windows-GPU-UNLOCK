/* GPL-2.0-only. Isolated payload-free pilot, never an unlock build. */
#pragma once
#if !defined(GU_RELEASE_PILOT) || !defined(GU_PILOT_STAGE) || GU_PILOT_STAGE != 1
#error Explicit fixed Stage 1 pilot is required
#endif
#if defined(GU_FULL_UNLOCK) || defined(GU_AUTOMATIC_BOOT) || defined(GU_INTERNAL_BOOT) || defined(GU_HEADLESS_RESEARCH) || defined(GU_EXPERIMENTAL_UNLOCK) || defined(GU_EXPERIMENTAL_3090) || defined(GU_ENTRY_LATCHED_SS1_FULL) || defined(GU_ENTRY_LATCHED_SS1_PRESERVE) || defined(GU_STOCK_PROFILE_ORIGINAL_03100062) || defined(GU_STOCK_PROFILE_OBSERVED_03000062) || defined(GU_HOST_WRITE_TEST) || defined(GU_PROFILE_B650M_3090FE) || defined(GU_PROFILE_B660M_3070TI) || defined(GU_PROFILE_MSI_X570_3080TI)
#error Pilot must not combine with production unlock or profile enablement
#endif
#if defined(GU_PROFILE_X870_3090TI) || defined(GU_PROFILE_B660PLUS_3060) || defined(GU_PROFILE_H610M_3060) || defined(GU_PROFILE_PROBE_H610M_3060) || defined(GU_PROFILE_LOCAL)
#error Pilot must not combine with production unlock or profile enablement
#endif
#if defined(GU_PILOT_HOST_TEST) && __STDC_HOSTED__ != 1
#error Host halt hook is forbidden in firmware
#endif
#define GU_CONSOLE_AGGREGATE 0
#define GU_SINGLE_RTX_CONSOLE 1
#include "profile-data.h"
#ifdef GU_PILOT_SYNTHETIC
#if !defined(GU_PILOT_HOST_TEST) || !defined(__STDC_HOSTED__) || __STDC_HOSTED__ != 1
#error Synthetic identity is host-test-only
#endif
#include "synthetic-profile.h"
#endif
