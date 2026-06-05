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
 * apic.c - Local APIC and Timer (amd64)
 *
 * Provides:
 *   - APIC initialization
 *   - APIC timer for periodic heartbeat interrupts
 */

#include <stdint.h>
#include "apic.h"
#include "console.h"
#include "vmm.h"
#include "arch.h"

/* ============================================================================
 * APIC Register Offsets
 * ============================================================================ */

#define LAPIC_DEFAULT_PHYS     0xFEE00000u
#define LAPIC_VIRT_BASE        0xFEE00000u  /* identity-mapped */

/* IA32_APIC_BASE MSR */
#define IA32_APIC_BASE_MSR          0x1Bu
#define IA32_APIC_BASE_GLOBAL_EN    (1ull << 11)  /* Global APIC hardware enable */
#define IA32_APIC_BASE_X2APIC_EN    (1ull << 10)  /* x2APIC mode enable */

/* Core registers */
#define APIC_REG_ID            0x020  /* APIC ID */
#define APIC_REG_TPR           0x080  /* Task Priority Register */
#define APIC_REG_EOI           0x0B0  /* End of Interrupt */
#define APIC_REG_SIVR          0x0F0  /* Spurious Interrupt Vector Register */

/* Timer registers */
#define APIC_REG_LVT_TIMER     0x320  /* LVT Timer Register */
#define APIC_REG_TIMER_ICR     0x380  /* Timer Initial Count Register */
#define APIC_REG_TIMER_CCR     0x390  /* Timer Current Count Register */
#define APIC_REG_TIMER_DCR     0x3E0  /* Timer Divide Configuration Register */

/* LVT Timer bits */
#define LVT_MASKED             (1u << 16)  /* Interrupt masked */
#define LVT_MODE_PERIODIC      (1u << 17)  /* Periodic mode */
#define LVT_MODE_ONESHOT       0           /* One-shot mode (default) */

/* Divide configuration values (for APIC_REG_TIMER_DCR) */
#define TIMER_DIV_1            0x0B  /* Divide by 1 */
#define TIMER_DIV_2            0x00  /* Divide by 2 */
#define TIMER_DIV_4            0x01  /* Divide by 4 */
#define TIMER_DIV_8            0x02  /* Divide by 8 */
#define TIMER_DIV_16           0x03  /* Divide by 16 */
#define TIMER_DIV_32           0x08  /* Divide by 32 */
#define TIMER_DIV_64           0x09  /* Divide by 64 */
#define TIMER_DIV_128          0x0A  /* Divide by 128 */

/* ============================================================================
 * State
 * ============================================================================ */

static volatile uint32_t *lapic_base = (volatile uint32_t *)(uintptr_t)LAPIC_VIRT_BASE;
static uint64_t lapic_phys_base = LAPIC_DEFAULT_PHYS;
static uint32_t timer_initial_count = 0;
static uint64_t timer_period_tsc_ticks = 0;
static uint32_t timer_tick_hz = 0;

/* ============================================================================
 * MSR access (freestanding — no libgcc, no libc)
 * ============================================================================ */

