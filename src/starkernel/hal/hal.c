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

#ifdef __STARKERNEL__

#include "starkernel/hal/hal.h"
#include "vm_host.h"
#include "starkernel/timer.h"
#include "starkernel/console.h"
#include "starkernel/arch.h"
#include "starkernel/vmm.h"
#include "dictionary_management.h"

extern char __text_start[];
extern char __text_end[];

/* Force PC-relative address load for linker-defined symbols.
 * With -fPIC, the compiler generates GOT-relative loads for extern symbols,
 * which loads CONTENTS instead of ADDRESS. These wrappers use inline asm
 * to ensure we get the actual addresses.
 *
 * Architecture-specific implementations:
 * - x86_64: LEA with RIP-relative addressing
 * - AArch64: ADRP + ADD for page-relative addressing
 * - RISC-V64: LA pseudo-instruction (AUIPC + ADDI)
 */
/**
 * @brief Return the runtime address of the @c __text_start linker symbol (x86_64).
 *
 * Uses a PC-relative @c LEA to load the address directly, bypassing the GOT
 * indirection that @c -fPIC would otherwise introduce for @c extern symbols.
 * Only compiled on x86_64 / _M_X64 targets.
 *
 * @return Physical address of the beginning of the kernel @c .text section
 */
#if defined(__x86_64__) || defined(_M_X64)
static inline uint64_t get_text_start_addr(void) {
    uint64_t addr;
    __asm__ volatile("leaq __text_start(%%rip), %0" : "=r"(addr));
    return addr;
}

/**
 * @brief Return the runtime address of the @c __text_end linker symbol (x86_64).
 *
 * PC-relative @c LEA counterpart to @c get_text_start_addr() for the end of
 * the kernel @c .text section.
 *
 * @return Physical address one byte past the end of the kernel @c .text section
 */
static inline uint64_t get_text_end_addr(void) {
    uint64_t addr;
    __asm__ volatile("leaq __text_end(%%rip), %0" : "=r"(addr));
    return addr;
}

/**
 * @brief Return the runtime address of @c __text_start (AArch64).
 *
 * Uses @c ADRP + @c ADD to form the page-relative address of @c __text_start,
 * avoiding @c -fPIC GOT indirection on AArch64 / _M_ARM64 targets.
 *
 * @return Physical address of the beginning of the kernel @c .text section
 */
#elif defined(__aarch64__) || defined(_M_ARM64)
static inline uint64_t get_text_start_addr(void) {
    uint64_t addr;
    __asm__ volatile(
        "adrp %0, __text_start\n\t"
        "add %0, %0, :lo12:__text_start"
        : "=r"(addr)
    );
    return addr;
}

/**
 * @brief Return the runtime address of @c __text_end (AArch64).
 *
 * @c ADRP + @c ADD counterpart to @c get_text_start_addr() for the end of
 * the kernel @c .text section on AArch64 / _M_ARM64 targets.
 *
 * @return Physical address one byte past the end of the kernel @c .text section
 */
static inline uint64_t get_text_end_addr(void) {
    uint64_t addr;
    __asm__ volatile(
        "adrp %0, __text_end\n\t"
        "add %0, %0, :lo12:__text_end"
        : "=r"(addr)
    );
    return addr;
}

/**
 * @brief Return the runtime address of @c __text_start (RISC-V 64-bit).
 *
 * Uses the @c LA pseudo-instruction (expands to @c AUIPC + @c ADDI) to load
 * the PC-relative address of @c __text_start without GOT indirection.
 *
 * @return Physical address of the beginning of the kernel @c .text section
 */
#elif defined(__riscv) && (__riscv_xlen == 64)
static inline uint64_t get_text_start_addr(void) {
    uint64_t addr;
    __asm__ volatile("la %0, __text_start" : "=r"(addr));
    return addr;
}

