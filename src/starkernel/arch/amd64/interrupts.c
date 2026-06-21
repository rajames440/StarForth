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

/**
 * interrupts.c (amd64) - Interrupt setup and exception/IRQ handling
 */

#include <stdint.h>
#include "arch.h"
#include "console.h"
#include "apic.h"
#include "timer.h"

/* Set by the FORTH dispatcher just before calling entry->func(vm).
 * Printed on fault to identify which word was executing. */
volatile const char *g_sk_fault_word = (void *)0;

#define IDT_ENTRIES 256
#define INTERRUPT_GATE 0x8E

/**
 * @brief Write a single byte to an x86 I/O port via @c OUT.
 *
 * Issues the @c OUT instruction to port @p port with data byte @p val.
 * Used exclusively to program the legacy 8259 PIC's interrupt-mask
 * registers during @c pic_disable(). In a kernel that uses the Local APIC
 * for all interrupt delivery the PIC must be fully masked before enabling
 * CPU interrupts; otherwise spurious IRQ0 from the PIC timer can corrupt
 * the APIC timer ISR path.
 *
 * The @c "Nd" constraint allows the assembler to emit the short-form
 * @c OUT @p imm8 encoding for constants in [0, 255] and the register form
 * @c OUT @c DX otherwise.
 *
 * @param port 16-bit I/O port number (e.g., 0x21 = PIC1 data, 0xA1 = PIC2 data).
 * @param val  Byte value to write to the port.
 */
static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

extern void *isr_stub_table[IDT_ENTRIES] __attribute__((visibility("hidden")));

/*
 * Assembly hands us:
 *  - vector
 *  - error_code (normalized to 0 if not present)
 *  - rip, cs, rflags
 *  - cr2 (captured for convenience; meaningful primarily for #PF)
 */
extern void isr_common_handler(uint64_t vector,
                              uint64_t error_code,
                              uint64_t rip,
                              uint64_t cs,
                              uint64_t rflags,
                              uint64_t cr2,
                              uint64_t stack_frame_ptr);

static struct idt_entry idt[IDT_ENTRIES];

/**
 * @brief Write a single 64-bit interrupt gate descriptor into the IDT.
 *
 * Fills @c idt[@p vector] with an x86-64 interrupt gate (type 0x8E):
 * - @c offset_low / @c offset_mid / @c offset_high — three parts of the
 *   ISR handler address @p isr split across the 16-byte descriptor layout.
 * - @c selector = 0x08 — kernel code segment (GDT slot 1).
 * - @c ist = 0 — no interrupt stack table; uses the current RSP.
 * - @c type_attr = @c INTERRUPT_GATE (0x8E) — P=1, DPL=0, 64-bit interrupt gate;
 *   the CPU automatically clears RFLAGS.IF on entry.
 * - @c zero = 0 — upper reserved dword must be zero.
 *
 * Called once per vector inside @c arch_interrupts_init()'s loop after
 * the relocation offset has been applied to the stub address from
 * @c isr_stub_table.
 *
 * @param vector IDT vector index [0, 255].
 * @param isr    Runtime address of the per-vector assembly stub in @c isr.S.
 */
static void set_idt_entry(int vector, void *isr) {
    uint64_t addr = (uint64_t)isr;
    idt[vector].offset_low  = (uint16_t)(addr & 0xFFFFu);
    idt[vector].selector    = 0x08; /* kernel code selector */
    idt[vector].ist         = 0;
    idt[vector].type_attr   = INTERRUPT_GATE;
    idt[vector].offset_mid  = (uint16_t)((addr >> 16) & 0xFFFFu);
    idt[vector].offset_high = (uint32_t)((addr >> 32) & 0xFFFFFFFFu);
    idt[vector].zero        = 0;
}

/**
 * @brief Load the Interrupt Descriptor Table register (@c LIDT).
 *
 * Executes the @c LIDT instruction with the descriptor at @p idtr_desc,
 * which contains the 16-bit limit (byte size of the IDT minus 1) and the
 * 64-bit linear base address of the IDT array. After this instruction the
 * CPU uses the new IDT for all exception and interrupt dispatch.
 *
 * Must be called after all 256 entries in @c idt[] have been filled by
 * @c set_idt_entry(). Called at the end of @c arch_interrupts_init().
 *
 * @param idtr_desc Pointer to an @c idtr struct containing limit and base.
 */