static uint64_t rdmsr64(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static void wrmsr64(uint32_t msr, uint64_t val)
{
    __asm__ volatile ("wrmsr"
        : : "a"((uint32_t)(val & 0xFFFFFFFFu)),
            "d"((uint32_t)(val >> 32)),
            "c"(msr));
}

/* ============================================================================
 * Low-level APIC access
 * ============================================================================ */

static void lapic_write(uint32_t reg, uint32_t val) {
    lapic_base[reg / 4] = val;
}

static uint32_t lapic_read(uint32_t reg) {
    return lapic_base[reg / 4];
}

/* ============================================================================
 * APIC Initialization
 * ============================================================================ */

int apic_init(BootInfo *boot_info) {
    (void)boot_info;
    lapic_phys_base = LAPIC_DEFAULT_PHYS;
    lapic_base = (volatile uint32_t *)(uintptr_t)LAPIC_VIRT_BASE;

    /*
     * Read IA32_APIC_BASE MSR.  UEFI (OVMF) may have left the APIC in x2APIC
     * mode (bit 10) or may have left the global hardware enable (bit 11) in an
     * indeterminate state.  We need xAPIC MMIO mode with global enable set.
     */
    uint64_t apic_base_msr = rdmsr64(IA32_APIC_BASE_MSR);
    console_puts("APIC: IA32_APIC_BASE MSR=0x");
    {
        /* print upper 32 bits then lower 32 bits */
        uint32_t hi = (uint32_t)(apic_base_msr >> 32);
        uint32_t lo = (uint32_t)(apic_base_msr & 0xFFFFFFFFu);
        for (int s = 28; s >= 0; s -= 4)
            console_putc("0123456789abcdef"[(hi >> s) & 0xF]);
        for (int s = 28; s >= 0; s -= 4)
            console_putc("0123456789abcdef"[(lo >> s) & 0xF]);
    }
    console_putc('\n');

    if (apic_base_msr & IA32_APIC_BASE_X2APIC_EN) {
        /*
         * x2APIC is active.  Switching back to xAPIC requires: disable x2APIC
         * (clear bit 10) while keeping global enable (bit 11) set.
         * Intel SDM Vol.3 §10.12.5: software must not clear both bits at once.
         */
        console_puts("APIC: x2APIC active — switching to xAPIC MMIO mode\n");
        apic_base_msr &= ~IA32_APIC_BASE_X2APIC_EN;
        wrmsr64(IA32_APIC_BASE_MSR, apic_base_msr);
    }

    if (!(apic_base_msr & IA32_APIC_BASE_GLOBAL_EN)) {
        console_puts("APIC: global enable was clear — asserting bit 11\n");
        apic_base_msr |= IA32_APIC_BASE_GLOBAL_EN;
        wrmsr64(IA32_APIC_BASE_MSR, apic_base_msr);
    }

    /* Software-enable APIC and set spurious vector */
    const uint8_t  SPURIOUS_VECTOR = 0xFF;
    const uint32_t APIC_ENABLE     = (1u << 8);
    lapic_write(APIC_REG_SIVR, APIC_ENABLE | SPURIOUS_VECTOR);

    /*
     * Clear Task Priority Register.  If UEFI left TPR non-zero, all APIC
     * interrupts at or below that priority are silently suppressed — the timer
     * ISR never fires.  TPR=0 allows all priorities through.
     */
    lapic_write(APIC_REG_TPR, 0);

    /*
     * Issue a spurious EOI to clear any stale in-service interrupt left by
     * UEFI firmware.  UEFI's last APIC timer tick may have fired just before
     * ExitBootServices with no subsequent EOI; the APIC's ISR then shows
     * vector 0x20 still "in service", which suppresses all future 0x20
     * delivery until an EOI is written.  Issuing EOI here is always safe —
     * it is a no-op if the ISR is already clear.
     */
    lapic_write(APIC_REG_EOI, 0);

    console_puts("APIC: initialized (xAPIC MMIO, TPR=0, SIVR=0x1FF, EOI-clear)\n");
    return 0;
}

void apic_eoi(void) {
    lapic_write(APIC_REG_EOI, 0);
}

/* ============================================================================
 * APIC Timer
 * ============================================================================ */

/**
 * Calibrate APIC timer using TSC.
 * Returns APIC timer ticks per TSC tick (approximate).
 *
 * Strategy:
 *   1. Set APIC timer to max count, divisor = 1
 *   2. Wait for a fixed TSC interval
 *   3. Read how many APIC timer ticks elapsed
 *   4. Compute ratio
 */
static uint32_t calibrate_apic_timer(uint64_t tsc_hz) {
    /* Use divisor 1 for maximum resolution */
    lapic_write(APIC_REG_TIMER_DCR, TIMER_DIV_1);

    /* Mask timer (disable interrupts during calibration) */
    lapic_write(APIC_REG_LVT_TIMER, LVT_MASKED);

    /* Set initial count to max */
    lapic_write(APIC_REG_TIMER_ICR, 0xFFFFFFFFu);

    /* Measure TSC for ~10ms (or less if tsc_hz is unknown) */
    uint64_t calibration_tsc_ticks;
    if (tsc_hz > 0) {
        calibration_tsc_ticks = tsc_hz / 100;  /* 10ms */
    } else {
        calibration_tsc_ticks = 10000000;  /* Fallback: ~10M cycles */
    }

    uint64_t tsc_start = arch_read_timestamp();
    uint64_t tsc_end = tsc_start + calibration_tsc_ticks;

    /* Spin until TSC reaches target */
    while (arch_read_timestamp() < tsc_end) {
        arch_relax();
    }

    /* Read how many APIC ticks elapsed */
    uint32_t apic_elapsed = 0xFFFFFFFFu - lapic_read(APIC_REG_TIMER_CCR);

    /* Stop timer */
    lapic_write(APIC_REG_TIMER_ICR, 0);

    /* Compute APIC ticks per second */
    /* apic_hz = apic_elapsed * (1 / calibration_time_seconds) */
    /* apic_hz = apic_elapsed * (tsc_hz / calibration_tsc_ticks) */
    uint64_t apic_hz;
    if (tsc_hz > 0 && calibration_tsc_ticks > 0) {
        apic_hz = ((uint64_t)apic_elapsed * tsc_hz) / calibration_tsc_ticks;
    } else {
        /* Rough fallback: assume APIC runs at ~1 GHz */
        apic_hz = (uint64_t)apic_elapsed * 100;
    }

    return (uint32_t)(apic_hz & 0xFFFFFFFFu);
}

int apic_timer_init(uint64_t tsc_hz, uint32_t tick_hz) {
    if (tick_hz == 0) {
        tick_hz = 100;  /* Default: 100 Hz (10ms period) */
    }

    console_puts("APIC Timer: calibrating...\r\n");

    /* Calibrate to find APIC timer frequency */
    uint32_t apic_hz = calibrate_apic_timer(tsc_hz);

    if (apic_hz == 0) {
        console_puts("APIC Timer: calibration failed!\r\n");
        return -1;
    }

    /* Compute initial count for desired tick rate */
    timer_initial_count = apic_hz / tick_hz;
    timer_tick_hz = tick_hz;

    /* Compute expected TSC ticks per heartbeat (for TIME-TRUST variance) */
    if (tsc_hz > 0) {
        timer_period_tsc_ticks = tsc_hz / tick_hz;
    } else {
        timer_period_tsc_ticks = 0;  /* Unknown */
    }

    console_puts("APIC Timer: apic_hz=");
    /* Simple decimal print for debugging */
    {
        char buf[32];
        uint64_t v = apic_hz;
        int i = 0;
        if (v == 0) buf[i++] = '0';
        else {
            char tmp[32];
            int j = 0;
            while (v > 0) { tmp[j++] = '0' + (v % 10); v /= 10; }
            while (j > 0) buf[i++] = tmp[--j];
        }
        buf[i] = '\0';
        console_puts(buf);
    }
    console_puts(", tick_hz=");
    {
        char buf[32];
        uint64_t v = tick_hz;
        int i = 0;
        if (v == 0) buf[i++] = '0';
        else {
            char tmp[32];
            int j = 0;
            while (v > 0) { tmp[j++] = '0' + (v % 10); v /= 10; }
            while (j > 0) buf[i++] = tmp[--j];
        }
        buf[i] = '\0';
        console_puts(buf);
    }
    console_puts(", initial_count=");
    {
        char buf[32];
        uint64_t v = timer_initial_count;
        int i = 0;
        if (v == 0) buf[i++] = '0';
        else {
            char tmp[32];
            int j = 0;
            while (v > 0) { tmp[j++] = '0' + (v % 10); v /= 10; }
            while (j > 0) buf[i++] = tmp[--j];
        }
        buf[i] = '\0';
        console_puts(buf);
    }
    console_println("");

    /* Configure timer: periodic mode, our vector, initially masked */
    lapic_write(APIC_REG_TIMER_DCR, TIMER_DIV_1);
    lapic_write(APIC_REG_LVT_TIMER, LVT_MASKED | LVT_MODE_PERIODIC | APIC_TIMER_VECTOR);
    lapic_write(APIC_REG_TIMER_ICR, timer_initial_count);

    console_puts("APIC Timer: configured (masked, ready to start)\r\n");
    return 0;
}

void apic_timer_start(void) {
    /* Unmask the timer first */
    uint32_t lvt = lapic_read(APIC_REG_LVT_TIMER);
    lvt &= ~LVT_MASKED;
    lapic_write(APIC_REG_LVT_TIMER, lvt);

    /*
     * Re-arm by writing ICR after unmask.  QEMU TCG's APIC emulation arms its
     * internal timer only when ICR is written; unmasking alone may not trigger
     * a new countdown cycle in the emulator.  On real hardware this is also
     * correct — ICR write restarts the countdown from the programmed value.
     */
    lapic_write(APIC_REG_TIMER_ICR, timer_initial_count);

    /* Readback verification — confirm writes reached the LAPIC */
    uint32_t lvt_rb  = lapic_read(APIC_REG_LVT_TIMER);
    uint32_t icr_rb  = lapic_read(APIC_REG_TIMER_ICR);
    console_puts("APIC Timer: started (LVT=0x");
    {
        for (int s = 28; s >= 0; s -= 4)
            console_putc("0123456789abcdef"[(lvt_rb >> s) & 0xF]);
    }
    console_puts(" ICR=");
    {
        char buf[16]; int i = 0;
        uint32_t v = icr_rb;
        if (v == 0) buf[i++] = '0';
        else { char tmp[16]; int j = 0;
               while (v > 0) { tmp[j++] = '0' + (v % 10); v /= 10; }
               while (j > 0) buf[i++] = tmp[--j]; }
        buf[i] = '\0';
        console_puts(buf);
    }
    console_puts(")\r\n");
}

void apic_timer_stop(void) {
    /* Mask the timer to stop interrupts */
    uint32_t lvt = lapic_read(APIC_REG_LVT_TIMER);
    lvt |= LVT_MASKED;
    lapic_write(APIC_REG_LVT_TIMER, lvt);
}

uint64_t apic_timer_period_tsc(void) {
    return timer_period_tsc_ticks;
}
