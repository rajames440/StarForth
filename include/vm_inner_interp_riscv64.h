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

#ifndef VM_INNER_INTERP_RISCV64_H
#define VM_INNER_INTERP_RISCV64_H

#include "vm.h"

/*
 * RISC-V 64 DIRECT-THREADED INNER INTERPRETER
 * ============================================
 *
 * Register Allocation (callee-saved s2–s6):
 * - s2 (x18): VM pointer
 * - s3 (x19): Instruction Pointer (IP)
 * - s4 (x20): Data Stack Pointer (DSP) — pointer to TOS cell
 * - s5 (x21): Return Stack Pointer (RSP) — pointer to top return entry
 * - s6 (x22): Top-Of-Stack cache (TOS) — avoids memory round-trip for TOS
 *
 * Using callee-saved registers means the compiler will save/restore them
 * around any C calls made while the inner interpreter is running.
 *
 * Stack convention (matches x86_64 and ARM64 implementations):
 * - Stack grows upward; DSP points to the current TOS cell.
 * - TOS is additionally cached in s6 to avoid one load per operation.
 * - RSP points to the top return-address cell (not past it).
 *
 * NEXT macro (direct-threaded dispatch):
 *   ld   a0, 0(s3)     # a0 = *IP
 *   addi s3, s3, 8     # IP += 8
 *   jr   a0            # jump to code field (jalr x0, a0, 0)
 *
 * Expected performance: 1.5–2.5x vs interpreter loop (fewer branch
 * mispredictions, tighter dispatch path). Less than ARM64 TOS-cache gain
 * because RISC-V lacks post-increment addressing for combined load+advance.
 */

#ifndef USE_DIRECT_THREADING
#define USE_DIRECT_THREADING 0
#endif

#if USE_DIRECT_THREADING

/*
 * NEXT — fetch and dispatch the next word
 *
 * 'jr a0' is a pseudoinstruction for 'jalr x0, a0, 0' which discards
 * the return address (x0 is hardwired zero).
 */
#define NEXT_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s3)\n\t"    /* a0 = *IP                */ \
        "addi    s3, s3, 8\n\t"    /* IP++                    */ \
        "jr      a0\n\t"           /* jump to code field      */ \
        ::: "a0", "memory" \
    )

/*
 * DOCOL — enter a colon definition
 */
static inline void vm_docol_rv64(VM *vm) {
    __asm__ __volatile__(
        /* Push IP onto return stack */
        "addi    s5, s5, 8\n\t"
        "sd      s3, 0(s5)\n\t"

        /* Load new IP from current_executing_entry->code_field */
        "ld      a0, %[entry]\n\t"
        "ld      s3, 0(a0)\n\t"

        /* NEXT */
        "ld      a0, 0(s3)\n\t"
        "addi    s3, s3, 8\n\t"
        "jr      a0\n\t"
        :
        : [entry]"m"(vm->current_executing_entry)
        : "a0", "s3", "s5", "memory"
    );
}

/*
 * EXIT — return from a colon definition
 */
static inline void vm_exit_rv64(void) {
    __asm__ __volatile__(
        /* Pop IP from return stack */
        "ld      s3, 0(s5)\n\t"
        "addi    s5, s5, -8\n\t"

        /* NEXT */
        "ld      a0, 0(s3)\n\t"
        "addi    s3, s3, 8\n\t"
        "jr      a0\n\t"
        ::: "a0", "s3", "s5", "memory"
    );
}

/*
 * Setup registers before entering the inner interpreter loop.
 * Saves all callee-saved registers we're about to clobber, then
 * loads VM state into s2–s6.
 */
static inline void vm_setup_registers_rv64(VM *vm) {
    __asm__ __volatile__(
        /* Save callee-saved regs we're about to use */
        "addi    sp, sp, -80\n\t"
        "sd      s2,  0(sp)\n\t"
        "sd      s3,  8(sp)\n\t"
        "sd      s4, 16(sp)\n\t"
        "sd      s5, 24(sp)\n\t"
        "sd      s6, 32(sp)\n\t"
        "sd      s7, 40(sp)\n\t"
        "sd      s8, 48(sp)\n\t"
        "sd      s9, 56(sp)\n\t"
        "sd      s10,64(sp)\n\t"
        "sd      s11,72(sp)\n\t"

        /* s2 = VM pointer */
        "mv      s2, %[vm]\n\t"

        /* s3 = IP */
        "ld      s3, %[ip]\n\t"

        /* s4 = &data_stack[dsp] */
        "lw      a0, %[dsp]\n\t"
        "ld      a1, %[dstack]\n\t"
        "slli    a0, a0, 3\n\t"
        "add     s4, a1, a0\n\t"

        /* s5 = &return_stack[rsp] */
        "lw      a0, %[rsp]\n\t"
        "ld      a1, %[rstack]\n\t"
        "slli    a0, a0, 3\n\t"
        "add     s5, a1, a0\n\t"

        /* s6 = TOS (load if stack non-empty) */
        "lw      a0, %[dsp]\n\t"
        "bltz    a0, 1f\n\t"
        "ld      s6, 0(s4)\n\t"
        "1:\n\t"
        :
        : [vm]"r"(vm),
          [ip]"m"(vm->ip),
          [dsp]"m"(vm->dsp),
          [rsp]"m"(vm->rsp),
          [dstack]"m"(vm->data_stack),
          [rstack]"m"(vm->return_stack)
        : "a0", "a1",
          "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11",
          "memory"
    );
}

