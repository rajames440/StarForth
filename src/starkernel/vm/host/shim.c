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
 * shim.c - Kernel shims for VM integration
 *
 * Provides minimal implementations of logging, mutex, time, and malloc/free
 * to satisfy the hosted VM when linked into the kernel. All logging is muted
 * to avoid dependencies on libc formatting.
 */

#ifdef __STARKERNEL__

/* shim.c provides concrete sf_time wrappers below; suppress the inline versions.
 * PLATFORM_TIME_NO_INLINE is also set globally via COMMON_CFLAGS in Makefile.starkernel,
 * but kept here as documentation and for non-Makefile build contexts. */
#ifndef PLATFORM_TIME_NO_INLINE
#define PLATFORM_TIME_NO_INLINE
#endif
#include "platform_time.h"
#include "platform_lock.h"
#include "log.h"
#include "vm_host.h"
#include "console.h"
#include "kmalloc.h"
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

/* -----------------------------------------------------------------------------
 * abort() — freestanding: print to console and spin
 * ---------------------------------------------------------------------------*/
/**
 * @brief Freestanding @c abort() replacement for the kernel build.
 *
 * Prints @c "[ABORT] abort() called — halting" to the kernel console and
 * enters an infinite spin loop with a memory-clobber barrier to prevent the
 * compiler from optimizing it away. Never returns. Replaces the libc
 * @c abort() so that code paths that call it on fatal errors (e.g.,
 * @c assert(), debug panics) are safe in the freestanding kernel environment.
 */
void abort(void) {
    console_println("[ABORT] abort() called — halting");
    for (;;) { __asm__ volatile ("" ::: "memory"); }
}

/* -----------------------------------------------------------------------------
 * Minimal malloc/free backed by kmalloc/kfree
 * ---------------------------------------------------------------------------*/
/**
 * @brief Kernel @c malloc() stub backed by @c kmalloc().
 *
 * Satisfies the libc @c malloc() contract expected by shared VM code. Returns
 * @c NULL for zero-size requests; otherwise delegates to @c kmalloc(). The
 * kernel heap is finite — callers must not assume the same retry or OOM
 * semantics as a POSIX @c malloc().
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to allocated memory, or @c NULL if @p size is zero or
 *         the kernel heap is exhausted.
 */
void *malloc(size_t size) {
    if (size == 0) return NULL;
    return kmalloc(size);
}

/**
 * @brief Kernel @c free() stub backed by @c kfree().
 *
 * Releases memory previously allocated by @c malloc(). Silently ignores
 * @c NULL pointers (matching standard @c free() behaviour). Delegates to
 * @c kfree() for the actual reclaim.
 *
 * @param ptr Pointer to free; no-op if @c NULL.
 */
void free(void *ptr) {
    if (ptr) kfree(ptr);
}

/**
 * @brief Kernel @c calloc() stub: allocate and zero @p n × @p size bytes.
 *
 * Computes the total size, delegates allocation to @c malloc(), then zeroes
 * the block with @c memset(). Does not guard against integer overflow in
 * @c n × @p size — callers must ensure the product fits in @c size_t.
 *
 * @param n    Number of elements to allocate.
 * @param size Size of each element in bytes.
 * @return Pointer to zeroed memory, or @c NULL on failure.
 */
void *calloc(size_t n, size_t size) {
    size_t total = n * size;
    void *p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

/**
 * @brief Kernel @c realloc() stub: resize a heap allocation.
 *
 * Implements standard @c realloc() semantics using @c malloc() + @c memcpy() +
 * @c free(). Because the old allocation size is not tracked by @c kmalloc(),
 * the copy uses @p size as the byte count — meaning this truncates the content
 * if @p size < old size, and does not guarantee the tail bytes are zero if
 * @p size > old size. Suitable for rare realloc calls in shared VM code.
 *
 * Special cases:
 * - @p ptr == @c NULL → equivalent to @c malloc(@p size).
 * - @p size == 0 → frees @p ptr and returns @c NULL.
 *
 * @param ptr  Pointer to existing allocation; may be @c NULL.
 * @param size New desired size in bytes.
 * @return Pointer to resized block, or @c NULL on failure or when @p size is 0.
 */
void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    /* Simple realloc: allocate new, copy min(old,new), free old */
    void *newp = malloc(size);
    if (newp) {
        /* Unknown old size; copy conservatively */
        memcpy(newp, ptr, size);
        free(ptr);
    }
    return newp;
}

/* -----------------------------------------------------------------------------
 * Minimal logging stubs (no formatting)
 * ---------------------------------------------------------------------------*/
static LogLevel current_level = LOG_INFO;

/**
 * @brief Set the active kernel log level filter.
 *
 * Messages at levels numerically greater than @p level are suppressed.
 * In release builds set to @c LOG_TEST; in @c SK_PARITY_DEBUG builds set to
 * @c LOG_DEBUG to capture traversal traces.
 *
 * @param level New log level threshold (e.g., @c LOG_INFO, @c LOG_DEBUG).
 */
void log_set_level(LogLevel level) { current_level = level; }

/**
 * @brief Return the currently active kernel log level filter.
 *
 * @return The @c LogLevel value last set by @c log_set_level(), or the
 *         default @c LOG_INFO if @c log_set_level() has not been called.
 */
LogLevel log_get_level(void) { return current_level; }

static int kvsnprintf(char *buf, size_t n, const char *fmt, va_list args);

