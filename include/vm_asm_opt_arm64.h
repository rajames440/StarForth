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

#ifndef VM_ASM_OPT_ARM64_H
#define VM_ASM_OPT_ARM64_H

#include "vm.h"
#include <stdint.h>
#ifdef __linux__
#include <sys/auxv.h>  /* For getauxval, AT_HWCAP */
#include <asm/hwcap.h> /* For HWCAP_ASIMD */
#endif

/*
 * Configuration: Define USE_ASM_OPT=1 to enable assembly optimizations
 */
#ifndef USE_ASM_OPT
#define USE_ASM_OPT 0
#endif

/*
 * ARM64 Architecture Notes:
 * =========================
 *
 * Registers (64-bit):
 * - x0-x7:   Argument/result registers
 * - x8:      Indirect result location
 * - x9-x15:  Temporary registers (caller-saved)
 * - x16-x17: Intra-procedure-call temporary (IP0, IP1)
 * - x18:     Platform register (reserved on some platforms)
 * - x19-x28: Callee-saved registers (must preserve)
 * - x29:     Frame pointer (FP)
 * - x30:     Link register (LR)
 * - sp:      Stack pointer
 *
 * 32-bit variants: w0-w30 (lower 32 bits of x0-x30)
 *
 * Advantages over x86_64:
 * - More registers (31 general-purpose vs 16)
 * - Conditional execution on most instructions
 * - Load/store with auto-increment
 * - Better power efficiency
 *
 * Raspberry Pi 4 specifics:
 * - Cortex-A72 CPU (4 cores @ 1.5GHz)
 * - ARMv8-A architecture
 * - 32KB L1I + 32KB L1D per core
 * - 1MB shared L2 cache
 * - NEON SIMD support
 */

/* ============================================================================
 * OPTIMIZATION 1: Stack Operations (vm_push, vm_pop, vm_rpush, vm_rpop)
 * ============================================================================
 *
 * ARM64 advantages:
 * - Conditional execution eliminates branches
 * - Load/store with immediate offset
 * - Post-increment addressing modes
 *
 * ============================================================================
 */

#if USE_ASM_OPT

/* vm_push optimized - Data stack push */
/**
 * @brief Optimized assembly implementation of data stack push operation
 * @param vm Pointer to VM instance
 * @param value Value to push onto data stack
 * @note Sets vm->error on stack overflow
 */
static inline void vm_push_asm(VM *vm, cell_t value) {
    int error = 0;

    __asm__ __volatile__(
        /* Load current dsp */
        "ldr     w0, %[dsp]\n\t"

        /* Check for overflow: dsp >= 1022 */
        "cmp     w0, #1022\n\t"
        "b.ge    1f\n\t" /* Branch if >= */

        /* No overflow: increment dsp */
        "add     w1, w0, #1\n\t" /* w1 = dsp + 1 */
        "str     w1, %[dsp]\n\t"

        /* Store value: vm->data_stack[dsp+1] = value */
        /* Calculate address: base + (dsp+1)*8 */
        "ldr     x2, %[stack]\n\t" /* Load stack base */
        "str     %[val], [x2, w1, sxtw #3]\n\t" /* Sign-extend w1, shift left 3, store */

        /* Set error=0 in success path */
        "mov     w3, #0\n\t"
        "str     w3, %[err]\n\t"
        "b       2f\n\t"

        /* Overflow path */
        "1:\n\t"
        "mov     w3, #1\n\t"
        "str     w3, %[err]\n\t"

        "2:\n\t"
        : [dsp]"+m"(vm->dsp),
        [err]"=m"(error)
        : [val]"r"(value),
        [stack]"m"(vm->data_stack)
        : "x0", "x1", "x2", "x3", "cc", "memory"
    );

    if (__builtin_expect(error, 0)) {
        vm->error = 1;
    }
}

/* vm_pop optimized - Data stack pop */
/**
 * @brief Optimized assembly implementation of data stack pop operation
 * @param vm Pointer to VM instance
 * @return Popped value, or 0 on underflow
 * @note Sets vm->error on stack underflow
 */