/*
 * Save registers back to VM structure and restore callee-saved regs.
 */
static inline void vm_save_registers_rv64(VM *vm) {
    __asm__ __volatile__(
        /* Flush TOS back to stack */
        "sd      s6, 0(s4)\n\t"

        /* Save IP */
        "sd      s3, %[ip]\n\t"

        /* dsp = (s4 - data_stack_base) / 8 */
        "ld      a0, %[dstack]\n\t"
        "sub     a1, s4, a0\n\t"
        "srli    a1, a1, 3\n\t"
        "sw      a1, %[dsp]\n\t"

        /* rsp = (s5 - return_stack_base) / 8 */
        "ld      a0, %[rstack]\n\t"
        "sub     a1, s5, a0\n\t"
        "srli    a1, a1, 3\n\t"
        "sw      a1, %[rsp]\n\t"

        /* Restore callee-saved regs */
        "ld      s2,  0(sp)\n\t"
        "ld      s3,  8(sp)\n\t"
        "ld      s4, 16(sp)\n\t"
        "ld      s5, 24(sp)\n\t"
        "ld      s6, 32(sp)\n\t"
        "ld      s7, 40(sp)\n\t"
        "ld      s8, 48(sp)\n\t"
        "ld      s9, 56(sp)\n\t"
        "ld      s10,64(sp)\n\t"
        "ld      s11,72(sp)\n\t"
        "addi    sp, sp, 80\n\t"
        : [ip]"=m"(vm->ip),
          [dsp]"=m"(vm->dsp),
          [rsp]"=m"(vm->rsp)
        : [dstack]"m"(vm->data_stack),
          [rstack]"m"(vm->return_stack)
        : "a0", "a1", "memory"
    );
}

/* ============================================================================
 * Primitive Word Macros
 *
 * TOS is cached in s6. s4 = DSP (pointer to TOS cell). s5 = RSP.
 * s2 = VM pointer (used for memory-relative ops).
 * a0/a1 are scratch (caller-saved, safe to clobber).
 * ============================================================================
 */

/* DUP — duplicate TOS */
#define PRIM_DUP_RV64() \
    __asm__ __volatile__ ( \
        "addi    s4, s4, 8\n\t"    /* DSP++                   */ \
        "sd      s6, 0(s4)\n\t"    /* push TOS copy           */ \
        ::: "s4", "memory" \
    )

/* DROP — discard TOS */
#define PRIM_DROP_RV64() \
    __asm__ __volatile__ ( \
        "ld      s6, -8(s4)\n\t"   /* new TOS = NOS           */ \
        "addi    s4, s4, -8\n\t"   /* DSP--                   */ \
        ::: "s4", "s6", "memory" \
    )

/* SWAP — swap TOS and NOS */
#define PRIM_SWAP_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s4)\n\t"    /* a0 = NOS                */ \
        "sd      s6, 0(s4)\n\t"    /* NOS = old TOS           */ \
        "mv      s6, a0\n\t"       /* TOS = old NOS           */ \
        ::: "a0", "s6", "memory" \
    )

/* OVER — copy NOS to TOS */
#define PRIM_OVER_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s4)\n\t"    /* a0 = NOS                */ \
        "addi    s4, s4, 8\n\t"    /* DSP++                   */ \
        "sd      s6, 0(s4)\n\t"    /* push old TOS            */ \
        "mv      s6, a0\n\t"       /* TOS = NOS               */ \
        ::: "a0", "s4", "s6", "memory" \
    )

/* ROT — rotate top three: ( a b c — b c a ) */
#define PRIM_ROT_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s4)\n\t"    /* a0 = item2 (NOS)        */ \
        "ld      a1, -8(s4)\n\t"   /* a1 = item3              */ \
        "sd      a1, 0(s4)\n\t"    /* item3 -> NOS position   */ \
        "sd      s6, -8(s4)\n\t"   /* TOS -> item3 position   */ \
        "mv      s6, a0\n\t"       /* item2 -> TOS            */ \
        ::: "a0", "a1", "s6", "memory" \
    )