/**
 * @brief Read an architecture-specific cycle or time counter for log timestamps.
 *
 * Returns a monotonically increasing 64-bit counter suitable for computing
 * relative elapsed ticks between log calls. The counter is:
 * - x86-64/i386: RDTSC (cycle counter)
 * - AArch64: @c cntpct_el0 (physical timer count, preceded by ISB)
 * - RISC-V: @c rdcycle pseudoinstruction
 * - Other: always 0
 *
 * The absolute frequency is not calibrated; the value is used only as a
 * relative delta from @c log_tsc_base captured on the first log call.
 *
 * @return Raw counter value for the current ISA.
 */
static inline uint64_t shim_rdtsc(void) {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#elif defined(__aarch64__) || defined(_M_ARM64)
    uint64_t val;
    __asm__ volatile ("isb\n\tmrs %0, cntpct_el0" : "=r"(val));
    return val;
#elif defined(__riscv)
    uint64_t val;
    __asm__ volatile ("rdcycle %0" : "=r"(val));
    return val;
#else
    return 0;
#endif
}

static uint64_t log_tsc_base = 0;  /* Captured on first log call */

/**
 * @brief Write a @c uint64_t value as a decimal ASCII string into @p buf.
 *
 * Writes decimal digits for @p val into @p buf starting at the current
 * position and returns a pointer one past the last digit written. Used by
 * @c log_message() to format the @c "[KRELTSC: N]" relative-tick prefix
 * without depending on @c sprintf() or libc digit-conversion routines.
 * No null terminator is written — the caller is responsible.
 *
 * @param buf Destination buffer; must have at least 20 bytes of space.
 * @param val Value to convert.
 * @return Pointer to the byte immediately after the last digit written.
 */
