# HAL Interface Specifications

## Overview

This document defines the **contract** for each HAL subsystem. These interfaces are platform-agnostic and must be implemented identically (semantically) on all platforms.

**Critical:** These are not optional "convenience wrappers"—they are the **only** way VM and physics code access platform resources.

---

## 1. Time & Timers (`include/hal/hal_time.h`)

### Purpose
Provide monotonic time, periodic/oneshot timers, and calibration for physics subsystems.

### Interface

```c
#ifndef HAL_TIME_H
#define HAL_TIME_H

#include <stdint.h>

/**
 * Initialize time subsystem.
 * Must be called before any other hal_time_* functions.
 *
 * Responsibilities:
 * - Calibrate high-resolution timer (TSC, HPET, etc.)
 * - Validate monotonicity
 * - Set up timer interrupt sources (if needed)
 *
 * Error handling:
 * - hal_panic() if no suitable time source available
 * - hal_panic() if calibration fails
 */
void hal_time_init(void);

/**
 * Get current monotonic time in nanoseconds.
 *
 * Guarantees:
 * - MUST be monotonic (never goes backward)
 * - SHOULD have sub-microsecond resolution
 * - MUST be callable from interrupt context (ISR-safe)
 * - MUST be callable concurrently (thread-safe)
 *
 * Performance:
 * - Target: < 50 cycles on x86_64
 * - Acceptable: < 200 cycles
 *
 * Returns:
 * - Nanoseconds since arbitrary epoch (typically boot time)
 */
uint64_t hal_time_now_ns(void);

/**
 * Busy-wait delay for specified nanoseconds.
 * Used for calibration and short delays.
 *
 * Guarantees:
 * - MUST delay for at least `ns` nanoseconds
 * - MAY delay longer (but not by more than 10%)
 * - MUST be callable from interrupt context
 *
 * Parameters:
 * - ns: Nanoseconds to delay (0 = no delay)
 *
 * Notes:
 * - This is a busy-wait, not a sleep
 * - For delays > 1ms, prefer hal_timer_oneshot()
 */
void hal_time_delay_ns(uint64_t ns);

/**
 * Timer callback function type.
 *
 * Parameters:
 * - ctx: User-provided context pointer
 *
 * Context:
 * - MAY be called from interrupt context (ISR)
 * - MUST NOT block or sleep
 * - MUST complete quickly (< 10µs target)
 */
typedef void (*hal_timer_callback_t)(void *ctx);

/**
 * Schedule a oneshot timer to fire after specified nanoseconds.
 *
 * Guarantees:
 * - Callback will fire at least `ns` nanoseconds in the future
 * - MAY fire up to 10% late (jitter allowed)
 * - Callback runs in interrupt context (ISR)
 *
 * Parameters:
 * - ns: Nanoseconds until callback fires (0 = ASAP)
 * - callback: Function to call when timer fires
 * - ctx: User context passed to callback
 *
 * Error handling:
 * - Returns 0 on success
 * - Returns -1 if timer resources exhausted
 *
 * Concurrency:
 * - Thread-safe (can be called from multiple threads)
 * - NOT reentrant from same timer callback
 */
int hal_timer_oneshot(uint64_t ns, hal_timer_callback_t callback, void *ctx);

/**
 * Schedule a periodic timer to fire every `period_ns` nanoseconds.
 *
 * Guarantees:
 * - Callback will fire repeatedly at ~period_ns intervals
 * - Jitter: < 10% of period (target: < 1%)
 * - Callback runs in interrupt context (ISR)
 * - Timer continues until hal_timer_cancel() called
 *
 * Parameters:
 * - period_ns: Period in nanoseconds (minimum: 1000ns = 1µs)
 * - callback: Function to call on each period
 * - ctx: User context passed to callback
 *
 * Returns:
 * - Timer ID (> 0) on success
 * - -1 if timer resources exhausted
 * - -2 if period_ns < minimum
 *
 * Notes:
 * - This is the PRIMARY mechanism for heartbeat ISR
 * - Platform MUST minimize jitter for experimental validity
 */
int hal_timer_periodic(uint64_t period_ns, hal_timer_callback_t callback, void *ctx);

/**
 * Cancel a periodic timer.
 *
 * Parameters:
 * - timer_id: ID returned from hal_timer_periodic()
 *
 * Guarantees:
 * - Callback will not fire after this call returns
 * - Safe to call even if timer already fired or was cancelled
 *
 * Returns:
 * - 0 on success
 * - -1 if timer_id invalid
 */
int hal_timer_cancel(int timer_id);

/**
 * Get timer frequency in Hz.
 * Used for diagnostics and experiment metadata.
 *
 * Returns:
 * - Frequency of underlying time source (e.g., TSC frequency)
 * - 0 if frequency is variable or unknown
 */
uint64_t hal_time_frequency_hz(void);

#endif /* HAL_TIME_H */
```

