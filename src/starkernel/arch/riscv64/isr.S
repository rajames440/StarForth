/*
 * isr.S (riscv64) - Trap vector installation.
 * Routes all S-mode exceptions to a common C handler.
 */

    .section .text
    .balign 4

    /* Direct-mode trap entry: stvec[1:0] = 0 means all traps branch here */
    .global riscv64_trap_entry
riscv64_trap_entry:
    call riscv64_exception_handler
    /* Should not return; if it does, spin */
1:  wfi
    j   1b

    .global riscv64_install_vectors
riscv64_install_vectors:
    la   t0, riscv64_trap_entry
    csrw stvec, t0
    ret
