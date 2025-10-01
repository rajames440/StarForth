/*
                                 ***   StarForth   ***
  vm_asm_opt.h - x86_64 Inline Assembly Optimizations
  Compatible with: Linux, L4Re microkernel, bare metal x86_64

  This work is released into the public domain under CC0 v1.0 Universal license.
  No warranty. Use at your own risk.
*/

#ifndef VM_ASM_OPT_H
#define VM_ASM_OPT_H

#include "vm.h"
#include <stdint.h>

/*
 * Configuration: Define USE_ASM_OPT=1 to enable assembly optimizations
 * Define USE_ASM_OPT=0 or leave undefined to use standard C implementations
 */
#ifndef USE_ASM_OPT
#define USE_ASM_OPT 0
#endif

/* ============================================================================
 * OPTIMIZATION 1: Stack Operations (vm_push, vm_pop, vm_rpush, vm_rpop)
 * ============================================================================
 *
 * IMPACT: EXTREME - These are called on nearly every Forth word execution
 *
 * Benefits:
 * - Eliminates function call overhead (inline)
 * - Reduces branch mispredictions with conditional moves
 * - Keeps frequently-used values in registers
 * - Uses lea for pointer arithmetic (no flags affected)
 *
 * L4Re Compatibility: YES - Pure computational code, no syscalls
 * ============================================================================
 */

#if USE_ASM_OPT

/* vm_push optimized - Data stack push */
static inline void vm_push_asm(VM *vm, cell_t value) {
    int error = 0;

    __asm__ __volatile__(
        /* Load current dsp into register with sign extension */
        "movslq  %[dsp], %%rax\n\t"

        /* Check for overflow: dsp >= STACK_SIZE-1 (1023) */
        "cmpq    $1022, %%rax\n\t"
        "jg      1f\n\t" /* Jump if overflow */

        /* No overflow: increment dsp */
        "addq    $1, %%rax\n\t" /* rax = dsp + 1 */
        "movl    %%eax, %[dsp]\n\t" /* Save new dsp (32-bit) */

        /* Store value: vm->data_stack[dsp] = value (dsp already incremented) */
        "movq    %[val], (%[stack], %%rax, 8)\n\t"

        /* Success: set error = 0 */
        "xorl    %[err], %[err]\n\t"
        "jmp     2f\n\t"

        /* Overflow path */
        "1:\n\t"
        "movl    $1, %[err]\n\t"

        "2:\n\t"
        : [dsp]"+m"(vm->dsp),
        [err]"=r"(error)
        : [val]"r"(value),
        [stack]"r"(vm->data_stack)
        : "rax", "cc", "memory"
    );

    if (__builtin_expect(error, 0)) {
        vm->error = 1;
        /* Note: Logging omitted for performance - enable in debug builds only */
    }
}

/* vm_pop optimized - Data stack pop */
static inline cell_t vm_pop_asm(VM *vm) {
    cell_t value = 0;
    int error = 0;

    __asm__ __volatile__(
        /* Load current dsp with sign extension */
        "movslq  %[dsp], %%rax\n\t"

        /* Check for underflow: dsp < 0 */
        "testq   %%rax, %%rax\n\t"
        "js      1f\n\t" /* Jump if negative (underflow) */

        /* No underflow: load value */
        "movq    (%[stack], %%rax, 8), %[val]\n\t"

        /* Decrement dsp */
        "subq    $1, %%rax\n\t"
        "movl    %%eax, %[dsp]\n\t"

        /* Success: set error = 0 */
        "xorl    %[err], %[err]\n\t"
        "jmp     2f\n\t"

        /* Underflow path */
        "1:\n\t"
        "movl    $1, %[err]\n\t"
        "xorq    %%rax, %%rax\n\t" /* Return 0 on error */
        "movq    %%rax, %[val]\n\t"

        "2:\n\t"
        : [dsp]"+m"(vm->dsp),
        [val]"=r"(value),
        [err]"=r"(error)
        : [stack]"r"(vm->data_stack)
        : "rax", "cc", "memory"
    );

    if (__builtin_expect(error, 0)) {
        vm->error = 1;
    }

    return value;
}

/* vm_rpush optimized - Return stack push */
static inline void vm_rpush_asm(VM *vm, cell_t value) {
    int error = 0;

    __asm__ __volatile__(
        "movslq  %[rsp], %%rax\n\t"
        "cmpq    $1022, %%rax\n\t"
        "jg      1f\n\t"
        "addq    $1, %%rax\n\t"
        "movl    %%eax, %[rsp]\n\t"
        "movq    %[val], (%[stack], %%rax, 8)\n\t"
        "xorl    %[err], %[err]\n\t"
        "jmp     2f\n\t"
        "1:\n\t"
        "movl    $1, %[err]\n\t"
        "2:\n\t"
        : [rsp]"+m"(vm->rsp),
        [err]"=r"(error)
        : [val]"r"(value),
        [stack]"r"(vm->return_stack)
        : "rax", "cc", "memory"
    );

    if (__builtin_expect(error, 0)) {
        vm->error = 1;
    }
}

