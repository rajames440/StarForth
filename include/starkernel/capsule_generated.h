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

extern const uint8_t         capsule_arena[];
extern const CapsuleDesc     capsule_descriptors[];
extern const CapsuleNameEntry capsule_names[];
extern const CapsuleDirHeader capsule_directory;

#ifdef __cplusplus
}
#endif

#endif /* STARKERNEL_CAPSULE_GENERATED_H */