/**
 * @brief Return the runtime address of @c __text_end (RISC-V 64-bit).
 *
 * @c LA counterpart to @c get_text_start_addr() for the end of the kernel
 * @c .text section on RISC-V 64-bit targets.
 *
 * @return Physical address one byte past the end of the kernel @c .text section
 */
static inline uint64_t get_text_end_addr(void) {
    uint64_t addr;
    __asm__ volatile("la %0, __text_end" : "=r"(addr));
    return addr;
}

#else
#error "Unsupported architecture for get_text_*_addr()"
#endif

typedef struct {
    uint64_t start;
    uint64_t end;
    const char *name;
} hal_exec_region_t;

#define HAL_MAX_EXEC_REGIONS 8
static hal_exec_region_t hal_exec_regions[HAL_MAX_EXEC_REGIONS];
static size_t hal_exec_region_count = 0;
static bool hal_exec_range_frozen = false;
static void hal_print_hex64(uint64_t value);

/**
 * @brief Return the kernel VM host-services vtable.
 *
 * Thin inline wrapper around @c sk_host_services() for use within this
 * translation unit.
 *
 * @return Pointer to the active @c VMHostServices struct, or NULL if not initialised
 */
static inline const VMHostServices *sk_hal_services(void) {
    return sk_host_services();
}

/**
 * @brief Add an address range to the HAL executable-region whitelist.
 *
 * Inserts @c [start, end) into @c hal_exec_regions[] if not already present
 * and the table has capacity (@c HAL_MAX_EXEC_REGIONS). Duplicate ranges
 * (matching both @c start and @c end) are silently ignored. No-ops on
 * zero-size ranges or when the table is full.
 *
 * @param start Inclusive start address of the executable region
 * @param end   Exclusive end address of the executable region; must be > @c start
 * @param name  Human-readable label for diagnostic output (may be NULL)
 */
static void hal_exec_register(uint64_t start, uint64_t end, const char *name) {
    if (start >= end || hal_exec_region_count >= HAL_MAX_EXEC_REGIONS) {
        return;
    }
    for (size_t i = 0; i < hal_exec_region_count; ++i) {
        if (hal_exec_regions[i].start == start && hal_exec_regions[i].end == end) {
            return;
        }
    }
    hal_exec_regions[hal_exec_region_count].start = start;
    hal_exec_regions[hal_exec_region_count].end = end;
    hal_exec_regions[hal_exec_region_count].name = name;
    hal_exec_region_count++;
}

/**
 * @brief Find the whitelisted executable region that contains @c addr.
 *
 * Performs a linear search over @c hal_exec_regions[].
 *
 * @param addr Address to locate
 * @return Pointer to the matching @c hal_exec_region_t, or NULL if not found
 */
static const hal_exec_region_t *hal_exec_region_for(uint64_t addr) {
    for (size_t i = 0; i < hal_exec_region_count; ++i) {
        const hal_exec_region_t *region = &hal_exec_regions[i];
        if (addr >= region->start && addr < region->end) {
            return region;
        }
    }
    return NULL;
}

/**
 * @brief Publicly register an address range as an allowed execution target.
 *
 * Delegates to @c hal_exec_register(). Called by @c sk_hal_init() for the
 * kernel @c .text section, and may be called by capsule or module loaders to
 * whitelist their own code before the range is frozen by @c sk_hal_freeze_exec_range().
 *
 * @param start Inclusive start address of the executable region
 * @param end   Exclusive end address; must be > @c start
 * @param name  Diagnostic label (may be NULL)
 */
void sk_hal_whitelist_exec_region(uint64_t start, uint64_t end, const char *name) {
    hal_exec_register(start, end, name);
}

/**
 * @brief Test whether @c addr falls within a whitelisted executable region.
 *
 * @param addr Address to test
 * @return @c true if the address is inside any registered region, @c false otherwise
 */
static bool hal_exec_addr_allowed(uint64_t addr) {
    return hal_exec_region_for(addr) != NULL;
}

