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

#ifndef VM_INNER_INTERP_ARM64_H
#define VM_INNER_INTERP_ARM64_H

#include "vm.h"

/*
 * ARM64 DIRECT-THREADED INNER INTERPRETER
 * ========================================
 *
 * ARM64 advantages over x86_64:
 * - More registers (31 vs 16)
 * - Better branch prediction
 * - Conditional execution
 * - Load/store with auto-increment
 * - Lower power consumption
 *
 * Register Allocation (using callee-saved registers):
 * - x19: VM pointer (callee-saved)
 * - x20: Instruction Pointer (IP)
 * - x21: Data Stack Pointer (DSP) - actual pointer
 * - x22: Return Stack Pointer (RSP) - actual pointer
 * - x23: Top of Stack cache (TOS) - keep in register
 * - x24-x28: Available for future use
 *
 * Why more registers help:
 * - Can keep TOS in register (x23)
 * - More scratch registers for complex operations
 * - Less memory traffic
 *
 * Performance on Raspberry Pi 4:
 * - Expected 3-5x speedup for threaded code
 * - Better than x86_64 due to more registers
 * - Lower power = less thermal throttling
 */

#ifndef USE_DIRECT_THREADING
#define USE_DIRECT_THREADING 0
#endif

#if USE_DIRECT_THREADING

/*
 * NEXT macro - ARM64 version
 *
 * On ARM64, we can use:
 * - Load with post-increment (ldr x0, [x20], #8)
 * - Indirect branch (br x0)
 */
#define NEXT_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x20], #8\n\t"       /* Load word ptr from [IP], IP += 8 */ \
        "br      x0\n\t"                  /* Branch to word */ \
        ::: "x0", "memory" \
    )

/*
 * DOCOL - Enter a colon definition (ARM64)
 */
static inline void vm_docol_arm64(VM *vm) {
    __asm__ __volatile__(
        /* Save current IP on return stack */
        "str     x20, [x22], #8\n\t" /* [RSP] = IP, RSP += 8 */

        /* IP = address of colon definition body */
        "ldr     x0, %[entry]\n\t"
        "ldr     x20, [x0]\n\t" /* IP = entry->code_field */

        /* Continue with NEXT */
        "ldr     x0, [x20], #8\n\t"
        "br      x0\n\t"
        :
        : [entry]"m"(vm->current_executing_entry)
        : "x0", "x20", "x22", "memory"
    );
}

/*
 * EXIT - Return from a colon definition (ARM64)
 */
static inline void vm_exit_arm64(void) {
    __asm__ __volatile__(
        /* Restore IP from return stack */
        "ldr     x20, [x22, #-8]!\n\t" /* RSP -= 8, IP = [RSP] (pre-decrement) */

        /* Continue with NEXT */
        "ldr     x0, [x20], #8\n\t"
        "br      x0\n\t"
        ::: "x0", "x20", "x22", "memory"
    );
}

/*
 * Setup registers before entering inner loop
 */
static inline void vm_setup_registers_arm64(VM *vm) {
    __asm__ __volatile__(
        /* Save callee-saved registers (required by ARM64 ABI) */
        "stp     x19, x20, [sp, #-80]!\n\t"
        "stp     x21, x22, [sp, #16]\n\t"
        "stp     x23, x24, [sp, #32]\n\t"
        "stp     x25, x26, [sp, #48]\n\t"
        "stp     x27, x28, [sp, #64]\n\t"

        /* Load VM pointer */
        "mov     x19, %[vm]\n\t" /* x19 = VM pointer */

        /* Load IP */
        "ldr     x20, %[ip]\n\t" /* x20 = IP */

        /* Calculate data stack pointer */
        "ldr     w0, %[dsp]\n\t"
        "ldr     x1, %[dstack]\n\t"
        "add     x21, x1, x0, sxtw #3\n\t" /* x21 = &data_stack[dsp] */

        /* Calculate return stack pointer */
        "ldr     w0, %[rsp]\n\t"
        "ldr     x1, %[rstack]\n\t"
        "add     x22, x1, x0, sxtw #3\n\t" /* x22 = &return_stack[rsp] */

        /* Load TOS into x23 if stack not empty */
        "ldr     w0, %[dsp]\n\t"
        "cmp     w0, #0\n\t"
        "b.lt    1f\n\t"
        "ldr     x23, [x21]\n\t" /* x23 = TOS (cached) */
        "1:\n\t"
        :
        : [vm]"r"(vm),
        [ip]"m"(vm->ip),
        [dsp]"m"(vm->dsp),
        [rsp]"m"(vm->rsp),
        [dstack]"m"(vm->data_stack),
        [rstack]"m"(vm->return_stack)
        : "x0", "x1", "x19", "x20", "x21", "x22", "x23", "x24",
        "x25", "x26", "x27", "x28", "memory"
    );
}

