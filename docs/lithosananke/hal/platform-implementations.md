# HAL Platform Implementation Guide

## Overview

This document explains how to implement the HAL for a new platform. It provides detailed examples for Linux, L4Re, and LithosAnanke (freestanding) implementations.

**Target audience:** Platform developers adding HAL support for a new target.

---

## Platform Directory Structure

```
src/platform/<platform>/
├── hal_time.c              # Time & timer HAL implementation
├── hal_interrupt.c         # Interrupt HAL implementation
├── hal_memory.c            # Memory HAL implementation
├── hal_console.c           # Console HAL implementation
├── hal_cpu.c               # CPU HAL implementation
├── hal_panic.c             # Panic/error HAL implementation
└── platform_init.c         # Platform-specific initialization (optional)
```

### Build Integration

Each platform is compiled independently via Makefile:

```makefile
# Platform selection
PLATFORM ?= linux    # linux | l4re | kernel

# Platform-specific source files
PLATFORM_SOURCES = \
    src/platform/$(PLATFORM)/hal_time.c \
    src/platform/$(PLATFORM)/hal_interrupt.c \
    src/platform/$(PLATFORM)/hal_memory.c \
    src/platform/$(PLATFORM)/hal_console.c \
    src/platform/$(PLATFORM)/hal_cpu.c \
    src/platform/$(PLATFORM)/hal_panic.c

# Platform-specific flags
ifeq ($(PLATFORM),kernel)
    CFLAGS += -ffreestanding -nostdlib -mno-red-zone
    LDFLAGS += -nostdlib -static
endif
```

---

## Implementation Examples

### 1. Linux Platform (`src/platform/linux/`)

Linux is the **reference platform** for development and testing. It provides the richest debugging environment.

#### `hal_time.c` (Linux)

```c
#include "hal/hal_time.h"
#include <time.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

/* Timer state */
typedef struct {
    timer_t timer_id;
    hal_timer_callback_t callback;
    void *ctx;
} linux_timer_t;

static linux_timer_t *periodic_timers = NULL;
static int timer_count = 0;

void hal_time_init(void) {
    /* Linux: clock_gettime is always available */
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        hal_panic("hal_time_init: CLOCK_MONOTONIC unavailable");
    }
}

uint64_t hal_time_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void hal_time_delay_ns(uint64_t ns) {
    struct timespec req;
    req.tv_sec = ns / 1000000000ULL;
    req.tv_nsec = ns % 1000000000ULL;
    nanosleep(&req, NULL);
}

/* Signal handler for periodic timer */
static void timer_signal_handler(int signo, siginfo_t *si, void *uc) {
    (void)signo;
    (void)uc;

    linux_timer_t *timer = (linux_timer_t *)si->si_value.sival_ptr;
    if (timer && timer->callback) {
        timer->callback(timer->ctx);
    }
}

int hal_timer_periodic(uint64_t period_ns, hal_timer_callback_t callback, void *ctx) {
    /* Allocate timer structure */
    linux_timer_t *timer = malloc(sizeof(linux_timer_t));
    if (!timer) return -1;

    timer->callback = callback;
    timer->ctx = ctx;

    /* Set up signal handler */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timer_signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGRTMIN, &sa, NULL);

    /* Create POSIX timer */
    struct sigevent sev;
    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;
    sev.sigev_value.sival_ptr = timer;

    if (timer_create(CLOCK_MONOTONIC, &sev, &timer->timer_id) != 0) {
        free(timer);
        return -1;
    }

    /* Arm the timer */
    struct itimerspec its;
    its.it_value.tv_sec = period_ns / 1000000000ULL;
    its.it_value.tv_nsec = period_ns % 1000000000ULL;
    its.it_interval = its.it_value;  /* Periodic */

    if (timer_settime(timer->timer_id, 0, &its, NULL) != 0) {
        timer_delete(timer->timer_id);
        free(timer);
        return -1;
    }

    return timer_count++;  /* Return timer ID */
}

int hal_timer_cancel(int timer_id) {
    /* TODO: Implement timer cancellation */
    (void)timer_id;
    return 0;
}

uint64_t hal_time_frequency_hz(void) {
    /* Linux: CLOCK_MONOTONIC is variable frequency, return 0 */
    return 0;
}
```

#### `hal_interrupt.c` (Linux)

