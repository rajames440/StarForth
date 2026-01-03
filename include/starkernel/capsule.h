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
 * capsule.h - Init Capsule Architecture (M7.1)
 *
 * Content-addressed, immutable init capsules for VM birth.
 * See docs/lithosananke/M7.1.md for full specification.
 *
 * Key invariants:
 * - Exactly ONE production (p) INIT defines a baby VM
 * - capsule_id == content_hash (content-addressed)
 * - No shared/implicit base INITs
 * - DOMAIN is Mama-only, PERSONALITY is baby-only
 */

#ifndef STARKERNEL_CAPSULE_H
#define STARKERNEL_CAPSULE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Magic signatures */
#define CAPSULE_DESC_MAGIC      0x53504143ULL   /* 'CAPS' little-endian */
#define CAPSULE_DIR_MAGIC       0x44504143ULL   /* 'CAPD' little-endian */

/** Version */
#define CAPSULE_VERSION_0       0

/** Limits */
#define CAPSULE_MAX_COUNT       256

/*===========================================================================
 * Hash Algorithm Enum
 *===========================================================================*/

typedef enum {
    CAPSULE_HASH_XXHASH64 = 0,
    CAPSULE_HASH_SHA256   = 1,
    CAPSULE_HASH_BLAKE3   = 2,
} CapsuleHashAlg;

/*===========================================================================
 * Flags
 *===========================================================================*/

/** State flags */
#define CAPSULE_FLAG_ACTIVE       0x00000001  /* Eligible for use */
#define CAPSULE_FLAG_REVOKED      0x00000002  /* Birth-blocked forever */
#define CAPSULE_FLAG_DEPRECATED   0x00000004  /* Eligible but discouraged */
#define CAPSULE_FLAG_PINNED       0x00000008  /* Immune to GC */

/** Mode flags (exactly one must be set for babies) */
#define CAPSULE_FLAG_PRODUCTION   0x00000010  /* (p) truth-bearing */
#define CAPSULE_FLAG_EXPERIMENT   0x00000020  /* (e) workload only */

/** Mama init flag (exactly one capsule must have this) */
#define CAPSULE_FLAG_MAMA_INIT    0x00000040  /* (m) Mama's init */

/** Validate mode flags: exactly one of (p), (e), or (m) must be set */
#define CAPSULE_MODE_VALID(f) \
    ((((f) & CAPSULE_FLAG_MAMA_INIT) != 0) ? \
        (!((f) & (CAPSULE_FLAG_PRODUCTION | CAPSULE_FLAG_EXPERIMENT))) : \
        ((((f) & CAPSULE_FLAG_PRODUCTION) != 0) ^ \
         (((f) & CAPSULE_FLAG_EXPERIMENT) != 0)))

/** Check if capsule is Mama's init */
#define CAPSULE_IS_MAMA_INIT(f) \
    (((f) & CAPSULE_FLAG_MAMA_INIT) && ((f) & CAPSULE_FLAG_ACTIVE))

/** Birth eligibility: production, active, not revoked */
#define CAPSULE_BIRTH_ELIGIBLE(f) \
    (((f) & CAPSULE_FLAG_PRODUCTION) && \
     ((f) & CAPSULE_FLAG_ACTIVE) && \
     !((f) & CAPSULE_FLAG_REVOKED))

/** DoE eligibility: experiment, active, not revoked */
#define CAPSULE_DOE_ELIGIBLE(f) \
    (((f) & CAPSULE_FLAG_EXPERIMENT) && \
     ((f) & CAPSULE_FLAG_ACTIVE) && \
     !((f) & CAPSULE_FLAG_REVOKED))

/*===========================================================================
 * Magic Field Packing
 *
 * bits  0..31 : 'CAPS' (0x53504143 little-endian)
 * bits 32..39 : version (0 for v0)
 * bits 40..47 : hashAlg (enum CapsuleHashAlg)
 * bits 48..63 : reserved (zero)
 *===========================================================================*/

#define CAPSULE_MAGIC_PACK(ver, alg) \
    (CAPSULE_DESC_MAGIC | ((uint64_t)(ver) << 32) | ((uint64_t)(alg) << 40))

#define CAPSULE_MAGIC_GET_SIG(m)     ((uint32_t)((m) & 0xFFFFFFFFULL))
#define CAPSULE_MAGIC_GET_VERSION(m) ((uint8_t)(((m) >> 32) & 0xFF))
#define CAPSULE_MAGIC_GET_HASHALG(m) ((uint8_t)(((m) >> 40) & 0xFF))

