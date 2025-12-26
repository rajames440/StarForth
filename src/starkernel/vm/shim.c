/**
 * shim.c - Kernel shims for VM integration
 *
 * Provides minimal implementations of logging, mutex, time, and malloc/free
 * to satisfy the hosted VM when linked into the kernel. All logging is muted
 * to avoid dependencies on libc formatting.
 */

#ifdef __STARKERNEL__

#include "../../../include/platform_time.h"
#include "../../../include/platform_lock.h"
#include "../../../include/log.h"
#include "../../../include/starkernel/vm_host.h"
#include "console.h"
#include "kmalloc.h"
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

/* -----------------------------------------------------------------------------
 * Minimal malloc/free backed by kmalloc/kfree
 * ---------------------------------------------------------------------------*/
void *malloc(size_t size) {
    if (size == 0) return NULL;
    return kmalloc(size);
}

void free(void *ptr) {
    if (ptr) kfree(ptr);
}

void *calloc(size_t n, size_t size) {
    size_t total = n * size;
    void *p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

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

void log_set_level(LogLevel level) { current_level = level; }
LogLevel log_get_level(void) { return current_level; }

static int kvsnprintf(char *buf, size_t n, const char *fmt, va_list args);

void log_message(LogLevel level, const char *fmt, ...) {
    if (level > current_level || !fmt) return;
    char buf[256];
    va_list args;
    va_start(args, fmt);
    kvsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    console_println(buf);
}

void log_test_result(const char *word_name, TestResult result) {
    (void)result;
    if (word_name) {
        console_println(word_name);
    }
}

void log_set_vm(struct VM *vm) { (void)vm; }

/* -----------------------------------------------------------------------------
 * Mutex stubs (single-threaded kernel)
 * ---------------------------------------------------------------------------*/
int sf_mutex_init(sf_mutex_t *mutex) {
    if (!mutex) return -1;
    mutex->state = 0;
    return 0;
}

void sf_mutex_destroy(sf_mutex_t *mutex) {
    (void)mutex;
}

void sf_mutex_lock(sf_mutex_t *mutex) {
    (void)mutex;
}

void sf_mutex_unlock(sf_mutex_t *mutex) {
    (void)mutex;
}

/* -----------------------------------------------------------------------------
 * Time backend using host services (deterministic in PARITY_MODE)
 * ---------------------------------------------------------------------------*/
static uint64_t shim_monotonic_ns(void) {
    return SK_TIME_NS();
}

static uint64_t shim_realtime_ns(void) {
    return shim_monotonic_ns();
}

static int shim_set_realtime_ns(uint64_t ns) {
    (void)ns;
    return -1;
}

static int shim_format_timestamp(uint64_t ns, char *buf, int format_24h) {
    (void)ns;
    (void)format_24h;
    if (!buf) return -1;
    buf[0] = '0';
    buf[1] = '\0';
    return 0;
}

static int shim_has_rtc(void) { return 0; }

static const sf_time_backend_t shim_backend = {
    .get_monotonic_ns = shim_monotonic_ns,
    .get_realtime_ns = shim_realtime_ns,
    .set_realtime_ns = shim_set_realtime_ns,
    .format_timestamp = shim_format_timestamp,
    .has_rtc = shim_has_rtc
};

const sf_time_backend_t *sf_time_backend = &shim_backend;

/* -----------------------------------------------------------------------------
 * Minimal string/memory functions
 * ---------------------------------------------------------------------------*/
void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

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

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *p = a, *q = b;
    while (n--) {
        if (*p != *q) return (int)*p - (int)*q;
        p++; q++;
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (s && *s++) n++;
    return n;
}

int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && (*a == *b)) { a++; b++; }
    return n == (size_t)-1 ? 0 : (unsigned char)*a - (unsigned char)*b;
}

char *strncpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return dst;
}

char *strchr(const char *s, int c) {
    while (*s) { if (*s == (char)c) return (char*)s; s++; }
    return (c == 0) ? (char*)s : NULL;
}

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

