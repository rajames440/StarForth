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

static inline uint64_t rdtsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t rdtscp_aux(uint32_t *aux)
{
    uint32_t lo, hi, a;
    __asm__ volatile ("rdtscp" : "=a"(lo), "=d"(hi), "=c"(a) ::);
    if (aux) *aux = a;
    return ((uint64_t)hi << 32) | lo;
}

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

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint32_t inl(uint16_t port)
{
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint64_t hpet_read(uint32_t offset)
{
    return hpet_regs[offset / 8];
}

static inline void hpet_write(uint32_t offset, uint64_t val)
{
    hpet_regs[offset / 8] = val;
}

static void timer_fatal(const char *msg)
{
    console_println(msg);
    console_println("Timer subsystem fatal. Halting.");
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/* ------------ CPU capability / environment detection ------------ */

static int cpu_has_invariant_tsc(void)
{
    /* CPUID.80000007H:EDX[8] = Invariant TSC */
    uint32_t a, b, c, d;
    cpuid(0x80000007u, 0, &a, &b, &c, &d);
    return (d & (1u << 8)) ? 1 : 0;
}

static int cpu_has_rdtscp(void)
{
    /* CPUID.80000001H:EDX[27] = RDTSCP */
    uint32_t a, b, c, d;
    cpuid(0x80000001u, 0, &a, &b, &c, &d);
    return (d & (1u << 27)) ? 1 : 0;
}

static int running_under_hypervisor(void)
{
    /* CPUID.1:ECX[31] = Hypervisor present */
    uint32_t a, b, c, d;
    cpuid(1u, 0, &a, &b, &c, &d);
    return (c & (1u << 31)) ? 1 : 0;
}

/* ------------ PM Timer (VM-friendly reference) ------------ */

static inline uint32_t pmtimer_read(void)
{
    return inl(PMTIMER_IO_PORT);
}

static uint64_t pmtimer_delta(uint32_t start, uint32_t cur)
{
    uint32_t s = start & (uint32_t)PMTIMER_MASK;
    uint32_t c = cur   & (uint32_t)PMTIMER_MASK;

    if (c >= s) return (uint64_t)(c - s);
    return (uint64_t)((PMTIMER_MASK - s) + 1u + c);
}

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

            uint64_t elapsed_ns = (uint64_t)(((__int128)elapsed_ticks * 1000000000ull) / PMTIMER_FREQ_HZ);
            if (elapsed_ns == 0) return 0;

            uint64_t delta_tsc = end_tsc - start_tsc;
            return (uint64_t)(((__int128)delta_tsc * 1000000000ull) / elapsed_ns);
        }

        if (++iters > timeout_iter) {
            return 0;
        }
    }
}

static void vm_timebase_init_from_pmtimer(void)
{
    vm_pm_start = pmtimer_read() & (uint32_t)PMTIMER_MASK;
    vm_ns_base  = 0;
}

/* ------------ HPET helpers (bare metal path) ------------ */

static void enable_hpet(void)
{
    uint64_t cfg = hpet_read(HPET_GEN_CONFIG);
    cfg |= 1ull;
    hpet_write(HPET_GEN_CONFIG, cfg);
}

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

static void pit_program_reload(uint16_t reload)
{
    outb(PIT_COMMAND, 0x34);
    outb(PIT_CHANNEL0_DATA, (uint8_t)(reload & 0xFF));
    outb(PIT_CHANNEL0_DATA, (uint8_t)((reload >> 8) & 0xFF));
}

static uint16_t pit_read_counter16(void)
{
    outb(PIT_COMMAND, 0x00);
    uint8_t lo = inb(PIT_CHANNEL0_DATA);
    uint8_t hi = inb(PIT_CHANNEL0_DATA);
    return (uint16_t)((hi << 8) | lo);
}

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

static void update_stats(const uint64_t *samples, uint32_t count, uint64_t *mean, uint64_t *stddev)
{
    if (count == 0) {
        *mean = 0;
        *stddev = 0;
        return;
    }

    __int128 sum = 0;
    for (uint32_t i = 0; i < count; ++i) sum += samples[i];
    uint64_t m = (uint64_t)(sum / count);

    __int128 var_acc = 0;
    for (uint32_t i = 0; i < count; ++i) {
        __int128 d = (__int128)samples[i] - m;
        var_acc += d * d;
    }

    uint64_t variance = (uint64_t)(var_acc / count);
    *mean = m;

    if (variance == 0) {
        *stddev = 0;
        return;
    }

    uint64_t x = variance;
    uint64_t r = x;
    for (int i = 0; i < 10; ++i) {
        r = (r + x / r) / 2;
    }
    *stddev = r;
}

