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

    /* 2 MiB kernel stack in BSS (zero-initialised by the PE/ELF loader) */
    .section .bss
    .align 16
    .global g_kernel_stack
g_kernel_stack:
    .space 0x200000          /* 2 MiB */
g_kernel_stack_top:          /* label at the high end; stack grows downward */
    .global g_kernel_stack_top

    .section .text
    .extern kernel_main_impl

    /*
     * kernel_main — trampoline: switch to g_kernel_stack and tail-call impl.
     * Never returns (kernel_main_impl runs the REPL forever or panics).
     */
    .global kernel_main
kernel_main:
    lea  rax, [rip + g_kernel_stack_top]
    and  rax, -16            /* guarantee 16-byte RSP alignment per SysV ABI */
    mov  rsp, rax
    xor  rbp, rbp            /* terminate the frame chain */
    jmp  kernel_main_impl    /* rdi (boot_info) already correct */
