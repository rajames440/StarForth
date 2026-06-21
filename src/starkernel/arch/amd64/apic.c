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

/**
 * @brief Read a 64-bit Model Specific Register via @c RDMSR.
 *
 * Executes the @c RDMSR instruction with @p msr in @c ECX. The processor
 * returns the high 32 bits in @c EDX and the low 32 bits in @c EAX; this
 * function combines them into a single 64-bit value. Must run at CPL 0
 * (ring 0); a #GP fault results if executed in user mode or if @p msr
 * denotes a reserved or non-existent register.
 *
 * @param msr 32-bit MSR index (e.g., @c IA32_APIC_BASE_MSR = 0x1B).
 * @return Current 64-bit value of the specified MSR.
 */
static uint64_t rdmsr64(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

/**
 * @brief Write a 64-bit value to a Model Specific Register via @c WRMSR.
 *
 * Executes the @c WRMSR instruction with @p msr in @c ECX, the high 32 bits
 * of @p val in @c EDX, and the low 32 bits in @c EAX. Must run at CPL 0;
 * a #GP results for reserved, non-existent, or read-only MSRs, or for writes
 * that set reserved bits. Used here exclusively to manipulate
 * @c IA32_APIC_BASE_MSR to switch from x2APIC to xAPIC mode and to assert
 * the global hardware-enable bit.
 *
 * @param msr 32-bit MSR index.
 * @param val 64-bit value to write.
 */
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

/**
 * @brief Write a 32-bit value to a Local APIC MMIO register.
 *
 * All xAPIC registers are 32-bit wide and 16-byte aligned within the
 * 4 KB MMIO page at @c lapic_base. The register offset @p reg is a
 * byte offset (e.g., @c APIC_REG_EOI = 0x0B0); dividing by 4 converts
 * it to a @c uint32_t array index. Writes must be 32-bit aligned and
 * must not be merged or reordered by the compiler, hence the
 * @c volatile qualifier on @c lapic_base.
 *
 * @param reg Byte offset of the APIC register (e.g., @c APIC_REG_EOI).
 * @param val 32-bit value to write.
 */
static void lapic_write(uint32_t reg, uint32_t val) {
    lapic_base[reg / 4] = val;
}

/**
 * @brief Read a 32-bit value from a Local APIC MMIO register.
 *
 * Performs a @c volatile 32-bit MMIO load from the register at byte
 * offset @p reg within the LAPIC page. The @c volatile qualifier on
 * @c lapic_base prevents the compiler from caching the value across
 * reads, which is essential for registers that change asynchronously
 * (e.g., @c APIC_REG_TIMER_CCR which counts down independently of the
 * CPU instruction stream).
 *
 * @param reg Byte offset of the APIC register (e.g., @c APIC_REG_TIMER_CCR).
 * @return Current 32-bit value of the register.
 */
static uint32_t lapic_read(uint32_t reg) {
    return lapic_base[reg / 4];
}

/* ============================================================================
 * APIC Initialization
 * ============================================================================ */

/**
 * @brief Initialise the Local APIC in xAPIC MMIO mode.
 *
 * Called during M4 (interrupt controller init) before the IDT is loaded.
 * Performs the following steps:
 *
 * 1. Reads @c IA32_APIC_BASE_MSR (0x1B) to determine the current APIC mode.
 * 2. If x2APIC mode (bit 10) is active, clears it to switch to xAPIC MMIO mode
 *    while keeping the global-enable bit (bit 11) set, as required by Intel
 *    SDM Vol.3 §10.12.5 (never clear both bits simultaneously).
 * 3. If the global-enable bit (bit 11) was clear, sets it.
 * 4. Software-enables the APIC via @c APIC_REG_SIVR (Spurious Interrupt Vector
 *    Register) and assigns spurious vector 0xFF.
 * 5. Clears the Task Priority Register (TPR=0) so all interrupt priorities
 *    are delivered; a non-zero TPR left by UEFI would silently suppress all
 *    APIC interrupts at or below that priority level.
 * 6. Issues a spurious EOI write to clear any stale in-service interrupt
 *    that UEFI may have left pending (specifically the APIC timer vector
 *    that fires just before @c ExitBootServices).
 *
 * The @p boot_info parameter is reserved for future use (e.g., reading the
 * ACPI MADT for the physical LAPIC base address); it is currently unused and
 * the default identity-mapped address @c LAPIC_DEFAULT_PHYS = 0xFEE00000 is
 * always used.
 *
 * @param boot_info Pointer to kernel @c BootInfo (ACPI/memory-map data);
 *                  currently unused, reserved for MADT-based LAPIC relocation.
 * @return 0 on success (always; the APIC is assumed to be present on x86-64).
 */
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

/**
 * @brief Signal End of Interrupt to the Local APIC.
 *
 * Writes zero to @c APIC_REG_EOI (offset 0x0B0). This informs the APIC
 * that the currently serviced interrupt has been handled so that the next
 * interrupt in the vector's priority class can be delivered. Must be called
 * at the end of every interrupt service routine before @c IRET; omitting
 * the EOI stalls all future interrupt delivery at the same or lower priority.
 *
 * For the APIC timer heartbeat path this is called inside
 * @c isr_common_handler() immediately after @c heartbeat_tick().
 */
void apic_eoi(void) {
    lapic_write(APIC_REG_EOI, 0);
}

/* ============================================================================
 * APIC Timer
 * ============================================================================ */

/**
 * @brief Calibrate the APIC timer against the TSC to determine its frequency.
 *
 * The APIC timer runs from an internal bus-clock-derived source that has no
 * fixed architectural relationship to the TSC. This function measures the
 * APIC timer frequency empirically using a TSC-timed spin loop:
 *
 * 1. Configures the APIC timer DCR (Divide Configuration Register) to
 *    divisor 1 for maximum resolution.
 * 2. Masks the LVT timer entry so no interrupt fires during calibration.
 * 3. Loads the maximum initial count (0xFFFFFFFF) so the counter does not
 *    wrap during the measurement window.
 * 4. Spins for a calibration window of @c tsc_hz/100 TSC ticks (≈ 10 ms)
 *    using @c arch_read_timestamp(). Falls back to 10,000,000 cycles if
 *    @p tsc_hz is not yet known.
 * 5. Reads @c APIC_REG_TIMER_CCR and subtracts from 0xFFFFFFFF to get the
 *    number of APIC ticks that elapsed in the window.
 * 6. Scales by @c tsc_hz / calibration_tsc_ticks to obtain APIC Hz.
 *
 * If @p tsc_hz is 0 (TSC not yet calibrated), the APIC count is multiplied
 * by 100 as a rough fallback (assumes the 10M-cycle window was ≈ 10 ms on
 * a 1 GHz core — good enough for initial bringup on QEMU TCG).
 *
 * @param tsc_hz TSC frequency in Hz from @c timer_init(); 0 if not yet known.
 * @return Measured APIC timer frequency in ticks per second (APIC Hz),
 *         or 0 if calibration could not produce a meaningful result.
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

/**
 * @brief Initialise and configure the APIC timer for periodic heartbeat delivery.
 *
 * Calibrates the APIC timer frequency via @c calibrate_apic_timer() then
 * programs the LVT Timer Register and Initial Count Register to deliver
 * @c APIC_TIMER_VECTOR periodically at @p tick_hz interrupts per second.
 * The timer is left @b masked after initialisation; call @c apic_timer_start()
 * after the IDT is loaded and interrupts are enabled to begin delivery.
 *
 * Derived quantities stored for later use:
 * - @c timer_initial_count — APIC ticks per heartbeat period
 *   (@c apic_hz / @p tick_hz), written to @c APIC_REG_TIMER_ICR.
 * - @c timer_tick_hz — requested tick rate (100 Hz default).
 * - @c timer_period_tsc_ticks — expected TSC ticks per heartbeat
 *   (@c tsc_hz / @p tick_hz), used by @c heartbeat_tick() to compute
 *   inter-tick variance for the TIME-TRUST Q48.16 metric.
 *
 * If @p tick_hz is 0 it defaults to 100 Hz (10 ms period). If calibration
 * returns 0 (no APIC timer detected or counter stuck) the function prints
 * a diagnostic and returns -1 without programming the timer.
 *
 * @param tsc_hz TSC frequency in Hz (from @c timer_init()); used to derive
 *               the 10 ms calibration window and @c timer_period_tsc_ticks.
 *               Pass 0 if TSC is not yet calibrated.
 * @param tick_hz Desired interrupt rate in Hz; 0 → defaults to 100 Hz.
 * @return 0 on success, -1 if APIC timer calibration failed.
 */
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

/**
 * @brief Unmask and re-arm the APIC timer to begin periodic interrupt delivery.
 *
 * Called after @c arch_interrupts_init() and @c arch_enable_interrupts()
 * to start the 100 Hz heartbeat that drives @c heartbeat_tick(). The
 * sequence is:
 *
 * 1. Read the current LVT Timer Register.
 * 2. Clear the @c LVT_MASKED bit (bit 16) to unmask the interrupt.
 * 3. Write the LVT register back.
 * 4. Write @c timer_initial_count to @c APIC_REG_TIMER_ICR to (re-)arm the
 *    countdown. QEMU TCG requires an explicit ICR write after unmask — it
 *    does not auto-restart a previously masked periodic timer. On real
 *    hardware this is harmless and equally correct.
 * 5. Reads back LVT and ICR values and prints them for boot-log verification.
 *
 * Must be called exactly once per boot after the IDT is loaded. Calling it
 * again restarts the countdown from the initial value without otherwise
 * reconfiguring the timer.
 */
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

/**
 * @brief Mask the APIC timer to suppress further periodic interrupts.
 *
 * Sets the @c LVT_MASKED bit (bit 16) in the LVT Timer Register by
 * performing a read-modify-write on @c APIC_REG_LVT_TIMER. The existing
 * periodic-mode and vector configuration is preserved; only the mask bit
 * changes. The APIC timer's internal countdown continues running but
 * interrupt delivery is suppressed. Calling @c apic_timer_start() again
 * after this function will unmask and re-arm the timer.
 *
 * Used in the kernel panic path and during any critical section where
 * heartbeat interrupts would corrupt shared state.
 */
void apic_timer_stop(void) {
    /* Mask the timer to stop interrupts */
    uint32_t lvt = lapic_read(APIC_REG_LVT_TIMER);
    lvt |= LVT_MASKED;
    lapic_write(APIC_REG_LVT_TIMER, lvt);
}

/**
 * @brief Return the expected TSC-tick count per APIC heartbeat period.
 *
 * Returns @c timer_period_tsc_ticks, computed by @c apic_timer_init() as
 * @c tsc_hz / @c tick_hz. The value represents the ideal number of TSC
 * ticks that should elapse between consecutive @c APIC_TIMER_VECTOR
 * interrupts when the TSC runs at @c tsc_hz Hz and the heartbeat runs at
 * @c tick_hz Hz.
 *
 * Used by @c timer.c to seed the heartbeat rolling window with the
 * nominal inter-tick interval so that the first few ticks produce
 * meaningful variance estimates rather than comparing against zero.
 * Returns 0 if @c tsc_hz was 0 at initialisation time (TSC unknown).
 *
 * @return Expected TSC ticks per heartbeat period, or 0 if unavailable.
 */
uint64_t apic_timer_period_tsc(void) {
    return timer_period_tsc_ticks;
}
