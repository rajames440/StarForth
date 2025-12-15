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
 * @file arch_detect.h
 * @brief Architecture Detection and Unified API for StarForth
 * @details Automatically selects x86_64 or ARM64 optimizations and provides
 *          a unified interface for architecture-specific optimizations.
 *
 * This work is released under license terms in LICENSE file.
 * No warranty. Use at your own risk.
 */

#ifndef ARCH_DETECT_H
#define ARCH_DETECT_H

/**
 * @defgroup arch_detect Architecture Detection
 * @{
 *
 * @brief Automatic detection of target architecture and inclusion of optimization headers
 *
 * This module automatically detects the target architecture at compile time and includes
 * the appropriate assembly optimization headers.
 *
 * Supported architectures:
 * - x86_64 (AMD64, Intel 64)
 * - ARM64 (AArch64, ARMv8-A)
 *
 * Unsupported architectures fall back to pure C implementations.
 * @}
 */

/* Detect architecture at compile time */
#if defined(__x86_64__) || defined(_M_X64) || defined(__amd64__)
#define ARCH_X86_64 1
#define ARCH_ARM64 0
#define ARCH_NAME "x86_64"
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__arm64__)
#define ARCH_X86_64 0
#define ARCH_ARM64 1
#define ARCH_NAME "ARM64"
#else
#define ARCH_X86_64 0
#define ARCH_ARM64 0
#define ARCH_NAME "Unknown"
#warning "Unknown architecture, assembly optimizations disabled"
#endif

/* Include architecture-specific headers */
#if ARCH_X86_64 && USE_ASM_OPT
#include "vm_asm_opt.h"
#if USE_DIRECT_THREADING
#include "vm_inner_interp_asm.h"
#endif
#elif ARCH_ARM64 && USE_ASM_OPT
#include "vm_asm_opt_arm64.h"
#if USE_DIRECT_THREADING
#include "vm_inner_interp_arm64.h"
#endif
#endif

/**
 * @defgroup unified_api Unified API Wrappers
 * @{
 *
 * @brief Consistent API macros across all architectures
 *
 * These macros provide a consistent API across all architectures by automatically
 * dispatching to the appropriate optimized or fallback implementation based on
 * the target architecture and available optimizations.
 * @}
 */

/* ========== Stack Operations ========== */

#if USE_ASM_OPT && (ARCH_X86_64 || ARCH_ARM64)
/* Use optimized assembly implementations */
#define vm_push_opt(vm, val) vm_push_asm(vm, val)
#define vm_pop_opt(vm) vm_pop_asm(vm)
#define vm_rpush_opt(vm, val) vm_rpush_asm(vm, val)
#define vm_rpop_opt(vm) vm_rpop_asm(vm)
#else
/* Fall back to standard C implementations */
#define vm_push_opt(vm, val) vm_push(vm, val)
#define vm_pop_opt(vm) vm_pop(vm)
#define vm_rpush_opt(vm, val) vm_rpush(vm, val)
#define vm_rpop_opt(vm) vm_rpop(vm)
#endif

/* ========== Arithmetic Operations ========== */

#if USE_ASM_OPT && (ARCH_X86_64 || ARCH_ARM64)
#define vm_add_overflow_opt(a, b, res) vm_add_check_overflow(a, b, res)
#define vm_sub_overflow_opt(a, b, res) vm_sub_check_overflow(a, b, res)
#define vm_mul_double_opt(a, b, hi, lo) vm_mul_double(a, b, hi, lo)
#define vm_divmod_opt(dividend, divisor, quot, rem) vm_divmod(dividend, divisor, quot, rem)
#else
/* Fallback implementations */
static inline int vm_add_overflow_opt(cell_t a, cell_t b, cell_t *res) {
    *res = a + b;
    /* Simple overflow check (not perfect) */
    return ((a > 0 && b > 0 && *res < 0) || (a < 0 && b < 0 && *res > 0));
}

