/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.
*/

/**
 * capsule_find.c - Capsule Lookup (M7.1)
 *
 * Lookup functions for capsule descriptors by ID, name, and role.
 * Freestanding - no libc dependency.
 */

#include "starkernel/capsule.h"

/*===========================================================================
 * Internal: string comparison (no libc)
 *===========================================================================*/

static int capsule_streq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

/*===========================================================================
 * capsule_find_by_id
 *===========================================================================*/

const CapsuleDesc *capsule_find_by_id(
    const CapsuleDirHeader *dir,
    const CapsuleDesc      *descs,
    uint64_t                id)
{
    if (!dir || !descs) return (void *)0;

    uint32_t i;
    for (i = 0; i < dir->desc_count; i++) {
        if (descs[i].capsule_id == id) {
            return &descs[i];
        }
    }
    return (void *)0;
}

/*===========================================================================
 * capsule_find_by_name
 *===========================================================================*/

const CapsuleDesc *capsule_find_by_name(
    const CapsuleDirHeader *dir,
    const CapsuleDesc      *descs,
    const CapsuleNameEntry *names,
    const char             *name)
{
    if (!dir || !descs || !names || !name) return (void *)0;

    uint32_t i;
    for (i = 0; i < dir->desc_count; i++) {
        if (capsule_streq(names[i].name, name)) {
            return &descs[i];
        }
    }
    return (void *)0;
}

/*===========================================================================
 * capsule_find_mama_init
 *===========================================================================*/

const CapsuleDesc *capsule_find_mama_init(
    const CapsuleDirHeader *dir,
    const CapsuleDesc      *descs)
{
    if (!dir || !descs) return (void *)0;

    uint32_t i;
    for (i = 0; i < dir->desc_count; i++) {
        if (CAPSULE_IS_MAMA_INIT(descs[i].flags)) {
            return &descs[i];
        }
    }
    return (void *)0;
}

/*===========================================================================
 * capsule_get_payload
 *===========================================================================*/

const uint8_t *capsule_get_payload(
    const CapsuleDesc *desc,
    const uint8_t     *arena_base)
{
    if (!desc || !arena_base) return (void *)0;
    return arena_base + desc->offset;
}
