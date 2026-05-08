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

#ifndef VM_ASM_OPT_RISCV64_H
#define VM_ASM_OPT_RISCV64_H

#include "vm.h"
#include <stdint.h>

/*
 * Configuration: Define USE_ASM_OPT=1 to enable assembly optimizations
 */
#ifndef USE_ASM_OPT
#define USE_ASM_OPT 0
#endif

/*
 * RISC-V 64 Architecture Notes:
 * ==============================
 *
 * Register ABI (RV64GC / rv64gc):
 * - x0  (zero): hardwired zero
 * - x1  (ra):   return address
 * - x2  (sp):   stack pointer
 * - x3  (gp):   global pointer
 * - x4  (tp):   thread pointer
 * - x5–x7  (t0–t2):  temporaries (caller-saved)
 * - x8–x9  (s0–s1):  callee-saved / frame pointer
 * - x10–x17 (a0–a7): argument/return (caller-saved)
 * - x18–x27 (s2–s11): callee-saved
 * - x28–x31 (t3–t6):  temporaries (caller-saved)
 *
 * Key differences vs x86_64 / ARM64:
 * - Pure load/store architecture — all ops work on registers
 * - No flags register; overflow detection requires explicit instructions
 * - No conditional move in base ISA (Zicond ext adds czero.eqz/czero.nez)
 * - No string-comparison instructions; byte loops required
 * - M extension: mul, mulh, div, rem — all present in rv64gc
 * - B extension (Zba/Zbb/Zbc): clz, ctz, cpop, etc. — NOT guaranteed in rv64gc
 *
 * Branchless min/max without Zicond:
 *   slt t0, a, b        # t0 = (a < b) ? 1 : 0
 *   neg t0, t0          # t0 = (a < b) ? -1 : 0  (all-ones mask or zero)
 *   sub t1, a, b        # t1 = a - b
 *   and t1, t1, t0      # t1 = (a-b) if a<b, else 0
 *   add res, b, t1      # res = b + (a-b if a<b) = min(a,b)
 *
 * Overflow detection (no flags):
 *   add res, a, b
 *   # signed overflow iff same-sign operands produce opposite-sign result
 *   xor t0, a, b        # t0[63]=0 means same sign
 *   xor t1, a, res      # t1[63]=1 means sign changed
 *   srai t0, t0, 63     # 0 or -1
 *   srai t1, t1, 63     # 0 or -1
 *   xori t0, t0, -1     # invert: -1 -> 0, 0 -> -1
 *   and  ovf, t0, t1    # -1 iff overflow
 *   snez ovf, ovf       # 0 or 1
 */

#if USE_ASM_OPT

/* ============================================================================
 * OPTIMIZATION 1: Stack Operations
 * ============================================================================
 */

static inline void vm_push_asm(VM *vm, cell_t value) {
    int error = 0;

    __asm__ __volatile__(
        "lw      t0, %[dsp]\n\t"          /* t0 = dsp */
        "li      t1, 1022\n\t"
        "bge     t0, t1, 1f\n\t"          /* branch if dsp >= 1022 (overflow) */
        "addi    t0, t0, 1\n\t"           /* dsp++ */
        "sw      t0, %[dsp]\n\t"
        "slli    t1, t0, 3\n\t"           /* t1 = dsp * 8 */
        "add     t1, %[stack], t1\n\t"    /* t1 = &data_stack[dsp] */
        "sd      %[val], 0(t1)\n\t"       /* store value */
        "sw      zero, %[err]\n\t"
        "j       2f\n\t"
        "1:\n\t"
        "li      t0, 1\n\t"
        "sw      t0, %[err]\n\t"
        "2:\n\t"
        : [dsp]"+m"(vm->dsp),
          [err]"=m"(error)
        : [val]"r"(value),
          [stack]"r"(vm->data_stack)
        : "t0", "t1", "memory"
    );

    if (__builtin_expect(error, 0)) {
        vm->error = 1;
    }
}

static inline cell_t vm_pop_asm(VM *vm) {
    cell_t value = 0;
    int error = 0;

    __asm__ __volatile__(
        "lw      t0, %[dsp]\n\t"
        "bltz    t0, 1f\n\t"              /* branch if dsp < 0 (underflow) */
        "slli    t1, t0, 3\n\t"
        "add     t1, %[stack], t1\n\t"
        "ld      %[val], 0(t1)\n\t"
        "addi    t0, t0, -1\n\t"
        "sw      t0, %[dsp]\n\t"
        "sw      zero, %[err]\n\t"
        "j       2f\n\t"
        "1:\n\t"
        "li      t0, 1\n\t"
        "sw      t0, %[err]\n\t"
        "mv      %[val], zero\n\t"
        "2:\n\t"
        : [dsp]"+m"(vm->dsp),
          [val]"=r"(value),
          [err]"=m"(error)
        : [stack]"r"(vm->data_stack)
        : "t0", "t1", "memory"
    );

    if (__builtin_expect(error, 0)) {
        vm->error = 1;
    }

    return value;
}