static inline cell_t vm_pop_asm(VM *vm) {
    cell_t value = 0;
    int error = 0;

    __asm__ __volatile__(
        /* Load current dsp */
        "ldr     w0, %[dsp]\n\t"

        /* Check for underflow: dsp < 0 */
        "cmp     w0, #0\n\t"
        "b.lt    1f\n\t" /* Branch if less than */

        /* No underflow: load value */
        "ldr     x2, %[stack]\n\t"
        "ldr     %[val], [x2, w0, sxtw #3]\n\t" /* Load with scaled index */

        /* Decrement dsp */
        "sub     w1, w0, #1\n\t"
        "str     w1, %[dsp]\n\t"

        /* Set error=0 in success path */
        "mov     w3, #0\n\t"
        "str     w3, %[err]\n\t"
        "b       2f\n\t"

        /* Underflow path */
        "1:\n\t"
        "mov     w3, #1\n\t"
        "str     w3, %[err]\n\t"
        "mov     %[val], #0\n\t" /* Return 0 on error */

        "2:\n\t"
        : [dsp]"+m"(vm->dsp),
        [val]"=r"(value),
        [err]"=m"(error)
        : [stack]"m"(vm->data_stack)
        : "x0", "x1", "x2", "x3", "cc", "memory"
    );

    if (__builtin_expect(error, 0)) {
        vm->error = 1;
    }

    return value;
}

/* vm_rpush optimized - Return stack push */
/**
 * @brief Optimized assembly implementation of return stack push operation
 * @param vm Pointer to VM instance
 * @param value Value to push onto return stack
 * @note Sets vm->error on stack overflow
 */
static inline void vm_rpush_asm(VM *vm, cell_t value) {
    int error = 0;

    __asm__ __volatile__(
        "ldr     w0, %[rsp]\n\t"
        "cmp     w0, #1022\n\t"
        "b.ge    1f\n\t"
        "add     w1, w0, #1\n\t"
        "str     w1, %[rsp]\n\t"
        "ldr     x2, %[stack]\n\t"
        "str     %[val], [x2, w1, sxtw #3]\n\t"
        "mov     w3, #0\n\t"
        "str     w3, %[err]\n\t"
        "b       2f\n\t"
        "1:\n\t"
        "mov     w3, #1\n\t"
        "str     w3, %[err]\n\t"
        "2:\n\t"
        : [rsp]"+m"(vm->rsp),
        [err]"=m"(error)
        : [val]"r"(value),
        [stack]"m"(vm->return_stack)
        : "x0", "x1", "x2", "x3", "cc", "memory"
    );

    if (__builtin_expect(error, 0)) {
        vm->error = 1;
    }
}

/* vm_rpop optimized - Return stack pop */
/**
 * @brief Optimized assembly implementation of return stack pop operation
 * @param vm Pointer to VM instance
 * @return Popped value, or 0 on underflow
 * @note Sets vm->error on stack underflow
 */
static inline cell_t vm_rpop_asm(VM *vm) {
    cell_t value = 0;
    int error = 0;

    __asm__ __volatile__(
        "ldr     w0, %[rsp]\n\t"
        "cmp     w0, #0\n\t"
        "b.lt    1f\n\t"
        "ldr     x2, %[stack]\n\t"
        "ldr     %[val], [x2, w0, sxtw #3]\n\t"
        "sub     w1, w0, #1\n\t"
        "str     w1, %[rsp]\n\t"
        "mov     w3, #0\n\t"
        "str     w3, %[err]\n\t"
        "b       2f\n\t"
        "1:\n\t"
        "mov     w3, #1\n\t"
        "str     w3, %[err]\n\t"
        "mov     %[val], #0\n\t"
        "2:\n\t"
        : [rsp]"+m"(vm->rsp),
        [val]"=r"(value),
        [err]"=m"(error)
        : [stack]"m"(vm->return_stack)
        : "x0", "x1", "x2", "x3", "cc", "memory"
    );

    if (__builtin_expect(error, 0)) {
        vm->error = 1;
    }

    return value;
}

