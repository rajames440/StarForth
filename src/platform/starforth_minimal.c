/*

                                 ***   StarForth   ***
  starforth_minimal.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/14/25, 8:44 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#include "../include/platform/starforth_platform.h"

#ifdef STARFORTH_MINIMAL

/* Simple memory allocator using a static pool */
static struct {
    uint8_t *pool;
    size_t pool_size;
    size_t allocated;
    uint8_t initialized;
} sf_allocator = {0};

void sf_init_allocator(void *pool, size_t pool_size) {
    sf_allocator.pool = (uint8_t *)pool;
    sf_allocator.pool_size = pool_size;
    sf_allocator.allocated = 0;
    sf_allocator.initialized = 1;
}

void *sf_malloc(size_t size) {
    if (!sf_allocator.initialized || size == 0) return 0;

    /* Align to 8-byte boundary */
    size = (size + 7) & ~7;

    if (sf_allocator.allocated + size > sf_allocator.pool_size) {
        return 0; /* Out of memory */
    }

    void *ptr = sf_allocator.pool + sf_allocator.allocated;
    sf_allocator.allocated += size;
    return ptr;
}

void sf_free(void *ptr) {
    /* Simple allocator doesn't support individual frees */
    /* In a real implementation, you might track allocations */
    SF_UNUSED(ptr);
}

/* String functions */
size_t sf_strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

int sf_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int sf_strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    return n ? *(unsigned char *)s1 - *(unsigned char *)s2 : 0;
}

char *sf_strcpy(char *dest, const char *src) {
    char *orig = dest;
    while ((*dest++ = *src++));
    return orig;
}

char *sf_strncpy(char *dest, const char *src, size_t n) {
    char *orig = dest;
    while (n && (*dest = *src)) {
        dest++;
        src++;
        n--;
    }
    while (n--) *dest++ = 0;
    return orig;
}

void *sf_memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void *sf_memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dest;
}

void *sf_memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

int sf_memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++;
        p2++;
    }
    return 0;
}