### Platform Implementation Notes

**Linux (`platform/linux/hal_time.c`):**
- `hal_time_now_ns()` → `clock_gettime(CLOCK_MONOTONIC)`
- `hal_timer_periodic()` → `timer_create()` with `SIGRTMIN` + signal handler
- Challenge: Signal-based timers have ~1-10µs latency jitter

**L4Re (`platform/l4re/hal_time.c`):**
- `hal_time_now_ns()` → `l4re_kip_clock()`
- `hal_timer_periodic()` → L4Re IPC-based timer + IRQ object
- Challenge: IPC overhead, but lower jitter than Linux

**Kernel (`platform/kernel/hal_time.c`):**
- `hal_time_now_ns()` → `rdtsc()` calibrated against HPET
- `hal_timer_periodic()` → APIC timer interrupt
- Challenge: TSC drift on old hardware, calibration complexity

---

## 2. Interrupts (`include/hal/hal_interrupt.h`)

### Purpose
Enable/disable interrupts, register ISRs, query interrupt context.

### Interface

```c
#ifndef HAL_INTERRUPT_H
#define HAL_INTERRUPT_H

#include <stdint.h>

/**
 * Initialize interrupt subsystem.
 * Must be called before any other hal_interrupt_* functions.
 *
 * Responsibilities:
 * - Set up interrupt controller (PIC/APIC on x86_64)
 * - Initialize IDT (on kernel platforms)
 * - Prepare for ISR registration
 *
 * Error handling:
 * - hal_panic() if interrupt controller unavailable
 */
void hal_interrupt_init(void);

/**
 * Enable interrupts globally.
 *
 * Guarantees:
 * - Interrupts will be delivered after this call
 * - Safe to call multiple times (idempotent)
 * - NOT callable from interrupt context
 *
 * Notes:
 * - On x86_64 kernel: sti
 * - On Linux: no-op (interrupts always enabled)
 */
void hal_irq_enable(void);

/**
 * Disable interrupts globally.
 *
 * Guarantees:
 * - Interrupts will NOT be delivered after this call
 * - Safe to call multiple times (idempotent)
 * - Callable from interrupt context
 *
 * Returns:
 * - Previous interrupt state (for hal_irq_restore)
 *
 * Notes:
 * - On x86_64 kernel: cli
 * - On Linux: no-op (interrupts always enabled)
 */
unsigned long hal_irq_disable(void);

/**
 * Restore previous interrupt state.
 *
 * Parameters:
 * - state: Value returned from hal_irq_disable()
 *
 * Use case:
 *   unsigned long flags = hal_irq_disable();
 *   // critical section
 *   hal_irq_restore(flags);
 */
void hal_irq_restore(unsigned long state);

/**
 * Interrupt service routine function type.
 *
 * Parameters:
 * - irq: IRQ number that triggered
 * - ctx: User-provided context pointer
 *
 * Context:
 * - ALWAYS called from interrupt context
 * - MUST NOT block or sleep
 * - MUST complete quickly (< 10µs target, < 100µs acceptable)
 * - MUST NOT call non-ISR-safe HAL functions
 */
typedef void (*hal_isr_t)(unsigned int irq, void *ctx);

/**
 * Register an interrupt service routine.
 *
 * Parameters:
 * - irq: IRQ number to register (platform-specific numbering)
 * - isr: Function to call when IRQ fires
 * - ctx: User context passed to ISR
 *
 * Guarantees:
 * - ISR will be called when IRQ fires
 * - Only one ISR per IRQ (replaces previous if registered)
 * - ISR runs with interrupts disabled (not reentrant)
 *
 * Returns:
 * - 0 on success
 * - -1 if IRQ number invalid
 * - -2 if ISR registration failed
 *
 * Notes:
 * - Used by hal_timer_periodic() internally
 * - Used by device drivers for hardware IRQs
 */
int hal_irq_register(unsigned int irq, hal_isr_t isr, void *ctx);

/**
 * Unregister an interrupt service routine.
 *
 * Parameters:
 * - irq: IRQ number to unregister
 *
 * Returns:
 * - 0 on success
 * - -1 if IRQ was not registered
 */
int hal_irq_unregister(unsigned int irq);

/**
 * Check if currently executing in interrupt context.
 *
 * Returns:
 * - 1 if in interrupt context (ISR)
 * - 0 if in normal execution context
 *
 * Use case:
 * - Validate ISR-only functions (e.g., heartbeat sampling)
 * - Conditional behavior (ISR vs. normal path)
 *
 * Performance:
 * - MUST be fast (< 10 cycles)
 * - Often implemented as checking a per-CPU flag
 */
int hal_in_interrupt_context(void);

#endif /* HAL_INTERRUPT_H */
```