/* + */
#define PRIM_PLUS_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s4)\n\t" \
        "addi    s4, s4, -8\n\t" \
        "add     s6, s6, a0\n\t" \
        ::: "a0", "s4", "s6" \
    )

/* - */
#define PRIM_MINUS_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s4)\n\t"    /* a0 = NOS                */ \
        "addi    s4, s4, -8\n\t" \
        "sub     s6, a0, s6\n\t"   /* TOS = NOS - TOS         */ \
        ::: "a0", "s4", "s6" \
    )

/* * */
#define PRIM_STAR_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s4)\n\t" \
        "addi    s4, s4, -8\n\t" \
        "mul     s6, s6, a0\n\t" \
        ::: "a0", "s4", "s6" \
    )

/* / — signed divide NOS/TOS */
#define PRIM_SLASH_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s4)\n\t"    /* a0 = NOS (dividend)     */ \
        "addi    s4, s4, -8\n\t" \
        "div     s6, a0, s6\n\t"   /* TOS = NOS / TOS         */ \
        ::: "a0", "s4", "s6" \
    )

/* MOD */
#define PRIM_MOD_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s4)\n\t" \
        "addi    s4, s4, -8\n\t" \
        "rem     s6, a0, s6\n\t" \
        ::: "a0", "s4", "s6" \
    )

/* AND */
#define PRIM_AND_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s4)\n\t" \
        "addi    s4, s4, -8\n\t" \
        "and     s6, s6, a0\n\t" \
        ::: "a0", "s4", "s6" \
    )

/* OR */
#define PRIM_OR_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s4)\n\t" \
        "addi    s4, s4, -8\n\t" \
        "or      s6, s6, a0\n\t" \
        ::: "a0", "s4", "s6" \
    )

/* XOR */
#define PRIM_XOR_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s4)\n\t" \
        "addi    s4, s4, -8\n\t" \
        "xor     s6, s6, a0\n\t" \
        ::: "a0", "s4", "s6" \
    )

/* INVERT — bitwise NOT */
#define PRIM_INVERT_RV64() \
    __asm__ __volatile__ ( \
        "not     s6, s6\n\t" \
        ::: "s6" \
    )

/* NEGATE — two's complement */
#define PRIM_NEGATE_RV64() \
    __asm__ __volatile__ ( \
        "neg     s6, s6\n\t" \
        ::: "s6" \
    )

/* @ — fetch cell from VM memory */
#define PRIM_FETCH_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s2)\n\t"    /* a0 = vm->memory         */ \
        "add     a0, a0, s6\n\t"   /* a0 = &memory[TOS]       */ \
        "ld      s6, 0(a0)\n\t"    /* TOS = memory[TOS]       */ \
        ::: "a0", "s6", "memory" \
    )

/* ! — store cell to VM memory */
#define PRIM_STORE_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s4)\n\t"    /* a0 = addr (NOS)         */ \
        "addi    s4, s4, -8\n\t" \
        "ld      a1, 0(s2)\n\t"    /* a1 = vm->memory         */ \
        "add     a1, a1, a0\n\t" \
        "sd      s6, 0(a1)\n\t"    /* memory[addr] = TOS      */ \
        "ld      s6, 0(s4)\n\t"    /* load new TOS            */ \
        "addi    s4, s4, -8\n\t" \
        ::: "a0", "a1", "s4", "s6", "memory" \
    )

/* C@ — fetch byte */
#define PRIM_C_FETCH_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s2)\n\t" \
        "add     a0, a0, s6\n\t" \
        "lbu     s6, 0(a0)\n\t"    /* zero-extend byte        */ \
        ::: "a0", "s6", "memory" \
    )

/* C! — store byte */
#define PRIM_C_STORE_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s4)\n\t"    /* a0 = addr               */ \
        "addi    s4, s4, -8\n\t" \
        "ld      a1, 0(s2)\n\t" \
        "add     a1, a1, a0\n\t" \
        "sb      s6, 0(a1)\n\t" \
        "ld      s6, 0(s4)\n\t" \
        "addi    s4, s4, -8\n\t" \
        ::: "a0", "a1", "s4", "s6", "memory" \
    )

/* >R — push TOS to return stack */
#define PRIM_TO_R_RV64() \
    __asm__ __volatile__ ( \
        "addi    s5, s5, 8\n\t" \
        "sd      s6, 0(s5)\n\t"    /* RS[top] = TOS           */ \
        "ld      s6, 0(s4)\n\t"    /* new TOS = NOS           */ \
        "addi    s4, s4, -8\n\t" \
        ::: "s4", "s5", "s6", "memory" \
    )

/* R> — pop return stack to data stack */
#define PRIM_R_FROM_RV64() \
    __asm__ __volatile__ ( \
        "addi    s4, s4, 8\n\t" \
        "sd      s6, 0(s4)\n\t"    /* push old TOS            */ \
        "ld      s6, 0(s5)\n\t"    /* TOS = RS top            */ \
        "addi    s5, s5, -8\n\t" \
        ::: "s4", "s5", "s6", "memory" \
    )