/**
 * @brief Freeze the executable-region whitelist and print the final set.
 *
 * Sets @c hal_exec_range_frozen = true on first call; subsequent calls are
 * no-ops. Prints all registered regions to the kernel console for audit.
 * After freezing, @c sk_hal_is_executable_ptr() rejects any pointer not in
 * the frozen set, so this should be called after all capsules and modules
 * have registered their code regions during boot.
 */
void sk_hal_freeze_exec_range(void) {
    if (!hal_exec_range_frozen) {
        hal_exec_range_frozen = true;
        console_puts("[HAL][exec] regions frozen:");
        console_println("");
        if (hal_exec_region_count == 0) {
            console_println("    (none registered)");
        }
        for (size_t i = 0; i < hal_exec_region_count; ++i) {
            const hal_exec_region_t *region = &hal_exec_regions[i];
            console_puts("    ");
            console_puts(region->name ? region->name : "region");
            console_puts(": [");
            hal_print_hex64(region->start);
            console_puts(", ");
            hal_print_hex64(region->end);
            console_println(")");
        }
    }
}

/**
 * @brief Print a 64-bit value as a zero-padded 18-character hex string to the console.
 *
 * Formats @c value as @c "0xNNNNNNNNNNNNNNNN" (16 hex digits, zero-padded)
 * and outputs it via @c console_puts(). No newline is appended.
 *
 * @param value 64-bit value to print
 */
static void hal_print_hex64(uint64_t value) {
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    buf[18] = '\0';
    for (int i = 0; i < 16; ++i) {
        uint8_t nibble = (uint8_t)((value >> ((15 - i) * 4)) & 0xF);
        buf[i + 2] = (nibble < 10) ? (char)('0' + nibble) : (char)('a' + nibble - 10);
    }
    console_puts(buf);
}

/**
 * @brief Log an execution-target (XT) validation failure to the kernel console.
 *
 * Prints @c "[HAL][XT] REASON ptr=ADDR\n" using @c console_puts() and
 * @c hal_print_hex64(). Called by @c sk_hal_is_executable_ptr() on each
 * rejection path so each failure mode is distinguishable in the boot log.
 *
 * @param reason Short human-readable failure description
 * @param addr   The rejected pointer address
 */
static void hal_log_xt_failure(const char *reason, uint64_t addr) {
    console_puts("[HAL][XT] ");
    console_puts(reason);
    console_puts(" ptr=");
    hal_print_hex64(addr);
    console_println("");
}

/**
 * @brief Test whether a 64-bit virtual address is in canonical form.
 *
 * On x86_64, canonical addresses have bits [63:48] all 0 (user space) or
 * all 1 (kernel space). Non-canonical addresses trigger a General Protection
 * Fault when dereferenced. This check is used to pre-screen XT pointers
 * before querying the page tables.
 *
 * @param addr Address to check
 * @return 1 if canonical (bits [63:48] are all 0 or all 1), 0 otherwise
 */
static inline int addr_is_canonical(uint64_t addr) {
    uint64_t upper = addr >> 48;
    return (upper == 0x0000ULL) || (upper == 0xFFFFULL);
}

/**
 * @brief Initialise the StarKernel HAL.
 *
 * Calls @c sk_host_init() to wire up the @c VMHostServices vtable, then
 * whitelists the kernel @c .text section (@c __text_start … @c __text_end)
 * as the initial executable region. Must be called once during M7 bootstrap
 * before any @c sk_hal_is_executable_ptr() call.
 */
void sk_hal_init(void) {
    sk_host_init();
    /* Use inline asm wrappers to avoid -fPIC GOT references */
    uint64_t text_start_addr = get_text_start_addr();
    uint64_t text_end_addr = get_text_end_addr();
    sk_hal_whitelist_exec_region(text_start_addr, text_end_addr, "kernel.text");
}

/**
 * @brief Allocate aligned memory via the kernel VM host-services vtable.
 *
 * Delegates to @c svc->alloc(). Defaults to @c sizeof(void*) alignment when
 * @c align == 0. Returns NULL if the host-services vtable is not initialised,
 * @c svc->alloc is NULL, or @c size is zero.
 *
 * @param size  Number of bytes to allocate
 * @param align Required byte alignment (0 = pointer-width default)
 * @return Aligned memory block, or NULL on failure
 */