double strtod(const char *nptr, char **endptr) {
    long v = strtol(nptr, endptr, 10);
    return (double)v;
}

/* -----------------------------------------------------------------------------
 * Minimal errno
 * ---------------------------------------------------------------------------*/
int *__errno_location(void) {
    static int e = 0;
    return &e;
}

/* -----------------------------------------------------------------------------
 * Minimal stdio stubs
 * ---------------------------------------------------------------------------*/
typedef struct { int dummy; } FILE;
FILE *stdin = (FILE*)0;
FILE *stdout = (FILE*)0;
FILE *stderr = (FILE*)0;

static int kvsnprintf(char *buf, size_t n, const char *fmt, va_list args) {
    size_t used = 0;
    const char *p = fmt;
    if (n == 0) return 0;
    while (*p && used + 1 < n) {
        if (*p == '%' && *(p + 1)) {
            p++;
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

int snprintf(char *buf, size_t n, const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    int r = kvsnprintf(buf, n, fmt, args);
    va_end(args);
    return r;
}

int printf(const char *fmt, ...) {
    char buf[256];
    va_list args; va_start(args, fmt);
    kvsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    console_println(buf);
    return (int)strlen(buf);
}

int fprintf(FILE *stream, const char *fmt, ...) {
    (void)stream;
    char buf[256];
    va_list args; va_start(args, fmt);
    kvsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    console_println(buf);
    return (int)strlen(buf);
}

int vfprintf(FILE *stream, const char *fmt, va_list args) {
    (void)stream;
    char buf[256];
    kvsnprintf(buf, sizeof(buf), fmt, args);
    console_println(buf);
    return (int)strlen(buf);
}

int puts(const char *s) { console_println(s ? s : ""); return 0; }
int putchar(int c) { console_putc((char)c); return c; }
int fflush(FILE *stream) { (void)stream; return 0; }
int fputs(const char *s, FILE *stream) { (void)stream; console_println(s ? s : ""); return 0; }

FILE *fopen(const char *path, const char *mode) { (void)path; (void)mode; return NULL; }
int fclose(FILE *f) { (void)f; return 0; }
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) { (void)ptr; (void)size; (void)nmemb; (void)stream; return 0; }
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) { (void)ptr; (void)size; (void)nmemb; (void)stream; return 0; }
int fseek(FILE *stream, long offset, int whence) { (void)stream; (void)offset; (void)whence; return -1; }
long ftell(FILE *stream) { (void)stream; return 0; }
char *fgets(char *s, int size, FILE *stream) { (void)s; (void)size; (void)stream; return NULL; }
int fputc(int c, FILE *stream) { (void)stream; console_putc((char)c); return c; }
int fgetc(FILE *stream) { (void)stream; return -1; }

int __isoc99_sscanf(const char *str, const char *fmt, ...) {
    (void)str; (void)fmt; return 0;
}

int __isoc99_fscanf(void *stream, const char *fmt, ...) {
    (void)stream; (void)fmt; return 0;
}

void * __sysv_signal(int sig, void *handler) {
    (void)sig; (void)handler; return NULL;
}

int raise(int sig) {
    (void)sig; return 0;
}

static const unsigned short ctype_table[257] = {0};
const unsigned short ** __ctype_b_loc(void) {
    static const unsigned short *p = &ctype_table[1];
    return &p;
}

int getchar(void) { return -1; }

/* -----------------------------------------------------------------------------
 * Misc platform stubs
 * ---------------------------------------------------------------------------*/
typedef int pid_t;
int sched_getscheduler(pid_t pid) { (void)pid; return 0; }
int sched_getparam(pid_t pid, void *param) { (void)pid; (void)param; return 0; }
int sched_rr_get_interval(pid_t pid, void *ts) { (void)pid; (void)ts; return 0; }
long sysconf(int name) { (void)name; return 0; }

/* -----------------------------------------------------------------------------
 * qsort (simple bubble sort)
 * ---------------------------------------------------------------------------*/
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
