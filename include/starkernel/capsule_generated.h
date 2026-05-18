/*
  StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.
  Licensed under the StarForth License, Version 1.0
*/

/**
 * capsule_generated.h - Declarations for mkcapsule-generated capsule store.
 *
 * The matching capsule_generated.c is produced at build time by mkcapsule.
 * These three arrays plus the directory header are compiled into .rodata.
 */

#ifndef STARKERNEL_CAPSULE_GENERATED_H
#define STARKERNEL_CAPSULE_GENERATED_H

#include <stdint.h>
#include "starkernel/capsule.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Mark as hidden so GCC uses direct RIP-relative addressing in PIC/PE builds,
 * avoiding GOT references which do not exist in PE/COFF binaries. */
__attribute__((visibility("hidden")))
extern const uint8_t         capsule_arena[];
__attribute__((visibility("hidden")))
extern const CapsuleDesc     capsule_descriptors[];
__attribute__((visibility("hidden")))
extern const CapsuleNameEntry capsule_names[];
__attribute__((visibility("hidden")))
extern const CapsuleDirHeader capsule_directory;

/*
 * Accessor functions — defined in the same TU as the symbols above.
 * PE/COFF builds with -fPIC do not convert GOTPCREL data references to
 * direct LEA the way the ELF linker does, so any cross-TU access to a
 * data symbol goes through GOT and reads garbage in a PE image.
 * These hidden accessor functions are called via a direct CALL (no PLT/GOT)
 * and access the symbols with direct RIP-relative addressing inside their TU.
 */
__attribute__((visibility("hidden")))
uint32_t                   capsule_get_desc_count(void);
__attribute__((visibility("hidden")))
const CapsuleDirHeader    *capsule_get_directory(void);
__attribute__((visibility("hidden")))
const CapsuleDesc         *capsule_get_descriptors(void);
__attribute__((visibility("hidden")))
const CapsuleNameEntry    *capsule_get_names(void);
__attribute__((visibility("hidden")))
const uint8_t             *capsule_get_arena(void);

#ifdef __cplusplus
}
#endif

#endif /* STARKERNEL_CAPSULE_GENERATED_H */