static char *u64_to_dec(char *buf, uint64_t val) {
    char tmp[21];
    int i = 0;
    if (val == 0) {
        *buf++ = '0';
        return buf;
    }
    while (val > 0) {
        tmp[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i > 0) {
        *buf++ = tmp[--i];
    }
    return buf;
}

/**
 * @brief Emit a formatted log message to the kernel console with ANSI colour.
 *
 * Formats @p fmt + variadic arguments using the internal @c kvsnprintf()
 * formatter (no libc dependency) and emits a coloured log line to the kernel
 * console in the form:
 * @code
 * COLOR[HADES][LEVEL] RESET[KRELTSC: N] message
 * @endcode
 *
 * The @c KRELTSC value is the TSC/timer delta (in raw ticks) from the first
 * call to this function, captured in @c log_tsc_base. This provides relative
 * timestamps in QEMU serial output without requiring a calibrated clock.
 *
 * Log level colours: ERROR=red, WARN=yellow, INFO=green, TEST=magenta,
 * DEBUG=blue. Messages whose @p level exceeds @c current_level are dropped.
 *
 * @param level Severity of the message; suppressed if > @c current_level.
 * @param fmt   @c printf-style format string (must not be @c NULL).
 * @param ...   Format arguments.
 */
void log_message(LogLevel level, const char *fmt, ...) {
    if (level > current_level || !fmt) return;

    /* Relative TSC timestamp */
    uint64_t now = shim_rdtsc();
    if (log_tsc_base == 0) log_tsc_base = now;
    uint64_t rel_tsc = now - log_tsc_base;

    /* ANSI colors and level names indexed by LogLevel (0–4) */
    static const char *colors[] = {
        "\x1b[31m",  /* ERROR — red     */
        "\x1b[33m",  /* WARN  — yellow  */
        "\x1b[32m",  /* INFO  — green   */
        "\x1b[35m",  /* TEST  — magenta */
        "\x1b[34m",  /* DEBUG — blue    */
    };
    static const char *names[] = {
        "ERROR", "WARN ", "INFO ", "TEST ", "DEBUG"
    };
    static const char *reset = "\x1b[0m";

    int idx = (level < LOG_ERROR || level > LOG_DEBUG) ? LOG_ERROR : level;

    /* Format message */
    char buf[LOG_LINE_MAX];
    va_list args;
    va_start(args, fmt);
    kvsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    /* Build KRELTSC prefix */
    char prefix[32];
    char *p = prefix;
    const char *tag = "[KRELTSC: ";
    while (*tag) *p++ = *tag++;
    p = u64_to_dec(p, rel_tsc);
    *p++ = ']';
    *p++ = ' ';
    *p = '\0';

    /* Emit: COLOR[HADES][LEVEL] RESET[KRELTSC: xxx] message\n */
    console_puts(colors[idx]);
    console_puts("[HADES][");
    console_puts(names[idx]);
    console_puts("] ");
    console_puts(reset);
    console_puts(prefix);
    console_println(buf);
}

/**
 * @brief Emit a POST test result line to the kernel console.
 *
 * Prints a coloured @c "Testing <word_name> ... PASS/FAIL/SKIP\n" line to
 * the kernel console, visible in QEMU serial output. Suppressed if
 * @c current_level < @c LOG_TEST or if @p word_name is @c NULL.
 *
 * @param word_name Name of the FORTH word under test.
 * @param result    Test outcome: @c TEST_PASS, @c TEST_FAIL, or @c TEST_SKIP.
 */
void log_test_result(const char *word_name, TestResult result) {
    if (current_level == LOG_NONE || current_level < LOG_TEST) return;
    if (!word_name) return;

    const char *color;
    const char *status;
    switch (result) {
        case TEST_PASS:  color = "\x1b[32m"; status = "PASS"; break;
        case TEST_FAIL:  color = "\x1b[31m"; status = "FAIL"; break;
        case TEST_SKIP:  color = "\x1b[33m"; status = "SKIP"; break;
        default:         color = "\x1b[0m";  status = "????"; break;
    }

    console_puts("\x1b[35m[HADES][TEST ] \x1b[0m");
    console_puts("Testing ");
    console_puts(word_name);
    console_puts(" ... ");
    console_puts(color);
    console_puts(status);
    console_println("\x1b[0m");
}

/**
 * @brief No-op VM pointer registration for the kernel log shim.
 *
 * The hosted log subsystem stores a VM pointer for context in formatted
 * log lines. The kernel shim has no such context (log goes directly to the
 * console) so this is a deliberate no-op.
 *
 * @param vm Ignored.
 */
void log_set_vm(struct VM *vm) { (void)vm; }

/* -----------------------------------------------------------------------------
 * Mutex stubs (single-threaded kernel)
 * ---------------------------------------------------------------------------*/
/**
 * @brief Initialise a kernel mutex (sets state to 0 — unlocked).
 *
 * The kernel VM runs on a single thread with no preemption at the VM level,
 * so the full spinlock/pthread path is unnecessary. This stub zeros the
 * @c state field in @p mutex to leave it in a consistent unlocked state.
 *
 * @param mutex Mutex to initialise; returns -1 immediately if @c NULL.
 * @return 0 on success, -1 if @p mutex is @c NULL.
 */
int sf_mutex_init(sf_mutex_t *mutex) {
    if (!mutex) return -1;
    mutex->state = 0;
    return 0;
}

/**
 * @brief Destroy a kernel mutex (no-op).
 *
 * Nothing to release in the kernel stub — the mutex holds only a single
 * @c int field and has no OS resources.
 *
 * @param mutex Ignored.
 */
void sf_mutex_destroy(sf_mutex_t *mutex) {
    (void)mutex;
}

/**
 * @brief Acquire a kernel mutex (no-op).
 *
 * Single-threaded kernel: no contention is possible at the VM level, so
 * acquiring the mutex is always immediate and guaranteed to succeed.
 *
 * @param mutex Ignored.
 */
void sf_mutex_lock(sf_mutex_t *mutex) {
    (void)mutex;
}

/**
 * @brief Release a kernel mutex (no-op).
 *
 * Single-threaded kernel: matching @c sf_mutex_lock() no-op.
 *
 * @param mutex Ignored.
 */
void sf_mutex_unlock(sf_mutex_t *mutex) {
    (void)mutex;
}

/* -----------------------------------------------------------------------------
 * Time backend using host services (deterministic in PARITY_MODE)
 * ---------------------------------------------------------------------------*/
#include "starkernel/hal/hal.h"
#undef SK_TIME_NS
#define SK_TIME_NS() sk_hal_time_ns()

/**
 * @brief Return the current monotonic time in nanoseconds via HAL.
 *
 * Delegates to @c SK_TIME_NS() which expands to @c sk_hal_time_ns().
 * In @c PARITY_MODE builds the HAL returns a deterministic synthetic clock
 * rather than the real TSC, ensuring tick-identical results across runs.
 *
 * @return Nanoseconds since boot (monotonic, never decreasing).
 */
static uint64_t shim_monotonic_ns(void) {
    return SK_TIME_NS();
}

/**
 * @brief Return the current wall-clock time in nanoseconds (aliased to monotonic).
 *
 * The kernel shim has no independent RTC path at the shim level; real-time
 * is served by the same HAL monotonic clock. Code that needs wall time should
 * use the HAL RTC capability directly.
 *
 * @return Same value as @c shim_monotonic_ns().
 */
static uint64_t shim_realtime_ns(void) {
    return shim_monotonic_ns();
}

/**
 * @brief Attempt to set the real-time clock (always fails in shim).
 *
 * The shim does not implement RTC write-back; the HAL owns that responsibility.
 * Returns -1 unconditionally so callers degrade gracefully.
 *
 * @param ns Desired epoch time in nanoseconds (ignored).
 * @return -1 always.
 */
static int shim_set_realtime_ns(uint64_t ns) {
    (void)ns;
    return -1;
}

/**
 * @brief Format a nanosecond timestamp as a human-readable string (stub).
 *
 * The shim does not implement date/time formatting — the kernel has no libc
 * @c strftime() or @c localtime(). Writes a single @c "0" string to @p buf
 * so that shared VM code expecting a non-NULL timestamp string does not crash.
 *
 * @param ns        Timestamp to format (ignored).
 * @param buf       Output buffer; must be at least 2 bytes.
 * @param format_24h Ignored.
 * @return 0 on success, -1 if @p buf is @c NULL.
 */
static int shim_format_timestamp(uint64_t ns, char *buf, int format_24h) {
    (void)ns;
    (void)format_24h;
    if (!buf) return -1;
    buf[0] = '0';
    buf[1] = '\0';
    return 0;
}

/**
 * @brief Report RTC availability (always unavailable in shim).
 *
 * The shim time backend has no RTC access. Returns 0 so callers fall back
 * to monotonic time rather than attempting RTC reads.
 *
 * @return 0 always (no RTC).
 */
static int shim_has_rtc(void) { return 0; }

static const sf_time_backend_t shim_backend = {
    .get_monotonic_ns = shim_monotonic_ns,
    .get_realtime_ns = shim_realtime_ns,
    .set_realtime_ns = shim_set_realtime_ns,
    .format_timestamp = shim_format_timestamp,
    .has_rtc = shim_has_rtc
};

const sf_time_backend_t *sf_time_backend = &shim_backend;

/**
 * @brief Initialise the shim time backend pointer.
 *
 * Explicitly sets @c sf_time_backend to @c &shim_backend rather than relying
 * on the static-const initialiser. This avoids GOT-relative relocation issues
 * in the PE32+ kernel image where @c -fPIC generates GOT accesses that cannot
 * be resolved at kernel link time. Must be called early in kernel bootstrap,
 * before any @c sf_monotonic_ns() or @c sf_realtime_ns() calls.
 */
void sf_time_init(void) {
    sf_time_backend = &shim_backend;
}

/* Direct wrappers for sf_time functions - avoids GOT indirection issues with -fPIC.
 * The inline functions in platform_time.h generate GOT-relative accesses for
 * sf_time_backend, which causes extra dereferences in PE files without a GOT.
 * These wrappers call the local shim functions directly. */

/**
 * @brief Return monotonic nanoseconds — kernel direct wrapper.
 *
 * Bypasses the @c sf_time_backend vtable to avoid GOT-relative dereferences
 * in the PE32+ image. Calls @c shim_monotonic_ns() directly.
 *
 * @return Nanoseconds since boot.
 */
sf_time_ns_t sf_monotonic_ns(void) {
    return shim_monotonic_ns();
}

/**
 * @brief Return real-time nanoseconds — kernel direct wrapper.
 *
 * Bypasses the @c sf_time_backend vtable. Returns the same monotonic value
 * as @c sf_monotonic_ns() (the shim aliases realtime to monotonic).
 *
 * @return Nanoseconds since boot (no RTC offset applied).
 */
sf_time_ns_t sf_realtime_ns(void) {
    return shim_realtime_ns();
}

/**
 * @brief Set the real-time clock — kernel direct wrapper (always fails).
 *
 * Bypasses the @c sf_time_backend vtable. Returns -1 unconditionally; the
 * kernel shim does not support RTC write-back.
 *
 * @param ns Desired epoch time in nanoseconds (ignored).
 * @return -1 always.
 */
int sf_set_realtime_ns(sf_time_ns_t ns) {
    return shim_set_realtime_ns(ns);
}

/**
 * @brief Format a timestamp — kernel direct wrapper (stub).
 *
 * Bypasses the @c sf_time_backend vtable. Writes @c "0" to @p buf; see
 * @c shim_format_timestamp() for full behaviour.
 *
 * @param ns         Timestamp to format (ignored).
 * @param buf        Output buffer (at least 2 bytes).
 * @param format_24h Ignored.
 * @return 0 on success, -1 if @p buf is @c NULL.
 */
int sf_format_timestamp(sf_time_ns_t ns, char *buf, int format_24h) {
    return shim_format_timestamp(ns, buf, format_24h);
}

/**
 * @brief Query RTC availability — kernel direct wrapper (always 0).
 *
 * Bypasses the @c sf_time_backend vtable.
 *
 * @return 0 always (no RTC in shim).
 */
int sf_has_rtc(void) {
    return shim_has_rtc();
}

/* -----------------------------------------------------------------------------
 * Minimal string/memory functions
 * ---------------------------------------------------------------------------*/
/**
 * @brief Fill @p n bytes of @p s with @p c (freestanding @c memset).
 *
 * Replaces libc @c memset() for the kernel build. Simple byte loop with no
 * SIMD acceleration; acceptable for the low-frequency use in VM init paths.
 *
 * @param s Destination buffer.
 * @param c Fill byte value (cast to @c unsigned char).
 * @param n Number of bytes to fill.
 * @return @p s.
 */
void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

/**
 * @brief Copy @p n bytes from @p src to @p dst (freestanding @c memcpy).
 *
 * Undefined behaviour if the regions overlap — use @c memmove() for that case.
 * Simple forward byte loop; no SIMD acceleration.
 *
 * @param dst Destination buffer.
 * @param src Source buffer.
 * @param n   Number of bytes to copy.
 * @return @p dst.
 */
void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

/**
 * @brief Copy @p n bytes from @p src to @p dst, handling overlap (freestanding @c memmove).
 *
 * Copies forward if @p dst < @p src, backward otherwise, so overlapping
 * regions are handled correctly. Returns immediately if @p dst == @p src or
 * @p n is zero.
 *
 * @param dst Destination buffer.
 * @param src Source buffer.
 * @param n   Number of bytes to copy.
 * @return @p dst.
 */
void *memmove(void *dst, const void *src, size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    if (d == s || n == 0) return dst;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

/**
 * @brief Compare @p n bytes of @p a and @p b (freestanding @c memcmp).
 *
 * @param a First buffer.
 * @param b Second buffer.
 * @param n Number of bytes to compare.
 * @return Negative if @p a < @p b, zero if equal, positive if @p a > @p b
 *         (as signed difference of the differing bytes).
 */
int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *p = a, *q = b;
    while (n--) {
        if (*p != *q) return (int)*p - (int)*q;
        p++; q++;
    }
    return 0;
}

/**
 * @brief Search @p n bytes of @p s for byte value @p c (freestanding @c memchr).
 *
 * @param s Pointer to memory to search.
 * @param c Byte value to find (compared as @c unsigned char).
 * @param n Maximum bytes to scan.
 * @return Pointer to first occurrence of @p c in @p s, or @c NULL if not found.
 */
void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = s;
    while (n--) {
        if (*p == (unsigned char)c) return (void *)p;
        p++;
    }
    return NULL;
}