static void lidt(struct idtr *idtr_desc) {
    __asm__ volatile ("lidt (%0)" :: "r"(idtr_desc));
}

/**
 * @brief Print a 64-bit value as 16 lowercase hex digits to the kernel console.
 *
 * Emits exactly 16 nibbles (no @c 0x prefix, no separators) by iterating
 * from bit 60 down to bit 0 in steps of 4. Used exclusively by
 * @c isr_common_handler() to print register values (RIP, CR2, RSP, RBP,
 * return address) in the exception diagnostic dump. No allocation or libc
 * dependency.
 *
 * @param value 64-bit value to format as lowercase hex.
 */
static void print_hex64(uint64_t value) {
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t nib = (uint8_t)((value >> (uint64_t)i) & 0xFu);
        console_putc((char)(nib < 10 ? ('0' + nib) : ('a' + (nib - 10))));
    }
}

/**
 * @brief Print a 64-bit unsigned integer in decimal to the kernel console.
 *
 * Converts @p v to a decimal ASCII string using a 32-byte stack buffer
 * (digit reversal technique) and emits each character via @c console_putc().
 * Prints "0" for a zero value. Used by @c isr_common_handler() to print
 * the exception vector number in decimal alongside its hex representation.
 * No allocation or libc dependency.
 *
 * @param v 64-bit unsigned integer to print.
 */