/*===========================================================================
 * CapsuleDesc - Capsule Descriptor (64 bytes, cache-line aligned)
 *===========================================================================*/

typedef struct __attribute__((aligned(64))) {
    uint64_t magic;          /* 0x00: 'CAPS' | ver | hashAlg | reserved */
    uint64_t capsule_id;     /* 0x08: == content_hash (content-addressed) */
    uint64_t content_hash;   /* 0x10: hash of payload bytes */
    uint64_t offset;         /* 0x18: byte offset into payload arena */
    uint64_t length;         /* 0x20: payload length in bytes */
    uint32_t flags;          /* 0x28: state + policy bits */
    uint32_t owner_vm;       /* 0x2C: 0 = mama, else child VM ID */
    uint64_t birth_count;    /* 0x30: how many VMs born from this */
    uint64_t created_ns;     /* 0x38: monotonic timestamp at registration */
} CapsuleDesc;               /* 0x40 = 64 bytes */

/*===========================================================================
 * CapsuleDirHeader - Directory Header
 *===========================================================================*/

typedef struct {
    uint64_t magic;          /* 'CAPD' | ver | reserved */
    uint64_t arena_base;     /* phys or virt base of payload arena */
    uint64_t arena_size;     /* bytes */
    uint32_t desc_count;     /* current number of descriptors */
    uint32_t desc_capacity;  /* max (fixed at compile time for Phase A) */
    uint64_t dir_hash;       /* hash of descriptor table (for parity) */
} CapsuleDirHeader;

/*===========================================================================
 * Validation
 *===========================================================================*/

typedef enum {
    CAPSULE_VALID = 0,
    CAPSULE_ERR_BAD_MAGIC,
    CAPSULE_ERR_BAD_VERSION,
    CAPSULE_ERR_BAD_HASH_ALG,
    CAPSULE_ERR_BOUNDS,
    CAPSULE_ERR_MODE_INVALID,
    CAPSULE_ERR_REVOKED_ACTIVE,
    CAPSULE_ERR_HASH_MISMATCH,
    CAPSULE_ERR_NULL_PTR,
} CapsuleValidateResult;

/**
 * capsule_validate - Validate a capsule descriptor
 *
 * @param desc         Capsule descriptor to validate
 * @param arena_base   Base address of payload arena
 * @param arena_size   Size of payload arena in bytes
 * @param verify_hash  If true, recompute and compare content hash
 * @return CAPSULE_VALID on success, error code otherwise
 */
CapsuleValidateResult capsule_validate(
    const CapsuleDesc *desc,
    const uint8_t *arena_base,
    uint64_t arena_size,
    int verify_hash
);

/**
 * capsule_validate_result_str - Get string for validation result
 */
const char *capsule_validate_result_str(CapsuleValidateResult result);

/*===========================================================================
 * Lookup
 *===========================================================================*/

/**
 * capsule_find_by_id - Find capsule by content hash ID
 *
 * @param dir   Directory header
 * @param descs Descriptor array
 * @param id    Capsule ID (content hash) to find
 * @return Pointer to descriptor, or NULL if not found
 */
const CapsuleDesc *capsule_find_by_id(
    const CapsuleDirHeader *dir,
    const CapsuleDesc *descs,
    uint64_t id
);

/**
 * capsule_get_payload - Get pointer to capsule payload bytes
 *
 * @param desc       Capsule descriptor
 * @param arena_base Base address of payload arena
 * @return Pointer to payload bytes, or NULL on error
 */
const uint8_t *capsule_get_payload(
    const CapsuleDesc *desc,
    const uint8_t *arena_base
);

/**
 * capsule_find_mama_init - Find the Mama init capsule
 *
 * Searches the descriptor array for the capsule with CAPSULE_FLAG_MAMA_INIT.
 * There must be exactly one such capsule.
 *
 * @param dir   Directory header
 * @param descs Descriptor array
 * @return Pointer to Mama's init descriptor, or NULL if not found
 */
const CapsuleDesc *capsule_find_mama_init(
    const CapsuleDirHeader *dir,
    const CapsuleDesc *descs
);

#ifdef __cplusplus
}
#endif

#endif /* STARKERNEL_CAPSULE_H */