/* ============================================================================
 * OPTIMIZATION 2: Fast Arithmetic with Overflow Detection
 * ============================================================================
 *
 * ARM64 has excellent support for overflow detection via condition flags
 * and conditional select instructions
 *
 * ============================================================================
 */

/* Fast add with overflow detection using ADDS and conditional select */
/**
 * @brief Fast addition with overflow detection using ARM64 ADDS instruction
 * @param a First operand
 * @param b Second operand
 * @param result Pointer to store result
 * @return 1 if overflow occurred, 0 otherwise
 */
static inline int vm_add_check_overflow(cell_t a, cell_t b, cell_t *result) {
    cell_t res;
    int overflow;

    __asm__(
        "adds    %[res], %[a], %[b]\n\t" /* Add and set flags */
        "cset    %w[ovf], vs\n\t" /* Set overflow flag (V flag) */
        : [res]"=r"(res),
        [ovf]"=r"(overflow)
        : [a]"r"(a),
        [b]"r"(b)
        : "cc"
    );

    *result = res;
    return overflow;
}

/* Fast subtract with overflow detection */
/**
 * @brief Fast subtraction with overflow detection using ARM64 SUBS instruction
 * @param a First operand
 * @param b Second operand
 * @param result Pointer to store result
 * @return 1 if overflow occurred, 0 otherwise
 */
static inline int vm_sub_check_overflow(cell_t a, cell_t b, cell_t *result) {
    cell_t res;
    int overflow;

    __asm__(
        "subs    %[res], %[a], %[b]\n\t"
        "cset    %w[ovf], vs\n\t"
        : [res]"=r"(res),
        [ovf]"=r"(overflow)
        : [a]"r"(a),
        [b]"r"(b)
        : "cc"
    );

    *result = res;
    return overflow;
}

/* Fast multiply with high-word result (for star-slash-MOD) */
/**
 * @brief Fast 128-bit multiplication using ARM64 SMULH instruction
 * @param a First operand
 * @param b Second operand
 * @param hi Pointer to store high 64 bits of result
 * @param lo Pointer to store low 64 bits of result
 */
static inline void vm_mul_double(cell_t a, cell_t b, cell_t *hi, cell_t *lo) {
    __asm__(
        "mul     %[lo], %[a], %[b]\n\t" /* Low 64 bits */
        "smulh   %[hi], %[a], %[b]\n\t" /* High 64 bits (signed) */
        : [lo]"=r"(*lo),
        [hi]"=r"(*hi)
        : [a]"r"(a),
        [b]"r"(b)
    );
}

/* Fast divide with remainder (for /MOD) */
/**
 * @brief Fast division with remainder using ARM64 SDIV and MSUB instructions
 * @param dividend Dividend value
 * @param divisor Divisor value
 * @param quotient Pointer to store quotient
 * @param remainder Pointer to store remainder
 */
static inline void vm_divmod(cell_t dividend, cell_t divisor,
                             cell_t *quotient, cell_t *remainder) {
    cell_t quot, rem;

    __asm__(
        "sdiv    %[quot], %[dividend], %[divisor]\n\t" /* Quotient */
        "msub    %[rem], %[quot], %[divisor], %[dividend]\n\t" /* Remainder = dividend - quot*divisor */
        : [quot]"=r"(quot),
        [rem]"=r"(rem)
        : [dividend]"r"(dividend),
        [divisor]"r"(divisor)
    );

    *quotient = quot;
    *remainder = rem;
}

/* ============================================================================
 * OPTIMIZATION 3: Fast String Operations for Dictionary Lookup
 * ============================================================================
 *
 * ARM64 doesn't have rep-style instructions, but we can:
 * - Use NEON for parallel comparison
 * - Use load-multiple for cache efficiency
 * - Unroll loops for small strings
 *
 * ============================================================================
 */

