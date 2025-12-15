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

/*
                                 ***   StarForth   ***
  vm_inner_interp_asm.h - Optimized Inner Interpreter using Direct Threading
  Compatible with: Linux, L4Re microkernel, bare metal x86_64

  This work is released under license terms in LICENSE file.
  No warranty. Use at your own risk.
*/

#ifndef VM_INNER_INTERP_ASM_H
#define VM_INNER_INTERP_ASM_H

#include "vm.h"

/*
 * INNER INTERPRETER OPTIMIZATION
 * ===============================
 *
 * This is the MOST CRITICAL optimization for Forth performance.
 *
 * Background:
 * -----------
 * Forth uses "threaded code" where compiled words are sequences of pointers
 * to other words. The inner interpreter's job is to:
 * 1. Fetch the next word pointer (IP)
 * 2. Execute that word
 * 3. Increment IP
 * 4. Repeat
 *
 * Traditional C implementation has massive overhead:
 * - Function call/return for each word (push/pop return address)
 * - Register spilling
 * - No tail-call optimization in most C compilers
 *
 * This optimization uses:
 * - Direct threading: Each word ends with a jump to NEXT
 * - Register allocation: Keep IP, SP, RP in registers
 * - Tail calls: Use jmp instead of call/ret
 * - Computed goto: Fast dispatch using indirect jump
 *
 * Performance Impact:
 * ------------------
 * Can improve interpreter speed by 2-5x for threaded code execution
 *
 * L4Re Compatibility: YES
 * All operations are unprivileged and pure computation
 */

#ifndef USE_DIRECT_THREADING
#define USE_DIRECT_THREADING 0
#endif

#if USE_DIRECT_THREADING

/**
 * @brief Register allocation strategy for optimized execution
 * 
 * Register usage:
 * @arg r12 VM pointer (callee-saved, preserved across calls)
 * @arg r13 Instruction Pointer (IP) - points to current cell
 * @arg r14 Data Stack Pointer (DSP) - points to vm->data_stack[vm->dsp]
 * @arg r15 Return Stack Pointer (RSP) - points to vm->return_stack[vm->rsp]
 *
 * Callee-saved registers are used because:
 * - They're preserved across function calls
 * - Allows mixing C and assembly code
 * - No need to save/restore on each word execution
 */

/**
 * @brief Core inner interpreter macro - executes next word
 * @details Fetches next word pointer from IP, increments IP, and jumps to word
 */
#define NEXT_ASM() \
    __asm__ __volatile__ ( \
        "movq    (%%r13), %%rax\n\t"      /* Load word pointer from [IP] */ \
        "addq    $8, %%r13\n\t"           /* IP++ */ \
        "jmpq    *%%rax\n\t"              /* Jump to word's code */ \
        ::: "rax", "memory" \
    )

/**
 * @brief Enter a colon definition (DOCOL)
 * @param vm Pointer to VM instance
 * @details Called when executing a user-defined word
 */
static inline void vm_docol_asm(VM *vm) {
    __asm__ __volatile__(
        /* Save current IP on return stack */
        "movq    %%r13, (%%r15)\n\t" /* *RSP = IP */
        "addq    $8, %%r15\n\t" /* RSP++ */

        /* IP = address of colon definition body (from DictEntry) */
        "movq    %[entry], %%rax\n\t"
        "movq    (%%rax), %%r13\n\t" /* IP = entry->code_field */

        /* Continue with NEXT */
        "movq    (%%r13), %%rax\n\t"
        "addq    $8, %%r13\n\t"
        "jmpq    *%%rax\n\t"
        :
        : [entry]"r"(vm->current_executing_entry)
        : "rax", "r13", "r15", "memory"
    );
}

/**
 * @brief Return from a colon definition (EXIT)
 * @details Pops IP from return stack and continues execution
 */
