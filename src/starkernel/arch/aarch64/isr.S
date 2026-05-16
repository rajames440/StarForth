/*
 * isr.S (aarch64) - Exception vector table stub.
 * Installs a minimal VBAR_EL1 that routes all exceptions to a common C handler.
 */

    .section .text
    .balign 2048

    .global aarch64_vector_table
aarch64_vector_table:

    /* Current EL with SP0 */
    .balign 128
synchronous_sp0:
    b aarch64_exception_handler

    .balign 128
irq_sp0:
    b aarch64_exception_handler

    .balign 128
fiq_sp0:
    b aarch64_exception_handler

    .balign 128
serror_sp0:
    b aarch64_exception_handler

    /* Current EL with SPx */
    .balign 128
synchronous_spx:
    b aarch64_exception_handler

    .balign 128
irq_spx:
    b aarch64_exception_handler

    .balign 128
fiq_spx:
    b aarch64_exception_handler

    .balign 128
serror_spx:
    b aarch64_exception_handler

    /* Lower EL using AArch64 */
    .balign 128
synchronous_lower_el64:
    b aarch64_exception_handler

    .balign 128
irq_lower_el64:
    b aarch64_exception_handler

    .balign 128
fiq_lower_el64:
    b aarch64_exception_handler

    .balign 128
serror_lower_el64:
    b aarch64_exception_handler

    /* Lower EL using AArch32 */
    .balign 128
synchronous_lower_el32:
    b aarch64_exception_handler

    .balign 128
irq_lower_el32:
    b aarch64_exception_handler

    .balign 128
fiq_lower_el32:
    b aarch64_exception_handler

    .balign 128
serror_lower_el32:
    b aarch64_exception_handler

    .text
    .global aarch64_install_vectors
aarch64_install_vectors:
    adr     x0, aarch64_vector_table
    msr     vbar_el1, x0
    isb
    ret