/* vm_rpop optimized - Return stack pop */
static inline cell_t vm_rpop_asm(VM *vm) {
    cell_t value = 0;
    int error = 0;

    __asm__ __volatile__(
        "movslq  %[rsp], %%rax\n\t"
        "testq   %%rax, %%rax\n\t"
        "js      1f\n\t"
        "movq    (%[stack], %%rax, 8), %[val]\n\t"
        "subq    $1, %%rax\n\t"
        "movl    %%eax, %[rsp]\n\t"
        "xorl    %[err], %[err]\n\t"
        "jmp     2f\n\t"
        "1:\n\t"
        "movl    $1, %[err]\n\t"
        "xorq    %%rax, %%rax\n\t"
        "movq    %%rax, %[val]\n\t"
        "2:\n\t"
        : [rsp]"+m"(vm->rsp),
        [val]"=r"(value),
        [err]"=r"(error)
        : [stack]"r"(vm->return_stack)
        : "rax", "cc", "memory"
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
 * IMPACT: MEDIUM-HIGH
 *
 * Benefits:
 * - Uses CPU overflow flag instead of manual checks
 * - Eliminates branches in common (no overflow) case
 * - Single instruction for addition/subtraction with flag check
 *
 * L4Re Compatibility: YES
 * ============================================================================
 */

/* Fast add with overflow detection */
static inline int vm_add_check_overflow(cell_t a, cell_t b, cell_t *result) {
    cell_t res;
    int overflow;

    __asm__(
        "movq    %[a], %%rax\n\t"
        "addq    %[b], %%rax\n\t"
        "movq    %%rax, %[res]\n\t"
        "seto    %%al\n\t" /* Set AL to 1 if overflow occurred */
        "movzbl  %%al, %[ovf]\n\t"
        : [res]"=r"(res),
        [ovf]"=r"(overflow)
        : [a]"r"(a),
        [b]"r"(b)
        : "rax", "cc"
    );

    *result = res;
    return overflow;
}

/* Fast multiply with double-width result for */ /* MOD operations */
static inline void vm_mul_double(cell_t a, cell_t b, cell_t *hi, cell_t *lo) {
    __asm__(
        "movq    %[a], %%rax\n\t"
        "imulq   %[b]\n\t" /* rdx:rax = rax * b (signed) */
        "movq    %%rax, %[lo]\n\t"
        "movq    %%rdx, %[hi]\n\t"
        : [lo]"=r"(*lo),
        [hi]"=r"(*hi)
        : [a]"r"(a),
        [b]"r"(b)
        : "rax", "rdx", "cc"
    );
}

/* Fast divide with remainder (for /MOD) */
static inline void vm_divmod(cell_t dividend, cell_t divisor,
                             cell_t *quotient, cell_t *remainder) {
    cell_t quot, rem;

    __asm__(
        "movq    %[dividend], %%rax\n\t"
        "cqo\n\t" /* Sign-extend rax into rdx:rax */
        "idivq   %[divisor]\n\t" /* rax = quotient, rdx = remainder */
        "movq    %%rax, %[quot]\n\t"
        "movq    %%rdx, %[rem]\n\t"
        : [quot]"=r"(quot),
        [rem]"=r"(rem)
        : [dividend]"r"(dividend),
        [divisor]"r"(divisor)
        : "rax", "rdx", "cc"
    );

    *quotient = quot;
    *remainder = rem;
}

/* ============================================================================
 * OPTIMIZATION 3: Fast String Operations for Dictionary Lookup
 * ============================================================================
 *
 * IMPACT: HIGH
 *
 * Benefits:
 * - Uses hardware string comparison (rep cmpsb)
 * - Processes bytes in parallel
 * - Much faster than byte-by-byte C loop
 *
 * L4Re Compatibility: YES
 * ============================================================================
 */

/* Fast string comparison - returns 0 if equal, non-zero otherwise */
static inline int vm_strcmp_asm(const char *s1, const char *s2, size_t len) {
    int result;

    __asm__(
        "movq    %[s1], %%rsi\n\t"
        "movq    %[s2], %%rdi\n\t"
        "movq    %[len], %%rcx\n\t"
        "xorl    %%eax, %%eax\n\t"
        "repe    cmpsb\n\t" /* Compare bytes while equal */
        "je      1f\n\t" /* If all equal, result = 0 */
        "movl    $1, %%eax\n\t" /* Not equal, result = 1 */
        "1:\n\t"
        : "=a"(result)
        : [s1]"r"(s1),
        [s2]"r"(s2),
        [len]"r"(len)
        : "rsi", "rdi", "rcx", "cc", "memory"
    );

    return result;
}

/* Fast memory copy for block operations */
static inline void vm_memcpy_asm(void *dest, const void *src, size_t len) {
    __asm__ __volatile__(
        "movq    %[dst], %%rdi\n\t"
        "movq    %[src], %%rsi\n\t"
        "movq    %[len], %%rcx\n\t"
        "rep     movsb\n\t" /* Copy bytes */
        :
        : [dst]"r"(dest),
        [src]"r"(src),
        [len]"r"(len)
        : "rdi", "rsi", "rcx", "memory"
    );
}

/* Fast memory zero for alignment padding */
static inline void vm_memzero_asm(void *dest, size_t len) {
    __asm__ __volatile__(
        "movq    %[dst], %%rdi\n\t"
        "xorl    %%eax, %%eax\n\t"
        "movq    %[len], %%rcx\n\t"
        "rep     stosb\n\t" /* Store zeros */
        :
        : [dst]"r"(dest),
        [len]"r"(len)
        : "rdi", "rax", "rcx", "memory"
    );
}

/* ============================================================================
 * OPTIMIZATION 4: Branchless Min/Max
 * ============================================================================
 *
 * IMPACT: MEDIUM
 *
 * Benefits:
 * - No branches = no mispredictions
 * - Single cmov instruction
 *
 * L4Re Compatibility: YES
 * ============================================================================
 */

static inline cell_t vm_min_asm(cell_t a, cell_t b) {
    cell_t result;
    __asm__(
        "movq    %[a], %%rax\n\t"
        "movq    %[b], %%rcx\n\t"
        "cmpq    %%rcx, %%rax\n\t"
        "cmovg   %%rcx, %%rax\n\t" /* Move if a > b */
        "movq    %%rax, %[res]\n\t"
        : [res]"=r"(result)
        : [a]"r"(a),
        [b]"r"(b)
        : "rax", "rcx", "cc"
    );
    return result;
}

static inline cell_t vm_max_asm(cell_t a, cell_t b) {
    cell_t result;
    __asm__(
        "movq    %[a], %%rax\n\t"
        "movq    %[b], %%rcx\n\t"
        "cmpq    %%rcx, %%rax\n\t"
        "cmovl   %%rcx, %%rax\n\t" /* Move if a < b */
        "movq    %%rax, %[res]\n\t"
        : [res]"=r"(result)
        : [a]"r"(a),
        [b]"r"(b)
        : "rax", "rcx", "cc"
    );
    return result;
}

/* ============================================================================
 * OPTIMIZATION 5: CPU Feature Detection
 * ============================================================================
 *
 * For future SIMD optimizations
 * L4Re Compatibility: YES (cpuid is unprivileged)
 * ============================================================================
 */

static inline void vm_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx,
                            uint32_t *ecx, uint32_t *edx) {
    __asm__(
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf)
    );
}