/* Fast string comparison for short strings (<=32 bytes) */
/**
 * @brief Optimized string comparison for short strings (<=32 bytes)
 * @param s1 First string
 * @param s2 Second string
 * @param len Length to compare
 * @return 0 if equal, 1 if different
 */
static inline int vm_strcmp_short_asm(const char *s1, const char *s2, size_t len) {
    int result = 0;

    if (len <= 8) {
        /* Use single 64-bit load for strings <= 8 bytes */
        __asm__(
            "mov     x0, #0\n\t" /* result = 0 */
            "cbz     %[len], 2f\n\t" /* if len==0, equal */
            "ldr     x1, [%[s1]]\n\t"
            "ldr     x2, [%[s2]]\n\t"
            "eor     x3, x1, x2\n\t" /* XOR to find differences */

            /* Mask off bits beyond length */
            "mov     x4, #64\n\t"
            "lsl     x5, %[len], #3\n\t" /* len * 8 */
            "sub     x4, x4, x5\n\t"
            "lsr     x1, x1, x4\n\t"
            "lsr     x2, x2, x4\n\t"
            "eor     x3, x1, x2\n\t"

            "cmp     x3, #0\n\t"
            "cset    %w[res], ne\n\t" /* result = (x3 != 0) */
            "2:\n\t"
            : [res]"=r"(result)
            : [s1]"r"(s1),
            [s2]"r"(s2),
            [len]"r"(len)
            : "x0", "x1", "x2", "x3", "x4", "x5", "cc", "memory"
        );
    } else {
        /* Fallback to byte-by-byte for longer strings */
        for (size_t i = 0; i < len; i++) {
            if (s1[i] != s2[i]) {
                result = 1;
                break;
            }
        }
    }

    return result;
}

/* Fast string comparison using NEON (for longer strings) */
#ifdef __ARM_NEON
static inline int vm_strcmp_neon(const char *s1, const char *s2, size_t len) {
    int result = 0;

    /* Process 16 bytes at a time using NEON */
    while (len >= 16) {
        __asm__ __volatile__(
            "ld1     {v0.16b}, [%[s1]], #16\n\t" /* Load 16 bytes from s1, post-increment */
            "ld1     {v1.16b}, [%[s2]], #16\n\t" /* Load 16 bytes from s2, post-increment */
            "cmeq    v2.16b, v0.16b, v1.16b\n\t" /* Compare equal */
            "uminv   b3, v2.16b\n\t" /* Get minimum (all must be 0xFF for equal) */
            "fmov    %w[res], s3\n\t" /* Move to general register */
            "cmp     %w[res], #0xff\n\t"
            "b.ne    1f\n\t" /* If not all equal, exit */
            "1:\n\t"
            : [res]"=r"(result),
            [s1]"+r"(s1),
            [s2]"+r"(s2)
            :
            : "v0", "v1", "v2", "v3", "cc", "memory"
        );

        if (result != 0xFF) {
            return 1; /* Not equal */
        }

        len -= 16;
    }

    /* Handle remaining bytes */
    while (len > 0) {
        if (*s1++ != *s2++) return 1;
        len--;
    }

    return 0;
}
#endif

/* Main string comparison wrapper */
static inline int vm_strcmp_asm(const char *s1, const char *s2, size_t len) {



#ifdef __ARM_NEON
if (len>= 16) {
        return vm_strcmp_neon(s1, s2, len);
    }
#endif
return vm_strcmp_short_asm(s1, s2, len);
}

/* Fast memory copy optimized for ARM64 */
/**
 * @brief Optimized memory copy using ARM64 load/store pair instructions
 * @param dest Destination buffer
 * @param src Source buffer
 * @param len Number of bytes to copy
 */
