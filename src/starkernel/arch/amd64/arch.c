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
 * arch.c (amd64) - Architecture abstractions for StarKernel on x86_64
 */

#include "arch.h"
#include <stdint.h>

/*
 * GDT entries for 64-bit mode.
 * In long mode, the base and limit fields of code/data segments are ignored,
 * but the access and flags bytes must be correct.
 */
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;  /* includes high nibble of limit */
    uint8_t  base_high;
} __attribute__((packed));

struct gdtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/*
 * GDT layout:
 *   0x00: Null descriptor
 *   0x08: 64-bit kernel code segment (selector used in IDT)
 *   0x10: Kernel data segment
 */
static struct gdt_entry gdt[3] __attribute__((aligned(16)));

/**
 * @brief Populate a GDT entry with limit 0xFFFF and a zero base.
 *
 * In 64-bit long mode the base and limit fields of code/data segment
 * descriptors are ignored by the processor; only the @p access and
 * @p granularity flag bytes are architecturally significant. This helper
 * writes the canonical long-mode descriptor layout: limit = 0xFFFF, all base
 * bytes = 0, then stores the provided access and granularity values.
 *
 * @param index       Index into the @c gdt[] array (0 = null, 1 = code, 2 = data).
 * @param access      Access byte (e.g., 0x9A for kernel code, 0x92 for data).
 * @param granularity Granularity/flags byte (e.g., 0x20 sets the L-bit for 64-bit code).
 */
static void gdt_set_entry(int index, uint8_t access, uint8_t granularity)
{
    gdt[index].limit_low   = 0xFFFF;
    gdt[index].base_low    = 0;
    gdt[index].base_mid    = 0;
    gdt[index].access      = access;
    gdt[index].granularity = granularity;
    gdt[index].base_high   = 0;
}

/**
 * @brief Initialise and load the minimal 3-entry GDT required for long mode.
 *
 * Builds three GDT entries in the @c gdt[] array and loads the descriptor via
 * @c LGDT followed by a far-return sequence to reload @c CS to selector 0x08
 * and all other segment registers to 0x10:
 *
 * - Slot 0 (0x00): Null descriptor — all fields zero.
 * - Slot 1 (0x08): 64-bit kernel code (access=0x9A, granularity=0x20).
 * - Slot 2 (0x10): Kernel data (access=0x92, granularity=0x00).
 *
 * Must be called before @c arch_interrupts_init() because the IDT uses
 * selector 0x08 for all gate descriptors.
 */
static void gdt_init(void)
{
    /* Null descriptor */
    gdt[0].limit_low   = 0;
    gdt[0].base_low    = 0;
    gdt[0].base_mid    = 0;
    gdt[0].access      = 0;
    gdt[0].granularity = 0;
    gdt[0].base_high   = 0;

    /*
     * 64-bit kernel code segment (selector 0x08)
     * Access: 0x9A = Present (1), DPL 0 (00), S=1 (code/data), Exec (1), Conforming (0), Readable (1), Accessed (0)
     * Granularity: 0x20 = Long mode (L=1), Default operand size (D=0, must be 0 for long mode)
     */
    gdt_set_entry(1, 0x9A, 0x20);

    /*
     * Kernel data segment (selector 0x10)
     * Access: 0x92 = Present (1), DPL 0 (00), S=1 (code/data), Exec (0), Direction (0), Writable (1), Accessed (0)
     * Granularity: 0x00 (no special flags for data segment)
     */
    gdt_set_entry(2, 0x92, 0x00);

    struct gdtr gdtr_val;
    gdtr_val.limit = sizeof(gdt) - 1;
    gdtr_val.base  = (uint64_t)&gdt[0];

    __asm__ volatile (
        "lgdt %0\n\t"
        "pushq $0x08\n\t"           /* Push new CS */
        "leaq 1f(%%rip), %%rax\n\t" /* Load address of label 1 */
        "pushq %%rax\n\t"           /* Push return address */
        "lretq\n\t"                 /* Far return to reload CS */
        "1:\n\t"
        "mov $0x10, %%ax\n\t"       /* Load new DS/ES/SS/FS/GS */
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%ss\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        :
        : "m"(gdtr_val)
        : "rax", "memory"
    );
}

/**
 * @brief Perform the earliest x86-64 architecture initialisation.
 *
 * Called immediately after the UEFI boot stub transfers control to the
 * kernel, before the console or interrupt controller are available.
 * Loads the minimal three-entry GDT via @c gdt_init() so that subsequent
 * code has valid segment selectors. IDT setup is deferred until after the
 * console is live (called separately via @c arch_interrupts_init()).
 */
void arch_early_init(void)
{
    gdt_init();  /* Must be done before IDT, since IDT uses selector 0x08 */
    /* IDT setup is done later via arch_interrupts_init() after console is live */
}

/**
 * @brief Enable hardware interrupts on the current CPU core (x86-64 STI).
 *
 * Issues the @c STI instruction to set the Interrupt Flag. Must be called
 * after the IDT is loaded and the APIC is configured, otherwise the first
 * interrupt will triple-fault.
 */
void arch_enable_interrupts(void)
{
    __asm__ volatile ("sti");
}

/**
 * @brief Disable hardware interrupts on the current CPU core (x86-64 CLI).
 *
 * Issues the @c CLI instruction to clear the Interrupt Flag. Used around
 * critical sections in the single-threaded kernel where interrupt delivery
 * would corrupt shared state.
 */
void arch_disable_interrupts(void)
{
    __asm__ volatile ("cli");
}

/**
 * @brief Halt the CPU until the next interrupt (x86-64 HLT).
 *
 * Issues the @c HLT instruction. In the kernel panic path this is called
 * in an infinite loop (interrupts disabled) to stop the processor permanently.
 * In normal use it is called once per idle iteration to reduce power consumption.
 */
void arch_halt(void)
{
    __asm__ volatile ("hlt");
}

/**
 * @brief Read the x86-64 Time Stamp Counter (RDTSC).
 *
 * Executes @c RDTSC and combines @c EDX:EAX into a 64-bit value. Does not
 * serialise instruction execution; use @c RDTSCP or a surrounding @c CPUID
 * where ordering guarantees are required.
 *
 * @return Current TSC value in CPU cycles.
 */
uint64_t arch_read_timestamp(void)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/**
 * @brief Initialise the x86-64 MMU (stub — deferred to a later milestone).
 *
 * Full four-level page table setup is deferred; the VMM subsystem
 * (@c vmm.c) handles page table management independently. This function
 * exists as an architectural hook for future per-CPU MMU configuration
 * that must run after GDT/IDT setup but before the first memory access
 * through a non-identity-mapped virtual address.
 */
void arch_mmu_init(void)
{
    /* Stub: paging will be wired up in later milestones */
}