/**
 * @brief Return the length of a null-terminated string (freestanding @c strlen).
 *
 * Returns 0 if @p s is @c NULL (defensive; standard @c strlen has undefined
 * behaviour on NULL).
 *
 * @param s Null-terminated string.
 * @return Number of bytes before the null terminator.
 */
size_t strlen(const char *s) {
    size_t n = 0;
    while (s && *s++) n++;
    return n;
}

/**
 * @brief Compare two null-terminated strings (freestanding @c strcmp).
 *
 * @param a First string.
 * @param b Second string.
 * @return Negative, zero, or positive as @p a is less than, equal to, or
 *         greater than @p b (unsigned char comparison).
 */
int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

/**
 * @brief Compare up to @p n bytes of two strings (freestanding @c strncmp).
 *
 * @param a First string.
 * @param b Second string.
 * @param n Maximum number of characters to compare.
 * @return Negative, zero, or positive as the bounded prefixes compare
 *         less than, equal to, or greater than each other.
 */
int strncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && (*a == *b)) { a++; b++; }
    return n == (size_t)-1 ? 0 : (unsigned char)*a - (unsigned char)*b;
}

/**
 * @brief Copy up to @p n bytes from @p src to @p dst, padding with NUL (freestanding @c strncpy).
 *
 * If @p src is shorter than @p n bytes, the remainder of @p dst is filled
 * with @c '\\0'. If @p src is @p n or more bytes long, @p dst is not
 * null-terminated — the same classic caveat as standard @c strncpy().
 *
 * @param dst Destination buffer of at least @p n bytes.
 * @param src Source null-terminated string.
 * @param n   Maximum bytes to write.
 * @return @p dst.
 */