static inline void vm_rpush_asm(VM *vm, cell_t value) {
    int error = 0;

    __asm__ __volatile__(
        "lw      t0, %[rsp]\n\t"
        "li      t1, 1022\n\t"
        "bge     t0, t1, 1f\n\t"
        "addi    t0, t0, 1\n\t"
        "sw      t0, %[rsp]\n\t"
        "slli    t1, t0, 3\n\t"
        "add     t1, %[stack], t1\n\t"
        "sd      %[val], 0(t1)\n\t"
        "sw      zero, %[err]\n\t"
        "j       2f\n\t"
        "1:\n\t"
        "li      t0, 1\n\t"
        "sw      t0, %[err]\n\t"
        "2:\n\t"
        : [rsp]"+m"(vm->rsp),
          [err]"=m"(error)
        : [val]"r"(value),
          [stack]"r"(vm->return_stack)
        : "t0", "t1", "memory"
    );

    if (__builtin_expect(error, 0)) {
        vm->error = 1;
    }
}

static inline cell_t vm_rpop_asm(VM *vm) {
    cell_t value = 0;
    int error = 0;

    __asm__ __volatile__(
        "lw      t0, %[rsp]\n\t"
        "bltz    t0, 1f\n\t"
        "slli    t1, t0, 3\n\t"
        "add     t1, %[stack], t1\n\t"
        "ld      %[val], 0(t1)\n\t"
        "addi    t0, t0, -1\n\t"
        "sw      t0, %[rsp]\n\t"
        "sw      zero, %[err]\n\t"
        "j       2f\n\t"
        "1:\n\t"
        "li      t0, 1\n\t"
        "sw      t0, %[err]\n\t"
        "mv      %[val], zero\n\t"
        "2:\n\t"
        : [rsp]"+m"(vm->rsp),
          [val]"=r"(value),
          [err]"=m"(error)
        : [stack]"r"(vm->return_stack)
        : "t0", "t1", "memory"
    );

    if (__builtin_expect(error, 0)) {
        vm->error = 1;
    }

    return value;
}

/* ============================================================================
 * OPTIMIZATION 2: Arithmetic with Overflow Detection
 * ============================================================================
 *
 * RISC-V has no flags register. Overflow is detected by inspecting operand
 * and result sign bits directly.
 *
 * Signed addition overflows iff operands have the same sign and the result
 * has the opposite sign:
 *   ovf = ~(a XOR b)[63] AND (a XOR result)[63]
 * ============================================================================
 */

static inline int vm_add_check_overflow(cell_t a, cell_t b, cell_t *result) {
    cell_t res;
    cell_t ovf;

    __asm__(
        "add     %[res], %[a], %[b]\n\t"
        "xor     t0, %[a], %[b]\n\t"      /* t0[63]=0 if same sign */
        "xor     t1, %[a], %[res]\n\t"    /* t1[63]=1 if sign changed */
        "not     t0, t0\n\t"              /* invert: same-sign -> MSB=1 */
        "and     t0, t0, t1\n\t"          /* both conditions met */
        "srli    %[ovf], t0, 63\n\t"      /* extract MSB as 0 or 1 */
        : [res]"=r"(res), [ovf]"=r"(ovf)
        : [a]"r"(a), [b]"r"(b)
        : "t0", "t1"
    );

    *result = res;
    return (int)ovf;
}

static inline int vm_sub_check_overflow(cell_t a, cell_t b, cell_t *result) {
    cell_t res;
    cell_t ovf;

    __asm__(
        "sub     %[res], %[a], %[b]\n\t"
        /* Signed subtraction a-b overflows iff a and b have different signs
         * AND result has different sign from a:
         *   ovf = (a XOR b)[63] AND (a XOR result)[63]  */
        "xor     t0, %[a], %[b]\n\t"      /* t0[63]=1 if different sign */
        "xor     t1, %[a], %[res]\n\t"    /* t1[63]=1 if result sign differs */
        "and     t0, t0, t1\n\t"
        "srli    %[ovf], t0, 63\n\t"
        : [res]"=r"(res), [ovf]"=r"(ovf)
        : [a]"r"(a), [b]"r"(b)
        : "t0", "t1"
    );

    *result = res;
    return (int)ovf;
}