static inline int vm_sub_overflow_opt(cell_t a, cell_t b, cell_t *res) {
    *res = a - b;
    return ((a > 0 && b < 0 && *res < 0) || (a < 0 && b > 0 && *res > 0));
}

static inline void vm_mul_double_opt(cell_t a, cell_t b, cell_t *hi, cell_t *lo) {
    /* Simplified - assumes 64-bit cell_t */
    *lo = a * b;
    *hi = 0; /* Would need 128-bit arithmetic for proper high word */
}

static inline void vm_divmod_opt(cell_t dividend, cell_t divisor, cell_t *quot, cell_t *rem) {
    *quot = dividend / divisor;
    *rem = dividend % divisor;
}
#endif

/* ========== String Operations ========== */

#if USE_ASM_OPT && (ARCH_X86_64 || ARCH_ARM64)
#define vm_strcmp_opt(s1, s2, len) vm_strcmp_asm(s1, s2, len)
#define vm_memcpy_opt(dst, src, len) vm_memcpy_asm(dst, src, len)
#define vm_memzero_opt(dst, len) vm_memzero_asm(dst, len)
#else
#include <string.h>

static inline int vm_strcmp_opt(const char *s1, const char *s2, size_t len) {
    return memcmp(s1, s2, len) != 0;
}

static inline void vm_memcpy_opt(void *dst, const void *src, size_t len) {
    memcpy(dst, src, len);
}

static inline void vm_memzero_opt(void *dst, size_t len) {
    memset(dst, 0, len);
}
#endif

/* ========== Min/Max Operations ========== */

#if USE_ASM_OPT && (ARCH_X86_64 || ARCH_ARM64)
#define vm_min_opt(a, b) vm_min_asm(a, b)
#define vm_max_opt(a, b) vm_max_asm(a, b)
#else
static inline cell_t vm_min_opt(cell_t a, cell_t b) {
    return (a < b) ? a : b;
}

static inline cell_t vm_max_opt(cell_t a, cell_t b) {
    return (a > b) ? a : b;
}
#endif

/* ========== Absolute Value ========== */

#if USE_ASM_OPT && ARCH_ARM64
#define vm_abs_opt(a) vm_abs_asm(a)
#else
static inline cell_t vm_abs_opt(cell_t a) {
    return (a < 0) ? -a : a;
}
#endif

/* ========== Bit Operations ========== */

#if USE_ASM_OPT && ARCH_ARM64
#define vm_clz_opt(x) vm_clz(x)
#define vm_ctz_opt(x) vm_ctz(x)
#define vm_popcnt_opt(x) vm_popcnt(x)
#elif USE_ASM_OPT && ARCH_X86_64
static inline int vm_clz_opt(cell_t x) {
    return __builtin_clzll((unsigned long long) x);
}
static inline int vm_ctz_opt(cell_t x) {
    return __builtin_ctzll((unsigned long long) x);
}
static inline int vm_popcnt_opt(cell_t x) {
    return __builtin_popcountll((unsigned long long) x);
}
#else
/* Software fallback */
static inline int vm_clz_opt(cell_t x) {
    if (x == 0) return 64;
    int n = 0;
    if ((x & 0xFFFFFFFF00000000ULL) == 0) {
        n += 32;
        x <<= 32;
    }
    if ((x & 0xFFFF000000000000ULL) == 0) {
        n += 16;
        x <<= 16;
    }
    if ((x & 0xFF00000000000000ULL) == 0) {
        n += 8;
        x <<= 8;
    }
    if ((x & 0xF000000000000000ULL) == 0) {
        n += 4;
        x <<= 4;
    }
    if ((x & 0xC000000000000000ULL) == 0) {
        n += 2;
        x <<= 2;
    }
    if ((x & 0x8000000000000000ULL) == 0) { n += 1; }
    return n;
}

