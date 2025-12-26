#ifdef __STARKERNEL__

#include "starkernel/hal/hal.h"
#include "starkernel/vm_host.h"
#include "starkernel/timer.h"
#include "starkernel/console.h"
#include "starkernel/arch.h"

static inline const VMHostServices *sk_hal_services(void) {
    return sk_host_services();
}

void sk_hal_init(void) {
    sk_host_init();
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
    while (1) {
        arch_halt();
    }
}

bool sk_hal_is_executable_ptr(const void *ptr) {
    (void)ptr;
    /* TODO(M7 Step 6): consult VMM mappings */
    return true;
}

const VMHostServices *sk_hal_host_services(void) {
    return sk_hal_services();
}

#endif /* __STARKERNEL__ */
