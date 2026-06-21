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
 * timer.c - Timer policy + TSC calibration (amd64)
 *
 * The kernel will always run with the best time source available, but will only
 * make determinism claims when the hardware contract is provably satisfied.
 *
 * Policy split:
 *  - VM path: avoid HPET MMIO. Prefer invariant TSC for ABSOLUTE time. Derive TSC Hz
 *    from CPUID leaves (0x15/0x16) when available; otherwise fall back to ACPI PM Timer
 *    (port I/O, stable under QEMU/KVM) to obtain TSC Hz. If invariant TSC is absent,
 *    kernel continues in RELATIVE time mode (PM Timer-based), unless strict mode is enabled.
 *  - Bare metal path: calibrate TSC using HPET as primary reference, with optional PIT
 *    cross-check. Fail hard on non-convergence.
 *
 * Failure mode: PANIC/HALT when invariants are not met.
 *
 * Strict VM policy:
 *   -DTIMER_VM_STRICT_INVARIANT_TSC=1 enforces invariant TSC in VM mode (fail-hard).
 */

#include <stdint.h>
#include "timer.h"
#include "console.h"
#include "vmm.h"

/* HPET registers */
#define HPET_GEN_CAP_ID      0x000
#define HPET_GEN_CONFIG      0x010
#define HPET_MAIN_COUNTER    0x0F0

#define HPET_PHYS_BASE       0xFED00000ull
#define HPET_VIRT_BASE       0xFED00000ull /* identity-mapped in this setup */

/* PIT (bare metal cross-check only; optional) */
#define PIT_CHANNEL0_DATA    0x40
#define PIT_COMMAND          0x43
#define PIT_FREQ_HZ          1193182u

/* PM Timer (VM-friendly reference; 3.579545 MHz; 24-bit typical) */
#define PMTIMER_IO_PORT      0x408
#define PMTIMER_FREQ_HZ      3579545ull
#define PMTIMER_WIDTH_BITS   24
#define PMTIMER_MASK         ((1ull << PMTIMER_WIDTH_BITS) - 1ull)

/* Convergence parameters (bare metal path) */
#define WARMUP_WINDOWS       2
#define SAMPLE_WINDOWS       8
#define WINDOW_TICKS_HPET    10000ull      /* ~1ms at 10MHz nominal */
#define WINDOW_TICKS_PIT     4000u         /* ~3.3ms nominal */
#define CV_EPSILON_PPM       1000ull       /* 0.1% */
#define CV_STABLE_COUNT      6
#define DRIFT_EPSILON_PPM    2000ull

#ifndef TIMER_REQUIRE_PIT
#define TIMER_REQUIRE_PIT    0
#endif

#ifndef TIMER_VM_STRICT_INVARIANT_TSC
#define TIMER_VM_STRICT_INVARIANT_TSC 0
#endif

static volatile uint64_t *hpet_regs = 0;
static uint64_t hpet_freq_hz = 0;

static uint64_t tsc_hz_locked = 0;
static uint64_t pit_tsc_hz_mean = 0;
static uint32_t windows_used = 0;

static timer_calibration_record_t calib_record = {0};
static int pit_available = 0;

/* VM RELATIVE time base (PM Timer anchored) */
static uint32_t vm_pm_start = 0;
static uint64_t vm_ns_base  = 0;

/* ------------ tiny utils (serial-friendly) ------------ */

/**
 * @brief Compute @c (a × b) / c using 128-bit intermediate precision.
 *
 * Uses the x86-64 @c MUL instruction to produce a 128-bit product in
 * @c RDX:RAX, then @c DIV to produce a 64-bit quotient. This avoids the
 * overflow that would occur with a 64-bit-only intermediate when @p a and
 * @p b are both large (e.g., nanosecond conversion of multi-GHz tick counts).
 * No libgcc dependency — pure inline assembly.
 *
 * Undefined behaviour if @c a × b / c overflows @c uint64_t (i.e., the
 * true quotient exceeds @c UINT64_MAX). Callers must ensure this cannot
 * happen with their inputs.
 *
 * @param a Multiplicand.
 * @param b Multiplier.
 * @param c Divisor (must be non-zero; division by zero causes a #DE fault).
 * @return Exact quotient @c (a * b) / c, truncated toward zero.
 */
static inline uint64_t muldiv64(uint64_t a, uint64_t b, uint64_t c)
{
    uint64_t lo, hi, result;
    /* mul: RDX:RAX = RAX * operand */
    __asm__ volatile (
        "mulq %3\n\t"       /* RDX:RAX = a * b */
        "divq %4\n\t"       /* RAX = RDX:RAX / c, RDX = remainder */
        : "=a"(result), "=d"(hi), "=r"(lo)
        : "r"(b), "r"(c), "0"(a)
        : "cc"
    );
    (void)hi;
    (void)lo;
    return result;
}

/**
 * @brief Print a @c uint64_t value as decimal digits to the kernel console.
 *
 * Uses a local reverse-digit buffer and @c console_putc(); no libc or
 * heap allocation required. Prints @c "0" for a zero value. Used throughout
 * the timer subsystem to emit calibration results without depending on
 * @c printf().
 *
 * @param val Value to print in decimal.
 */
static void print_dec(uint64_t val)
{
    char buf[32];
    int i = 0;
    if (val == 0) {
        console_putc('0');
        return;
    }
    while (val > 0 && i < (int)sizeof(buf)) {
        buf[i++] = (char)('0' + (val % 10));
        val /= 10;
    }
    while (i-- > 0) {
        console_putc(buf[i]);
    }
}

/**
 * @brief Read the x86-64 Time Stamp Counter without serialisation (RDTSC).
 *
 * Executes the @c RDTSC instruction and combines @c EDX:EAX into a 64-bit
 * result. May be reordered relative to surrounding instructions by the
 * out-of-order execution unit; use @c rdtscp_aux() where ordering matters.
 *
 * @return Current TSC value in CPU cycles.
 */
static inline uint64_t rdtsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/**
 * @brief Read TSC and processor ID with weak serialisation (RDTSCP).
 *
 * @c RDTSCP is weakly serialising: all prior loads are complete before the
 * read, but it does not wait for prior stores. Optionally captures the
 * @c IA32_TSC_AUX MSR value (processor ID / NUMA node) in @p aux. Falls
 * back to @c rdtsc() at call sites that check @c cpu_has_rdtscp() first.
 *
 * @param aux If non-NULL, receives the @c ECX value from @c RDTSCP
 *            (processor/socket ID programmed by the OS or hypervisor).
 * @return Current TSC value in CPU cycles.
 */