static inline int vm_ctz_opt(cell_t x) {
    if (x == 0) return 64;
    int n = 0;
    if ((x & 0x00000000FFFFFFFFULL) == 0) {
        n += 32;
        x >>= 32;
    }
    if ((x & 0x000000000000FFFFULL) == 0) {
        n += 16;
        x >>= 16;
    }
    if ((x & 0x00000000000000FFULL) == 0) {
        n += 8;
        x >>= 8;
    }
    if ((x & 0x000000000000000FULL) == 0) {
        n += 4;
        x >>= 4;
    }
    if ((x & 0x0000000000000003ULL) == 0) {
        n += 2;
        x >>= 2;
    }
    if ((x & 0x0000000000000001ULL) == 0) { n += 1; }
    return n;
}

static inline int vm_popcnt_opt(cell_t x) {
    int count = 0;
    while (x) {
        count += x & 1;
        x >>= 1;
    }
    return count;
}
#endif

/* ========== Cache Operations ========== */

#if USE_ASM_OPT && ARCH_ARM64
#define vm_prefetch_opt(addr) vm_prefetch(addr)
#define vm_prefetch_stream_opt(addr) vm_prefetch_stream(addr)
#elif USE_ASM_OPT && ARCH_X86_64
static inline void vm_prefetch_opt(const void *addr) {
    __builtin_prefetch(addr, 0, 3); /* Read, high temporal locality */
}
static inline void vm_prefetch_stream_opt(const void *addr) {
    __builtin_prefetch(addr, 0, 0); /* Read, no temporal locality */
}
#else
#define vm_prefetch_opt(addr) ((void)0)
#define vm_prefetch_stream_opt(addr) ((void)0)
#endif

/* ========== Inner Interpreter (Direct Threading) ========== */

#if USE_DIRECT_THREADING && ARCH_X86_64
#define NEXT_OPT() NEXT_ASM()
#define vm_setup_registers_opt(vm) vm_setup_registers(vm)
#define vm_save_registers_opt(vm) vm_save_registers(vm)

/* Primitive word macros */
#define PRIM_DUP_OPT() PRIM_DUP()
#define PRIM_DROP_OPT() PRIM_DROP()
#define PRIM_SWAP_OPT() PRIM_SWAP()
#define PRIM_PLUS_OPT() PRIM_PLUS()
#define PRIM_MINUS_OPT() PRIM_MINUS()
#define PRIM_STAR_OPT() PRIM_STAR()
#define PRIM_FETCH_OPT() PRIM_FETCH()
#define PRIM_STORE_OPT() PRIM_STORE()
#define PRIM_TO_R_OPT() PRIM_TO_R()
#define PRIM_R_FROM_OPT() PRIM_R_FROM()
#define PRIM_R_FETCH_OPT() PRIM_R_FETCH()
#define PRIM_BRANCH_OPT() PRIM_BRANCH()
#define PRIM_ZBRANCH_OPT() PRIM_ZBRANCH()

#elif USE_DIRECT_THREADING && ARCH_ARM64
#define NEXT_OPT() NEXT_ARM64()
#define vm_setup_registers_opt(vm) vm_setup_registers_arm64(vm)
#define vm_save_registers_opt(vm) vm_save_registers_arm64(vm)