static void print_dec_u64(uint64_t v) {
    /* tiny decimal printer, no malloc, no libc */
    char buf[32];
    int i = 0;

    if (v == 0) {
        console_putc('0');
        return;
    }

    while (v > 0 && i < (int)sizeof(buf)) {
        buf[i++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    while (i-- > 0) {
        console_putc(buf[i]);
    }
}

/**
 * @brief Fully mask the legacy 8259A PIC to suppress all legacy IRQs.
 *
 * Writes 0xFF to PIC1's interrupt-mask register (port 0x21) and PIC2's
 * interrupt-mask register (port 0xA1). When all bits are set every IRQ
 * line (IRQ0–IRQ15) is masked and the PIC will not assert INTR to the CPU.
 *
 * This is necessary because UEFI firmware initialises the 8259 PIC in
 * compatibility mode with IRQ0 (timer) running at 18.2 Hz. After
 * @c ExitBootServices() the firmware no longer handles these IRQs; if the
 * PIC is not masked before @c STI, spurious PIC interrupts are delivered
 * to the kernel's IDT at legacy vectors 0x20–0x2F, most of which have not
 * been set up as ISRs and will triple-fault.
 *
 * Called from @c arch_interrupts_init() after the IDT is loaded but
 * before @c arch_enable_interrupts().
 */
static void pic_disable(void)
{
    /* Mask all IRQs on PIC1 and PIC2 to avoid spurious legacy interrupts */
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

/**
 * @brief Decode and print the x86-64 Page Fault error code to the kernel console.
 *
 * The processor pushes a 32-bit error code onto the stack for a #PF
 * (vector 14). Each bit encodes information about the faulting access:
 *
 * - bit 0 (P): 0 = page not present; 1 = protection violation.
 * - bit 1 (W/R): 0 = read access; 1 = write access.
 * - bit 2 (U/S): 0 = supervisor mode access; 1 = user mode access.
 * - bit 3 (RSVD): 1 = fault caused by a reserved bit set in a PTE.
 * - bit 4 (I/D): 1 = instruction fetch (no-execute violation).
 * - bit 5 (PK): 1 = protection-key violation (if PKRU is active).
 * - bit 6 (SS): 1 = shadow-stack access violation (CET).
 * - bit 15 (SGX): 1 = SGX enclave access violation.
 *
 * Prints each active flag as a short tag to the kernel console followed
 * by a newline. The CR2 register (captured by the ISR stub) holds the
 * faulting linear address and is printed separately by
 * @c isr_common_handler().
 *
 * @param ec 64-bit page fault error code (upper 32 bits are always zero
 *           on current x86-64 hardware; tested up to bit 15 per SDM).
 */
static void decode_pf_error(uint64_t ec)
{
    /*
     * Page Fault Error Code bits:
     *  bit0 P: 0=non-present, 1=protection violation
     *  bit1 W/R: 0=read, 1=write
     *  bit2 U/S: 0=supervisor, 1=user
     *  bit3 Rsvd: 1=reserved bit violation
     *  bit4 I/D: 1=instruction fetch
     *  bit5 PK: 1=protection key violation (if supported)
     *  bit6 SS: 1=shadow stack (if supported)
     *  bit15 SGX: 1=SGX (if supported)
     */
    console_puts("PF EC  : ");
    console_puts((ec & 0x1u) ? "PROT " : "NP ");
    console_puts((ec & 0x2u) ? "W " : "R ");
    console_puts((ec & 0x4u) ? "USR " : "SUP ");
    if (ec & 0x8u)  console_puts("RSVD ");
    if (ec & 0x10u) console_puts("IFETCH ");
    if (ec & 0x20u) console_puts("PK ");
    if (ec & 0x40u) console_puts("SS ");
    if (ec & 0x8000u) console_puts("SGX ");
    console_println("");
}

/**
 * @brief Unified C-level interrupt and exception handler.
 *
 * All 256 IDT entries funnel through a common assembly trampoline in
 * @c isr.S which captures caller-saved registers, reads @c CR2 (the
 * page-fault linear address), normalises the error code to 0 for vectors
 * that do not push one, and then calls this function with six arguments
 * reconstructed from the interrupt stack frame.
 *
 * **APIC timer path (vector == @c APIC_TIMER_VECTOR):**
 * Calls @c heartbeat_tick() to update the rolling window of inter-tick TSC
 * deviations, then issues @c apic_eoi() and returns. This is the hot path
 * — it executes at 100 Hz during normal kernel operation and must be kept
 * brief.
 *
 * **Exception path (all other vectors):**
 * Prints a structured diagnostic to the kernel console:
 * - Vector number (decimal and hex)
 * - Error code (hex)
 * - RIP (hex) — faulting instruction pointer
 * - CR2 (hex) — page-fault linear address (meaningful only for vector 14)
 * - RSP (derived from @p stack_frame_ptr + 160, covering the ISR stub's
 *   pushed register block)
 * - RBP (read from the saved-register block at @p stack_frame_ptr + 64)
 * - Return address (first quadword at the derived RSP)
 * - Vector-specific decoding: @c #DE (0), @c #GP (13) with the active FORTH
 *   word name from @c g_sk_fault_word, @c #PF (14) with @c decode_pf_error().
 * Then halts the CPU in an infinite @c HLT loop — exceptions are fatal.
 *
 * The @p cs and @p rflags parameters are currently unused (suppressed with
 * @c (void) casts) but retained in the signature for future use (e.g.,
 * printing privilege level on #GP).
 *
 * @param vector         IDT vector that fired [0, 255].
 * @param error_code     Error code from the CPU stack (0 if not applicable).
 * @param rip            Faulting or interrupted instruction pointer.
 * @param cs             Code segment selector at the time of the interrupt.
 * @param rflags         RFLAGS at the time of the interrupt.
 * @param cr2            Value of @c CR2 (page-fault linear address; only
 *                       architecturally meaningful for vector 14).
 * @param stack_frame_ptr Address of the ISR stub's saved-register block on
 *                        the stack; used to reconstruct RSP and RBP for the
 *                        diagnostic dump.
 */
void isr_common_handler(uint64_t vector,
                        uint64_t error_code,
                        uint64_t rip,
                        uint64_t cs,
                        uint64_t rflags,
                        uint64_t cr2,
                        uint64_t stack_frame_ptr)
{
    /* Handle APIC timer interrupt (heartbeat) */
    if (vector == APIC_TIMER_VECTOR) {
        heartbeat_tick();

        /* Acknowledge interrupt and return (don't halt!) */
        apic_eoi();
        return;
    }

    /* All other vectors are exceptions - print diagnostic and halt */
    console_println("\n=== INTERRUPT/EXCEPTION ===");

    console_puts("Vector : ");
    print_dec_u64(vector);
    console_puts(" (0x");
    print_hex64(vector);
    console_println(")");

    console_puts("Error  : 0x"); print_hex64(error_code); console_println("");

    (void)cs;
    (void)rflags;

    console_puts("RIP    : 0x"); print_hex64(rip); console_println("");
    console_puts("CR2    : 0x"); print_hex64(cr2); console_println("");

    uint64_t fault_rsp = stack_frame_ptr + 160u;
    console_puts("RSP    : 0x"); print_hex64(fault_rsp); console_println("");
    console_puts("RBP    : 0x");
    print_hex64(*((uint64_t *)(uintptr_t)(stack_frame_ptr + 64u)));
    console_println("");
    console_puts("RET    : 0x");
    print_hex64(*((uint64_t *)(uintptr_t)fault_rsp));
    console_println("");

    switch (vector) {
        case 0:
            console_println("Fault: Divide Error (#DE)");
            break;
        case 13:
            console_println("Fault: General Protection (#GP)");
            if (g_sk_fault_word) {
                console_puts("Word   : ");
                console_println((const char *)g_sk_fault_word);
            }
            break;
        case 14:
            console_println("Fault: Page Fault (#PF)");
            decode_pf_error(error_code);
            break;
        default:
            console_println("Fault: Unhandled vector");
            break;
    }

    console_println("Halting.");
    while (1) {
        arch_halt();
    }
}

/*
 * Compute the runtime load offset.
 * The linker places isr_stub0 at a known link-time address. At runtime,
 * we compare that to the actual address to determine the relocation delta.
 * This is necessary because UEFI PE loading doesn't apply relocations to
 * addresses stored in the .data section (isr_stub_table).
 */
extern void isr_stub0(void) __attribute__((visibility("hidden")));  /* Defined in isr.S */

/**
 * @brief Initialise the x86-64 IDT and disable the legacy 8259 PIC (M4).
 *
 * This is the M4 milestone function called from @c kernel_main() after the
 * GDT is live (@c arch_early_init()) and before @c arch_enable_interrupts().
 * It performs three operations:
 *
 * 1. **Relocation fixup for the ISR stub table.**
 *    @c isr.S exports @c isr_stub_table[], a 256-entry table of link-time
 *    function pointers assembled at their expected virtual addresses. In a
 *    UEFI PE32+ image the UEFI loader places the image at an arbitrary base;
 *    pointer-sized relocations in @c .data are not applied by the PE loader
 *    (only code-relative relocations in @c .text are). The fixup computes the
 *    runtime delta between @c isr_stub0's runtime address and its link-time
 *    address stored in @c isr_stub_table[0], then applies that delta to every
 *    entry before installing it in the IDT.
 *
 * 2. **Fill all 256 IDT entries.**
 *    Calls @c set_idt_entry() for each vector with the corrected stub address,
 *    selector 0x08, IST 0, and type_attr @c INTERRUPT_GATE (0x8E).
 *
 * 3. **Load the IDT and mask the PIC.**
 *    Calls @c lidt() with the fully populated @c idt[] array, then calls
 *    @c pic_disable() to mask both PIC1 and PIC2 before @c STI is issued.
 *    This prevents spurious legacy PIC IRQs from reaching the IDT before the
 *    Local APIC takes over.
 *
 * Must be called after @c gdt_init() (for selector 0x08) and before
 * @c apic_init() / @c apic_timer_init() / @c arch_enable_interrupts().
 */
void arch_interrupts_init(void)
{
    uint64_t link_time_addr = (uint64_t)isr_stub_table[0];
    uint64_t runtime_addr = (uint64_t)&isr_stub0;
    int64_t reloc_offset = (int64_t)(runtime_addr - link_time_addr);

    for (int i = 0; i < IDT_ENTRIES; ++i) {
        void *isr_addr = (void *)((uint64_t)isr_stub_table[i] + (uint64_t)reloc_offset);
        set_idt_entry(i, isr_addr);
    }

    struct idtr idtr_desc;
    idtr_desc.limit = (uint16_t)(sizeof(idt) - 1u);
    idtr_desc.base  = (uint64_t)&idt[0];

    lidt(&idtr_desc);
    pic_disable();
}