```c
#include "hal/hal_interrupt.h"
#include <signal.h>

/* Linux uses signals as "interrupts" */
static __thread int in_signal_handler = 0;

void hal_interrupt_init(void) {
    /* No initialization needed on Linux */
}

void hal_irq_enable(void) {
    /* Linux: interrupts (signals) always enabled */
}

unsigned long hal_irq_disable(void) {
    /* Linux: can't disable signals, return dummy state */
    return 0;
}

void hal_irq_restore(unsigned long state) {
    (void)state;
}

int hal_irq_register(unsigned int irq, hal_isr_t isr, void *ctx) {
    /* Linux: IRQs are handled via signal mechanism in hal_timer_periodic() */
    (void)irq;
    (void)isr;
    (void)ctx;
    return 0;
}

int hal_irq_unregister(unsigned int irq) {
    (void)irq;
    return 0;
}

int hal_in_interrupt_context(void) {
    return in_signal_handler;
}
```

#### `hal_memory.c` (Linux)

```c
#include "hal/hal_memory.h"
#include <stdlib.h>
#include <string.h>

void hal_mem_init(void) {
    /* malloc is always available on Linux */
}

void *hal_mem_alloc(size_t size) {
    if (size == 0) return NULL;
    void *ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);  /* Zero-initialize */
    }
    return ptr;
}

void hal_mem_free(void *ptr) {
    free(ptr);
}

uint64_t hal_mem_alloc_pages(size_t count) {
    /* Linux: just allocate from heap (no separate PMM) */
    size_t size = count * hal_mem_page_size();
    return (uint64_t)hal_mem_alloc(size);
}

void hal_mem_free_pages(uint64_t paddr, size_t count) {
    (void)count;
    hal_mem_free((void *)paddr);
}

int hal_mem_map(uint64_t vaddr, uint64_t paddr, size_t size, unsigned int flags) {
    /* Linux: identity mapping, no-op */
    (void)vaddr;
    (void)paddr;
    (void)size;
    (void)flags;
    return 0;
}

size_t hal_mem_page_size(void) {
    return 4096;  /* Standard page size */
}
```

#### `hal_console.c` (Linux)

```c
#include "hal/hal_console.h"
#include <stdio.h>
#include <unistd.h>
#include <poll.h>

void hal_console_init(void) {
    /* stdin/stdout always available */
}

void hal_console_putc(char c) {
    putchar(c);
    fflush(stdout);  /* Ensure character is visible */
}

void hal_console_puts(const char *s) {
    if (s) {
        fputs(s, stdout);
        fflush(stdout);
    }
}

int hal_console_getc(void) {
    return getchar();
}

int hal_console_has_input(void) {
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    return poll(&pfd, 1, 0) > 0;
}
```

#### `hal_cpu.c` (Linux)

```c
#include "hal/hal_cpu.h"
#include <unistd.h>
#include <sched.h>

void hal_cpu_init(void) {
    /* No initialization needed */
}

unsigned int hal_cpu_id(void) {
    /* Linux: return thread ID (not true CPU ID) */
    return 0;  /* Single-threaded for now */
}

void hal_cpu_relax(void) {
    sched_yield();
}

void hal_cpu_halt(void) {
    pause();  /* Wait for signal */
}

unsigned int hal_cpu_count(void) {
    return sysconf(_SC_NPROCESSORS_ONLN);
}
```

#### `hal_panic.c` (Linux)

```c
#include "hal/hal_panic.h"
#include <stdio.h>
#include <stdlib.h>

void hal_panic(const char *msg) {
    fprintf(stderr, "PANIC: %s\n", msg ? msg : "unknown error");
    fflush(stderr);
    abort();
}
```

---

### 2. Kernel Platform (`src/platform/kernel/`)

The kernel platform is **freestanding**—no libc, no OS, direct hardware access.

#### `hal_time.c` (Kernel)