char *strncpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return dst;
}

/**
 * @brief Find the first occurrence of byte @p c in string @p s (freestanding @c strchr).
 *
 * Searches forward including the null terminator, so @c strchr(s, 0) returns
 * a pointer to the null terminator.
 *
 * @param s Null-terminated string to search.
 * @param c Character to find.
 * @return Pointer to first occurrence, or @c NULL if @p c is not in @p s
 *         (and @p c != 0).
 */
char *strchr(const char *s, int c) {
    while (*s) { if (*s == (char)c) return (char*)s; s++; }
    return (c == 0) ? (char*)s : NULL;
}

/**
 * @brief Find the first occurrence of needle @p n in haystack @p h (freestanding @c strstr).
 *
 * Returns @p h immediately if @p n is an empty string. Uses a naive O(|h||n|)
 * scan — acceptable for short strings in kernel diagnostics.
 *
 * @param h Null-terminated haystack string.
 * @param n Null-terminated needle string.
 * @return Pointer to first occurrence of @p n in @p h, or @c NULL.
 */
char *strstr(const char *h, const char *n) {
    if (!*n) return (char*)h;
    for (; *h; h++) {
        if (*h == *n) {
            const char *a = h, *b = n;
            while (*a && *b && *a == *b) { a++; b++; }
            if (!*b) return (char*)h;
        }
    }
    return NULL;
}

/**
 * @brief Convert a string to a @c long integer (freestanding @c strtol).
 *
 * Supports base 10 and hex digit characters (a–f, A–F). Handles a leading
 * '-' for negative values. Sets @c *endptr to the first unconverted character
 * if @p endptr is non-NULL. Does not set @c errno; does not detect overflow.
 *
 * @param nptr   Null-terminated string to convert.
 * @param endptr If non-NULL, receives pointer past the last converted digit.
 * @param base   Numeric base (only 0 and 10 are treated identically; base 16
 *               digit letters are always recognised).
 * @return Converted @c long value, or 0 if @p nptr is @c NULL.
 */
long strtol(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    long sign = 1, acc = 0;
    if (!s) return 0;
    if (*s == '-') { sign = -1; s++; }
    if (base == 0) base = 10;
    while (*s) {
        int d = (*s >= '0' && *s <= '9') ? *s - '0' :
                (*s >= 'a' && *s <= 'f') ? *s - 'a' + 10 :
                (*s >= 'A' && *s <= 'F') ? *s - 'A' + 10 : -1;
        if (d < 0 || d >= base) break;
        acc = acc * base + d;
        s++;
    }
    if (endptr) *endptr = (char*)s;
    return sign * acc;
}

/** @brief Convert string to @c unsigned long; wraps @c strtol() with cast. */
unsigned long strtoul(const char *s, char **endptr, int base)
    { return (unsigned long)strtol(s, endptr, base); }
/** @brief Convert string to @c long long; wraps @c strtol() with cast. */
long long strtoll(const char *s, char **endptr, int base)
    { return (long long)strtol(s, endptr, base); }
/** @brief Convert decimal string to @c int; wraps @c strtol(s, NULL, 10). */
int  atoi(const char *s) { return (int)strtol(s, (char**)0, 10); }
/** @brief Convert decimal string to @c long; wraps @c strtol(s, NULL, 10). */
long atol(const char *s) { return strtol(s, (char**)0, 10); }

/**
 * @brief Convert a string to @c double (freestanding stub — integer only).
 *
 * Parses an integer with @c strtol() and casts to @c double. Fractional
 * parts after a decimal point are silently discarded. The kernel has no
 * floating-point parse infrastructure; this stub satisfies link-time
 * references from shared VM code that rarely reaches the @c strtod() call
 * path in a kernel context.
 *
 * @param nptr   Null-terminated string to convert.
 * @param endptr If non-NULL, receives pointer past the last converted digit
 *               (set by @c strtol()).
 * @return Integral @c double representation of the leading integer in @p nptr.
 */
double strtod(const char *nptr, char **endptr) {
    long v = strtol(nptr, endptr, 10);
    return (double)v;
}

/* -----------------------------------------------------------------------------
 * Minimal errno
 * ---------------------------------------------------------------------------*/
