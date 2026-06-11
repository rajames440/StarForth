/*
 * kernel_entry.S — kernel stack setup and entry trampoline (amd64)
 *
 * UEFI hands control to kernel_main() running on a small UEFI-provided stack
 * (typically 128 KB–256 KB).  The FORTH interpreter + DOE experiment loop can
 * easily exceed that depth.  This trampoline allocates a 2 MiB BSS stack and
 * switches to it before calling the real implementation (kernel_main_impl).
 *
 * kernel_main is the symbol the UEFI loader calls (extern in uefi_loader.c).
 * kernel_main_impl is defined in kernel_main.c and contains all the C code.
 *
 * Calling convention: SysV AMD64 — boot_info pointer arrives in %rdi and is
 * passed unchanged to kernel_main_impl via the tail-call.
 */

    .intel_syntax noprefix

#include "starkernel/boot_info_offsets.h"

    /* 2 MiB kernel stack in BSS — fallback when no dynamic stack requested */
    .section .bss
    .align 16
    .global g_kernel_stack
g_kernel_stack:
    .space 0x200000
g_kernel_stack_top:
    .global g_kernel_stack_top

    .section .text
    .extern kernel_main_impl

    /*
     * kernel_main — stack-switch trampoline, then tail-call kernel_main_impl.
     *
     * rdi = BootInfo*  (SysV AMD64 ABI — first argument register)
     *
     * Stack selection (checked in order):
     *   1. boot_info->kernel_stack_base != 0  →  use loader-allocated stack
     *      (top = base + size, aligned down to 16, minus 8 for ABI)
     *   2. Otherwise  →  fall back to g_kernel_stack (2 MiB BSS)
     *
     * Never returns; kernel_main_impl runs the REPL forever or panics.
     */
    .global kernel_main
kernel_main:
    mov  rax, [rdi + BOOT_INFO_KERNEL_STACK_BASE_OFFSET]
    test rax, rax
    jnz  .Ldynamic_stack

    /* BSS fallback */
    lea  rax, [rip + g_kernel_stack_top]
    jmp  .Lstack_ready

.Ldynamic_stack:
    mov  rcx, [rdi + BOOT_INFO_KERNEL_STACK_SIZE_OFFSET]
    add  rax, rcx           /* top = base + size (stack grows down) */

.Lstack_ready:
    and  rax, -16           /* 16-byte alignment */
    sub  rax, 8             /* SysV ABI: RSP ≡ 8 mod 16 at C call boundary */
    mov  rsp, rax
    xor  rbp, rbp           /* terminate frame chain */
    jmp  kernel_main_impl   /* rdi (boot_info) already in place */