```c
#include "hal/hal_time.h"
#include "hal/hal_interrupt.h"
#include "kernel/apic.h"
#include "kernel/hpet.h"
#include "kernel/tsc.h"

/* TSC calibration state */
static uint64_t tsc_frequency_hz = 0;
static uint64_t tsc_offset_ns = 0;

void hal_time_init(void) {
    /* Calibrate TSC using HPET */
    tsc_frequency_hz = tsc_calibrate_hpet();

    if (tsc_frequency_hz == 0) {
        hal_panic("hal_time_init: TSC calibration failed");
    }

    /* Validate monotonicity */
    uint64_t t1 = hal_time_now_ns();
    for (volatile int i = 0; i < 1000; i++);
    uint64_t t2 = hal_time_now_ns();

    if (t2 < t1) {
        hal_panic("hal_time_init: TSC not monotonic");
    }
}

uint64_t hal_time_now_ns(void) {
    /* Read TSC and convert to nanoseconds */
    uint64_t tsc = rdtsc();
    return tsc_to_ns(tsc) - tsc_offset_ns;
}

void hal_time_delay_ns(uint64_t ns) {
    uint64_t end = hal_time_now_ns() + ns;
    while (hal_time_now_ns() < end) {
        hal_cpu_relax();
    }
}

/* APIC timer state */
static hal_timer_callback_t apic_timer_callback = NULL;
static void *apic_timer_ctx = NULL;

/* APIC timer ISR (called from IDT) */
void apic_timer_isr(void) {
    if (apic_timer_callback) {
        apic_timer_callback(apic_timer_ctx);
    }
    apic_eoi();  /* Send End-Of-Interrupt */
}

int hal_timer_periodic(uint64_t period_ns, hal_timer_callback_t callback, void *ctx) {
    if (period_ns < 1000) return -2;  /* Minimum 1µs */

    apic_timer_callback = callback;
    apic_timer_ctx = ctx;

    /* Configure APIC timer */
    uint64_t ticks = (period_ns * apic_timer_frequency_hz()) / 1000000000ULL;
    apic_timer_periodic(ticks);

    return 1;  /* Timer ID */
}

int hal_timer_cancel(int timer_id) {
    (void)timer_id;
    apic_timer_stop();
    apic_timer_callback = NULL;
    return 0;
}

uint64_t hal_time_frequency_hz(void) {
    return tsc_frequency_hz;
}
```

#### `hal_interrupt.c` (Kernel)

```c
#include "hal/hal_interrupt.h"
#include "kernel/idt.h"
#include "kernel/apic.h"

/* Per-CPU interrupt nesting count */
static __thread unsigned int interrupt_depth = 0;

void hal_interrupt_init(void) {
    /* Initialize IDT */
    idt_init();

    /* Initialize Local APIC */
    apic_init();

    /* Disable legacy PIC (use APIC instead) */
    pic_disable();
}

void hal_irq_enable(void) {
    __asm__ volatile("sti");
}

unsigned long hal_irq_disable(void) {
    unsigned long flags;
    __asm__ volatile(
        "pushfq\n"
        "popq %0\n"
        "cli\n"
        : "=r"(flags)
    );
    return flags;
}

void hal_irq_restore(unsigned long state) {
    __asm__ volatile(
        "pushq %0\n"
        "popfq\n"
        :
        : "r"(state)
    );
}

/* ISR table (256 entries for x86_64 IDT) */
static hal_isr_t isr_table[256] = {0};
static void *isr_ctx_table[256] = {0};

/* Common ISR entry point (called from IDT stubs) */
void common_isr_handler(unsigned int vector) {
    interrupt_depth++;

    if (isr_table[vector]) {
        isr_table[vector](vector, isr_ctx_table[vector]);
    }

    interrupt_depth--;
    apic_eoi();
}

int hal_irq_register(unsigned int irq, hal_isr_t isr, void *ctx) {
    if (irq >= 256) return -1;

    isr_table[irq] = isr;
    isr_ctx_table[irq] = ctx;

    /* Configure IOAPIC to route IRQ to Local APIC */
    ioapic_route_irq(irq, 0);  /* Route to CPU 0 */

    return 0;
}

int hal_irq_unregister(unsigned int irq) {
    if (irq >= 256) return -1;

    isr_table[irq] = NULL;
    isr_ctx_table[irq] = NULL;

    return 0;
}

int hal_in_interrupt_context(void) {
    return interrupt_depth > 0;
}
```

#### `hal_memory.c` (Kernel)

```c
#include "hal/hal_memory.h"
#include "kernel/pmm.h"    /* Physical Memory Manager */
#include "kernel/vmm.h"    /* Virtual Memory Manager */
#include "kernel/kmalloc.h" /* Kernel heap allocator */

void hal_mem_init(void) {
    /* Initialize physical memory manager (parse UEFI memory map) */
    pmm_init();

    /* Initialize virtual memory manager (set up page tables) */
    vmm_init();

    /* Initialize kernel heap allocator */
    kmalloc_init();
}

void *hal_mem_alloc(size_t size) {
    if (size == 0) return NULL;
    return kmalloc(size);  /* Kernel heap allocator */
}

void hal_mem_free(void *ptr) {
    kfree(ptr);
}

uint64_t hal_mem_alloc_pages(size_t count) {
    return pmm_alloc_pages(count);
}

void hal_mem_free_pages(uint64_t paddr, size_t count) {
    pmm_free_pages(paddr, count);
}

int hal_mem_map(uint64_t vaddr, uint64_t paddr, size_t size, unsigned int flags) {
    /* Translate HAL flags to page table flags */
    unsigned int pt_flags = 0;
    if (flags & HAL_MEM_WRITE) pt_flags |= PAGE_WRITE;
    if (flags & HAL_MEM_EXEC)  pt_flags |= PAGE_EXEC;
    if (flags & HAL_MEM_NOCACHE) pt_flags |= PAGE_NOCACHE;

    return vmm_map(vaddr, paddr, size, pt_flags);
}

size_t hal_mem_page_size(void) {
    return 4096;
}
```