/**
 * @brief Return a pointer to the kernel's single-thread @c errno variable.
 *
 * The kernel has no thread-local storage and runs VM code on a single thread,
 * so a static @c int is sufficient. Returns the same address on every call.
 * Satisfies the glibc @c __errno_location() symbol expected by code that
 * accesses @c errno through the macro.
 *
 * @return Pointer to the static @c errno storage.
 */
int *__errno_location(void) {
    static int e = 0;
    return &e;
}

/**
 * @brief Minimal @c strerror(): no filesystem/locale in the kernel, so this
 * just returns a fixed string regardless of @p errnum.
 *
 * Diagnostic callers only need a non-NULL string to format; the numeric
 * errno value itself is the actionable part of the message.
 *
 * @param errnum Ignored.
 * @return Pointer to a static, constant string.
 */
char *strerror(int errnum) {
    (void)errnum;
    return "error";
}

/* -----------------------------------------------------------------------------
 * Minimal stdio stubs
 * ---------------------------------------------------------------------------*/
typedef struct { int dummy; } FILE;
FILE *stdin = (FILE*)0;
FILE *stdout = (FILE*)0;
FILE *stderr = (FILE*)0;

/**
 * @brief Minimal @c vsnprintf implementation for the freestanding kernel.
 *
 * Formats @p fmt + @p args into @p buf, writing at most @p n - 1 characters
 * and always null-terminating (unless @p n is zero). Supports the following
 * format specifiers:
 *
 * - @c %%s — null-terminated string (@c "(null)" if pointer is @c NULL)
 * - @c %%d / @c %%u — signed/unsigned @c int
 * - @c %%x — unsigned @c int as lowercase hex
 * - @c %%p — @c void* as "0x..." lowercase hex
 * - @c %%ld — @c long
 * - @c %%lu — @c unsigned long
 * - @c %%llu — @c unsigned long long
 * - @c %%zu — @c size_t
 * - @c %%.*s — length-bounded string (precision from @c int argument)
 * - @c %%%% — literal @c %
 *
 * All other specifiers are silently consumed without output. No width/padding
 * flags are supported. Used by @c log_message(), @c printf(), and @c snprintf()
 * to avoid any libc dependency.
 *
 * @param buf  Output buffer.
 * @param n    Buffer capacity including null terminator.
 * @param fmt  Format string.
 * @param args Variadic argument list opened by the caller.
 * @return Number of characters written (not counting the null terminator).
 */
static int kvsnprintf(char *buf, size_t n, const char *fmt, va_list args) {
    size_t used = 0;
    const char *p = fmt;
    if (n == 0) return 0;
    while (*p && used + 1 < n) {
        if (*p == '%' && *(p + 1)) {
            p++;
            /* Handle %.*s (precision string) */
            if (*p == '.' && *(p + 1) == '*' && *(p + 2) == 's') {
                int precision = va_arg(args, int);
                const char *s = va_arg(args, const char*);
                if (!s) s = "(null)";
                int count = 0;
                while (*s && count < precision && used + 1 < n) {
                    buf[used++] = *s++;
                    count++;
                }
                p += 3; /* skip ".*s" */
                continue;
            }
            /* Handle %zu (size_t) */
            if (*p == 'z' && *(p + 1) == 'u') {
                size_t v = va_arg(args, size_t);
                char tmp[32]; int i = 0;
                do { tmp[i++] = (char)('0' + (v % 10)); v /= 10; } while (v && i < (int)sizeof(tmp));
                while (i-- && used + 1 < n) buf[used++] = tmp[i];
                p += 2; /* skip "zu" */
                continue;
            }
            /* Handle %ld (long) */
            if (*p == 'l' && *(p + 1) == 'd') {
                long v = va_arg(args, long);
                char tmp[32]; int neg = 0, i = 0;
                if (v < 0) { neg = 1; v = -v; }
                do { tmp[i++] = (char)('0' + (v % 10)); v /= 10; } while (v && i < (int)sizeof(tmp));
                if (neg && i < (int)sizeof(tmp)) tmp[i++] = '-';
                while (i-- && used + 1 < n) buf[used++] = tmp[i];
                p += 2; /* skip "ld" */
                continue;
            }
            /* Handle %llu (unsigned long long — unambiguous 64-bit on all arches) */
            if (*p == 'l' && *(p + 1) == 'l' && *(p + 2) == 'u') {
                unsigned long long v = va_arg(args, unsigned long long);
                char tmp[32]; int i = 0;
                do { tmp[i++] = (char)('0' + (v % 10)); v /= 10; } while (v && i < (int)sizeof(tmp));
                while (i-- && used + 1 < n) buf[used++] = tmp[i];
                p += 3; /* skip "llu" */
                continue;
            }
            /* Handle %lu (unsigned long) */
            if (*p == 'l' && *(p + 1) == 'u') {
                unsigned long v = va_arg(args, unsigned long);
                char tmp[32]; int i = 0;
                do { tmp[i++] = (char)('0' + (v % 10)); v /= 10; } while (v && i < (int)sizeof(tmp));
                while (i-- && used + 1 < n) buf[used++] = tmp[i];
                p += 2; /* skip "lu" */
                continue;
            }
            if (*p == 's') {
                const char *s = va_arg(args, const char*);
                if (!s) s = "(null)";
                while (*s && used + 1 < n) buf[used++] = *s++;
            } else if (*p == 'd' || *p == 'u') {
                long v = (*p == 'd') ? va_arg(args, int) : (long)va_arg(args, unsigned int);
                char tmp[32]; int neg = 0, i = 0;
                if (*p == 'd' && v < 0) { neg = 1; v = -v; }
                do { tmp[i++] = (char)('0' + (v % 10)); v /= 10; } while (v && i < (int)sizeof(tmp));
                if (neg && i < (int)sizeof(tmp)) tmp[i++] = '-';
                while (i-- && used + 1 < n) buf[used++] = tmp[i];
            } else if (*p == 'x' || *p == 'p') {
                uint64_t v = (*p == 'p') ? (uint64_t)(uintptr_t)va_arg(args, void*) : (uint64_t)va_arg(args, unsigned int);
                const char *hex = "0123456789abcdef";
                char tmp[32]; int i = 0;
                do { tmp[i++] = hex[v & 0xF]; v >>= 4; } while (v && i < (int)sizeof(tmp));
                if (*p == 'p' && used + 2 < n) { buf[used++] = '0'; buf[used++] = 'x'; }
                while (i-- && used + 1 < n) buf[used++] = tmp[i];
            } else if (*p == '%') {
                buf[used++] = '%';
            }
            p++;
            continue;
        }
        buf[used++] = *p++;
    }
    buf[used] = '\0';
    return (int)used;
}

