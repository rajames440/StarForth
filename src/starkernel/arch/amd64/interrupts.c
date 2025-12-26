/**
 * interrupts.c (amd64) - Interrupt setup and exception/IRQ handling
 */

#include <stdint.h>
#include "arch.h"
#include "console.h"
#include "apic.h"
#include "timer.h"
#include "q48_16.h"

#define IDT_ENTRIES 256
#define INTERRUPT_GATE 0x8E

/* Print Q48.16 as decimal (e.g., "0.99609" for 0xFE00) */
static void print_q48_16(q48_16_t q)
{
    /* Integer part */
    uint64_t integer_part = q >> 16;
    uint16_t frac_bits = (uint16_t)(q & 0xFFFF);

    /* Print integer part */
    char buf[32];
    int i = 0;
    uint64_t temp = integer_part;
    if (temp == 0) {
        buf[i++] = '0';
    } else {
        while (temp > 0 && i < (int)sizeof(buf)) {
            buf[i++] = (char)('0' + (temp % 10));
            temp /= 10;
        }
    }
    while (i-- > 0) {
        console_putc(buf[i]);
    }

    console_putc('.');

    /* Fractional part: frac_bits / 65536 * 100000 to get 5 decimal digits */
    uint64_t frac = ((uint64_t)frac_bits * 100000ULL) / 65536ULL;
    char frac_buf[6] = {'0', '0', '0', '0', '0', '\0'};
    for (int j = 4; j >= 0; j--) {
        frac_buf[j] = (char)('0' + (frac % 10));
        frac /= 10;
    }
    console_puts(frac_buf);
}

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

extern void *isr_stub_table[IDT_ENTRIES];

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
                              uint64_t cr2);

static struct idt_entry idt[IDT_ENTRIES];

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

static void lidt(struct idtr *idtr_desc) {
    __asm__ volatile ("lidt (%0)" :: "r"(idtr_desc));
}

static void print_hex64(uint64_t value) {
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t nib = (uint8_t)((value >> (uint64_t)i) & 0xFu);
        console_putc((char)(nib < 10 ? ('0' + nib) : ('a' + (nib - 10))));
    }
}

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

static void pic_disable(void)
{
    /* Mask all IRQs on PIC1 and PIC2 to avoid spurious legacy interrupts */
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

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

void isr_common_handler(uint64_t vector,
                        uint64_t error_code,
                        uint64_t rip,
                        uint64_t cs,
                        uint64_t rflags,
                        uint64_t cr2)
{
    /* Handle APIC timer interrupt (heartbeat) */
    if (vector == APIC_TIMER_VECTOR) {
        heartbeat_tick();

        /* Print every 100 ticks (1 second at 100Hz) for visibility */
        uint64_t ticks = heartbeat_ticks();
        if ((ticks % 100) == 0) {
            console_puts("Heartbeat: ");
            print_dec_u64(ticks);
            console_puts(" ticks  TIME-TRUST=");
            print_q48_16(heartbeat_trust());
            console_println("");
        }

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

    switch (vector) {
        case 0:
            console_println("Fault: Divide Error (#DE)");
            break;
        case 13:
            console_println("Fault: General Protection (#GP)");
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
extern void isr_stub0(void);  /* Defined in isr.S */

void arch_interrupts_init(void)
{
    /*
     * Calculate relocation offset: difference between runtime address
     * and link-time address stored in isr_stub_table[0].
     */
    uint64_t link_time_addr = (uint64_t)isr_stub_table[0];
    uint64_t runtime_addr = (uint64_t)&isr_stub0;
    int64_t reloc_offset = (int64_t)(runtime_addr - link_time_addr);

    for (int i = 0; i < IDT_ENTRIES; ++i) {
        /* Apply relocation offset to get correct runtime address */
        void *isr_addr = (void *)((uint64_t)isr_stub_table[i] + (uint64_t)reloc_offset);
        set_idt_entry(i, isr_addr);
    }

    struct idtr idtr_desc;
    idtr_desc.limit = (uint16_t)(sizeof(idt) - 1u);
    idtr_desc.base  = (uint64_t)&idt[0];

    lidt(&idtr_desc);
    pic_disable();
    console_println("IDT installed.");
}