static inline void vm_exit_asm(void) {
    __asm__ __volatile__(
        /* Restore IP from return stack */
        "subq    $8, %%r15\n\t" /* RSP-- */
        "movq    (%%r15), %%r13\n\t" /* IP = *RSP */

        /* Continue with NEXT */
        "movq    (%%r13), %%rax\n\t"
        "addq    $8, %%r13\n\t"
        "jmpq    *%%rax\n\t"
        ::: "rax", "r13", "r15", "memory"
    );
}

/**
 * @brief Main interpreter loop implementation
 * @details Replaces execute_colon_word function with optimized assembly
 *
 * Usage example:
 * @code
 * vm_setup_registers(vm);
 * VM_INNER_LOOP();
 * @endcode
 */

/* Setup registers before entering inner loop */
static inline void vm_setup_registers(VM *vm) {
    __asm__ __volatile__(
        "movq    %[vm], %%r12\n\t" /* R12 = VM pointer */
        "movq    %[ip], %%r13\n\t" /* R13 = IP */

        /* Calculate actual stack pointers */
        "movl    %[dsp], %%eax\n\t"
        "leaq    (%[dstack], %%rax, 8), %%r14\n\t" /* R14 = &data_stack[dsp] */

        "movl    %[rsp], %%eax\n\t"
        "leaq    (%[rstack], %%rax, 8), %%r15\n\t" /* R15 = &return_stack[rsp] */
        :
        : [vm]"r"(vm),
        [ip]"r"(vm->ip),
        [dsp]"m"(vm->dsp),
        [rsp]"m"(vm->rsp),
        [dstack]"r"(vm->data_stack),
        [rstack]"r"(vm->return_stack)
        : "r12", "r13", "r14", "r15", "rax", "memory"
    );
}

/* Save registers back to VM structure */
static inline void vm_save_registers(VM *vm) {
    __asm__ __volatile__(
        "movq    %%r13, %[ip]\n\t" /* Save IP */

        /* Calculate dsp from R14 */
        "movq    %%r14, %%rax\n\t"
        "subq    %[dstack], %%rax\n\t"
        "shrq    $3, %%rax\n\t" /* / 8 */
        "movl    %%eax, %[dsp]\n\t"

        /* Calculate rsp from R15 */
        "movq    %%r15, %%rax\n\t"
        "subq    %[rstack], %%rax\n\t"
        "shrq    $3, %%rax\n\t"
        "movl    %%eax, %[rsp]\n\t"
        : [ip]"=m"(vm->ip),
        [dsp]"=m"(vm->dsp),
        [rsp]"=m"(vm->rsp)
        : [dstack]"r"(vm->data_stack),
        [rstack]"r"(vm->return_stack)
        : "rax", "memory"
    );
}

/*
 * Fast primitive word implementations using register-based stacks
 *
 * These use the register-allocated stack pointers (R14/R15) directly
 * without going through memory or function calls
 */

/* DUP - duplicate top of stack */
#define PRIM_DUP() \
    __asm__ __volatile__ ( \
        "movq    (%%r14), %%rax\n\t"      /* Load TOS */ \
        "addq    $8, %%r14\n\t"           /* DSP++ */ \
        "movq    %%rax, (%%r14)\n\t"      /* Store duplicate */ \
        ::: "rax", "r14", "memory" \
    )

/* DROP - remove top of stack */
#define PRIM_DROP() \
    __asm__ __volatile__ ( \
        "subq    $8, %%r14\n\t"           /* DSP-- */ \
        ::: "r14" \
    )

/* SWAP - swap top two stack items */
#define PRIM_SWAP() \
    __asm__ __volatile__ ( \
        "movq    (%%r14), %%rax\n\t"      /* Load TOS */ \
        "movq    -8(%%r14), %%rcx\n\t"    /* Load NOS */ \
        "movq    %%rcx, (%%r14)\n\t"      /* Store NOS to TOS */ \
        "movq    %%rax, -8(%%r14)\n\t"    /* Store TOS to NOS */ \
        ::: "rax", "rcx", "memory" \
    )