int sf_isspace(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

int sf_isdigit(int c) {
    return c >= '0' && c <= '9';
}

int sf_isalpha(int c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int sf_tolower(int c) {
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

int sf_toupper(int c) {
    return (c >= 'a' && c <= 'z') ? c - 32 : c;
}

/* Math functions */
int sf_abs(int x) {
    return x < 0 ? -x : x;
}

long sf_labs(long x) {
    return x < 0 ? -x : x;
}

double sf_fabs(double x) {
    return x < 0.0 ? -x : x;
}

/* Weak platform-specific implementations - to be overridden by target */
__attribute__((weak)) int sf_putchar(int c) {
    /* Default: do nothing */
    SF_UNUSED(c);
    return c;
}

__attribute__((weak)) int sf_puts(const char *s) {
    while (*s) sf_putchar(*s++);
    sf_putchar('\n');
    return 1;
}

__attribute__((weak)) char *sf_fgets(char *s, int size, void *stream) {
    /* Default: return empty string */
    SF_UNUSED(stream);
    if (size > 0) {
        s[0] = '\0';
        return s;
    }
    return 0;
}

__attribute__((weak)) int sf_fputs(const char *s, void *stream) {
    SF_UNUSED(stream);
    return sf_puts(s);
}

__attribute__((weak)) void sf_fflush(void *stream) {
    SF_UNUSED(stream);
    /* Default: do nothing */
}

__attribute__((weak)) sf_clock_t sf_clock(void) {
    /* Default: return 0 */
    return 0;
}

__attribute__((weak)) sf_time_t sf_time(sf_time_t *timer) {
    /* Default: return 0 (epoch) */
    sf_time_t t = 0;
    if (timer) *timer = t;
    return t;
}

__attribute__((weak)) sf_tm *sf_localtime(const sf_time_t *timer) {
    /* Default: return epoch time broken down */
    static sf_tm tm_buf = {0, 0, 0, 1, 0, 70, 4, 0, 0}; /* Jan 1, 1970 */
    SF_UNUSED(timer);
    return &tm_buf;
}

__attribute__((weak)) size_t sf_strftime(char *str, size_t maxsize, const char *format, const sf_tm *timeptr) {
    /* Simple implementation - just return "00:00:00" for time formats */
    SF_UNUSED(format);
    SF_UNUSED(timeptr);

    if (maxsize >= 9) {
        sf_strcpy(str, "00:00:00");
        return 8;
    }
    return 0;
}

__attribute__((weak)) sf_time_t sf_time(sf_time_t *tloc) {
    /* Default: return a fake timestamp */
    sf_time_t fake_time = 1609459200; /* 2021-01-01 00:00:00 UTC */
    if (tloc) *tloc = fake_time;
    return fake_time;
}

__attribute__((weak)) struct sf_tm *sf_localtime(const sf_time_t *timep) {
    /* Default: return a static fake time structure */
    static struct sf_tm fake_tm = {0, 0, 12, 1, 0, 121, 5, 0, 0}; /* 2021-01-01 12:00:00 */
    SF_UNUSED(timep);
    return &fake_tm;
}

__attribute__((weak)) size_t sf_strftime(char *s, size_t max, const char *format, const struct sf_tm *tm) {
    /* Minimal implementation - just handle %H:%M:%S format */
    SF_UNUSED(format);
    if (max < 9) return 0;

    /* Simple fixed format for minimal builds */
    sf_sprintf(s, max, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
    return 8;
}
/*
                                 ***   StarForth   ***
  starforth_minimal.c - Minimal platform implementations
 Last modified - 8/14/25
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
*/

#include "../include/platform/starforth_platform.h"

#ifdef STARFORTH_MINIMAL

/* Simple memory allocator using a static pool */
static struct {
    uint8_t *pool;
    size_t pool_size;
    size_t allocated;
    uint8_t initialized;
} sf_allocator = {0};

void sf_init_allocator(void *pool, size_t pool_size) {
    sf_allocator.pool = (uint8_t *)pool;
    sf_allocator.pool_size = pool_size;
    sf_allocator.allocated = 0;
    sf_allocator.initialized = 1;
}

void *sf_malloc(size_t size) {
    if (!sf_allocator.initialized || size == 0) return NULL;

    /* Align to 8-byte boundary */
    size = (size + 7) & ~7;

    if (sf_allocator.allocated + size > sf_allocator.pool_size) {
        return NULL; /* Out of memory */
    }

    void *ptr = sf_allocator.pool + sf_allocator.allocated;
    sf_allocator.allocated += size;
    return ptr;
}

void sf_free(void *ptr) {
    /* Simple allocator doesn't support individual frees */
    SF_UNUSED(ptr);
}

/* String functions */
size_t sf_strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

int sf_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int sf_strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    return n ? *(unsigned char *)s1 - *(unsigned char *)s2 : 0;
}

char *sf_strcpy(char *dest, const char *src) {
    char *orig = dest;
    while ((*dest++ = *src++));
    return orig;
}

char *sf_strncpy(char *dest, const char *src, size_t n) {
    char *orig = dest;
    while (n && (*dest = *src)) {
        dest++;
        src++;
        n--;
    }
    while (n--) *dest++ = 0;
    return orig;
}

void *sf_memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void *sf_memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dest;
}

void *sf_memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

int sf_memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++;
        p2++;
    }
    return 0;
}