/*
 * Save registers back to VM structure
 */
static inline void vm_save_registers_arm64(VM *vm) {
    __asm__ __volatile__(
        /* Save TOS back to stack */
        "str     x23, [x21]\n\t"

        /* Save IP */
        "str     x20, %[ip]\n\t"

        /* Calculate and save dsp */
        "ldr     x0, %[dstack]\n\t"
        "sub     x1, x21, x0\n\t"
        "lsr     x1, x1, #3\n\t" /* / 8 */
        "str     w1, %[dsp]\n\t"

        /* Calculate and save rsp */
        "ldr     x0, %[rstack]\n\t"
        "sub     x1, x22, x0\n\t"
        "lsr     x1, x1, #3\n\t"
        "str     w1, %[rsp]\n\t"

        /* Restore callee-saved registers */
        "ldp     x27, x28, [sp, #64]\n\t"
        "ldp     x25, x26, [sp, #48]\n\t"
        "ldp     x23, x24, [sp, #32]\n\t"
        "ldp     x21, x22, [sp, #16]\n\t"
        "ldp     x19, x20, [sp], #80\n\t"
        : [ip]"=m"(vm->ip),
        [dsp]"=m"(vm->dsp),
        [rsp]"=m"(vm->rsp)
        : [dstack]"m"(vm->data_stack),
        [rstack]"m"(vm->return_stack)
        : "x0", "x1", "memory"
    );
}

/**
 * @defgroup primitives Fast Primitive Word Implementations
 * @brief Register-based stack primitives optimized for ARM64
 *
 * Key optimization: TOS stays in x23, avoiding memory traffic
 * @{
 */

/** @brief DUP primitive - duplicates top of stack item */
#define PRIM_DUP_ARM64() \
    __asm__ __volatile__ ( \
        "str     x23, [x21, #8]!\n\t"     /* Store TOS, advance DSP */ \
        ::: "x21", "memory" \
    )

/* DROP - remove top of stack */
#define PRIM_DROP_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x23, [x21], #-8\n\t"     /* Load new TOS, decrement DSP */ \
        ::: "x21", "x23", "memory" \
    )

/* SWAP - swap top two stack items */
#define PRIM_SWAP_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x21]\n\t"           /* Load NOS */ \
        "str     x23, [x21]\n\t"          /* Store TOS to NOS position */ \
        "mov     x23, x0\n\t"             /* TOS = old NOS */ \
        ::: "x0", "x23", "memory" \
    )

/* OVER - copy second item to top */
#define PRIM_OVER_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x21]\n\t"           /* Load NOS */ \
        "str     x23, [x21, #8]!\n\t"     /* Store TOS, advance DSP */ \
        "mov     x23, x0\n\t"             /* TOS = NOS */ \
        ::: "x0", "x21", "x23", "memory" \
    )

/* ROT - rotate top three items */
#define PRIM_ROT_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x21]\n\t"           /* Load item2 */ \
        "ldr     x1, [x21, #-8]\n\t"      /* Load item3 */ \
        "str     x1, [x21]\n\t"           /* item3 -> NOS */ \
        "str     x23, [x21, #-8]\n\t"     /* TOS -> item3 */ \
        "mov     x23, x0\n\t"             /* item2 -> TOS */ \
        ::: "x0", "x1", "x23", "memory" \
    )

/* + - add top two stack items */
#define PRIM_PLUS_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x21], #-8\n\t"      /* Load NOS, decrement DSP */ \
        "add     x23, x23, x0\n\t"        /* TOS = TOS + NOS */ \
        ::: "x0", "x21", "x23" \
    )

/* - - subtract top from second */
#define PRIM_MINUS_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x21], #-8\n\t"      /* Load NOS, decrement DSP */ \
        "sub     x23, x0, x23\n\t"        /* TOS = NOS - TOS */ \
        ::: "x0", "x21", "x23" \
    )

/* * - multiply top two stack items */
#define PRIM_STAR_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x21], #-8\n\t"      /* Load NOS, decrement DSP */ \
        "mul     x23, x23, x0\n\t"        /* TOS = TOS * NOS */ \
        ::: "x0", "x21", "x23" \
    )

/* / - divide second by top */
#define PRIM_SLASH_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x21], #-8\n\t"      /* Load NOS (dividend), decrement DSP */ \
        "sdiv    x23, x0, x23\n\t"        /* TOS = NOS / TOS */ \
        ::: "x0", "x21", "x23" \
    )

