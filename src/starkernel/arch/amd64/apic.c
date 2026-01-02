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

/* Core registers */
#define APIC_REG_ID            0x020  /* APIC ID */
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

    /* Enable local APIC via Spurious Interrupt Vector Register */
    const uint8_t  SPURIOUS_VECTOR = 0xFF;
    const uint32_t APIC_ENABLE = (1u << 8);

    lapic_write(APIC_REG_SIVR, APIC_ENABLE | SPURIOUS_VECTOR);
    console_println("APIC enabled (SIVR=0xFF).");
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
    /* Unmask the timer to start periodic interrupts */
    uint32_t lvt = lapic_read(APIC_REG_LVT_TIMER);
    lvt &= ~LVT_MASKED;  /* Clear mask bit */
    lapic_write(APIC_REG_LVT_TIMER, lvt);
    console_puts("APIC Timer: started\r\n");
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