static inline uint64_t rdtscp_aux(uint32_t *aux)
{
    uint32_t lo, hi, a;
    __asm__ volatile ("rdtscp" : "=a"(lo), "=d"(hi), "=c"(a) ::);
    if (aux) *aux = a;
    return ((uint64_t)hi << 32) | lo;
}

/**
 * @brief Execute the x86-64 @c CPUID instruction and capture all output registers.
 *
 * Passes @p leaf in @c EAX and @p subleaf in @c ECX before executing @c CPUID.
 * Any of the output pointers may be @c NULL; the corresponding register value
 * is then discarded.
 *
 * @param leaf    CPUID function leaf (EAX input).
 * @param subleaf CPUID sub-leaf (ECX input, for structured extended leaves).
 * @param a       If non-NULL, receives @c EAX on return.
 * @param b       If non-NULL, receives @c EBX on return.
 * @param c       If non-NULL, receives @c ECX on return.
 * @param d       If non-NULL, receives @c EDX on return.
 */
static inline void cpuid(uint32_t leaf, uint32_t subleaf,
                         uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile ("cpuid"
                      : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                      : "a"(leaf), "c"(subleaf));
    if (a) *a = eax;
    if (b) *b = ebx;
    if (c) *c = ecx;
    if (d) *d = edx;
}

/**
 * @brief Write a byte to an x86 I/O port (OUT instruction).
 *
 * @param port I/O port address.
 * @param val  Byte value to write.
 */
static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/**
 * @brief Read a byte from an x86 I/O port (IN instruction).
 *
 * @param port I/O port address.
 * @return Byte value read from the port.
 */
static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/**
 * @brief Read a 32-bit dword from an x86 I/O port (INL instruction).
 *
 * Used to read the ACPI PM Timer register which is a 24-bit counter
 * accessed as a 32-bit I/O port.
 *
 * @param port I/O port address.
 * @return 32-bit value read from the port.
 */