/* Primitive word macros */
#define PRIM_DUP_OPT() PRIM_DUP_ARM64()
#define PRIM_DROP_OPT() PRIM_DROP_ARM64()
#define PRIM_SWAP_OPT() PRIM_SWAP_ARM64()
#define PRIM_OVER_OPT() PRIM_OVER_ARM64()
#define PRIM_ROT_OPT() PRIM_ROT_ARM64()
#define PRIM_PLUS_OPT() PRIM_PLUS_ARM64()
#define PRIM_MINUS_OPT() PRIM_MINUS_ARM64()
#define PRIM_STAR_OPT() PRIM_STAR_ARM64()
#define PRIM_SLASH_OPT() PRIM_SLASH_ARM64()
#define PRIM_MOD_OPT() PRIM_MOD_ARM64()
#define PRIM_AND_OPT() PRIM_AND_ARM64()
#define PRIM_OR_OPT() PRIM_OR_ARM64()
#define PRIM_XOR_OPT() PRIM_XOR_ARM64()
#define PRIM_INVERT_OPT() PRIM_INVERT_ARM64()
#define PRIM_NEGATE_OPT() PRIM_NEGATE_ARM64()
#define PRIM_FETCH_OPT() PRIM_FETCH_ARM64()
#define PRIM_STORE_OPT() PRIM_STORE_ARM64()
#define PRIM_C_FETCH_OPT() PRIM_C_FETCH_ARM64()
#define PRIM_C_STORE_OPT() PRIM_C_STORE_ARM64()
#define PRIM_TO_R_OPT() PRIM_TO_R_ARM64()
#define PRIM_R_FROM_OPT() PRIM_R_FROM_ARM64()
#define PRIM_R_FETCH_OPT() PRIM_R_FETCH_ARM64()
#define PRIM_2DUP_OPT() PRIM_2DUP_ARM64()
#define PRIM_2DROP_OPT() PRIM_2DROP_ARM64()
#define PRIM_ZERO_EQUALS_OPT() PRIM_ZERO_EQUALS_ARM64()
#define PRIM_ZERO_LESS_OPT() PRIM_ZERO_LESS_ARM64()
#define PRIM_EQUALS_OPT() PRIM_EQUALS_ARM64()
#define PRIM_LESS_OPT() PRIM_LESS_ARM64()
#define PRIM_GREATER_OPT() PRIM_GREATER_ARM64()
#define PRIM_BRANCH_OPT() PRIM_BRANCH_ARM64()
#define PRIM_ZBRANCH_OPT() PRIM_ZBRANCH_ARM64()
#define PRIM_EXECUTE_OPT() PRIM_EXECUTE_ARM64()

#else
/* No direct threading - use standard C function calls */
#define NEXT_OPT() do {} while(0)
#define vm_setup_registers_opt(vm) do {} while(0)
#define vm_save_registers_opt(vm) do {} while(0)

/* Primitives fall back to function calls */
#define PRIM_DUP_OPT() do {} while(0)
#define PRIM_DROP_OPT() do {} while(0)
/* ... etc ... */
#endif

/**
 * @defgroup runtime_detect Runtime Architecture Detection
 * @{
 *
 * @brief Runtime detection of CPU features
 *
 * Functions for detecting available CPU features at runtime, such as SIMD
 * support (SSE4.2, AVX, NEON) and other architecture-specific capabilities.
 * @}
 */

#if ARCH_X86_64
/* Check for SSE4.2, AVX, etc. */
static inline int vm_has_sse42_opt(void) {


#if USE_ASM_OPT
return vm_has_sse42();
#else
return 0;
#endif
}
#elif ARCH_ARM64
/* Check for NEON, CRC32, etc. */
static inline int vm_has_neon_opt(void) {


#if USE_ASM_OPT
return vm_has_neon();
#else
return 0;
#endif
}
#else
static inline int vm_has_sse42_opt(void) { return 0; }
static inline int vm_has_neon_opt(void) { return 0; }
#endif

/**
 * @defgroup build_info Build Information
 * @{
 *
 * @brief Functions for querying build configuration
 *
 * Provides information about the build configuration, including target
 * architecture, enabled optimizations, and threading model.
 * @}
 */

static inline const char *vm_get_arch_name(void) {
    return ARCH_NAME;
}

static inline int vm_has_asm_opt(void) {
#if USE_ASM_OPT
    return 1;
#else
    return 0;
#endif
}

static inline int vm_has_direct_threading(void) {
#if USE_DIRECT_THREADING
    return 1;
#else
    return 0;
#endif
}

static inline const char *vm_get_build_info(void) {
    static char info[256];
    snprintf(info, sizeof(info),
             "Architecture: %s, ASM: %s, Direct Threading: %s",
             vm_get_arch_name(),
             vm_has_asm_opt() ? "enabled" : "disabled",
             vm_has_direct_threading() ? "enabled" : "disabled"
    );
    return info;
}

#endif /* ARCH_DETECT_H */