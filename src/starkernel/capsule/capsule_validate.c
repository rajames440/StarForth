/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
      https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

  See the License for the specific language governing permissions and
  limitations under the License.
*/

/**
 * capsule_validate.c - Capsule Validation (M7.1)
 *
 * Validates capsule descriptors for structural integrity and content hash.
 * Freestanding - no libc dependency.
 */

#include "starkernel/capsule.h"
#include "starkernel/xxhash64.h"

/*===========================================================================
 * Validation Result Strings
 *===========================================================================*/

static const char *validate_result_strings[] = {
    "valid",
    "bad magic",
    "bad version",
    "bad hash algorithm",
    "bounds error",
    "mode invalid",
    "revoked but active",
    "hash mismatch",
    "null pointer",
};

const char *capsule_validate_result_str(CapsuleValidateResult result) {
    if (result < sizeof(validate_result_strings) / sizeof(validate_result_strings[0])) {
        return validate_result_strings[result];
    }
    return "unknown error";
}

/*===========================================================================
 * Validation
 *===========================================================================*/

CapsuleValidateResult capsule_validate(
    const CapsuleDesc *desc,
    const uint8_t *arena_base,
    uint64_t arena_size,
    int verify_hash)
{
    /* Check for null pointers */
    if (!desc) {
        return CAPSULE_ERR_NULL_PTR;
    }

    /* 1. Check magic signature */
    uint32_t sig = CAPSULE_MAGIC_GET_SIG(desc->magic);
    if (sig != (uint32_t)CAPSULE_DESC_MAGIC) {
        return CAPSULE_ERR_BAD_MAGIC;
    }

    /* 2. Check version */
    uint8_t version = CAPSULE_MAGIC_GET_VERSION(desc->magic);
    if (version != CAPSULE_VERSION_0) {
        return CAPSULE_ERR_BAD_VERSION;
    }

    /* 3. Check hash algorithm is known */
    uint8_t hash_alg = CAPSULE_MAGIC_GET_HASHALG(desc->magic);
    if (hash_alg > CAPSULE_HASH_BLAKE3) {
        return CAPSULE_ERR_BAD_HASH_ALG;
    }

    /* 4. Check bounds: offset + length <= arena_size */
    if (arena_base && arena_size > 0) {
        if (desc->offset > arena_size ||
            desc->length > arena_size ||
            desc->offset + desc->length > arena_size) {
            return CAPSULE_ERR_BOUNDS;
        }
    }

    /* 5. Check mode flags: exactly one of (p) or (e) must be set */
    if (!CAPSULE_MODE_VALID(desc->flags)) {
        return CAPSULE_ERR_MODE_INVALID;
    }

    /* 6. Warn/error if REVOKED and ACTIVE are both set
     * (REVOKED should override, but this is a data consistency issue) */
    if ((desc->flags & CAPSULE_FLAG_REVOKED) &&
        (desc->flags & CAPSULE_FLAG_ACTIVE)) {
        /* This is allowed but suspicious - REVOKED overrides ACTIVE.
         * For strict validation, we could return an error here.
         * For now, just allow it (REVOKED wins at runtime). */
    }

    /* 7. Verify content hash if requested */
    if (verify_hash && arena_base && desc->length > 0) {
        /* Currently only xxHash64 is supported */
        if (hash_alg != CAPSULE_HASH_XXHASH64) {
            /* Can't verify unsupported hash algorithms */
            return CAPSULE_ERR_BAD_HASH_ALG;
        }

        const uint8_t *payload = arena_base + desc->offset;
        uint64_t computed_hash = xxhash64_capsule(payload, (size_t)desc->length);

        if (computed_hash != desc->content_hash) {
            return CAPSULE_ERR_HASH_MISMATCH;
        }

        /* Also verify capsule_id == content_hash (content-addressed invariant) */
        if (desc->capsule_id != desc->content_hash) {
            return CAPSULE_ERR_HASH_MISMATCH;
        }
    }

    return CAPSULE_VALID;
}

/*===========================================================================
 * Lookup Functions
 *===========================================================================*/

const CapsuleDesc *capsule_find_by_id(
    const CapsuleDirHeader *dir,
    const CapsuleDesc *descs,
    uint64_t id)
{
    if (!dir || !descs) {
        return (void *)0;
    }

    for (uint32_t i = 0; i < dir->desc_count; i++) {
        if (descs[i].capsule_id == id) {
            return &descs[i];
        }
    }

    return (void *)0;
}

const uint8_t *capsule_get_payload(
    const CapsuleDesc *desc,
    const uint8_t *arena_base)
{
    if (!desc || !arena_base) {
        return (void *)0;
    }

    return arena_base + desc->offset;
}

const CapsuleDesc *capsule_find_mama_init(
    const CapsuleDirHeader *dir,
    const CapsuleDesc *descs)
{
    if (!dir || !descs) {
        return (void *)0;
    }

    for (uint32_t i = 0; i < dir->desc_count; i++) {
        if (CAPSULE_IS_MAMA_INIT(descs[i].flags)) {
            return &descs[i];
        }
    }

    return (void *)0;
}