#### `hal_console.c` (Kernel)

```c
#include "hal/hal_console.h"
#include "kernel/uart.h"
#include "kernel/framebuffer.h"

void hal_console_init(void) {
    /* Initialize UART 16550 (serial console) */
    uart_init(0x3F8, 115200);  /* COM1, 115200 baud */

    /* Initialize framebuffer (if available from UEFI GOP) */
    fb_init();
}

void hal_console_putc(char c) {
    /* Output to both serial and framebuffer */
    uart_putc(c);
    fb_putc(c);
}

void hal_console_puts(const char *s) {
    if (!s) return;
    while (*s) {
        hal_console_putc(*s++);
    }
}

int hal_console_getc(void) {
    /* Read from UART (blocking) */
    return uart_getc();
}

int hal_console_has_input(void) {
    return uart_has_data();
}
```

#### `hal_cpu.c` (Kernel)

```c
#include "hal/hal_cpu.h"
#include "kernel/apic.h"

static unsigned int cpu_count = 1;

void hal_cpu_init(void) {
    /* Detect CPU features */
    cpu_detect_features();

    /* Bring up secondary CPUs (SMP) */
    cpu_count = smp_init();
}

unsigned int hal_cpu_id(void) {
    /* Get Local APIC ID */
    return apic_get_id();
}

void hal_cpu_relax(void) {
    __asm__ volatile("pause");
}

void hal_cpu_halt(void) {
    __asm__ volatile("hlt");
}

unsigned int hal_cpu_count(void) {
    return cpu_count;
}
```

---

## Platform Testing Strategy

### 1. **Unit Tests (Per-Platform)**

Each platform should have unit tests for HAL functions:

```c
/* test/platform/linux/hal_time_test.c */
void test_hal_time_monotonic(void) {
    hal_time_init();

    uint64_t t1 = hal_time_now_ns();
    hal_time_delay_ns(1000000);  /* 1ms */
    uint64_t t2 = hal_time_now_ns();

    assert(t2 > t1);
    assert((t2 - t1) >= 1000000);
    assert((t2 - t1) < 2000000);  /* Allow 100% jitter for test */
}
```

### 2. **Cross-Platform VM Tests**

VM tests should run identically on all platforms:

```bash
# Linux
make PLATFORM=linux test

# L4Re
make PLATFORM=l4re test

# Kernel (QEMU)
make PLATFORM=kernel test-qemu
```

**Success criteria:** All 936+ tests pass on all platforms.

### 3. **Determinism Validation**

Physics subsystems must show 0% algorithmic variance:

```bash
# Run DoE on each platform
for platform in linux l4re kernel; do
    make PLATFORM=$platform fastest
    ./build/$ARCH/fastest/starforth --doe > results_$platform.csv
done

# Compare results (should be identical)
diff results_linux.csv results_kernel.csv
```

---

## Common Implementation Pitfalls

### 1. **Non-Monotonic Time**

**Symptom:** Physics metrics show negative deltas, rolling window corrupted.

**Cause:** TSC goes backward (multi-core, frequency scaling).

**Fix:** Use `rdtscp` instead of `rdtsc`, synchronize TSC across cores, or fall back to HPET.

### 2. **Timer Jitter**

**Symptom:** Heartbeat intervals vary by 10-50%, metrics show high variance.

**Cause:** Signal latency (Linux), interrupt latency (kernel).

**Fix:** Minimize ISR work, use high-priority timer interrupt, disable frequency scaling.

### 3. **Memory Leaks**

**Symptom:** `hal_mem_alloc()` calls grow without matching `hal_mem_free()`.

**Cause:** Missing free in error paths, lost pointers.

**Fix:** Valgrind on Linux, manual leak tracking on kernel.

### 4. **Interrupt Context Violations**

**Symptom:** Crashes, hangs, or corruption when calling blocking HAL functions from ISR.

**Cause:** Calling `hal_mem_alloc()` or `hal_console_getc()` from timer ISR.

**Fix:** Assert `!hal_in_interrupt_context()` in blocking functions.

---

## Next Steps

- See `migration-plan.md` for refactoring existing code
- See `lithosananke-integration.md` for kernel-specific details