static inline void vm_memcpy_asm(void *dest, const void *src, size_t len) {
    /* ARM64 has efficient load/store pair instructions */
    if (len >= 16) {
        __asm__ __volatile__(
            "1:\n\t"
            "ldp     x0, x1, [%[src]], #16\n\t" /* Load pair, post-increment */
            "stp     x0, x1, [%[dst]], #16\n\t" /* Store pair, post-increment */
            "subs    %[len], %[len], #16\n\t"
            "b.ge    1b\n\t"
            : [src]"+r"(src),
            [dst]"+r"(dest),
            [len]"+r"(len)
            :
            : "x0", "x1", "cc", "memory"
        );
        len += 16; /* Adjust for overshoot */
    }

    /* Copy remaining bytes */
    char *d = (char *) dest;
    const char *s = (const char *) src;
    while (len-- > 0) {
        *d++ = *s++;
    }
}

/* Fast memory zero using store pair */
/**
 * @brief Fast memory zeroing using ARM64 store pair instructions
 * @param dest Buffer to zero
 * @param len Number of bytes to zero
 */
static inline void vm_memzero_asm(void *dest, size_t len) {
    if (len >= 16) {
        __asm__ __volatile__(
            "1:\n\t"
            "stp     xzr, xzr, [%[dst]], #16\n\t" /* Store zero pair */
            "subs    %[len], %[len], #16\n\t"
            "b.ge    1b\n\t"
            : [dst]"+r"(dest),
            [len]"+r"(len)
            :
            : "cc", "memory"
        );
        len += 16;
    }

    /* Zero remaining bytes */
    char *d = (char *) dest;
    while (len-- > 0) {
        *d++ = 0;
    }
}

/* ============================================================================
 * OPTIMIZATION 4: Branchless Min/Max using CSEL
 * ============================================================================
 *
 * ARM64's conditional select is perfect for branchless operations
 *
 * ============================================================================
 */

static inline cell_t vm_min_asm(cell_t a, cell_t b) {
    cell_t result;
    __asm__(
        "cmp     %[a], %[b]\n\t"
        "csel    %[res], %[a], %[b], lt\n\t" /* Select a if a < b, else b */
        : [res]"=r"(result)
        : [a]"r"(a),
        [b]"r"(b)
        : "cc"
    );
    return result;
}

static inline cell_t vm_max_asm(cell_t a, cell_t b) {
    cell_t result;
    __asm__(
        "cmp     %[a], %[b]\n\t"
        "csel    %[res], %[a], %[b], gt\n\t" /* Select a if a > b, else b */
        : [res]"=r"(result)
        : [a]"r"(a),
        [b]"r"(b)
        : "cc"
    );
    return result;
}

/* Absolute value using conditional negate */
static inline cell_t vm_abs_asm(cell_t a) {
    cell_t result;
    __asm__(
        "cmp     %[a], #0\n\t"
        "cneg    %[res], %[a], lt\n\t" /* Negate if negative */
        : [res]"=r"(result)
        : [a]"r"(a)
        : "cc"
    );
    return result;
}

/* ============================================================================
 * OPTIMIZATION 5: Bit Manipulation
 * ============================================================================
 *
 * ARM64 has excellent bit manipulation instructions
 *
 * ============================================================================
 */

/* Count leading zeros */
static inline int vm_clz(cell_t x) {
    int result;
    __asm__("clz %[res], %[x]": [res]"=r"(result): [x]"r"(x));
    return result;
}

/* Count trailing zeros */
static inline int vm_ctz(cell_t x) {
    int result;
    __asm__(
        "rbit    %[tmp], %[x]\n\t" /* Reverse bits */
        "clz     %[res], %[tmp]\n\t" /* Count leading zeros of reversed */
        : [res]"=r"(result),
        [tmp]"=r"(x)
        : [x]"r"(x)
    );
    return result;
}

/* Population count (count set bits) */
static inline int vm_popcnt(cell_t x) {
    int result;
    __asm__(
        "fmov    d0, %[x]\n\t" /* Move to NEON register */
        "cnt     v0.8b, v0.8b\n\t" /* Count bits in each byte */
        "addv    b0, v0.8b\n\t" /* Sum all bytes */
        "fmov    %w[res], s0\n\t" /* Move back to general register */
        : [res]"=r"(result)
        : [x]"r"(x)
        : "v0"
    );
    return result;
}