/**
 * @brief Kernel @c snprintf() stub backed by @c kvsnprintf().
 *
 * @param buf Output buffer.
 * @param n   Buffer capacity including null terminator.
 * @param fmt Format string.
 * @param ... Format arguments.
 * @return Number of characters written (not counting null terminator).
 */
int snprintf(char *buf, size_t n, const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    int r = kvsnprintf(buf, n, fmt, args);
    va_end(args);
    return r;
}

/**
 * @brief Kernel @c printf() stub: format and emit to the console.
 *
 * Formats into a 256-byte stack buffer via @c kvsnprintf() and prints with
 * @c console_println(). Output exceeding 255 characters is silently truncated.
 *
 * @param fmt Format string.
 * @param ... Format arguments.
 * @return Number of characters in the formatted output.
 */
int printf(const char *fmt, ...) {
    char buf[256];
    va_list args; va_start(args, fmt);
    kvsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    console_println(buf);
    return (int)strlen(buf);
}

/**
 * @brief Kernel @c fprintf() stub: ignores stream, formats and emits to console.
 *
 * The kernel has no real file streams; @p stream is discarded. Output goes to
 * the kernel console via @c console_println(). Truncated at 255 characters.
 *
 * @param stream Ignored.
 * @param fmt    Format string.
 * @param ...    Format arguments.
 * @return Number of characters in the formatted output.
 */
int fprintf(FILE *stream, const char *fmt, ...) {
    (void)stream;
    char buf[256];
    va_list args; va_start(args, fmt);
    kvsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    console_println(buf);
    return (int)strlen(buf);
}

/**
 * @brief Kernel @c vfprintf() stub: ignores stream, formats and emits to console.
 *
 * Same as @c fprintf() but accepts a pre-started @c va_list. Used by code
 * that wraps printf-family functions with @c va_list propagation.
 *
 * @param stream Ignored.
 * @param fmt    Format string.
 * @param args   Variadic argument list (caller must have called @c va_start()).
 * @return Number of characters in the formatted output.
 */
int vfprintf(FILE *stream, const char *fmt, va_list args) {
    (void)stream;
    char buf[256];
    kvsnprintf(buf, sizeof(buf), fmt, args);
    console_println(buf);
    return (int)strlen(buf);
}

/** @brief Kernel @c puts(): print @p s + newline to the console; returns 0. */
int puts(const char *s) { console_println(s ? s : ""); return 0; }
/** @brief Kernel @c putchar(): emit one character to the console; returns @p c. */
int putchar(int c) { console_putc((char)c); return c; }
/** @brief Kernel @c fflush(): no-op (console writes are synchronous); returns 0. */
int fflush(FILE *stream) { (void)stream; return 0; }
/** @brief Kernel @c fputs(): ignores stream, prints @p s + newline to console. */
int fputs(const char *s, FILE *stream) { (void)stream; console_println(s ? s : ""); return 0; }

/** @brief Kernel @c fopen(): always returns @c NULL — no filesystem in kernel. */
FILE *fopen(const char *path, const char *mode) { (void)path; (void)mode; return NULL; }
/** @brief Kernel @c fclose(): no-op; returns 0. */
int fclose(FILE *f) { (void)f; return 0; }
/** @brief Kernel @c fread(): always returns 0 — no filesystem in kernel. */
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) { (void)ptr; (void)size; (void)nmemb; (void)stream; return 0; }
/** @brief Kernel @c fwrite(): always returns 0 — no filesystem in kernel. */
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) { (void)ptr; (void)size; (void)nmemb; (void)stream; return 0; }
/** @brief Kernel @c fseek(): always returns -1 — no filesystem in kernel. */
int fseek(FILE *stream, long offset, int whence) { (void)stream; (void)offset; (void)whence; return -1; }
/** @brief Kernel @c ftell(): always returns 0 — no filesystem in kernel. */
long ftell(FILE *stream) { (void)stream; return 0; }
/** @brief Kernel @c rewind(): no-op — no filesystem in kernel. */
void rewind(FILE *stream) { (void)stream; }
/** @brief Kernel @c fscanf(): always returns -1 — no filesystem in kernel. */
int  fscanf(FILE *stream, const char *fmt, ...) { (void)stream; (void)fmt; return -1; }
/** @brief Kernel @c sscanf(): always returns -1 — not implemented in shim. */
int  sscanf(const char *str, const char *fmt, ...) { (void)str; (void)fmt; return -1; }
/** @brief Kernel @c fgets(): always returns @c NULL — no filesystem in kernel. */
char *fgets(char *s, int size, FILE *stream) { (void)s; (void)size; (void)stream; return NULL; }
/** @brief Kernel @c fputc(): ignores stream; emits @p c to kernel console. */
int fputc(int c, FILE *stream) { (void)stream; console_putc((char)c); return c; }
/** @brief Kernel @c fgetc(): always returns -1 (EOF) — no filesystem in kernel. */
int fgetc(FILE *stream) { (void)stream; return -1; }