void *sk_hal_alloc(size_t size, size_t align) {
    const VMHostServices *svc = sk_hal_services();
    if (!svc || !svc->alloc || size == 0) {
        return NULL;
    }
    if (align == 0) {
        align = sizeof(void *);
    }
    return svc->alloc(size, align);
}

/**
 * @brief Free a block previously allocated by @c sk_hal_alloc().
 *
 * Delegates to @c svc->free(). No-op if @c ptr is NULL or the host-services
 * vtable is not initialised.
 *
 * @param ptr Block to release (may be NULL)
 */
void sk_hal_free(void *ptr) {
    const VMHostServices *svc = sk_hal_services();
    if (ptr && svc && svc->free) {
        svc->free(ptr);
    }
}

/**
 * @brief Return the current monotonic time in nanoseconds (kernel HAL).
 *
 * Delegates to @c svc->monotonic_ns(). Returns 0 if the host-services vtable
 * is not initialised or the callback is NULL.
 *
 * @return Nanoseconds since an arbitrary boot-time epoch, or 0 on error
 */
uint64_t sk_hal_time_ns(void) {
    const VMHostServices *svc = sk_hal_services();
    if (svc && svc->monotonic_ns) {
        return svc->monotonic_ns();
    }
    return 0;
}

/**
 * @brief Return the current kernel heartbeat tick count.
 *
 * Delegates to @c heartbeat_ticks() (driven by the APIC timer interrupt at
 * 100 Hz). Used by the VM physics engine to track elapsed wall-clock intervals
 * on bare metal where @c clock_gettime() is unavailable.
 *
 * @return Current heartbeat tick counter value
 */
uint64_t sk_hal_heartbeat_ticks(void) {
    return heartbeat_ticks();
}

/**
 * @brief Write @c len bytes from @c buf to the kernel console.
 *
 * Iterates over each byte and dispatches to @c svc->putc() if available,
 * falling back to @c console_putc() when only @c svc->puts() is provided.
 * Returns 0 if @c buf is NULL, @c len is 0, or the host-services vtable is
 * uninitialised; stops early if @c putc() returns a negative error code.
 *
 * @param buf Data to write (need not be NUL-terminated)
 * @param len Number of bytes to write
 * @return Number of bytes actually written
 */
size_t sk_hal_console_write(const char *buf, size_t len) {
    if (!buf || len == 0) {
        return 0;
    }
    const VMHostServices *svc = sk_hal_services();
    if (!svc) {
        return 0;
    }
    size_t written = 0;
    for (size_t i = 0; i < len; i++) {
        int ch = (unsigned char)buf[i];
        if (svc->putc) {
            if (svc->putc(ch) < 0) {
                break;
            }
        } else if (svc->puts) {
            /* puts requires NUL-terminated strings; fall back to console_putc */
            console_putc((char)ch);
        }
        written++;
    }
    return written;
}

/**
 * @brief Write a single character to the kernel console.
 *
 * Delegates to @c svc->putc() when available; falls back to @c console_putc().
 * Returns the character written (as unsigned char promoted to int), mirroring
 * the @c fputc() convention.
 *
 * @param c Character to write (as @c int; only low byte is used)
 * @return @c c on success (always succeeds in current implementation)
 */
int sk_hal_console_putc(int c) {
    const VMHostServices *svc = sk_hal_services();
    if (svc && svc->putc) {
        return svc->putc(c);
    }
    console_putc((char)c);
    return c;
}

/**
 * @brief Trigger a kernel panic — print message and halt all CPUs.
 *
 * Prints @c "[StarKernel HAL] PANIC: MESSAGE\nSystem halted.\n" to the console,
 * then enters an infinite spin loop calling @c arch_halt() on each iteration.
 * This function never returns. @c message may be NULL (prints "unknown").
 *
 * @param message Human-readable panic description, or NULL
 */
