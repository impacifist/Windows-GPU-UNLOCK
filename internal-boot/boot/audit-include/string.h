/* GPL-2.0-only. Freestanding RAM intrinsics supplied by efi_runtime.c. */
#pragma once
#include <stddef.h>
void *memcpy(void *destination,const void *source,size_t bytes);
void *memset(void *destination,int value,size_t bytes);