/**
 * @brief Kernel @c __isoc99_sscanf() stub (ISO C99 internal sscanf symbol).
 *
 * Some libc builds emit @c __isoc99_sscanf rather than @c sscanf for C99
 * format strings. This stub satisfies the link-time reference; always
 * returns 0 (no items matched).
 *
 * @param str Ignored.
 * @param fmt Ignored.
 * @return 0 always.
 */
int __isoc99_sscanf(const char *str, const char *fmt, ...) {
    (void)str; (void)fmt; return 0;
}

/**
 * @brief Kernel @c __isoc99_fscanf() stub (ISO C99 internal fscanf symbol).
 *
 * Satisfies link-time references from shared code compiled with glibc headers.
 * Always returns 0.
 *
 * @param stream Ignored.
 * @param fmt    Ignored.
 * @return 0 always.
 */
int __isoc99_fscanf(void *stream, const char *fmt, ...) {
    (void)stream; (void)fmt; return 0;
}

/**
 * @brief Kernel @c signal() stub (SysV ABI; always returns @c NULL).
 *
 * Signal handling is not meaningful in the freestanding kernel environment.
 * Returns @c NULL (SIG_DFL equivalent) to satisfy link-time references.
 *
 * @param sig     Signal number (ignored).
 * @param handler Handler pointer (ignored).
 * @return @c NULL always.
 */
void * __sysv_signal(int sig, void *handler) {
    (void)sig; (void)handler; return NULL;
}

/**
 * @brief Kernel @c raise() stub: silently discards signal.
 *
 * No signal mechanism exists in the freestanding kernel. Returns 0 to
 * indicate success so callers do not enter error paths.
 *
 * @param sig Signal number (ignored).
 * @return 0 always.
 */
int raise(int sig) {
    (void)sig; return 0;
}

static const unsigned short ctype_table[257] = {0};
/**
 * @brief Return a pointer to the kernel's stub @c ctype locale table.
 *
 * The glibc @c __ctype_b_loc() function returns a pointer to the thread-local
 * ctype classification table used by @c isalpha(), @c isdigit(), etc.
 * The kernel shim provides a zeroed 257-entry table (all characters classify
 * as unknown/false) to satisfy link-time references from libc-aware code.
 * Callers that rely on correct ctype classification must not be used in the
 * kernel build.
 *
 * @return Pointer to a static pointer to the zeroed ctype table.
 */
const unsigned short ** __ctype_b_loc(void) {
    static const unsigned short *p = &ctype_table[1];
    return &p;
}

/** @brief Kernel @c getchar(): always returns -1 (EOF) — no stdin in kernel. */
int getchar(void) { return -1; }

/* -----------------------------------------------------------------------------
 * Misc platform stubs
 * ---------------------------------------------------------------------------*/
typedef int pid_t;
/** @brief Kernel @c sched_getscheduler() stub; always returns 0 (SCHED_OTHER). */
int sched_getscheduler(pid_t pid) { (void)pid; return 0; }
/** @brief Kernel @c sched_getparam() stub; always returns 0 (success, no data). */
int sched_getparam(pid_t pid, void *param) { (void)pid; (void)param; return 0; }
/** @brief Kernel @c sched_rr_get_interval() stub; always returns 0. */
int sched_rr_get_interval(pid_t pid, void *ts) { (void)pid; (void)ts; return 0; }
/** @brief Kernel @c sysconf() stub; always returns 0 (value not available). */
long sysconf(int name) { (void)name; return 0; }

/* -----------------------------------------------------------------------------
 * qsort (simple bubble sort)
 * ---------------------------------------------------------------------------*/
/**
 * @brief Sort an array in place (freestanding @c qsort — O(n²) bubble sort).
 *
 * Implements the standard @c qsort() interface with a simple bubble sort.
 * Performance is O(n²) and unsuitable for large arrays, but the kernel build
 * only calls @c qsort() in the diagnostics path (@c ALL-HEATS) on ≤1024
 * entries, where the cost is acceptable.
 *
 * No-op if @p base is @c NULL, @p nmemb < 2, @p size is 0, or @p compar
 * is @c NULL.
 *
 * @param base   Pointer to the first element of the array.
 * @param nmemb  Number of elements in the array.
 * @param size   Size in bytes of each element.
 * @param compar Comparison function: returns < 0, 0, or > 0.
 */
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    if (!base || nmemb < 2 || size == 0 || !compar) return;
    unsigned char *b = base;
    for (size_t i = 0; i < nmemb - 1; i++) {
        for (size_t j = 0; j < nmemb - i - 1; j++) {
            unsigned char *x = b + j * size;
            unsigned char *y = x + size;
            if (compar(x, y) > 0) {
                for (size_t k = 0; k < size; k++) {
                    unsigned char tmp = x[k];
                    x[k] = y[k];
                    y[k] = tmp;
                }
            }
        }
    }
}

#endif /* __STARKERNEL__ */