/* RV64M: mulh gives the high 64 bits of signed 64×64 product */
static inline void vm_mul_double(cell_t a, cell_t b, cell_t *hi, cell_t *lo) {
    __asm__(
        "mul     %[lo], %[a], %[b]\n\t"   /* low 64 bits */
        "mulh    %[hi], %[a], %[b]\n\t"   /* high 64 bits (signed) */
        : [lo]"=r"(*lo), [hi]"=r"(*hi)
        : [a]"r"(a), [b]"r"(b)
    );
}

/* RV64M: div and rem */
static inline void vm_divmod(cell_t dividend, cell_t divisor,
                              cell_t *quotient, cell_t *remainder) {
    cell_t quot, rem;
    __asm__(
        "div     %[quot], %[dividend], %[divisor]\n\t"
        "rem     %[rem],  %[dividend], %[divisor]\n\t"
        : [quot]"=r"(quot), [rem]"=r"(rem)
        : [dividend]"r"(dividend), [divisor]"r"(divisor)
    );
    *quotient = quot;
    *remainder = rem;
}

/* ============================================================================
 * OPTIMIZATION 3: String Operations
 * ============================================================================
 *
 * RISC-V has no string or SIMD instructions in the base ISA (rv64gc).
 * We write tight scalar loops in asm to avoid libc call overhead inside
 * the inner interpreter hot path.
 * ============================================================================
 */

static inline int vm_strcmp_asm(const char *s1, const char *s2, size_t len) {
    int result = 0;
    __asm__ __volatile__(
        "beqz    %[len], 2f\n\t"
        "1:\n\t"
        "lb      t0, 0(%[s1])\n\t"
        "lb      t1, 0(%[s2])\n\t"
        "bne     t0, t1, 3f\n\t"
        "addi    %[s1], %[s1], 1\n\t"
        "addi    %[s2], %[s2], 1\n\t"
        "addi    %[len], %[len], -1\n\t"
        "bnez    %[len], 1b\n\t"
        "2:\n\t"
        "mv      %[res], zero\n\t"
        "j       4f\n\t"
        "3:\n\t"
        "li      %[res], 1\n\t"
        "4:\n\t"
        : [res]"=r"(result),
          [s1]"+r"(s1), [s2]"+r"(s2), [len]"+r"(len)
        :
        : "t0", "t1", "memory"
    );
    return result;
}

static inline void vm_memcpy_asm(void *dest, const void *src, size_t len) {
    /* Copy 8 bytes at a time, then handle remainder */
    __asm__ __volatile__(
        "srli    t2, %[len], 3\n\t"       /* t2 = len / 8 */
        "beqz    t2, 2f\n\t"
        "1:\n\t"
        "ld      t0, 0(%[src])\n\t"
        "sd      t0, 0(%[dst])\n\t"
        "addi    %[src], %[src], 8\n\t"
        "addi    %[dst], %[dst], 8\n\t"
        "addi    t2, t2, -1\n\t"
        "bnez    t2, 1b\n\t"
        "2:\n\t"
        "andi    %[len], %[len], 7\n\t"   /* remaining bytes */
        "beqz    %[len], 4f\n\t"
        "3:\n\t"
        "lb      t0, 0(%[src])\n\t"
        "sb      t0, 0(%[dst])\n\t"
        "addi    %[src], %[src], 1\n\t"
        "addi    %[dst], %[dst], 1\n\t"
        "addi    %[len], %[len], -1\n\t"
        "bnez    %[len], 3b\n\t"
        "4:\n\t"
        : [src]"+r"(src), [dst]"+r"(dest), [len]"+r"(len)
        :
        : "t0", "t2", "memory"
    );
}

static inline void vm_memzero_asm(void *dest, size_t len) {
    __asm__ __volatile__(
        "srli    t2, %[len], 3\n\t"
        "beqz    t2, 2f\n\t"
        "1:\n\t"
        "sd      zero, 0(%[dst])\n\t"
        "addi    %[dst], %[dst], 8\n\t"
        "addi    t2, t2, -1\n\t"
        "bnez    t2, 1b\n\t"
        "2:\n\t"
        "andi    %[len], %[len], 7\n\t"
        "beqz    %[len], 4f\n\t"
        "3:\n\t"
        "sb      zero, 0(%[dst])\n\t"
        "addi    %[dst], %[dst], 1\n\t"
        "addi    %[len], %[len], -1\n\t"
        "bnez    %[len], 3b\n\t"
        "4:\n\t"
        : [dst]"+r"(dest), [len]"+r"(len)
        :
        : "t2", "memory"
    );
}

