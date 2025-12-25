/*
 * isr.S - Interrupt/exception stubs for amd64
 *
 * Stack on entry to ISR stub (after CPU pushes):
 *   [rsp+0]  = error code (if any, or pushed 0)
 *   [rsp+8]  = vector (pushed by stub)
 *   [rsp+16] = RIP
 *   [rsp+24] = CS
 *   [rsp+32] = RFLAGS
 *   [rsp+40] = RSP (if privilege change)
 *   [rsp+48] = SS (if privilege change)
 *
 * For IRQs (vector >= 32), we return via IRETQ.
 * For exceptions (vector < 32), we halt.
 */

    .intel_syntax noprefix
    .section .text

.macro ISR_NOERR num
    .global isr_stub\num
isr_stub\num:
    push 0          /* fake error code */
    push \num       /* vector number */
    jmp isr_common_entry
.endm

.macro ISR_ERR num
    .global isr_stub\num
isr_stub\num:
    /* error code already on stack from CPU */
    push \num       /* vector number */
    jmp isr_common_entry
.endm

.global isr_common_entry
isr_common_entry:
    /* Save all general-purpose registers */
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    /* Stack layout now:
     * [rsp+0]   = r15
     * [rsp+8]   = r14
     * ...
     * [rsp+112] = rax
     * [rsp+120] = vector
     * [rsp+128] = error
     * [rsp+136] = rip
     * [rsp+144] = cs
     * [rsp+152] = rflags
     */

    /* Set up arguments for isr_common_handler(vector, error, rip, cs, rflags, cr2) */
    mov rdi, [rsp + 120]   /* vector */
    mov rsi, [rsp + 128]   /* error */
    mov rdx, [rsp + 136]   /* rip */
    mov rcx, [rsp + 144]   /* cs */
    mov r8,  [rsp + 152]   /* rflags */
    mov r9,  cr2           /* cr2 */

    /* Align stack and call handler */
    call isr_common_handler

    /* Restore general-purpose registers */
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    /* Remove vector and error code from stack */
    add rsp, 16

    /* Return from interrupt */
    iretq

/* Exception/interrupt stubs (0-31 explicit) */
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

/* Stub for APIC Timer (vector 0x20 = 32) */
ISR_NOERR 32

/* Default stub for other vectors (33-255) */
.global isr_stub_default
isr_stub_default:
    push 0      /* fake error */
    push 255    /* unknown vector, will be logged */
    jmp isr_common_entry

    /* Use .data instead of .rodata so UEFI applies relocations */
    .section .data
    .align 8
    .global isr_stub_table
isr_stub_table:
    .quad isr_stub0, isr_stub1, isr_stub2, isr_stub3
    .quad isr_stub4, isr_stub5, isr_stub6, isr_stub7
    .quad isr_stub8, isr_stub9, isr_stub10, isr_stub11
    .quad isr_stub12, isr_stub13, isr_stub14, isr_stub15
    .quad isr_stub16, isr_stub17, isr_stub18, isr_stub19
    .quad isr_stub20, isr_stub21, isr_stub22, isr_stub23
    .quad isr_stub24, isr_stub25, isr_stub26, isr_stub27
    .quad isr_stub28, isr_stub29, isr_stub30, isr_stub31
    /* Vector 32 = APIC Timer */
    .quad isr_stub32
    /* Vectors 33-255 default to unknown stub */
    .rept 223
        .quad isr_stub_default
    .endr
