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

static inline const VMHostServices *sk_hal_services(void) {
    return sk_host_services();
}

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

static const hal_exec_region_t *hal_exec_region_for(uint64_t addr) {
    for (size_t i = 0; i < hal_exec_region_count; ++i) {
        const hal_exec_region_t *region = &hal_exec_regions[i];
        if (addr >= region->start && addr < region->end) {
            return region;
        }
    }
    return NULL;
}

void sk_hal_whitelist_exec_region(uint64_t start, uint64_t end, const char *name) {
    hal_exec_register(start, end, name);
}

static bool hal_exec_addr_allowed(uint64_t addr) {
    return hal_exec_region_for(addr) != NULL;
}

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

static void hal_log_xt_failure(const char *reason, uint64_t addr) {
    console_puts("[HAL][XT] ");
    console_puts(reason);
    console_puts(" ptr=");
    hal_print_hex64(addr);
    console_println("");
}

static inline int addr_is_canonical(uint64_t addr) {
    uint64_t upper = addr >> 48;
    return (upper == 0x0000ULL) || (upper == 0xFFFFULL);
}

void sk_hal_init(void) {
    sk_host_init();
    sk_hal_whitelist_exec_region((uint64_t)(uintptr_t)__text_start,
                                 (uint64_t)(uintptr_t)__text_end,
                                 "kernel.text");
}

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

void sk_hal_free(void *ptr) {
    const VMHostServices *svc = sk_hal_services();
    if (ptr && svc && svc->free) {
        svc->free(ptr);
    }
}

uint64_t sk_hal_time_ns(void) {
    const VMHostServices *svc = sk_hal_services();
    if (svc && svc->monotonic_ns) {
        return svc->monotonic_ns();
    }
    return 0;
}

uint64_t sk_hal_heartbeat_ticks(void) {
    return heartbeat_ticks();
}

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

int sk_hal_console_putc(int c) {
    const VMHostServices *svc = sk_hal_services();
    if (svc && svc->putc) {
        return svc->putc(c);
    }
    console_putc((char)c);
    return c;
}

void sk_hal_panic(const char *message) {
    console_puts("\n[StarKernel HAL] PANIC: ");
    if (message) {
        console_puts(message);
    } else {
        console_puts("unknown");
    }
    console_puts("\nSystem halted.\n");
#ifdef __STARKERNEL__
    extern void vm_dictionary_log_last_word(struct VM *vm, const char *tag);
    vm_dictionary_log_last_word(NULL, "hal_panic");
#endif
    while (1) {
        arch_halt();
    }
}

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

const VMHostServices *sk_hal_host_services(void) {
    return sk_hal_services();
}

#endif /* __STARKERNEL__ */