/* ============================================================================
 * OPTIMIZATION 4: Branchless Min/Max
 * ============================================================================
 *
 * RISC-V base ISA has no conditional move. The bit-mask trick avoids branches:
 *   mask = -(a < b)   →  all-ones if a<b, zero otherwise
 *   min  = b + (a-b) & mask
 * ============================================================================
 */

static inline cell_t vm_min_asm(cell_t a, cell_t b) {
    cell_t result;
    __asm__(
        "slt     t0, %[a], %[b]\n\t"      /* t0 = (a < b) ? 1 : 0 */
        "neg     t0, t0\n\t"              /* t0 = mask (-1 or 0) */
        "sub     t1, %[a], %[b]\n\t"
        "and     t1, t1, t0\n\t"
        "add     %[res], %[b], t1\n\t"
        : [res]"=r"(result)
        : [a]"r"(a), [b]"r"(b)
        : "t0", "t1"
    );
    return result;
}

static inline cell_t vm_max_asm(cell_t a, cell_t b) {
    cell_t result;
    __asm__(
        "slt     t0, %[b], %[a]\n\t"      /* t0 = (b < a) ? 1 : 0  →  (a > b) */
        "neg     t0, t0\n\t"
        "sub     t1, %[a], %[b]\n\t"
        "and     t1, t1, t0\n\t"
        "add     %[res], %[b], t1\n\t"
        : [res]"=r"(result)
        : [a]"r"(a), [b]"r"(b)
        : "t0", "t1"
    );
    return result;
}

static inline cell_t vm_abs_asm(cell_t a) {
    cell_t result;
    __asm__(
        "srai    t0, %[a], 63\n\t"        /* t0 = sign mask (-1 or 0) */
        "xor     %[res], %[a], t0\n\t"    /* flip bits if negative */
        "sub     %[res], %[res], t0\n\t"  /* add 1 if negative (two's complement) */
        : [res]"=r"(result)
        : [a]"r"(a)
        : "t0"
    );
    return result;
}

/* ============================================================================
 * OPTIMIZATION 5: Bit Manipulation
 * ============================================================================
 *
 * rv64gc does NOT include the B extension (Zbb), so clz/ctz/cpop are not
 * available as single instructions. GCC builtins map to the best available
 * form — if the toolchain targets Zbb they'll lower to single instructions;
 * on plain rv64gc they fall back to efficient multi-instruction sequences.
 * ============================================================================
 */

static inline int vm_clz(cell_t x) {
    return __builtin_clzll((unsigned long long)x);
}

static inline int vm_ctz(cell_t x) {
    return __builtin_ctzll((unsigned long long)x);
}

static inline int vm_popcnt(cell_t x) {
    return __builtin_popcountll((unsigned long long)x);
}

/* ============================================================================
 * OPTIMIZATION 6: Cache Operations
 * ============================================================================
 *
 * RISC-V uses fence.i for instruction cache coherence.
 * There is no user-space prefetch instruction in the base ISA;
 * we use GCC's __builtin_prefetch which lowers to a hint if the target
 * supports one (e.g., Zicbop extension), otherwise it's a no-op.
 * ============================================================================
 */

static inline void vm_prefetch(const void *addr) {
    __builtin_prefetch(addr, 0, 3);
}

static inline void vm_prefetch_stream(const void *addr) {
    __builtin_prefetch(addr, 0, 0);
}

/* ============================================================================
 * OPTIMIZATION 7: CPU Feature Detection
 * ============================================================================
 *
 * RISC-V has no CPUID equivalent in user space. Extension detection requires
 * either kernel support (/proc/cpuinfo, riscv_hwprobe syscall on Linux 6.4+)
 * or compiler defines. We expose a simple interface here.
 * ============================================================================
 */

static inline int vm_has_riscv_m_ext(void) {
#if defined(__riscv_mul)
    return 1;
#else
    return 0;
#endif
}

static inline int vm_has_riscv_b_ext(void) {
#if defined(__riscv_zbb)
    return 1;
#else
    return 0;
#endif
}

#else /* !USE_ASM_OPT */

#define vm_push_asm(vm, val)    vm_push(vm, val)
#define vm_pop_asm(vm)          vm_pop(vm)
#define vm_rpush_asm(vm, val)   vm_rpush(vm, val)
#define vm_rpop_asm(vm)         vm_rpop(vm)

#endif /* USE_ASM_OPT */

#endif /* VM_ASM_OPT_RISCV64_H */