### Platform Implementation Notes

**Linux:**
- `hal_irq_enable/disable()` → no-op (can't disable signals)
- `hal_irq_register()` → `sigaction()` + signal handler
- `hal_in_interrupt_context()` → thread-local flag set by signal handler

**Kernel:**
- `hal_irq_enable/disable()` → `sti`/`cli` (x86_64)
- `hal_irq_register()` → IDT entry + APIC configuration
- `hal_in_interrupt_context()` → check per-CPU interrupt nesting count

---

## 3. Memory (`include/hal/hal_memory.h`)

### Purpose
Allocate/free memory, map pages, manage heap.

### Interface

```c
#ifndef HAL_MEMORY_H
#define HAL_MEMORY_H

#include <stddef.h>
#include <stdint.h>

/**
 * Initialize memory subsystem.
 * Must be called before any other hal_mem_* functions.
 *
 * Responsibilities:
 * - Set up heap allocator
 * - Initialize page tables (kernel platforms)
 * - Parse memory map (kernel platforms)
 *
 * Error handling:
 * - hal_panic() if insufficient memory
 */
void hal_mem_init(void);

/**
 * Allocate memory from heap.
 *
 * Parameters:
 * - size: Bytes to allocate (0 = returns NULL)
 *
 * Guarantees:
 * - Returns aligned pointer (at least 8-byte aligned)
 * - Memory is zero-initialized
 * - Thread-safe (can be called concurrently)
 * - NOT callable from interrupt context
 *
 * Returns:
 * - Pointer to allocated memory
 * - NULL if allocation failed
 *
 * Notes:
 * - On Linux: wrapper around malloc()
 * - On kernel: custom heap allocator (kmalloc)
 */
void *hal_mem_alloc(size_t size);

/**
 * Free memory allocated by hal_mem_alloc().
 *
 * Parameters:
 * - ptr: Pointer returned from hal_mem_alloc() (NULL = no-op)
 *
 * Guarantees:
 * - Thread-safe
 * - NOT callable from interrupt context
 * - Safe to call with NULL
 */
void hal_mem_free(void *ptr);

/**
 * Allocate contiguous physical pages.
 * Used for DMA buffers, page tables, large allocations.
 *
 * Parameters:
 * - count: Number of pages to allocate (page size is platform-specific)
 *
 * Returns:
 * - Physical address of first page
 * - 0 if allocation failed
 *
 * Notes:
 * - Pages are NOT mapped into virtual address space
 * - Caller must call hal_mem_map() to access
 * - On Linux: may use mmap() with MAP_ANONYMOUS
 * - On kernel: physical memory manager (PMM)
 */
uint64_t hal_mem_alloc_pages(size_t count);

/**
 * Free physical pages allocated by hal_mem_alloc_pages().
 *
 * Parameters:
 * - paddr: Physical address returned from hal_mem_alloc_pages()
 * - count: Number of pages to free
 */
void hal_mem_free_pages(uint64_t paddr, size_t count);

/**
 * Map physical memory into virtual address space.
 *
 * Parameters:
 * - vaddr: Virtual address to map (must be page-aligned)
 * - paddr: Physical address to map (must be page-aligned)
 * - size: Size in bytes (must be page-multiple)
 * - flags: Mapping flags (HAL_MEM_*)
 *
 * Returns:
 * - 0 on success
 * - -1 if mapping failed
 *
 * Flags:
 * - HAL_MEM_READ: Readable
 * - HAL_MEM_WRITE: Writable
 * - HAL_MEM_EXEC: Executable
 * - HAL_MEM_NOCACHE: Uncached (for MMIO)
 *
 * Notes:
 * - On Linux: no-op (identity mapping assumed)
 * - On kernel: update page tables, flush TLB
 */
int hal_mem_map(uint64_t vaddr, uint64_t paddr, size_t size, unsigned int flags);

/* Mapping flags */
#define HAL_MEM_READ    (1 << 0)
#define HAL_MEM_WRITE   (1 << 1)
#define HAL_MEM_EXEC    (1 << 2)
#define HAL_MEM_NOCACHE (1 << 3)

/**
 * Get page size in bytes.
 *
 * Returns:
 * - Page size (typically 4096 on x86_64)
 */
size_t hal_mem_page_size(void);

#endif /* HAL_MEMORY_H */
```

---

## 4. Console (`include/hal/hal_console.h`)

### Purpose
Character I/O for REPL and diagnostics.

### Interface

```c
#ifndef HAL_CONSOLE_H
#define HAL_CONSOLE_H

/**
 * Initialize console subsystem.
 * Must be called before any other hal_console_* functions.
 *
 * Responsibilities:
 * - Initialize UART (kernel platforms)
 * - Set up framebuffer (if available)
 * - Configure terminal settings (Linux)
 *
 * Error handling:
 * - hal_panic() if no console available
 */
void hal_console_init(void);

/**
 * Write a single character to console.
 *
 * Parameters:
 * - c: Character to write
 *
 * Guarantees:
 * - Character will be visible on console
 * - Blocking (waits if output buffer full)
 * - Callable from interrupt context (for panic messages)
 *
 * Performance:
 * - May be slow (UART is ~1µs per char)
 * - Buffering recommended for bulk output
 */
void hal_console_putc(char c);

/**
 * Write a null-terminated string to console.
 *
 * Parameters:
 * - s: String to write (NULL = no-op)
 *
 * Notes:
 * - Convenience wrapper around hal_console_putc()
 */
void hal_console_puts(const char *s);

/**
 * Read a single character from console.
 *
 * Guarantees:
 * - Blocking (waits until character available)
 * - NOT callable from interrupt context
 *
 * Returns:
 * - Character read (0-255)
 * - -1 on error or EOF
 */
int hal_console_getc(void);

/**
 * Check if input is available (non-blocking).
 *
 * Returns:
 * - 1 if character available (hal_console_getc() will not block)
 * - 0 if no character available
 */
int hal_console_has_input(void);

#endif /* HAL_CONSOLE_H */
```

---

## 5. CPU (`include/hal/hal_cpu.h`)

### Purpose
CPU identification, relax/halt, SMP coordination.

### Interface

```c
#ifndef HAL_CPU_H
#define HAL_CPU_H

#include <stdint.h>

/**
 * Initialize CPU subsystem.
 * Must be called before any other hal_cpu_* functions.
 *
 * Responsibilities:
 * - Detect CPU features (SSE, AVX, etc.)
 * - Set up per-CPU storage
 * - Bring up secondary CPUs (SMP)
 *
 * Error handling:
 * - hal_panic() if CPU unsupported
 */
void hal_cpu_init(void);

/**
 * Get current CPU ID.
 *
 * Returns:
 * - CPU ID (0 = boot processor, 1+ = application processors)
 *
 * Guarantees:
 * - Callable from interrupt context
 * - Fast (< 10 cycles)
 *
 * Notes:
 * - On x86_64 kernel: APIC ID
 * - On Linux: thread ID (not true CPU ID)
 */
unsigned int hal_cpu_id(void);

/**
 * Hint to CPU to relax (pause in spin-wait loop).
 *
 * Guarantees:
 * - Callable from interrupt context
 * - No functional effect (pure performance hint)
 *
 * Notes:
 * - On x86_64: pause instruction
 * - On ARM: yield instruction
 * - On Linux: sched_yield()
 */
void hal_cpu_relax(void);

/**
 * Halt CPU until next interrupt.
 *
 * Guarantees:
 * - CPU enters low-power state
 * - Wakes on next interrupt
 * - NOT callable from interrupt context
 *
 * Notes:
 * - On x86_64 kernel: hlt instruction
 * - On Linux: select() or nanosleep()
 */
void hal_cpu_halt(void);

/**
 * Get number of CPUs in system.
 *
 * Returns:
 * - Number of CPUs (1 = single-core, 2+ = multi-core)
 */
unsigned int hal_cpu_count(void);

#endif /* HAL_CPU_H */
```

---

## 6. Panic/Error Handling (`include/hal/hal_panic.h`)

### Purpose
Fatal error handling (system halt or abort).

### Interface

```c
#ifndef HAL_PANIC_H
#define HAL_PANIC_H

/**
 * Fatal error: print message and halt system.
 *
 * Parameters:
 * - msg: Error message (NULL = "panic")
 *
 * Guarantees:
 * - NEVER returns
 * - Message printed to console (if available)
 * - System halted (kernel) or abort() (hosted)
 * - Callable from interrupt context
 *
 * Notes:
 * - Use for unrecoverable errors only
 * - On kernel: disable interrupts + hlt loop
 * - On Linux: fprintf(stderr) + abort()
 */
void hal_panic(const char *msg) __attribute__((noreturn));

#endif /* HAL_PANIC_H */
```

---

## Interface Summary

| Subsystem | Key Functions | ISR-Safe | Thread-Safe |
|-----------|---------------|----------|-------------|
| **Time** | `hal_time_now_ns()` | ✅ | ✅ |
|  | `hal_timer_periodic()` | ❌ | ✅ |
| **Interrupt** | `hal_in_interrupt_context()` | ✅ | ✅ |
|  | `hal_irq_disable()` | ✅ | ✅ |
| **Memory** | `hal_mem_alloc()` | ❌ | ✅ |
|  | `hal_mem_map()` | ❌ | ⚠️ |
| **Console** | `hal_console_putc()` | ✅ | ⚠️ |
|  | `hal_console_getc()` | ❌ | ❌ |
| **CPU** | `hal_cpu_id()` | ✅ | ✅ |
|  | `hal_cpu_relax()` | ✅ | ✅ |

**Legend:**
- ✅ = Safe
- ❌ = Unsafe
- ⚠️ = Platform-dependent

---

## Next Steps

See companion documentation:
- `platform-implementations.md` - How to implement HAL for a platform
- `migration-plan.md` - Refactoring existing code to use HAL
- `lithosananke-integration.md` - LithosAnanke-specific HAL implementation