/* Check for SSE4.2 support (for fast string operations) */
static inline int vm_has_sse42(void) {
    uint32_t eax, ebx, ecx, edx;
    vm_cpuid(1, &eax, &ebx, &ecx, &edx);
    return (ecx >> 20) & 1; /* Bit 20 = SSE4.2 */
}

#else /* !USE_ASM_OPT */

/* Fallback to standard implementations when assembly is disabled */
#define vm_push_asm(vm, val) vm_push(vm, val)
#define vm_pop_asm(vm) vm_pop(vm)
#define vm_rpush_asm(vm, val) vm_rpush(vm, val)
#define vm_rpop_asm(vm) vm_rpop(vm)

#endif /* USE_ASM_OPT */

/* ============================================================================
 * L4Re SPECIFIC NOTES
 * ============================================================================
 *
 * All optimizations above are L4Re-compatible because they:
 * 1. Don't use privileged instructions
 * 2. Don't make syscalls
 * 3. Don't access hardware directly
 * 4. Are pure computational optimizations
 *
 * For L4Re integration:
 * - Compile with -march=native or -march=x86-64-v3 for modern CPUs
 * - Use L4Re's memory allocators for VM memory
 * - Consider using L4Re IPC for inter-VM communication
 * - Profile using L4Re's performance counters
 *
 * Build flags for optimal performance:
 *   -DUSE_ASM_OPT=1 -O3 -march=native -flto
 *
 * For StarshipOS integration:
 * - These functions can be used in both userspace and kernel contexts
 * - No floating point operations (safe for kernel mode)
 * - No stack-intensive operations (safe for small kernel stacks)
 * ============================================================================
 */

#endif /* VM_ASM_OPT_H */