static inline uint32_t inl(uint16_t port)
{
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/**
 * @brief Read a 64-bit HPET register at @p offset bytes from the HPET base.
 *
 * Accesses @c hpet_regs[] (the identity-mapped MMIO region) using a
 * naturally-aligned 64-bit read. @p offset must be a multiple of 8.
 *
 * @param offset Byte offset of the target register (e.g., @c HPET_MAIN_COUNTER = 0x0F0).
 * @return 64-bit register value.
 */
static inline uint64_t hpet_read(uint32_t offset)
{
    return hpet_regs[offset / 8];
}

/**
 * @brief Write a 64-bit value to an HPET register at @p offset bytes from base.
 *
 * @param offset Byte offset of the target register (must be a multiple of 8).
 * @param val    64-bit value to write.
 */
static inline void hpet_write(uint32_t offset, uint64_t val)
{
    hpet_regs[offset / 8] = val;
}

/**
 * @brief Emit a fatal timer error message and halt the processor permanently.
 *
 * Prints @p msg followed by @c "Timer subsystem fatal. Halting." to the
 * kernel console, then enters an infinite @c HLT loop. Never returns.
 * Called when a hard timer invariant cannot be satisfied (e.g., HPET
 * counter not advancing, TSC calibration failed to converge).
 *
 * @param msg Null-terminated error description to print before halting.
 */
static void timer_fatal(const char *msg)
{
    console_println(msg);
    console_println("Timer subsystem fatal. Halting.");
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/* ------------ CPU capability / environment detection ------------ */

/**
 * @brief Test whether the processor supports Invariant TSC (non-stop TSC).
 *
 * Reads @c CPUID.80000007H:EDX[8]. Invariant TSC means the TSC counts at a
 * constant rate regardless of CPU frequency scaling or sleep states — a
 * requirement for using RDTSC as a reliable nanosecond source.
 *
 * @return 1 if invariant TSC is supported, 0 otherwise.
 */
static int cpu_has_invariant_tsc(void)
{
    /* CPUID.80000007H:EDX[8] = Invariant TSC */
    uint32_t a, b, c, d;
    cpuid(0x80000007u, 0, &a, &b, &c, &d);
    return (d & (1u << 8)) ? 1 : 0;
}

/**
 * @brief Test whether the processor supports the RDTSCP instruction.
 *
 * Reads @c CPUID.80000001H:EDX[27]. RDTSCP provides weak serialisation and
 * simultaneously reads both the TSC and the @c IA32_TSC_AUX MSR (which the
 * OS programs to hold a per-CPU/socket identifier). Used in @c timer_now_ns()
 * to prefer RDTSCP over the non-serialising RDTSC when available.
 *
 * @return 1 if RDTSCP is supported, 0 otherwise.
 */
static int cpu_has_rdtscp(void)
{
    /* CPUID.80000001H:EDX[27] = RDTSCP */
    uint32_t a, b, c, d;
    cpuid(0x80000001u, 0, &a, &b, &c, &d);
    return (d & (1u << 27)) ? 1 : 0;
}

/**
 * @brief Detect whether the kernel is running under a hypervisor.
 *
 * Reads @c CPUID.1:ECX[31] (Hypervisor Present bit). When set, the kernel
 * switches to VM mode: HPET MMIO calibration is disabled (VM-exits would
 * poison timing), and TSC frequency is derived from CPUID leaves 0x15/0x16
 * or the ACPI PM Timer instead.
 *
 * @return 1 if a hypervisor is present (including QEMU/TCG), 0 on bare metal.
 */
static int running_under_hypervisor(void)
{
    /* CPUID.1:ECX[31] = Hypervisor present */
    uint32_t a, b, c, d;
    cpuid(1u, 0, &a, &b, &c, &d);
    return (c & (1u << 31)) ? 1 : 0;
}

/* ------------ PM Timer (VM-friendly reference) ------------ */

/**
 * @brief Read the raw ACPI PM Timer counter from I/O port @c PMTIMER_IO_PORT.
 *
 * The PM Timer runs at 3.579545 MHz and is a 24-bit counter (bits [23:0])
 * in most systems. The returned 32-bit value may contain non-zero bits
 * above bit 23 on 32-bit PM Timer implementations; callers must mask with
 * @c PMTIMER_MASK before use.
 *
 * @return Raw 32-bit PM Timer register value.
 */
static inline uint32_t pmtimer_read(void)
{
    return inl(PMTIMER_IO_PORT);
}

/**
 * @brief Compute elapsed PM Timer ticks from @p start to @p cur, handling wrap-around.
 *
 * Masks both values to @c PMTIMER_WIDTH_BITS (24 bits) before comparison.
 * When the counter wraps — i.e., @p cur < @p start after masking — the
 * function returns the distance through the wrap: @c (MASK - start + 1 + cur).
 *
 * @param start PM Timer value at the beginning of the interval (unmasked).
 * @param cur   PM Timer value at the end of the interval (unmasked).
 * @return Number of PM Timer ticks elapsed, accounting for a single wrap.
 */
static uint64_t pmtimer_delta(uint32_t start, uint32_t cur)
{
    uint32_t s = start & (uint32_t)PMTIMER_MASK;
    uint32_t c = cur   & (uint32_t)PMTIMER_MASK;

    if (c >= s) return (uint64_t)(c - s);
    return (uint64_t)((PMTIMER_MASK - s) + 1u + c);
}

/**
 * @brief Calibrate TSC frequency using the ACPI PM Timer as reference.
 *
 * Waits for 1000 PM Timer ticks (~279 µs at 3.58 MHz), measures the TSC
 * delta over the same interval, then converts to Hz using @c muldiv64().
 * Includes a 5-million-iteration timeout to avoid hanging under TCG or
 * broken PM Timer implementations.
 *
 * Used by the VM path when CPUID leaves 0x15/0x16 do not provide a TSC
 * frequency. The PM Timer is port I/O-based and does not cause VM-exits on
 * most hypervisors (unlike HPET MMIO), making it suitable for VM calibration.
 *
 * @return TSC frequency in Hz, or 0 if calibration timed out or produced an
 *         invalid result.
 */
static uint64_t calibrate_tsc_with_pmtimer(void)
{
    /* Use a small delta to avoid long waits under TCG */
    const uint32_t target_ticks = 1000u; /* ~279us at 3.58MHz */
    const uint64_t timeout_iter = 5000000ull;

    uint32_t start = pmtimer_read() & (uint32_t)PMTIMER_MASK;
    uint64_t start_tsc = rdtsc();
    uint64_t iters = 0;

    while (1) {
        uint32_t cur = pmtimer_read() & (uint32_t)PMTIMER_MASK;
        uint64_t delta = pmtimer_delta(start, cur);

        if (delta >= target_ticks) {
            uint64_t end_tsc = rdtsc();
            uint64_t elapsed_ticks = delta;

            uint64_t elapsed_ns = muldiv64(elapsed_ticks, 1000000000ull, PMTIMER_FREQ_HZ);
            if (elapsed_ns == 0) return 0;

            uint64_t delta_tsc = end_tsc - start_tsc;
            return muldiv64(delta_tsc, 1000000000ull, elapsed_ns);
        }

        if (++iters > timeout_iter) {
            return 0;
        }
    }
}

/**
 * @brief Initialise the VM relative-time base anchored to the PM Timer.
 *
 * Records the current PM Timer counter value in @c vm_pm_start and resets
 * @c vm_ns_base to zero. Subsequent calls to @c timer_now_ns_vm_relative()
 * compute elapsed nanoseconds from this anchor point.
 *
 * Called during @c init_timer_vm_mode() after the calibration phase, so the
 * first call to @c timer_now_ns() returns a small positive value rather than
 * a large absolute-time offset.
 */
static void vm_timebase_init_from_pmtimer(void)
{
    vm_pm_start = pmtimer_read() & (uint32_t)PMTIMER_MASK;
    vm_ns_base  = 0;
}

/* ------------ HPET helpers (bare metal path) ------------ */

/**
 * @brief Enable the HPET main counter by setting the ENABLE_CNF bit.
 *
 * Reads the @c HPET_GEN_CONFIG register, sets bit 0 (ENABLE_CNF), and
 * writes it back. Must be called before the main counter is used for
 * calibration. The HPET must be identity-mapped at @c HPET_VIRT_BASE
 * before this function is called.
 */
static void enable_hpet(void)
{
    uint64_t cfg = hpet_read(HPET_GEN_CONFIG);
    cfg |= 1ull;
    hpet_write(HPET_GEN_CONFIG, cfg);
}

/**
 * @brief Verify the HPET main counter is advancing (quick sanity check).
 *
 * Reads the @c HPET_MAIN_COUNTER repeatedly up to 200,000 times. If the
 * counter value changes at least once, the check passes silently. If the
 * counter never changes — indicating a hardware fault, frozen HPET, or
 * mapping error — calls @c timer_fatal() which halts the processor.
 *
 * Only used on the bare-metal path; skipped in VM mode.
 */
static void sanity_check_hpet_running_quick(void)
{
    uint64_t start = hpet_read(HPET_MAIN_COUNTER);
    for (int i = 0; i < 200000; ++i) {
        if (hpet_read(HPET_MAIN_COUNTER) != start) {
            return;
        }
    }
    timer_fatal("HPET counter not advancing.");
}

/* ------------ PIT helpers (bare metal only) ------------ */

/**
 * @brief Programme the 8253/8254 PIT Channel 0 with a 16-bit reload value.
 *
 * Issues the mode command byte (0x34 = channel 0, lo/hi access, mode 2
 * rate generator) followed by the low and high bytes of @p reload to the
 * PIT data port. After this call the PIT downcounts at @c PIT_FREQ_HZ
 * (1193182 Hz) and reloads automatically.
 *
 * @param reload 16-bit initial counter value (higher = longer period).
 */
static void pit_program_reload(uint16_t reload)
{
    outb(PIT_COMMAND, 0x34);
    outb(PIT_CHANNEL0_DATA, (uint8_t)(reload & 0xFF));
    outb(PIT_CHANNEL0_DATA, (uint8_t)((reload >> 8) & 0xFF));
}

/**
 * @brief Read the current 16-bit counter value from PIT Channel 0.
 *
 * Sends a latch command (0x00) to the PIT command port to atomically snap
 * the current count, then reads the low and high bytes from the Channel 0
 * data port in two @c INB instructions. Returns the counter as a 16-bit
 * value; note the PIT counter counts down from the reload value.
 *
 * @return Current PIT Channel 0 counter value (decreasing, wraps at 0).
 */
static uint16_t pit_read_counter16(void)
{
    outb(PIT_COMMAND, 0x00);
    uint8_t lo = inb(PIT_CHANNEL0_DATA);
    uint8_t hi = inb(PIT_CHANNEL0_DATA);
    return (uint16_t)((hi << 8) | lo);
}

/**
 * @brief Probe whether the PIT is available and counting.
 *
 * Programs the PIT with reload value 0xFFFF and polls up to 20,000 times
 * for a counter change. Returns 1 if the counter advances (PIT running),
 * 0 if it never changes (PIT absent, frozen, or emulation disabled). Used
 * on the bare-metal path to decide whether to enable the PIT cross-check
 * for TSC convergence.
 *
 * @return 1 if the PIT is running, 0 if it is unavailable.
 */
static int pit_probe_running(void)
{
    pit_program_reload(0xFFFF);
    uint16_t last = pit_read_counter16();
    for (uint32_t i = 0; i < 20000u; ++i) {
        uint16_t cur = pit_read_counter16();
        if (cur != last) return 1;
    }
    return 0;
}

/* ------------ stats / convergence (bare metal) ------------ */

/**
 * @brief Compute mean and standard deviation of an array of @c uint64_t samples.
 *
 * Uses 64-bit integer arithmetic throughout. For TSC-frequency samples
 * (~3 GHz) with ≤16 windows, both the sum and variance accumulator remain
 * within @c uint64_t range so long as deviations are below ~1 MHz (which is
 * well within the @c CV_EPSILON_PPM = 1000 ppm convergence bound).
 *
 * Standard deviation is approximated via integer Newton-Raphson square root
 * (10 iterations) over the variance.
 *
 * Sets @c *mean = @c *stddev = 0 when @p count is 0.
 *
 * @param samples Array of @p count frequency samples in Hz.
 * @param count   Number of valid samples.
 * @param mean    Output: arithmetic mean of @p samples.
 * @param stddev  Output: population standard deviation of @p samples.
 */
static void update_stats(const uint64_t *samples, uint32_t count, uint64_t *mean, uint64_t *stddev)
{
    if (count == 0) {
        *mean = 0;
        *stddev = 0;
        return;
    }

    /* For small sample counts (<=16) of Hz values (~3GHz), 64-bit sum is safe */
    uint64_t sum = 0;
    for (uint32_t i = 0; i < count; ++i) sum += samples[i];
    uint64_t m = sum / count;

    /* Variance: sum of (sample - mean)^2 / count
     * Each (sample - mean) is at most ~1MHz for converged data.
     * (1e6)^2 * 16 = 16e12, fits in 64 bits. */
    uint64_t var_acc = 0;
    for (uint32_t i = 0; i < count; ++i) {
        int64_t d = (int64_t)samples[i] - (int64_t)m;
        var_acc += (uint64_t)(d * d);
    }

    uint64_t variance = var_acc / count;
    *mean = m;

    if (variance == 0) {
        *stddev = 0;
        return;
    }

    /* Integer square root via Newton-Raphson */
    uint64_t x = variance;
    uint64_t r = x;
    for (int i = 0; i < 10; ++i) {
        r = (r + x / r) / 2;
    }
    *stddev = r;
}

/**
 * @brief Measure TSC frequency over a fixed HPET tick window.
 *
 * Spins until the HPET main counter advances by @p target_ticks ticks,
 * measuring the TSC delta over the same interval. Converts the result to Hz
 * via @c muldiv64(). Returns 0 if the elapsed nanoseconds are zero (which
 * indicates an invalid HPET frequency or overflow).
 *
 * Used exclusively on the bare-metal path; the tight spin loop would cause
 * excessive VM-exits if called under a hypervisor.
 *
 * @param target_ticks HPET ticks to wait (~1 ms at 10 MHz HPET).
 * @return Estimated TSC frequency in Hz, or 0 on error.
 */
static uint64_t calibrate_window_hpet(uint64_t target_ticks)
{
    uint64_t start_cnt = hpet_read(HPET_MAIN_COUNTER);
    uint64_t start_tsc = rdtsc();

    while ((hpet_read(HPET_MAIN_COUNTER) - start_cnt) < target_ticks) {
        /* tight loop; bare metal only */
    }

    uint64_t end_tsc = rdtsc();
    uint64_t elapsed_ticks = hpet_read(HPET_MAIN_COUNTER) - start_cnt;

    uint64_t elapsed_ns = muldiv64(elapsed_ticks, 1000000000ull, hpet_freq_hz);
    if (elapsed_ns == 0) return 0;

    uint64_t delta_tsc = end_tsc - start_tsc;
    return muldiv64(delta_tsc, 1000000000ull, elapsed_ns);
}

/**
 * @brief Measure TSC frequency over a fixed PIT tick window.
 *
 * Programs Channel 0 with reload 0xFFFF and polls until @p target_ticks
 * PIT ticks have elapsed (up to 200,000 poll iterations). Returns the TSC
 * Hz measured over the interval, or 0 if @c pit_available is false or the
 * poll timed out.
 *
 * @param target_ticks Number of PIT ticks to measure over (~3.3 ms at 1.19 MHz).
 * @return Estimated TSC frequency in Hz, or 0 if PIT is unavailable or timed out.
 */
static uint64_t calibrate_window_pit(uint32_t target_ticks)
{
    if (!pit_available) return 0;

    pit_program_reload(0xFFFF);
    uint16_t start_cnt = pit_read_counter16();
    uint64_t start_tsc = rdtsc();

    for (uint32_t it = 0; it < 200000u; ++it) {
        uint16_t cur = pit_read_counter16();
        uint16_t delta = (uint16_t)((start_cnt - cur) & 0xFFFF);
        if (delta >= (uint16_t)target_ticks) {
            uint64_t end_tsc = rdtsc();
            uint64_t elapsed_ns = muldiv64((uint64_t)delta, 1000000000ull, PIT_FREQ_HZ);
            if (elapsed_ns == 0) return 0;
            uint64_t delta_tsc = end_tsc - start_tsc;
            return muldiv64(delta_tsc, 1000000000ull, elapsed_ns);
        }
    }
    return 0;
}

/**
 * @brief Converge TSC frequency on bare metal using HPET + optional PIT cross-check.
 *
 * Runs @c WARMUP_WINDOWS (2) warm-up windows then up to @c SAMPLE_WINDOWS (8)
 * measurement windows, alternating HPET and PIT samples. Declares convergence
 * when @c CV_STABLE_COUNT (6) consecutive windows all have CV below
 * @c CV_EPSILON_PPM (1000 ppm). If both HPET and PIT are active the inter-source
 * drift must also be < @c CV_EPSILON_PPM before the stable counter increments.
 *
 * Fills @c calib_record on success. Calls @c timer_fatal() — never returns — on
 * any hard error (HPET window failure, PIT required but unavailable).
 *
 * @param locked_hz      Output: converged TSC frequency in Hz.
 * @param pit_mean_out   Output: mean PIT-derived TSC Hz (0 if PIT disabled).
 * @return 0 on successful convergence, -1 if the sample limit was exhausted.
 */
static int converge_tsc_bare_metal(uint64_t *locked_hz, uint64_t *pit_mean_out)
{
    uint64_t hpet_samples[SAMPLE_WINDOWS];
    uint64_t pit_samples[SAMPLE_WINDOWS];
    uint32_t hpet_count = 0;
    uint32_t pit_count  = 0;

    console_println("Timer: warm-up...");
    for (int i = 0; i < WARMUP_WINDOWS; ++i) {
        uint64_t h = calibrate_window_hpet(WINDOW_TICKS_HPET);
        if (h == 0) timer_fatal("Timer warm-up failed (HPET window).");

        if (pit_available) {
            uint64_t p = calibrate_window_pit(WINDOW_TICKS_PIT);
            if (p == 0) {
                if (TIMER_REQUIRE_PIT) timer_fatal("Timer warm-up failed (PIT window).");
                pit_available = 0;
                console_println("Timer: PIT disabled (warm-up stall).");
            }
        }
        console_putc('.');
    }
    console_println("");

    console_println("Timer: convergence...");
    uint32_t stable_streak = 0;

    for (int i = 0; i < SAMPLE_WINDOWS; ++i) {
        uint64_t h = calibrate_window_hpet(WINDOW_TICKS_HPET);
        if (h == 0) timer_fatal("Timer window failed (HPET).");
        hpet_samples[hpet_count++] = h;

        if (pit_available) {
            uint64_t p = calibrate_window_pit(WINDOW_TICKS_PIT);
            if (p == 0) {
                if (TIMER_REQUIRE_PIT) timer_fatal("Timer window failed (PIT).");
                pit_available = 0;
                console_println("Timer: PIT disabled (convergence stall).");
            } else {
                pit_samples[pit_count++] = p;
            }
        }

        if (hpet_count < 4) {
            console_putc('.');
            continue;
        }

        uint64_t mean_h = 0, std_h = 0;
        update_stats(hpet_samples, hpet_count, &mean_h, &std_h);
        uint64_t cv_h_ppm = (mean_h == 0) ? UINT64_MAX : muldiv64(std_h, 1000000ull, mean_h);

        if (!pit_available) {
            if (cv_h_ppm < CV_EPSILON_PPM) stable_streak++;
            else stable_streak = 0;

            if (stable_streak >= CV_STABLE_COUNT) {
                windows_used = i + 1;
                *locked_hz = mean_h;
                *pit_mean_out = 0;

                calib_record.hpet_hz = hpet_freq_hz;
                calib_record.tsc_hz_mean = mean_h;
                calib_record.pit_hz_mean = 0;
                calib_record.cv_hpet_ppm = cv_h_ppm;
                calib_record.cv_pit_ppm = 0;
                calib_record.diff_ppm = 0;
                calib_record.windows_used = windows_used;
                calib_record.converged = 1;
                calib_record.vm_mode = 0;
                calib_record.trust = TIMER_TRUST_ABSOLUTE;

                console_puts("Timer converged (HPET-only): TSC=");
                print_dec(mean_h);
                console_puts(" Hz, HPET CV=");
                print_dec(cv_h_ppm);
                console_println(" ppm");
                return 0;
            }

            console_putc('.');
            continue;
        }

        if (pit_count < 4) {
            console_putc('.');
            continue;
        }

        uint64_t mean_p = 0, std_p = 0;
        update_stats(pit_samples, pit_count, &mean_p, &std_p);

        uint64_t cv_p_ppm = (mean_p == 0) ? UINT64_MAX : muldiv64(std_p, 1000000ull, mean_p);

        uint64_t min_hz = (mean_h < mean_p) ? mean_h : mean_p;
        uint64_t max_hz = (mean_h > mean_p) ? mean_h : mean_p;
        uint64_t diff_ppm = (min_hz == 0) ? UINT64_MAX : muldiv64(max_hz - min_hz, 1000000ull, min_hz);

        if (cv_h_ppm < CV_EPSILON_PPM && cv_p_ppm < CV_EPSILON_PPM && diff_ppm < CV_EPSILON_PPM) stable_streak++;
        else stable_streak = 0;

        if (stable_streak >= CV_STABLE_COUNT) {
            windows_used = i + 1;
            *locked_hz = mean_h;
            *pit_mean_out = mean_p;

            calib_record.hpet_hz = hpet_freq_hz;
            calib_record.tsc_hz_mean = mean_h;
            calib_record.pit_hz_mean = mean_p;
            calib_record.cv_hpet_ppm = cv_h_ppm;
            calib_record.cv_pit_ppm = cv_p_ppm;
            calib_record.diff_ppm = diff_ppm;
            calib_record.windows_used = windows_used;
            calib_record.converged = 1;
            calib_record.vm_mode = 0;
            calib_record.trust = TIMER_TRUST_ABSOLUTE;

            console_puts("Timer converged: TSC=");
            print_dec(mean_h);
            console_puts(" Hz, HPET CV=");
            print_dec(cv_h_ppm);
            console_puts(" ppm, PIT CV=");
            print_dec(cv_p_ppm);
            console_puts(" ppm, diff=");
            print_dec(diff_ppm);
            console_println(" ppm");
            return 0;
        }

        console_putc('.');
    }

    return -1;
}

/* ------------ VM path ------------ */

/**
 * @brief Derive TSC frequency from CPUID leaves 0x15 and 0x16.
 *
 * Checks leaf 0x15 (TSC/Crystal Ratio):
 * - If EAX, EBX, and ECX are all non-zero: @c hz = @c ECX * @c EBX / @c EAX.
 *
 * Falls back to leaf 0x16 (base frequency in MHz) if leaf 0x15 gives 0.
 *
 * Both leaves are available on Broadwell and later Intel CPUs; older CPUs
 * and AMD processors may return 0. Under QEMU/KVM, leaf 0x15 is usually
 * populated; under QEMU/TCG it often is not.
 *
 * @return TSC frequency in Hz if derivable, 0 otherwise.
 */
static uint64_t derive_tsc_hz_from_cpuid(void)
{
    uint32_t a, b, c, d;

    /* CPUID.15H: TSC/Crystal ratio */
    cpuid(0x15u, 0, &a, &b, &c, &d);
    if (a != 0 && b != 0 && c != 0) {
        /* hz = (c * b) / a using muldiv64 to avoid overflow */
        uint64_t hz = muldiv64((uint64_t)c, (uint64_t)b, (uint64_t)a);
        if (hz > 0) return hz;
    }

    /* CPUID.16H: base freq in MHz (best-effort) */
    cpuid(0x16u, 0, &a, &b, &c, &d);
    if (a != 0) {
        uint64_t mhz = (uint64_t)(a & 0xFFFFu);
        if (mhz > 0) return mhz * 1000000ull;
    }

    return 0;
}

/**
 * @brief Initialise the timer subsystem in VM (hypervisor-present) mode.
 *
 * Skips HPET MMIO entirely (VM-exits would corrupt timing measurements).
 * Instead:
 * 1. Checks for invariant TSC. If absent and @c TIMER_VM_STRICT_INVARIANT_TSC
 *    is set, calls @c timer_fatal(). Otherwise logs a warning and uses
 *    @c TIMER_TRUST_RELATIVE mode.
 * 2. Attempts to derive TSC Hz from CPUID leaves 0x15/0x16.
 * 3. Falls back to @c calibrate_tsc_with_pmtimer() if CPUID yields 0.
 * 4. If TSC Hz is still 0, logs a warning and forces @c TIMER_TRUST_RELATIVE
 *    (PM Timer only; no TSC→ns conversion).
 * 5. Calls @c vm_timebase_init_from_pmtimer() to anchor relative time.
 * 6. Fills @c calib_record with @c vm_mode = 1.
 */
static void init_timer_vm_mode(void)
{
    console_println("Timer: VM mode detected (hypervisor present).");
    console_println("Timer: HPET calibration disabled (VM-exit MMIO would poison timing).");

    int inv = cpu_has_invariant_tsc();

    if (!inv) {
        if (TIMER_VM_STRICT_INVARIANT_TSC) {
            timer_fatal("Timer: invariant TSC required under hypervisor (strict).");
        }

        console_println("Timer: WARNING: invariant TSC not present under hypervisor.");
        console_println("Timer:          continuing in RELATIVE mode (no determinism guarantees).");
        calib_record.trust = TIMER_TRUST_RELATIVE;
    } else {
        calib_record.trust = TIMER_TRUST_ABSOLUTE;
    }

    if (!cpu_has_rdtscp()) {
        console_println("Timer: RDTSCP not present; using RDTSC (less serialized).");
    }

    /* Try to lock TSC Hz anyway (useful even in RELATIVE mode) */
    tsc_hz_locked = derive_tsc_hz_from_cpuid();
    if (tsc_hz_locked == 0) {
        console_println("Timer: CPUID frequency unavailable; trying PM Timer...");
        tsc_hz_locked = calibrate_tsc_with_pmtimer();
    }

    if (tsc_hz_locked == 0) {
        /* Without a frequency, TSC->ns is meaningless. In VM RELATIVE mode we can still
           provide PM Timer-based ns. */
        console_println("Timer: WARNING: could not derive TSC frequency; PM Timer will be used for RELATIVE ns.");
        calib_record.trust = TIMER_TRUST_RELATIVE;
    }

    /* Initialize VM PM Timer timebase for RELATIVE ns */
    vm_timebase_init_from_pmtimer();

    calib_record.hpet_hz = 0;
    calib_record.tsc_hz_mean = tsc_hz_locked;
    calib_record.pit_hz_mean = 0;
    calib_record.cv_hpet_ppm = 0;
    calib_record.cv_pit_ppm = 0;
    calib_record.diff_ppm = 0;
    calib_record.windows_used = 0;
    calib_record.converged = 1;
    calib_record.vm_mode = 1;

    console_puts("Timer: trust=");
    print_dec((uint64_t)calib_record.trust);
    console_puts(" (0=NONE,1=REL,2=ABS), TSC=");
    print_dec(tsc_hz_locked);
    console_println(" Hz");
}

/* ------------ public API ------------ */

/**
 * @brief Initialise the kernel timer subsystem (M5).
 *
 * Detects the execution environment (bare metal vs. hypervisor) and
 * follows the appropriate calibration path:
 *
 * - **VM / hypervisor path** (CPUID.1:ECX[31] set): calls
 *   @c init_timer_vm_mode() — HPET MMIO disabled, TSC from CPUID/PM Timer.
 * - **Bare metal path**: maps HPET MMIO, enables the counter, optionally
 *   probes the PIT, then calls @c converge_tsc_bare_metal() until TSC Hz
 *   converges within @c CV_EPSILON_PPM ppm. Calls @c timer_fatal() on any
 *   hard failure.
 *
 * On success, @c tsc_hz_locked holds the calibrated TSC frequency and
 * @c calib_record is fully populated. @c timer_now_ns() is safe to call
 * after this function returns.
 *
 * @param boot_info BootInfo pointer from the UEFI loader (currently unused;
 *                  reserved for future ACPI HPET table lookup).
 * @return 0 on success; calls @c timer_fatal() (never returns) on hard failure.
 */
int timer_init(BootInfo *boot_info)
{
    (void)boot_info;

    console_println("Timer: init start");

    /* default record */
    calib_record.trust = TIMER_TRUST_NONE;
    calib_record.converged = 0;
    calib_record.vm_mode = 0;

    if (running_under_hypervisor()) {
        init_timer_vm_mode();
        return 0;
    }

    /* Bare metal path */
    if (vmm_map_range(HPET_VIRT_BASE, HPET_PHYS_BASE, 0x1000, VMM_FLAG_WRITABLE) != 0) {
        timer_fatal("Failed to map HPET MMIO.");
    }
    hpet_regs = (volatile uint64_t *)(uintptr_t)HPET_VIRT_BASE;

    uint64_t cap = hpet_read(HPET_GEN_CAP_ID);
    uint64_t period_fs = cap >> 32;
    if (period_fs == 0) {
        timer_fatal("HPET period invalid.");
    }
    hpet_freq_hz = 1000000000000000ull / period_fs;

    enable_hpet();
    sanity_check_hpet_running_quick();

    pit_available = pit_probe_running() ? 1 : 0;
    if (!pit_available && TIMER_REQUIRE_PIT) {
        timer_fatal("PIT required but not available.");
    }

    if (converge_tsc_bare_metal(&tsc_hz_locked, &pit_tsc_hz_mean) != 0) {
        timer_fatal("TSC did not converge.");
    }

    console_puts("Timer: HPET freq = ");
    print_dec(hpet_freq_hz);
    console_println(" Hz");

    console_puts("Timer: TSC locked = ");
    print_dec(tsc_hz_locked);
    console_println(" Hz");

    if (pit_available) {
        console_puts("Timer: PIT mean  = ");
        print_dec(pit_tsc_hz_mean);
        console_println(" Hz");
    } else {
        console_println("Timer: PIT cross-check skipped.");
    }

    return 0;
}

/**
 * @brief Return the locked TSC frequency in Hz.
 *
 * Returns the value established by @c timer_init(). Zero means calibration
 * has not completed or failed to derive a TSC frequency (PM Timer relative
 * mode only).
 *
 * @return TSC frequency in Hz, or 0 if unknown.
 */
uint64_t timer_tsc_hz(void)
{
    return tsc_hz_locked;
}

/**
 * @brief Return nanoseconds since boot using the ACPI PM Timer (VM relative mode).
 *
 * Reads the current PM Timer counter, computes ticks elapsed since
 * @c vm_pm_start (set by @c vm_timebase_init_from_pmtimer()), converts to
 * nanoseconds via @c muldiv64(), and adds @c vm_ns_base. Used when the
 * calibration record shows @c vm_mode = 1 and @c trust < TIMER_TRUST_ABSOLUTE.
 *
 * @return Nanoseconds elapsed since @c vm_timebase_init_from_pmtimer() was called.
 */
static uint64_t timer_now_ns_vm_relative(void)
{
    /* Monotonic-ish RELATIVE ns based on ACPI PM Timer (port I/O). */
    uint32_t cur = pmtimer_read() & (uint32_t)PMTIMER_MASK;
    uint64_t ticks = pmtimer_delta(vm_pm_start, cur);

    uint64_t ns = muldiv64(ticks, 1000000000ull, PMTIMER_FREQ_HZ);
    return vm_ns_base + ns;
}

/**
 * @brief Return the current monotonic time in nanoseconds.
 *
 * Selects the best available time source based on the calibration record:
 *
 * - @c TIMER_TRUST_NONE: returns 0 (no time base yet).
 * - VM mode + trust < @c TIMER_TRUST_ABSOLUTE: returns PM Timer relative ns.
 * - Absolute mode (@c tsc_hz_locked > 0): reads TSC (RDTSCP if supported,
 *   else RDTSC) and converts to ns via @c muldiv64().
 *
 * Called by the HAL (@c sk_hal_time_ns()) to feed the physics engine.
 *
 * @return Nanoseconds since boot, or 0 if the timer is not yet calibrated.
 */
uint64_t timer_now_ns(void)
{
    /* If we don't have a time base yet, be honest. */
    if (calib_record.trust == TIMER_TRUST_NONE) return 0;

    /* VM RELATIVE mode: return PM Timer derived ns */
    if (calib_record.vm_mode && calib_record.trust < TIMER_TRUST_ABSOLUTE) {
        return timer_now_ns_vm_relative();
    }

    /* ABSOLUTE mode: use TSC -> ns */
    if (tsc_hz_locked == 0) return 0;

    uint64_t t;
    if (cpu_has_rdtscp()) {
        uint32_t aux;
        t = rdtscp_aux(&aux);
    } else {
        t = rdtsc();
    }

    return muldiv64(t, 1000000000ull, tsc_hz_locked);
}

/**
 * @brief Check TSC drift against the HPET reference on bare metal.
 *
 * Takes a single HPET calibration window and compares the measured TSC Hz
 * to @c tsc_hz_locked. If the drift exceeds @c DRIFT_EPSILON_PPM (2000 ppm),
 * calls @c timer_fatal() — never returns. Returns -1 if either reading is
 * invalid.
 *
 * Skipped in VM mode (@c timer_check_drift_now() returns 0 immediately).
 *
 * @return 0 if drift is within bound, -1 on measurement failure.
 */
static int timer_drift_check_bare_metal(void)
{
    uint64_t current = calibrate_window_hpet(WINDOW_TICKS_HPET);
    if (current == 0 || tsc_hz_locked == 0) return -1;

    uint64_t min_hz = (current < tsc_hz_locked) ? current : tsc_hz_locked;
    uint64_t max_hz = (current > tsc_hz_locked) ? current : tsc_hz_locked;

    uint64_t diff_ppm = (min_hz == 0) ? UINT64_MAX
        : muldiv64(max_hz - min_hz, 1000000ull, min_hz);

    if (diff_ppm > DRIFT_EPSILON_PPM) {
        timer_fatal("Timer drift exceeded runtime bound.");
    }
    return 0;
}

/**
 * @brief Perform a runtime TSC drift check (no-op in VM mode).
 *
 * In bare-metal mode delegates to @c timer_drift_check_bare_metal(). In VM
 * mode returns 0 immediately: VM drift checking requires a VM-friendly
 * reference (such as pvclock) that is not yet implemented.
 *
 * @return 0 if drift is acceptable or in VM mode, -1 on bare-metal
 *         measurement failure.
 */
int timer_check_drift_now(void)
{
    if (calib_record.vm_mode) {
        /* VM policy: drift checks require a VM-friendly reference (e.g., pvclock) */
        return 0;
    }
    return timer_drift_check_bare_metal();
}

/**
 * @brief Return a pointer to the current timer calibration record.
 *
 * Provides read-only access to the @c calib_record populated by
 * @c timer_init(). Callers can inspect fields such as @c trust,
 * @c tsc_hz_mean, @c cv_hpet_ppm, and @c converged for diagnostic output
 * or to determine whether absolute timing guarantees are available.
 *
 * @return Pointer to the static @c timer_calibration_record_t (never NULL).
 */
const timer_calibration_record_t *timer_calibration_record(void)
{
    return &calib_record;
}

/* ============================================================================
 * M5 Heartbeat Subsystem
 * ============================================================================
 *
 * Implements TIME-TICKS and TIME-TRUST:
 *   - TIME-TICKS: Monotonic heartbeat count
 *   - TIME-TRUST: Continuous Q48.16 confidence derived from timing variance
 *
 * On each tick:
 *   1. Sample TSC
 *   2. Compute delta from last tick
 *   3. Record deviation in rolling window
 *   4. Compute variance from window
 *   5. Derive TIME-TRUST from variance
 */

static TimeTrustState g_heartbeat;

/**
 * @brief Push a signed delta value into the heartbeat rolling window.
 *
 * Overwrites the oldest entry at @c w->pos (circular), then advances @c pos
 * modulo @c TIME_WINDOW_SIZE and increments @c count up to the window
 * capacity. Called on every heartbeat tick to record the deviation of the
 * actual inter-tick TSC delta from @c expected_delta.
 *
 * @param w     Heartbeat window to update.
 * @param delta Signed deviation (actual_delta - expected_delta) in TSC ticks.
 */
static void window_push(TimeWindow *w, int64_t delta)
{
    w->deltas[w->pos] = delta;
    w->pos = (w->pos + 1) % TIME_WINDOW_SIZE;
    if (w->count < TIME_WINDOW_SIZE) {
        w->count++;
    }
}

/**
 * @brief Compute the relative variance of heartbeat deltas as a Q48.16 value.
 *
 * Computes the population variance of the signed deviation samples in @p w,
 * then normalises by @c expected_delta² to produce a dimensionless relative
 * variance. The result is represented in Q48.16 fixed-point (value 65536 =
 * 1.0 = 100% relative variance). Returns 0 if fewer than 2 samples are
 * present or @p expected_delta is 0.
 *
 * Overflow guards: large deviations are clamped to ±0x7FFFFFFF ticks before
 * squaring; @c var_tsc is shifted right if it exceeds @c 0x0000FFFFFFFFFFFF.
 *
 * @param w              Rolling window of inter-tick TSC deviations.
 * @param expected_delta Nominal inter-tick TSC delta (tsc_hz / tick_hz).
 * @return Relative variance as Q48.16, or 0 if insufficient data.
 */
static q48_16_t window_variance_q48(const TimeWindow *w, uint64_t expected_delta)
{
    if (w->count < 2 || expected_delta == 0) {
        return 0;
    }

    /* Compute mean */
    int64_t sum = 0;
    for (uint32_t i = 0; i < w->count; i++) {
        sum += w->deltas[i];
    }
    int64_t mean = sum / (int64_t)w->count;

    /* Compute variance = sum((delta - mean)^2) / count */
    uint64_t sum_sq = 0;
    for (uint32_t i = 0; i < w->count; i++) {
        int64_t diff = w->deltas[i] - mean;
        /* Clamp to prevent overflow */
        if (diff > 0x7FFFFFFF) diff = 0x7FFFFFFF;
        if (diff < -0x7FFFFFFF) diff = -0x7FFFFFFF;
        sum_sq += (uint64_t)(diff * diff);
    }
    uint64_t var_tsc = sum_sq / w->count;

    /* Normalize by expected_delta^2 to get relative variance */
    uint64_t exp_sq = expected_delta;
    if (exp_sq > 0xFFFFFFFF) {
        var_tsc >>= 16;
        exp_sq >>= 8;
    }
    exp_sq = exp_sq * exp_sq;
    if (exp_sq == 0) return 0;

    /* Clamp to avoid overflow when shifting */
    if (var_tsc > 0x0000FFFFFFFFFFFFULL) {
        var_tsc = 0x0000FFFFFFFFFFFFULL;
    }

    return (var_tsc << 16) / exp_sq;
}

/**
 * @brief Derive TIME-TRUST from a Q48.16 relative variance value.
 *
 * Applies the formula @c trust = 1 / (1 + variance) in Q48.16 arithmetic:
 * @c denom = Q48_ONE + variance, then @c trust = Q48_ONE / denom. Returns
 * @c Q48_ONE (maximum trust = 1.0) if @p denom would be zero (defensive).
 *
 * As variance → 0, trust → 1.0 (perfect timing). As variance → ∞, trust
 * → 0.0 (unusable). The Q48.16 output is stored in @c TimeTrustState.trust
 * and exported to the VM via @c heartbeat_trust().
 *
 * @param variance Q48.16 relative variance from @c window_variance_q48().
 * @return Q48.16 time-trust value in range (0, 1].
 */
static q48_16_t variance_to_trust(q48_16_t variance)
{
    q48_16_t denom = q48_add(Q48_ONE, variance);
    if (denom == 0) {
        return Q48_ONE;
    }
    return q48_div(Q48_ONE, denom);
}

/**
 * @brief Initialise the M5 heartbeat / TIME-TRUST subsystem.
 *
 * Zeros the @c g_heartbeat state: tick count, TSC anchor, sample counter,
 * variance, and rolling window. Sets the initial trust to @c Q48_ONE (1.0,
 * maximum trust) so the VM starts with a confident time signal.
 *
 * Computes @c expected_delta = @p tsc_hz / @p tick_hz to calibrate the
 * deviation window. Falls back to 10 ms at 1 GHz if either argument is zero.
 *
 * Called from @c kernel_main() after @c timer_init() establishes
 * @c tsc_hz_locked.
 *
 * @param tsc_hz  Locked TSC frequency in Hz (from @c timer_tsc_hz()).
 * @param tick_hz Target heartbeat rate in Hz (typically 100).
 */
void heartbeat_init(uint64_t tsc_hz, uint64_t tick_hz)
{
    g_heartbeat.ticks = 0;
    g_heartbeat.last_tsc = 0;
    g_heartbeat.total_samples = 0;
    g_heartbeat.variance = 0;
    g_heartbeat.trust = Q48_ONE;

    g_heartbeat.window.pos = 0;
    g_heartbeat.window.count = 0;
    for (int i = 0; i < TIME_WINDOW_SIZE; i++) {
        g_heartbeat.window.deltas[i] = 0;
    }

    if (tick_hz > 0 && tsc_hz > 0) {
        g_heartbeat.expected_delta = tsc_hz / tick_hz;
    } else {
        g_heartbeat.expected_delta = 10000000;  /* 10ms at 1GHz */
    }
}

/**
 * @brief Process one heartbeat tick — update tick count, variance, and TIME-TRUST.
 *
 * Called by the APIC timer ISR at @c tick_hz (100 Hz by default). On each call:
 * 1. Reads the current TSC via @c rdtsc().
 * 2. Increments @c ticks and @c total_samples.
 * 3. On the first sample, records @c last_tsc and returns (no delta yet).
 * 4. Computes the signed deviation of the actual inter-tick TSC delta from
 *    @c expected_delta and pushes it into the rolling window.
 * 5. Recomputes @c variance and @c trust from the updated window.
 *
 * Must only be called from interrupt context. Does not take a lock; the
 * single-threaded kernel guarantees no concurrent access.
 */
void heartbeat_tick(void)
{
    uint64_t now = rdtsc();
    TimeTrustState *s = &g_heartbeat;

    s->ticks++;
    s->total_samples++;

    /* First tick: just record TSC */
    if (s->total_samples == 1) {
        s->last_tsc = now;
        return;
    }

    /* Compute delta from last tick */
    uint64_t actual_delta = now - s->last_tsc;
    s->last_tsc = now;

    /* Compute deviation from expected (signed) */
    int64_t deviation = (int64_t)actual_delta - (int64_t)s->expected_delta;

    /* Add to rolling window */
    window_push(&s->window, deviation);

    /* Recompute variance and trust */
    s->variance = window_variance_q48(&s->window, s->expected_delta);
    s->trust = variance_to_trust(s->variance);
}

/**
 * @brief Return the total number of heartbeat ticks since @c heartbeat_init().
 *
 * The tick count monotonically increases with each @c heartbeat_tick() call.
 * Used by the HAL shim (@c sk_hal_heartbeat_ticks()) and the VM's @c M5
 * time-trust bridge.
 *
 * @return Number of heartbeat ticks elapsed.
 */
uint64_t heartbeat_ticks(void)
{
    return g_heartbeat.ticks;
}

/**
 * @brief Return the current TIME-TRUST value as Q48.16 fixed-point.
 *
 * Returns @c g_heartbeat.trust, updated on every tick by @c variance_to_trust().
 * Value is in range (0, 1]: @c Q48_ONE (65536) = perfect trust, values near
 * zero indicate high inter-tick jitter.
 *
 * @return Current time-trust as a @c time_trust_t (Q48.16 alias).
 */
time_trust_t heartbeat_trust(void)
{
    return g_heartbeat.trust;
}

/**
 * @brief Return a pointer to the full heartbeat / TIME-TRUST state.
 *
 * Provides read-only access to @c g_heartbeat for diagnostic output or
 * parity logging. The caller must not write through the returned pointer.
 *
 * @return Pointer to the static @c TimeTrustState (never NULL).
 */
const TimeTrustState *heartbeat_state(void)
{
    return &g_heartbeat;
}