/* ============================================================================
 * OPTIMIZATION 6: CPU Feature Detection
 * ============================================================================
 *
 * ARM64 uses a different mechanism than x86 CPUID
 *
 * ============================================================================
 */

/* Read ARM64 CPU ID register */
static inline uint64_t vm_read_midr(void) {
    uint64_t midr;
    __asm__("mrs %0, midr_el1": "=r"(midr));
    return midr;
}

/* Check for specific CPU features via auxiliary control register */
static inline int vm_has_neon(void) {



#ifdef __ARM_NEON
return 1;
#else
return 0;
#endif
}

/* Check for Cortex-A72 (Raspberry Pi 4) */
static inline int vm_is_cortex_a72(void) {



/* Cortex-A72 MIDR: 0x410FD083 */
/* Note: Reading MIDR requires EL1, may not work in userspace */
/* Use hwcaps instead for userspace detection */
#ifdef __linux__
unsigned long hwcap = getauxval(AT_HWCAP);
    return (hwcap &HWCAP_ASIMD)!= 0; /* ASIMD = Advanced SIMD (NEON) */
#else
return 0;
#endif
}

/* ============================================================================
 * OPTIMIZATION 7: Cache Management (Raspberry Pi 4 specific)
 * ============================================================================
 *
 * Raspberry Pi 4 cache hierarchy:
 * - L1I: 32KB (3-way, 64-byte lines)
 * - L1D: 32KB (2-way, 64-byte lines)
 * - L2: 1MB (16-way, 64-byte lines)
 *
 * ============================================================================
 */

/* Prefetch data into cache */
static inline void vm_prefetch(const void *addr) {
    __asm__ __volatile__(
        "prfm    pldl1keep, [%0]\n\t" /* Prefetch for load, L1 cache, temporal */
        :
        : "r"(addr)
        : "memory"
    );
}

/* Prefetch with streaming hint (for sequential access) */
static inline void vm_prefetch_stream(const void *addr) {
    __asm__ __volatile__(
        "prfm    pldl1strm, [%0]\n\t" /* Prefetch for load, L1 cache, streaming */
        :
        : "r"(addr)
        : "memory"
    );
}

/* Data cache zero (fast way to zero cache lines) */
static inline void vm_dc_zva(void *addr) {
    __asm__ __volatile__(
        "dc      zva, %0\n\t" /* Zero entire cache line (64 bytes) */
        :
        : "r"(addr)
        : "memory"
    );
}

#else /* !USE_ASM_OPT */

/* Fallback to standard implementations */
#define vm_push_asm(vm, val) vm_push(vm, val)
#define vm_pop_asm(vm) vm_pop(vm)
#define vm_rpush_asm(vm, val) vm_rpush(vm, val)
#define vm_rpop_asm(vm) vm_rpop(vm)

#endif /* USE_ASM_OPT */

/* ============================================================================
 * RASPBERRY PI 4 SPECIFIC NOTES
 * ============================================================================
 *
 * CPU: Broadcom BCM2711 (Quad-core Cortex-A72 @ 1.5GHz)
 * Architecture: ARMv8-A
 * L1 Cache: 32KB I + 32KB D per core
 * L2 Cache: 1MB shared
 * RAM: 1GB/2GB/4GB/8GB LPDDR4-3200
 *
 * Optimization Tips:
 * 1. Align hot data to 64-byte cache lines
 * 2. Use NEON for parallel operations
 * 3. Prefetch sequential data access
 * 4. Keep hot code < 32KB (fits in L1I)
 * 5. Use load/store pairs (ldp/stp) for efficiency
 *
 * Thermal Throttling:
 * - CPU throttles at 80°C
 * - Add heatsink for sustained performance
 * - Monitor: vcgencmd measure_temp
 *
 * Build Flags:
 *   -march=armv8-a+crc+simd -mtune=cortex-a72 -O3 -DUSE_ASM_OPT=1
 *
 * Cross-compilation:
 *   CC=aarch64-linux-gnu-gcc
 *
 * ============================================================================
 */

#endif /* VM_ASM_OPT_ARM64_H */