void sk_hal_panic(const char *message) {
    console_puts("\n[StarKernel HAL] PANIC: ");
    if (message) {
        console_puts(message);
    } else {
        console_puts("unknown");
    }
    console_puts("\nSystem halted.\n");
#if 0  /* vm_dictionary_log_last_word not implemented in minimal kernel */
#ifdef __STARKERNEL__
    extern void vm_dictionary_log_last_word(struct VM *vm, const char *tag);
    vm_dictionary_log_last_word(NULL, "hal_panic");
#endif
#endif
    while (1) {
        arch_halt();
    }
}

/**
 * @brief Validate that a function pointer is safe to call via EXECUTE.
 *
 * Performs a multi-stage check before the FORTH @c EXECUTE word is allowed
 * to dispatch through @c ptr:
 * 1. NULL rejection
 * 2. Canonical-address check (bits [63:48] must be all-zero or all-one)
 * 3. VMM page-table query (@c vmm_query_page()) — page must be present
 * 4. NX (no-execute) bit check — page must be marked executable
 * 5. Whitelist check — address must fall within a registered exec region
 *
 * Failures are logged to the console via @c hal_log_xt_failure() or inline
 * @c console_puts() calls. The allowed regions are printed on whitelist failure
 * to aid post-mortem analysis.
 *
 * @param ptr Pointer to validate
 * @return @c true if all checks pass, @c false on any failure
 */
bool sk_hal_is_executable_ptr(const void *ptr) {
    uint64_t addr = (uint64_t)(uintptr_t)ptr;

    if (addr == 0) {
        hal_log_xt_failure("reject: NULL pointer", addr);
        return false;
    }

    if (!addr_is_canonical(addr)) {
        hal_log_xt_failure("reject: non-canonical address", addr);
        return false;
    }

    vmm_page_info_t info;
    if (!vmm_query_page(addr, &info) || !info.present) {
        console_puts("[HAL][XT] reject: unmapped pointer=");
        hal_print_hex64(addr);
        console_println("");
        return false;
    }

    if (!info.executable) {
        hal_log_xt_failure("reject: NX mapping", addr);
        return false;
    }

    if (!hal_exec_addr_allowed(addr)) {
        console_puts("[HAL][XT] reject: pointer ");
        hal_print_hex64(addr);
        console_puts(" outside allowed executable ranges");
        console_println("");
        for (size_t i = 0; i < hal_exec_region_count; ++i) {
            console_puts("    allowed ");
            console_puts(hal_exec_regions[i].name ? hal_exec_regions[i].name : "region");
            console_puts(": [");
            hal_print_hex64(hal_exec_regions[i].start);
            console_puts(", ");
            hal_print_hex64(hal_exec_regions[i].end);
            console_println(")");
        }
        return false;
    }

    return true;
}

/**
 * @brief Return the kernel VM host-services vtable (public accessor).
 *
 * @return Pointer to the active @c VMHostServices struct, or NULL if not initialised
 */
const VMHostServices *sk_hal_host_services(void) {
    return sk_hal_services();
}

/**
 * @brief Return the physical address of the kernel @c .text section start.
 *
 * Uses the ISA-appropriate @c get_text_start_addr() inline-asm wrapper to
 * avoid GOT indirection under @c -fPIC. Suitable for use as the lower bound
 * of an exec-region whitelist entry.
 *
 * @return Physical address of @c __text_start
 */
uint64_t sk_hal_text_start(void) {
    return get_text_start_addr();
}

/**
 * @brief Return the physical address one byte past the kernel @c .text section end.
 *
 * Uses the ISA-appropriate @c get_text_end_addr() inline-asm wrapper. The
 * returned value is the exclusive upper bound of the kernel text region.
 *
 * @return Physical address of @c __text_end
 */
uint64_t sk_hal_text_end(void) {
    return get_text_end_addr();
}

#endif /* __STARKERNEL__ */