static uint64_t calibrate_window_hpet(uint64_t target_ticks)
{
    uint64_t start_cnt = hpet_read(HPET_MAIN_COUNTER);
    uint64_t start_tsc = rdtsc();

    while ((hpet_read(HPET_MAIN_COUNTER) - start_cnt) < target_ticks) {
        /* tight loop; bare metal only */
    }

    uint64_t end_tsc = rdtsc();
    uint64_t elapsed_ticks = hpet_read(HPET_MAIN_COUNTER) - start_cnt;

    uint64_t elapsed_ns = (uint64_t)(((__int128)elapsed_ticks * 1000000000ull) / hpet_freq_hz);
    if (elapsed_ns == 0) return 0;

    uint64_t delta_tsc = end_tsc - start_tsc;
    return (uint64_t)(((__int128)delta_tsc * 1000000000ull) / elapsed_ns);
}

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
            uint64_t elapsed_ns = (uint64_t)(((__int128)delta * 1000000000ull) / PIT_FREQ_HZ);
            if (elapsed_ns == 0) return 0;
            uint64_t delta_tsc = end_tsc - start_tsc;
            return (uint64_t)(((__int128)delta_tsc * 1000000000ull) / elapsed_ns);
        }
    }
    return 0;
}

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
        uint64_t cv_h_ppm = (mean_h == 0) ? UINT64_MAX : (uint64_t)((__int128)std_h * 1000000ull / mean_h);

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

        uint64_t cv_p_ppm = (mean_p == 0) ? UINT64_MAX : (uint64_t)((__int128)std_p * 1000000ull / mean_p);

        uint64_t min_hz = (mean_h < mean_p) ? mean_h : mean_p;
        uint64_t max_hz = (mean_h > mean_p) ? mean_h : mean_p;
        uint64_t diff_ppm = (min_hz == 0) ? UINT64_MAX : (uint64_t)((__int128)(max_hz - min_hz) * 1000000ull / min_hz);

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

static uint64_t derive_tsc_hz_from_cpuid(void)
{
    uint32_t a, b, c, d;

    /* CPUID.15H: TSC/Crystal ratio */
    cpuid(0x15u, 0, &a, &b, &c, &d);
    if (a != 0 && b != 0 && c != 0) {
        __int128 num = (__int128)c * (__int128)b;
        uint64_t hz = (uint64_t)(num / a);
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

uint64_t timer_tsc_hz(void)
{
    return tsc_hz_locked;
}

static uint64_t timer_now_ns_vm_relative(void)
{
    /* Monotonic-ish RELATIVE ns based on ACPI PM Timer (port I/O). */
    uint32_t cur = pmtimer_read() & (uint32_t)PMTIMER_MASK;
    uint64_t ticks = pmtimer_delta(vm_pm_start, cur);

    uint64_t ns = (uint64_t)(((__int128)ticks * 1000000000ull) / PMTIMER_FREQ_HZ);
    return vm_ns_base + ns;
}

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

    return (uint64_t)(((__int128)t * 1000000000ull) / tsc_hz_locked);
}

static int timer_drift_check_bare_metal(void)
{
    uint64_t current = calibrate_window_hpet(WINDOW_TICKS_HPET);
    if (current == 0 || tsc_hz_locked == 0) return -1;

    uint64_t min_hz = (current < tsc_hz_locked) ? current : tsc_hz_locked;
    uint64_t max_hz = (current > tsc_hz_locked) ? current : tsc_hz_locked;

    uint64_t diff_ppm = (min_hz == 0) ? UINT64_MAX
        : (uint64_t)((__int128)(max_hz - min_hz) * 1000000ull / min_hz);

    if (diff_ppm > DRIFT_EPSILON_PPM) {
        timer_fatal("Timer drift exceeded runtime bound.");
    }
    return 0;
}

int timer_check_drift_now(void)
{
    if (calib_record.vm_mode) {
        /* VM policy: drift checks require a VM-friendly reference (e.g., pvclock) */
        return 0;
    }
    return timer_drift_check_bare_metal();
}

const timer_calibration_record_t *timer_calibration_record(void)
{
    return &calib_record;
}