/* + - add top two stack items */
#define PRIM_PLUS() \
    __asm__ __volatile__ ( \
        "movq    (%%r14), %%rax\n\t"      /* Load TOS */ \
        "addq    %%rax, -8(%%r14)\n\t"    /* Add to NOS */ \
        "subq    $8, %%r14\n\t"           /* DSP-- */ \
        ::: "rax", "r14", "cc", "memory" \
    )

/* - - subtract top from second */
#define PRIM_MINUS() \
    __asm__ __volatile__ ( \
        "movq    (%%r14), %%rax\n\t"      /* Load TOS */ \
        "subq    %%rax, -8(%%r14)\n\t"    /* Subtract from NOS */ \
        "subq    $8, %%r14\n\t"           /* DSP-- */ \
        ::: "rax", "r14", "cc", "memory" \
    )

/* * - multiply top two stack items */
#define PRIM_STAR() \
    __asm__ __volatile__ ( \
        "movq    (%%r14), %%rax\n\t"      /* Load TOS */ \
        "imulq   -8(%%r14), %%rax\n\t"    /* Multiply with NOS */ \
        "movq    %%rax, -8(%%r14)\n\t"    /* Store result */ \
        "subq    $8, %%r14\n\t"           /* DSP-- */ \
        ::: "rax", "rdx", "r14", "cc", "memory" \
    )

/* @ - fetch cell from memory */
#define PRIM_FETCH() \
    __asm__ __volatile__ ( \
        "movq    (%%r14), %%rax\n\t"      /* Load address from TOS */ \
        "movq    (%%r12), %%rcx\n\t"      /* Load VM->memory */ \
        "movq    (%%rcx, %%rax, 1), %%rax\n\t"  /* Load cell from memory */ \
        "movq    %%rax, (%%r14)\n\t"      /* Store to TOS */ \
        ::: "rax", "rcx", "memory" \
    )

/* ! - store cell to memory */
#define PRIM_STORE() \
    __asm__ __volatile__ ( \
        "movq    (%%r14), %%rax\n\t"      /* Load address from TOS */ \
        "movq    -8(%%r14), %%rcx\n\t"    /* Load value from NOS */ \
        "movq    (%%r12), %%rdx\n\t"      /* Load VM->memory */ \
        "movq    %%rcx, (%%rdx, %%rax, 1)\n\t"  /* Store to memory */ \
        "subq    $16, %%r14\n\t"          /* DSP -= 2 */ \
        ::: "rax", "rcx", "rdx", "r14", "memory" \
    )

/* >R - push to return stack */
#define PRIM_TO_R() \
    __asm__ __volatile__ ( \
        "movq    (%%r14), %%rax\n\t"      /* Load from TOS */ \
        "subq    $8, %%r14\n\t"           /* DSP-- */ \
        "movq    %%rax, (%%r15)\n\t"      /* Store to return stack */ \
        "addq    $8, %%r15\n\t"           /* RSP++ */ \
        ::: "rax", "r14", "r15", "memory" \
    )

/* R> - pop from return stack */
#define PRIM_R_FROM() \
    __asm__ __volatile__ ( \
        "subq    $8, %%r15\n\t"           /* RSP-- */ \
        "movq    (%%r15), %%rax\n\t"      /* Load from return stack */ \
        "addq    $8, %%r14\n\t"           /* DSP++ */ \
        "movq    %%rax, (%%r14)\n\t"      /* Store to TOS */ \
        ::: "rax", "r14", "r15", "memory" \
    )

/* R@ - copy top of return stack to data stack */
#define PRIM_R_FETCH() \
    __asm__ __volatile__ ( \
        "movq    -8(%%r15), %%rax\n\t"    /* Load from top of return stack */ \
        "addq    $8, %%r14\n\t"           /* DSP++ */ \
        "movq    %%rax, (%%r14)\n\t"      /* Store to TOS */ \
        ::: "rax", "r14", "memory" \
    )