/* R@ — copy top of return stack (no pop) */
#define PRIM_R_FETCH_RV64() \
    __asm__ __volatile__ ( \
        "addi    s4, s4, 8\n\t" \
        "sd      s6, 0(s4)\n\t" \
        "ld      s6, 0(s5)\n\t"    /* TOS = RS top (no pop)   */ \
        ::: "s4", "s6", "memory" \
    )

/* 2DUP — duplicate top two */
#define PRIM_2DUP_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s4)\n\t"    /* a0 = NOS                */ \
        "addi    s4, s4, 8\n\t" \
        "sd      a0, 0(s4)\n\t"    /* push NOS                */ \
        "addi    s4, s4, 8\n\t" \
        "sd      s6, 0(s4)\n\t"    /* push TOS copy           */ \
        ::: "a0", "s4", "memory" \
    )

/* 2DROP — drop top two */
#define PRIM_2DROP_RV64() \
    __asm__ __volatile__ ( \
        "ld      s6, -8(s4)\n\t"   /* new TOS = item3         */ \
        "addi    s4, s4, -16\n\t" \
        ::: "s4", "s6", "memory" \
    )

/* 0= */
#define PRIM_ZERO_EQUALS_RV64() \
    __asm__ __volatile__ ( \
        "seqz    s6, s6\n\t"       /* 1 if zero, else 0       */ \
        "neg     s6, s6\n\t"       /* convert to -1 (true)    */ \
        ::: "s6" \
    )

/* 0< */
#define PRIM_ZERO_LESS_RV64() \
    __asm__ __volatile__ ( \
        "srai    s6, s6, 63\n\t"   /* -1 if negative, 0 if >= 0 */ \
        ::: "s6" \
    )

/* = */
#define PRIM_EQUALS_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s4)\n\t" \
        "addi    s4, s4, -8\n\t" \
        "sub     s6, a0, s6\n\t" \
        "seqz    s6, s6\n\t" \
        "neg     s6, s6\n\t" \
        ::: "a0", "s4", "s6" \
    )

/* < — NOS < TOS */
#define PRIM_LESS_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s4)\n\t"    /* a0 = NOS                */ \
        "addi    s4, s4, -8\n\t" \
        "slt     s6, a0, s6\n\t"   /* 1 if NOS < TOS          */ \
        "neg     s6, s6\n\t" \
        ::: "a0", "s4", "s6" \
    )

/* > — NOS > TOS */
#define PRIM_GREATER_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s4)\n\t" \
        "addi    s4, s4, -8\n\t" \
        "slt     s6, s6, a0\n\t"   /* 1 if TOS < NOS, i.e. NOS > TOS */ \
        "neg     s6, s6\n\t" \
        ::: "a0", "s4", "s6" \
    )

/* BRANCH — unconditional */
#define PRIM_BRANCH_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s3)\n\t"    /* offset                  */ \
        "add     s3, s3, a0\n\t"   /* IP += offset            */ \
        ::: "a0", "s3", "memory" \
    )

/* 0BRANCH — branch if TOS == 0 */
#define PRIM_ZBRANCH_RV64() \
    __asm__ __volatile__ ( \
        "ld      a0, 0(s3)\n\t"    /* offset                  */ \
        "ld      s6, 0(s4)\n\t"    /* new TOS                 */ \
        "addi    s4, s4, -8\n\t" \
        /* if old TOS was zero: IP += offset; else IP += 8 */ \
        "bnez    s6, 1f\n\t"       /* skip branch if nonzero  */ \
        "add     s3, s3, a0\n\t" \
        "j       2f\n\t" \
        "1:\n\t" \
        "addi    s3, s3, 8\n\t" \
        "2:\n\t" \
        ::: "a0", "s3", "s4", "s6", "memory" \
    )

/* EXECUTE — execute word whose xt is TOS */
#define PRIM_EXECUTE_RV64() \
    __asm__ __volatile__ ( \
        "mv      a0, s6\n\t"       /* a0 = xt                 */ \
        "ld      s6, 0(s4)\n\t"    /* new TOS                 */ \
        "addi    s4, s4, -8\n\t" \
        "jr      a0\n\t"           /* jump to xt              */ \
        ::: "a0", "s4", "s6", "memory" \
    )

#else /* !USE_DIRECT_THREADING */

#define NEXT_RV64()                       do {} while(0)
#define vm_setup_registers_rv64(vm)       do {} while(0)
#define vm_save_registers_rv64(vm)        do {} while(0)

#endif /* USE_DIRECT_THREADING */

#endif /* VM_INNER_INTERP_RISCV64_H */