int sf_isspace(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

int sf_isdigit(int c) {
    return c >= '0' && c <= '9';
}

int sf_isalpha(int c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int sf_tolower(int c) {
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

int sf_toupper(int c) {
    return (c >= 'a' && c <= 'z') ? c - 32 : c;
}

/* Math functions */
int sf_abs(int x) {
    return x < 0 ? -x : x;
}

long sf_labs(long x) {
    return x < 0 ? -x : x;
}

double sf_fabs(double x) {
    return x < 0.0 ? -x : x;
}

/* Weak platform-specific implementations - to be overridden by target */
__attribute__((weak)) int sf_putchar(int c) {
    /* Default: do nothing */
    SF_UNUSED(c);
    return c;
}

__attribute__((weak)) int sf_puts(const char *s) {
    while (*s) sf_putchar(*s++);
    sf_putchar('\n');
    return 1;
}

__attribute__((weak)) char *sf_fgets(char *s, int size, void *stream) {
    /* Default: return empty string */
    SF_UNUSED(stream);
    if (size > 0) {
        s[0] = '\0';
        return s;
    }
    return NULL;
}

__attribute__((weak)) int sf_fputs(const char *s, void *stream) {
    SF_UNUSED(stream);
    return sf_puts(s);
}

__attribute__((weak)) void sf_fflush(void *stream) {
    SF_UNUSED(stream);
}

__attribute__((weak)) sf_clock_t sf_clock(void) {
    return 0;
}

__attribute__((weak)) sf_time_t sf_time(sf_time_t *timer) {
    sf_time_t t = 0;
    if (timer) *timer = t;
    return t;
}

__attribute__((weak)) sf_tm *sf_localtime(const sf_time_t *timer) {
    static sf_tm tm_buf = {0, 0, 0, 1, 0, 70, 4, 0, 0};
    SF_UNUSED(timer);
    return &tm_buf;
}

__attribute__((weak)) size_t sf_strftime(char *str, size_t maxsize, const char *format, const sf_tm *timeptr) {
    SF_UNUSED(format);
    SF_UNUSED(timeptr);

    if (maxsize >= 9) {
        sf_strcpy(str, "00:00:00");
        return 8;
    }
    return 0;
}

/* Simple printf implementation */
int sf_printf(const char *fmt, ...) {
    /* Very basic implementation for now */
    SF_UNUSED(fmt);
    return 0;
}

int sf_sprintf(char *str, size_t size, const char *fmt, ...) {
    /* Very basic implementation */
    SF_UNUSED(str);
    SF_UNUSED(size);
    SF_UNUSED(fmt);
    return 0;
}

int sf_fprintf(void *stream, const char *fmt, ...) {
    SF_UNUSED(stream);
    SF_UNUSED(fmt);
    return 0;
}

int sf_vfprintf(void *stream, const char *fmt, va_list args) {
    SF_UNUSED(stream);
    SF_UNUSED(fmt);
    SF_UNUSED(args);
    return 0;
}

/* Standard streams - placeholders */
void *sf_stdin = (void *)1;
void *sf_stdout = (void *)2;
void *sf_stderr = (void *)3;

#endif /* STARFORTH_MINIMAL */
int sf_printf(const char *fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    int result = sf_vprintf_impl(NULL, 0, fmt, args);
    __builtin_va_end(args);
    return result;
}

static int sf_vprintf_impl(char *buffer, size_t bufsize, const char *fmt, __builtin_va_list args) {
    char *buf_ptr = buffer;
    char *buf_end = buffer ? buffer + bufsize - 1 : NULL;
    const char *p = fmt;
    int count = 0;

    while (*p && (!buffer || buf_ptr < buf_end)) {
        if (*p != '%') {
            if (buffer) {
                *buf_ptr++ = *p;
            } else {
                sf_putchar(*p);
            }
            count++;
            p++;
            continue;
        }

        p++; /* Skip '%' */

        /* Parse width */
        int width = 0;
        while (sf_isdigit(*p)) {
            width = width * 10 + (*p - '0');
            p++;
        }

        switch (*p) {
            case 'd': {
                int num = __builtin_va_arg(args, int);
                char temp[32];
                int i = 0;
                int negative = 0;

                if (num < 0) {
                    negative = 1;
                    num = -num;
                }

                if (num == 0) {
                    temp[i++] = '0';
                } else {
                    while (num > 0) {
                        temp[i++] = '0' + (num % 10);
                        num /= 10;
                    }
                }

                if (negative) temp[i++] = '-';

                /* Reverse and output */
                while (i > 0 && (!buffer || buf_ptr < buf_end)) {
                    char c = temp[--i];
                    if (buffer) {
                        *buf_ptr++ = c;
                    } else {
                        sf_putchar(c);
                    }
                    count++;
                }
                break;
            }
            case 'l':
                if (p[1] == 'd') {
                    long num = __builtin_va_arg(args, long);
                    char temp[64];
                    int i = 0;
                    int negative = 0;

                    if (num < 0) {
                        negative = 1;
                        num = -num;
                    }

                    if (num == 0) {
                        temp[i++] = '0';
                    } else {
                        while (num > 0) {
                            temp[i++] = '0' + (num % 10);
                            num /= 10;
                        }
                    }

                    if (negative) temp[i++] = '-';

                    /* Reverse and output */
                    while (i > 0 && (!buffer || buf_ptr < buf_end)) {
                        char c = temp[--i];
                        if (buffer) {
                            *buf_ptr++ = c;
                        } else {
                            sf_putchar(c);
                        }
                        count++;
                    }
                    p++; /* Skip extra 'd' */
                }
                break;
            case 'x': {
                unsigned int num = __builtin_va_arg(args, unsigned int);
                char temp[32];
                int i = 0;
                const char *digits = "0123456789abcdef";

                if (num == 0) {
                    temp[i++] = '0';
                } else {
                    while (num > 0) {
                        temp[i++] = digits[num % 16];
                        num /= 16;
                    }
                }

                /* Reverse and output */
                while (i > 0 && (!buffer || buf_ptr < buf_end)) {
                    char c = temp[--i];
                    if (buffer) {
                        *buf_ptr++ = c;
                    } else {
                        sf_putchar(c);
                    }
                    count++;
                }
                break;
            }
            case 'c': {
                int c = __builtin_va_arg(args, int);
                if (buffer) {
                    *buf_ptr++ = (char)c;
                } else {
                    sf_putchar(c);
                }
                count++;
                break;
            }
            case 's': {
                const char *s = __builtin_va_arg(args, const char *);
                if (s) {
                    while (*s && (!buffer || buf_ptr < buf_end)) {
                        if (buffer) {
                            *buf_ptr++ = *s;
                        } else {
                            sf_putchar(*s);
                        }
                        s++;
                        count++;
                    }
                }
                break;
            }
            case '%':
                if (buffer) {
                    *buf_ptr++ = '%';
                } else {
                    sf_putchar('%');
                }
                count++;
                break;
            default:
                if (buffer) {
                    *buf_ptr++ = *p;
                } else {
                    sf_putchar(*p);
                }
                count++;
                break;
        }
        p++;
    }

    if (buffer && buf_ptr < buffer + bufsize) {
        *buf_ptr = '\0';
    }

    return count;
}

int sf_sprintf(char *str, size_t size, const char *fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    int result = sf_vprintf_impl(str, size, fmt, args);
    __builtin_va_end(args);
    return result;
}

int sf_vfprintf(void *stream, const char *fmt, __builtin_va_list args) {
    SF_UNUSED(stream);
    /* For minimal implementation, just redirect to vprintf */
    return sf_vprintf_impl(NULL, 0, fmt, args);
}

int sf_fprintf(void *stream, const char *fmt, ...) {
    SF_UNUSED(stream);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    int result = sf_vprintf_impl(NULL, 0, fmt, args);

    __builtin_va_end(args);
    return result;
}

/* Standard streams - these are just placeholders */
void *sf_stdin = (void *)1;
void *sf_stdout = (void *)2;
void *sf_stderr = (void *)3;

#endif /* STARFORTH_MINIMAL */