/* MOD - modulo */
#define PRIM_MOD_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x21], #-8\n\t"      /* Load NOS, decrement DSP */ \
        "sdiv    x1, x0, x23\n\t"         /* quotient = NOS / TOS */ \
        "msub    x23, x1, x23, x0\n\t"    /* TOS = NOS - quotient * TOS */ \
        ::: "x0", "x1", "x21", "x23" \
    )

/* AND - bitwise and */
#define PRIM_AND_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x21], #-8\n\t" \
        "and     x23, x23, x0\n\t" \
        ::: "x0", "x21", "x23" \
    )

/* OR - bitwise or */
#define PRIM_OR_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x21], #-8\n\t" \
        "orr     x23, x23, x0\n\t" \
        ::: "x0", "x21", "x23" \
    )

/* XOR - bitwise xor */
#define PRIM_XOR_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x21], #-8\n\t" \
        "eor     x23, x23, x0\n\t" \
        ::: "x0", "x21", "x23" \
    )

/* INVERT - bitwise not */
#define PRIM_INVERT_ARM64() \
    __asm__ __volatile__ ( \
        "mvn     x23, x23\n\t" \
        ::: "x23" \
    )

/* NEGATE - two's complement */
#define PRIM_NEGATE_ARM64() \
    __asm__ __volatile__ ( \
        "neg     x23, x23\n\t" \
        ::: "x23" \
    )

/* @ - fetch cell from memory */
#define PRIM_FETCH_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x19]\n\t"           /* Load VM->memory */ \
        "ldr     x23, [x0, x23]\n\t"      /* TOS = memory[TOS] */ \
        ::: "x0", "x23", "memory" \
    )

/* ! - store cell to memory */
#define PRIM_STORE_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x21], #-8\n\t"      /* Load address, decrement DSP */ \
        "ldr     x1, [x19]\n\t"           /* Load VM->memory */ \
        "str     x23, [x1, x0]\n\t"       /* memory[addr] = value */ \
        "ldr     x23, [x21]\n\t"          /* Load new TOS */ \
        ::: "x0", "x1", "x21", "x23", "memory" \
    )

/* C@ - fetch byte from memory */
#define PRIM_C_FETCH_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x19]\n\t" \
        "ldrb    w23, [x0, x23]\n\t"      /* Load byte, zero-extend */ \
        ::: "x0", "x23", "memory" \
    )

/* C! - store byte to memory */
#define PRIM_C_STORE_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x21], #-8\n\t" \
        "ldr     x1, [x19]\n\t" \
        "strb    w23, [x1, x0]\n\t"       /* Store byte */ \
        "ldr     x23, [x21]\n\t" \
        ::: "x0", "x1", "x21", "x23", "memory" \
    )

/* >R - push to return stack */
#define PRIM_TO_R_ARM64() \
    __asm__ __volatile__ ( \
        "str     x23, [x22], #8\n\t"      /* Store to RS, advance RSP */ \
        "ldr     x23, [x21], #-8\n\t"     /* Load new TOS, decrement DSP */ \
        ::: "x21", "x22", "x23", "memory" \
    )

/* R> - pop from return stack */
#define PRIM_R_FROM_ARM64() \
    __asm__ __volatile__ ( \
        "str     x23, [x21, #8]!\n\t"     /* Store TOS, advance DSP */ \
        "ldr     x23, [x22, #-8]!\n\t"    /* Load from RS, decrement RSP */ \
        ::: "x21", "x22", "x23", "memory" \
    )

/* R@ - copy top of return stack to data stack */
#define PRIM_R_FETCH_ARM64() \
    __asm__ __volatile__ ( \
        "str     x23, [x21, #8]!\n\t"     /* Store TOS, advance DSP */ \
        "ldr     x23, [x22, #-8]\n\t"     /* Load from RS (don't pop) */ \
        ::: "x21", "x23", "memory" \
    )

/* 2DUP - duplicate top two items */
#define PRIM_2DUP_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x21]\n\t"           /* Load NOS */ \
        "stp     x0, x23, [x21, #8]\n\t"  /* Store pair */ \
        "add     x21, x21, #16\n\t"       /* DSP += 2 */ \
        ::: "x0", "x21", "memory" \
    )

/* 2DROP - drop top two items */
#define PRIM_2DROP_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x23, [x21, #-8]\n\t"     /* Load new TOS */ \
        "sub     x21, x21, #16\n\t"       /* DSP -= 2 */ \
        ::: "x21", "x23", "memory" \
    )