/*
 * BRANCH and 0BRANCH - Control flow primitives
 * These modify IP based on conditions
 */

/* BRANCH - unconditional branch */
#define PRIM_BRANCH() \
    __asm__ __volatile__ ( \
        "movq    (%%r13), %%rax\n\t"      /* Load offset from IP */ \
        "addq    %%rax, %%r13\n\t"        /* IP += offset */ \
        ::: "rax", "r13", "memory" \
    )

/* 0BRANCH - branch if TOS is zero */
#define PRIM_ZBRANCH() \
    __asm__ __volatile__ ( \
        "movq    (%%r14), %%rax\n\t"      /* Load TOS */ \
        "subq    $8, %%r14\n\t"           /* DSP-- */ \
        "testq   %%rax, %%rax\n\t"        /* Test if zero */ \
        "jnz     1f\n\t"                  /* Skip if non-zero */ \
        "movq    (%%r13), %%rax\n\t"      /* Load offset */ \
        "addq    %%rax, %%r13\n\t"        /* IP += offset */ \
        "jmp     2f\n\t" \
        "1:\n\t" \
        "addq    $8, %%r13\n\t"           /* Skip offset */ \
        "2:\n\t" \
        ::: "rax", "r13", "r14", "cc", "memory" \
    )

#else /* !USE_DIRECT_THREADING */

/* Fallback definitions when direct threading is disabled */
#define NEXT_ASM() do {} while(0)
#define vm_setup_registers(vm) do {} while(0)
#define vm_save_registers(vm) do {} while(0)

#endif /* USE_DIRECT_THREADING */

/*
 * ============================================================================
 * INTEGRATION GUIDE
 * ============================================================================
 *
 * To integrate these optimizations:
 *
 * 1. Enable in your build:
 *    -DUSE_DIRECT_THREADING=1 -DUSE_ASM_OPT=1
 *
 * 2. Modify your inner interpreter in vm.c:
 *
 *    void execute_threaded_code(VM *vm) {
 *        vm_setup_registers(vm);
 *
 *        // Execute words...
 *        // Each primitive word should use PRIM_* macros
 *        // and end with NEXT_ASM()
 *
 *        vm_save_registers(vm);
 *    }
 *
 * 3. For each primitive word, replace function with macro:
 *
 *    Instead of:
 *      void word_dup(VM *vm) { vm_push(vm, vm->data_stack[vm->dsp]); }
 *
 *    Use:
 *      PRIM_DUP(); NEXT_ASM();
 *
 * 4. Benchmark before and after!
 *
 * ============================================================================
 * PERFORMANCE NOTES
 * ============================================================================
 *
 * Expected speedup: 2-5x for threaded code execution
 *
 * Why so fast?
 * - No function call overhead (10+ cycles saved per word)
 * - Register allocation eliminates memory traffic
 * - Direct jumps instead of call/return (better branch prediction)
 * - Cache-friendly: hot path stays in I-cache
 *
 * Trade-offs:
 * - More complex code
 * - Harder to debug (use regular C version for debugging)
 * - Platform-specific (x86_64 only)
 * - Requires more careful register management
 *
 * ============================================================================
 * L4RE INTEGRATION NOTES
 * ============================================================================
 *
 * All safe for L4Re because:
 * - No privileged instructions
 * - No system calls
 * - Preserved ABI (callee-saved registers)
 * - Can be used in both user and kernel contexts
 *
 * For StarshipOS:
 * - These optimizations work in L4Re tasks
 * - Safe for real-time contexts (deterministic timing)
 * - No FPU usage (kernel-safe)
 * - Stack usage is minimal
 *
 * ============================================================================
 */

#endif /* VM_INNER_INTERP_ASM_H */