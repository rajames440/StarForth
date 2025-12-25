/*
 * isr.S - Interrupt/exception stubs for amd64
 */

    .intel_syntax noprefix
    .section .text

.macro ISR_NOERR num
    .global isr_stub\num
isr_stub\num:
    push 0
    push \num
    jmp isr_common_entry
.endm

.macro ISR_ERR num
    .global isr_stub\num
isr_stub\num:
    push \num
    jmp isr_common_entry
.endm

.global isr_common_entry
isr_common_entry:
    /* Arguments: vector, error, rip, cs, rflags, cr2 */
    mov rdi, [rsp]        /* vector */
    mov rsi, [rsp + 8]    /* error */
    mov rdx, [rsp + 16]   /* rip */
    mov rcx, [rsp + 24]   /* cs */
    mov r8,  [rsp + 32]   /* rflags */
    mov r9,  cr2          /* cr2 */

    sub rsp, 8            /* align stack to 16 bytes before call */
    call isr_common_handler
    add rsp, 8

.Lhalt:
    hlt
    jmp .Lhalt

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

/* Default stub for other vectors */
.global isr_stub_default
isr_stub_default:
    push 0
    push 0
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

    /* Vectors 32-255 default to the same stub */
    .rept 224
        .quad isr_stub_default
    .endr