/* 0= - test if zero */
#define PRIM_ZERO_EQUALS_ARM64() \
    __asm__ __volatile__ ( \
        "cmp     x23, #0\n\t" \
        "cset    x23, eq\n\t"             /* Set to 1 if zero, else 0 */ \
        "neg     x23, x23\n\t"            /* Convert to -1 (true flag) */ \
        ::: "x23", "cc" \
    )

/* 0< - test if negative */
#define PRIM_ZERO_LESS_ARM64() \
    __asm__ __volatile__ ( \
        "cmp     x23, #0\n\t" \
        "cset    x23, lt\n\t" \
        "neg     x23, x23\n\t" \
        ::: "x23", "cc" \
    )

/* = - test equality */
#define PRIM_EQUALS_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x21], #-8\n\t" \
        "cmp     x0, x23\n\t" \
        "cset    x23, eq\n\t" \
        "neg     x23, x23\n\t" \
        ::: "x0", "x21", "x23", "cc" \
    )

/* < - test less than (signed) */
#define PRIM_LESS_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x21], #-8\n\t" \
        "cmp     x0, x23\n\t" \
        "cset    x23, lt\n\t" \
        "neg     x23, x23\n\t" \
        ::: "x0", "x21", "x23", "cc" \
    )

/* > - test greater than (signed) */
#define PRIM_GREATER_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x21], #-8\n\t" \
        "cmp     x0, x23\n\t" \
        "cset    x23, gt\n\t" \
        "neg     x23, x23\n\t" \
        ::: "x0", "x21", "x23", "cc" \
    )

/*
 * Control flow primitives
 */

/* BRANCH - unconditional branch */
#define PRIM_BRANCH_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x20]\n\t"           /* Load offset */ \
        "add     x20, x20, x0\n\t"        /* IP += offset */ \
        ::: "x0", "x20", "memory" \
    )

/* 0BRANCH - branch if TOS is zero */
#define PRIM_ZBRANCH_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x0, [x20]\n\t"           /* Load offset */ \
        "cmp     x23, #0\n\t" \
        "add     x1, x20, x0\n\t"         /* Calculate target */ \
        "add     x2, x20, #8\n\t"         /* Calculate skip */ \
        "csel    x20, x1, x2, eq\n\t"     /* Select target if zero */ \
        "ldr     x23, [x21], #-8\n\t"     /* Load new TOS */ \
        ::: "x0", "x1", "x2", "x20", "x21", "x23", "cc", "memory" \
    )

/* EXECUTE - execute word whose xt is on stack */
#define PRIM_EXECUTE_ARM64() \
    __asm__ __volatile__ ( \
        "ldr     x23, [x21], #-8\n\t"     /* Load new TOS */ \
        "br      x0\n\t"                  /* Branch to xt */ \
        ::: "x0", "x21", "x23", "memory" \
    )

#else /* !USE_DIRECT_THREADING */

/* Fallback definitions */
#define NEXT_ARM64() do {} while(0)
#define vm_setup_registers_arm64(vm) do {} while(0)
#define vm_save_registers_arm64(vm) do {} while(0)

#endif /* USE_DIRECT_THREADING */

/*
 * ============================================================================
 * RASPBERRY PI 4 PERFORMANCE TUNING
 * ============================================================================
 *
 * Cache Optimization:
 * -------------------
 * 1. Align VM structure to cache line (64 bytes):
 *    typedef struct VM { ... } __attribute__((aligned(64))) VM;
 *
 * 2. Separate hot/cold data:
 *    - Keep stacks, IP, SP together (hot)
 *    - Move error handling, I/O to separate structure (cold)
 *
 * 3. Prefetch dictionary entries:
 *    vm_prefetch(vm->latest);
 *    vm_prefetch(vm->latest->link);
 *
 * Thermal Management:
 * -------------------
 * 1. Monitor temperature:
 *    vcgencmd measure_temp
 *
 * 2. Add heatsink or fan for sustained load
 *
 * 3. Reduce CPU frequency if needed:
 *    echo 1200000 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq
 *
 * Power Optimization:
 * -------------------
 * 1. Use WFE (Wait For Event) in idle loops
 * 2. Disable unused cores if single-threaded
 * 3. Use NEON for parallel operations (lower power than scalar)
 *
 * Build Configuration:
 * --------------------
 * Native: -march=armv8-a+crc+simd -mtune=cortex-a72
 * Cross:  CC=aarch64-linux-gnu-gcc
 *
 * ============================================================================
 */

#endif /* VM_INNER_INTERP_ARM64_H */