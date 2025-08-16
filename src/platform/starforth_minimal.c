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

#include "../../include/platform/starforth_platform.h"
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <limits.h>

/* ========================================================================
 * MEMORY MANAGEMENT
 * ======================================================================== */

/* Static memory pool for minimal allocator */
#define MEMORY_POOL_SIZE (4 * 1024 * 1024) /* 4MB static pool */
static unsigned char memory_pool[MEMORY_POOL_SIZE];
static size_t memory_pool_used = 0;

/* Memory management functions */
void sf_init_allocator(void *pool, size_t pool_size) {
    if (pool != NULL && pool_size > 0) {
        /* External pool provided - implementation would use this instead */
        /* For simplicity, we'll stick with the static pool */
    }
    memory_pool_used = 0;
}

void *sf_malloc(size_t size) {
    if (size == 0) return NULL;

    /* Align to 8-byte boundary */
    size = (size + 7) & ~7;

    if (memory_pool_used + size > MEMORY_POOL_SIZE) {
        /* Out of memory */
        return NULL;
    }

    void *ptr = &memory_pool[memory_pool_used];
    memory_pool_used += size;

    return ptr;
}

void *sf_calloc(size_t nmemb, size_t size) {
    size_t total_size = nmemb * size;
    /* Check for overflow */
    if (size != 0 && total_size / size != nmemb) return NULL;

    void *ptr = sf_malloc(total_size);
    if (ptr != NULL) {
        /* Zero the allocated memory */
        sf_memset(ptr, 0, total_size);
    }

    return ptr;
}

void *sf_realloc(void *ptr, size_t size) {
    if (ptr == NULL) return sf_malloc(size);
    if (size == 0) {
        sf_free(ptr);
        return NULL;
    }

    /* Since we're using a simple bump allocator, realloc can only work 
       if the block is at the end of the used portion of the pool */
    void *end_ptr = &memory_pool[memory_pool_used];

    /* Check if this is the last allocated block */
    if ((unsigned char *) ptr + (size_t) ((unsigned char *) end_ptr - (unsigned char *) ptr) == end_ptr) {
        /* This is the last block, we can resize it */
        size_t current_size = (unsigned char *) end_ptr - (unsigned char *) ptr;

        if (size <= current_size) {
            /* Shrinking the block */
            memory_pool_used -= (current_size - size);
            return ptr;
        } else {
            /* Growing the block */
            size_t additional = size - current_size;
            if (memory_pool_used + additional <= MEMORY_POOL_SIZE) {
                memory_pool_used += additional;
                return ptr;
            }
        }
    }

    /* We need to allocate a new block and copy the data */
    void *new_ptr = sf_malloc(size);
    if (new_ptr == NULL) return NULL;

    /* We don't know the original size, so just estimate it based on pointer difference */
    size_t est_size = (unsigned char *) ptr >= memory_pool &&
                      (unsigned char *) ptr < memory_pool + memory_pool_used
                          ? (size_t) ((unsigned char *) end_ptr - (unsigned char *) ptr)
                          : 0;

    if (est_size > 0) {
        size_t copy_size = est_size < size ? est_size : size;
        sf_memcpy(new_ptr, ptr, copy_size);
    }

    /* In a real implementation, we would free the old block */
    /* In this simple allocator, we don't free memory */

    return new_ptr;
}

void sf_free(void *ptr) {
    /* No-op in this simple allocator */
    (void) ptr;
}

void sf_exit(int status) {
    /* In a minimal implementation, we can just enter an infinite loop */
    /* This would be replaced with platform-specific exit mechanism */
    while (1) {
    }
}

void sf_abort(void) {
    /* In a minimal implementation, we can just enter an infinite loop */
    /* This would be replaced with platform-specific abort mechanism */
    sf_exit(1);
}

int sf_atexit(void (*func)(void)) {
    /* In minimal mode, we don't support atexit */
    (void) func;
    return 0;
}

char *sf_getenv(const char *name) {
    /* In minimal mode, environment variables aren't supported */
    (void) name;
    return NULL;
}

int sf_system(const char *command) {
    /* In minimal mode, system() isn't supported */
    (void) command;
    return -1;
}

/* ========================================================================
 * STRING FUNCTIONS
 * ======================================================================== */

size_t sf_strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return p - s;
}

int sf_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *) s1 - *(const unsigned char *) s2;
}

int sf_strncmp(const char *s1, const char *s2, size_t n) {
    if (n == 0) return 0;

    while (n-- > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }

    if (n == (size_t) -1) return 0;
    return *(const unsigned char *) s1 - *(const unsigned char *) s2;
}

char *sf_strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *sf_strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    size_t i;

    for (i = 0; i < n && src[i]; i++) {
        dest[i] = src[i];
    }

    for (; i < n; i++) {
        dest[i] = '\0';
    }

    return dest;
}

char *sf_strcat(char *dest, const char *src) {
    char *d = dest;
    /* Find the end of dest */
    while (*d) d++;
    /* Append src to dest */
    while ((*d++ = *src++));
    return dest;
}

char *sf_strncat(char *dest, const char *src, size_t n) {
    char *d = dest;
    /* Find the end of dest */
    while (*d) d++;
    /* Append at most n characters from src to dest */
    size_t i;
    for (i = 0; i < n && src[i]; i++) {
        d[i] = src[i];
    }
    d[i] = '\0';
    return dest;
}

char *sf_strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char) c) return (char *) s;
        s++;
    }
    if ((char) c == '\0') return (char *) s;
    return NULL;
}

char *sf_strrchr(const char *s, int c) {
    const char *found = NULL;
    while (*s) {
        if (*s == (char) c) found = s;
        s++;
    }
    if ((char) c == '\0') return (char *) s;
    return (char *) found;
}

char *sf_strstr(const char *haystack, const char *needle) {
    size_t needle_len = sf_strlen(needle);
    if (needle_len == 0) return (char *) haystack;

    while (*haystack) {
        if (*haystack == *needle && sf_strncmp(haystack, needle, needle_len) == 0) {
            return (char *) haystack;
        }
        haystack++;
    }

    return NULL;
}

char *sf_strtok(char *str, const char *delim) {
    static char *last = NULL;

    if (str == NULL) {
        str = last;
        if (str == NULL) return NULL;
    }

    /* Skip leading delimiters */
    str += sf_strspn(str, delim);
    if (*str == '\0') {
        last = NULL;
        return NULL;
    }

    /* Find the end of the token */
    char *end = str + sf_strcspn(str, delim);
    if (*end == '\0') {
        last = NULL;
    } else {
        *end = '\0';
        last = end + 1;
    }

    return str;
}

size_t sf_strspn(const char *s, const char *accept) {
    const char *p = s;

    while (*p) {
        const char *a = accept;
        int found = 0;

        while (*a) {
            if (*p == *a) {
                found = 1;
                break;
            }
            a++;
        }

        if (!found) break;
        p++;
    }

    return p - s;
}

size_t sf_strcspn(const char *s, const char *reject) {
    const char *p = s;

    while (*p) {
        const char *r = reject;

        while (*r) {
            if (*p == *r) {
                return p - s;
            }
            r++;
        }

        p++;
    }

    return p - s;
}

char *sf_strpbrk(const char *s, const char *accept) {
    while (*s) {
        const char *a = accept;

        while (*a) {
            if (*s == *a) {
                return (char *) s;
            }
            a++;
        }

        s++;
    }

    return NULL;
}

int sf_strcoll(const char *s1, const char *s2) {
    /* In minimal mode, just use strcmp */
    return sf_strcmp(s1, s2);
}

size_t sf_strxfrm(char *dest, const char *src, size_t n) {
    /* In minimal mode, just use strncpy and return length */
    size_t len = sf_strlen(src);
    if (n > 0) {
        size_t copy_len = len < n ? len : n - 1;
        sf_memcpy(dest, src, copy_len);
        dest[copy_len] = '\0';
    }
    return len;
}

char *sf_strerror(int errnum) {
    /* In minimal mode, just return a generic error message */
    static char error_buffer[32];
    sf_sprintf(error_buffer, sizeof(error_buffer), "Error %d", errnum);
    return error_buffer;
}

void *sf_memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *) s;

    while (n--) {
        *p++ = (unsigned char) c;
    }

    return s;
}

void *sf_memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *) dest;
    const unsigned char *s = (const unsigned char *) src;

    while (n--) {
        *d++ = *s++;
    }

    return dest;
}

void *sf_memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *) dest;
    const unsigned char *s = (const unsigned char *) src;

    if (d < s) {
        /* Copy forwards */
        while (n--) {
            *d++ = *s++;
        }
    } else if (d > s) {
        /* Copy backwards */
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }

    return dest;
}

int sf_memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *) s1;
    const unsigned char *p2 = (const unsigned char *) s2;

    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }

    return 0;
}

void *sf_memchr(const void *s, int c, size_t n) {
    const unsigned char *p = (const unsigned char *) s;

    while (n--) {
        if (*p == (unsigned char) c) {
            return (void *) p;
        }
        p++;
    }

    return NULL;
}

/* ========================================================================
 * CHARACTER CLASSIFICATION
 * ======================================================================== */

int sf_isalnum(int c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9');
}

int sf_isalpha(int c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z');
}

int sf_iscntrl(int c) {
    return (c >= 0 && c <= 31) || c == 127;
}

int sf_isdigit(int c) {
    return c >= '0' && c <= '9';
}

int sf_isgraph(int c) {
    return c >= 33 && c <= 126;
}

int sf_islower(int c) {
    return c >= 'a' && c <= 'z';
}

int sf_isprint(int c) {
    return c >= 32 && c <= 126;
}

int sf_ispunct(int c) {
    return (c >= 33 && c <= 126) && !sf_isalnum(c);
}

int sf_isspace(int c) {
    return c == ' ' || c == '\f' || c == '\n' ||
           c == '\r' || c == '\t' || c == '\v';
}

int sf_isupper(int c) {
    return c >= 'A' && c <= 'Z';
}

int sf_isxdigit(int c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

int sf_tolower(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

int sf_toupper(int c) {
    if (c >= 'a' && c <= 'z') {
        return c - ('a' - 'A');
    }
    return c;
}

/* ========================================================================
 * STDIO EMULATION
 * ======================================================================== */

/* Stream buffers for minimal I/O */
static char stdin_buffer[1024];
static char stdout_buffer[4096];
static char stderr_buffer[4096];
static size_t stdin_pos = 0;
static size_t stdin_size = 0;
static size_t stdout_pos = 0;
static size_t stderr_pos = 0;

/* Stream pointers */
void *sf_stdin = &stdin_buffer;
void *sf_stdout = &stdout_buffer;
void *sf_stderr = &stderr_buffer;

/* Basic I/O functions */
int sf_putchar(int c) {
    /* Output to stdout buffer */
    if (stdout_pos < sizeof(stdout_buffer) - 1) {
        stdout_buffer[stdout_pos++] = (char) c;
        stdout_buffer[stdout_pos] = '\0';
    }

/* Echo to actual stdout if available in debug builds */
#if !defined(NDEBUG) && (defined(__unix__) || defined(__APPLE__) || defined(_WIN32))
#include <stdio.h>
putchar (c);
#endif

return
c;
}

int sf_puts(const char *s) {
    int count = 0;
    while (*s) {
        sf_putchar(*s++);
        count++;
    }
    sf_putchar('\n');
    return count;
}

int sf_fputs(const char *s, void *stream) {
    if (stream == sf_stdout) {
        int count = 0;
        while (*s) {
            sf_putchar(*s++);
            count++;
        }
        return count;
    } else if (stream == sf_stderr) {
        int count = 0;
        while (*s && stderr_pos < sizeof(stderr_buffer) - 1) {
            stderr_buffer[stderr_pos++] = *s++;
            stderr_buffer[stderr_pos] = '\0';
            count++;
        }

/* Echo to actual stderr if available in debug builds */
#if !defined(NDEBUG) && (defined(__unix__) || defined(__APPLE__) || defined(_WIN32))
#include <stdio.h>
fputs (s
-
count
,
stderr
);
#endif

return
count;
}
return
-
1;
}

void sf_fflush(void *stream) {



/* In minimal mode, this is a no-op */
/* In a real implementation, this would flush the buffer to the actual output */
#if !defined(NDEBUG) && (defined(__unix__) || defined(__APPLE__) || defined(_WIN32))
#include <stdio.h>
if
(stream
==
sf_stdout
)
fflush (stdout);
else
if
(stream
==
sf_stderr
)
fflush (stderr);
#endif
}

/* Format and print functions */
int sf_vfprintf(void *stream, const char *fmt, va_list args) {
    /* Simple implementation of printf that handles basic format specifiers */
    int count = 0;

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;

            /* Check for format flags */
            int pad_zero = 0;
            int left_align = 0;
            int width = 0;
            int precision = -1;

            /* Parse flags */
            while (*fmt == '-' || *fmt == '0') {
                if (*fmt == '-') left_align = 1;
                else if (*fmt == '0') pad_zero = 1;
                fmt++;
            }

            /* Parse width */
            if (*fmt == '*') {
                width = va_arg(args, int);
                fmt++;
            } else {
                while (*fmt >= '0' && *fmt <= '9') {
                    width = width * 10 + (*fmt - '0');
                    fmt++;
                }
            }

            /* Parse precision */
            if (*fmt == '.') {
                fmt++;
                precision = 0;
                if (*fmt == '*') {
                    precision = va_arg(args, int);
                    fmt++;
                } else {
                    while (*fmt >= '0' && *fmt <= '9') {
                        precision = precision * 10 + (*fmt - '0');
                        fmt++;
                    }
                }
            }

            /* Handle format specifiers */
            switch (*fmt) {
                case 'd':
                case 'i': {
                    int value = va_arg(args, int);
                    char buffer[32];
                    char *p = buffer + sizeof(buffer) - 1;
                    *p = '\0';

                    if (value < 0) {
                        /* Handle negative numbers */
                        int abs_value = value == INT_MIN ? INT_MAX + 1 : -value;

                        do {
                            *--p = '0' + (abs_value % 10);
                            abs_value /= 10;
                        } while (abs_value > 0);

                        *--p = '-';
                    } else if (value == 0) {
                        *--p = '0';
                    } else {
                        /* Handle positive numbers */
                        do {
                            *--p = '0' + (value % 10);
                            value /= 10;
                        } while (value > 0);
                    }

                    int len = buffer + sizeof(buffer) - 1 - p;

                    /* Pad to width if necessary */
                    if (width > len) {
                        int pad_len = width - len;
                        if (left_align) {
                            /* Output number then padding */
                            sf_fputs(p, stream);
                            count += len;
                            while (pad_len--) {
                                sf_putchar(' ');
                                count++;
                            }
                        } else {
                            /* Output padding then number */
                            char pad_char = pad_zero ? '0' : ' ';
                            while (pad_len--) {
                                sf_putchar(pad_char);
                                count++;
                            }
                            sf_fputs(p, stream);
                            count += len;
                        }
                    } else {
                        /* No padding needed */
                        sf_fputs(p, stream);
                        count += len;
                    }
                    break;
                }

                case 'u': {
                    unsigned int value = va_arg(args, unsigned int);
                    char buffer[32];
                    char *p = buffer + sizeof(buffer) - 1;
                    *p = '\0';

                    if (value == 0) {
                        *--p = '0';
                    } else {
                        do {
                            *--p = '0' + (value % 10);
                            value /= 10;
                        } while (value > 0);
                    }

                    int len = buffer + sizeof(buffer) - 1 - p;

                    /* Pad to width if necessary */
                    if (width > len) {
                        int pad_len = width - len;
                        if (left_align) {
                            sf_fputs(p, stream);
                            count += len;
                            while (pad_len--) {
                                sf_putchar(' ');
                                count++;
                            }
                        } else {
                            char pad_char = pad_zero ? '0' : ' ';
                            while (pad_len--) {
                                sf_putchar(pad_char);
                                count++;
                            }
                            sf_fputs(p, stream);
                            count += len;
                        }
                    } else {
                        sf_fputs(p, stream);
                        count += len;
                    }
                    break;
                }

                case 'x':
                case 'X': {
                    unsigned int value = va_arg(args, unsigned int);
                    char buffer[32];
                    char *p = buffer + sizeof(buffer) - 1;
                    *p = '\0';

                    const char *digits = (*fmt == 'x') ? "0123456789abcdef" : "0123456789ABCDEF";

                    if (value == 0) {
                        *--p = '0';
                    } else {
                        do {
                            *--p = digits[value & 0xF];
                            value >>= 4;
                        } while (value > 0);
                    }

                    int len = buffer + sizeof(buffer) - 1 - p;

                    /* Pad to width if necessary */
                    if (width > len) {
                        int pad_len = width - len;
                        if (left_align) {
                            sf_fputs(p, stream);
                            count += len;
                            while (pad_len--) {
                                sf_putchar(' ');
                                count++;
                            }
                        } else {
                            char pad_char = pad_zero ? '0' : ' ';
                            while (pad_len--) {
                                sf_putchar(pad_char);
                                count++;
                            }
                            sf_fputs(p, stream);
                            count += len;
                        }
                    } else {
                        sf_fputs(p, stream);
                        count += len;
                    }
                    break;
                }

                case 'c': {
                    int c = va_arg(args, int);
                    /* Pad to width if necessary */
                    if (width > 1) {
                        int pad_len = width - 1;
                        if (left_align) {
                            sf_putchar(c);
                            count++;
                            while (pad_len--) {
                                sf_putchar(' ');
                                count++;
                            }
                        } else {
                            while (pad_len--) {
                                sf_putchar(' ');
                                count++;
                            }
                            sf_putchar(c);
                            count++;
                        }
                    } else {
                        sf_putchar(c);
                        count++;
                    }
                    break;
                }

                case 's': {
                    const char *s = va_arg(args, const char *);
                    if (s == NULL) s = "(null)";

                    /* Calculate length, respecting precision if specified */
                    int len = 0;
                    const char *p = s;
                    while (*p) {
                        if (precision >= 0 && len >= precision) break;
                        len++;
                        p++;
                    }

                    /* Pad to width if necessary */
                    if (width > len) {
                        int pad_len = width - len;
                        if (left_align) {
                            /* Output string then padding */
                            for (int i = 0; i < len; i++) {
                                sf_putchar(s[i]);
                                count++;
                            }
                            while (pad_len--) {
                                sf_putchar(' ');
                                count++;
                            }
                        } else {
                            /* Output padding then string */
                            while (pad_len--) {
                                sf_putchar(' ');
                                count++;
                            }
                            for (int i = 0; i < len; i++) {
                                sf_putchar(s[i]);
                                count++;
                            }
                        }
                    } else {
                        /* No padding needed */
                        for (int i = 0; i < len; i++) {
                            sf_putchar(s[i]);
                            count++;
                        }
                    }
                    break;
                }

                case 'p': {
                    void *ptr = va_arg(args, void *);
                    uintptr_t value = (uintptr_t) ptr;

                    /* Output "0x" prefix */
                    sf_putchar('0');
                    sf_putchar('x');
                    count += 2;

                    /* Convert pointer to hex */
                    char buffer[32];
                    char *p = buffer + sizeof(buffer) - 1;
                    *p = '\0';

                    if (value == 0) {
                        *--p = '0';
                    } else {
                        do {
                            *--p = "0123456789abcdef"[value & 0xF];
                            value >>= 4;
                        } while (value > 0);
                    }

                    sf_fputs(p, stream);
                    count += buffer + sizeof(buffer) - 1 - p;
                    break;
                }

                case '%':
                    sf_putchar('%');
                    count++;
                    break;

                default:
                    /* Unknown format specifier, output as-is */
                    sf_putchar('%');
                    sf_putchar(*fmt);
                    count += 2;
                    break;
            }
        } else {
            sf_putchar(*fmt);
            count++;
        }

        fmt++;
    }

    return count;
}

int sf_fprintf(void *stream, const char *fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = sf_vfprintf(stream, fmt, args);
    va_end(args);

    return ret;
}

int sf_printf(const char *fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = sf_vfprintf(sf_stdout, fmt, args);
    va_end(args);

    return ret;
}

int sf_vsprintf(char *str, size_t size, const char *fmt, va_list args) {
    /* Create a temporary stream that writes to the string */
    struct {
        char *buffer;
        size_t size;
        size_t pos;
    } string_stream = {str, size, 0};

    /* Implement a custom putchar for string streams */
    int (*old_putchar)(int) = sf_putchar;
    sf_putchar = (int (*)(int)) &string_stream;

    /* Call vfprintf with the custom stream */
    int ret = sf_vfprintf(&string_stream, fmt, args);

    /* Restore the original putchar */
    sf_putchar = old_putchar;

    /* Null-terminate the string */
    if (string_stream.pos < string_stream.size) {
        string_stream.buffer[string_stream.pos] = '\0';
    } else if (string_stream.size > 0) {
        string_stream.buffer[string_stream.size - 1] = '\0';
    }

    return ret;
}

int sf_sprintf(char *str, size_t size, const char *fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = sf_vsprintf(str, size, fmt, args);
    va_end(args);

    return ret;
}

int sf_sscanf(const char *str, const char *format, ...) {
    /* In minimal mode, implement a very basic sscanf for essential needs */
    /* This is a simplified implementation that only handles a few format specifiers */
    va_list args;
    va_start(args, format);

    int count = 0;
    const char *s = str;

    while (*format && *s) {
        if (*format == '%') {
            format++;

            /* Skip width specifier */
            while (*format >= '0' && *format <= '9') {
                format++;
            }

            switch (*format) {
                case 'd': {
                    int *p = va_arg(args, int *);
                    int value = 0;
                    int neg = 0;

                    /* Skip whitespace */
                    while (*s && sf_isspace(*s)) s++;

                    /* Handle sign */
                    if (*s == '-') {
                        neg = 1;
                        s++;
                    } else if (*s == '+') {
                        s++;
                    }

                    /* Convert digits */
                    if (!sf_isdigit(*s)) {
                        /* No valid digits found */
                        va_end(args);
                        return count;
                    }

                    while (*s && sf_isdigit(*s)) {
                        value = value * 10 + (*s - '0');
                        s++;
                    }

                    *p = neg ? -value : value;
                    count++;
                    break;
                }

                case 's': {
                    char *p = va_arg(args, char *);

                    /* Skip whitespace */
                    while (*s && sf_isspace(*s)) s++;

                    /* Copy non-whitespace characters */
                    while (*s && !sf_isspace(*s)) {
                        *p++ = *s++;
                    }

                    *p = '\0';
                    count++;
                    break;
                }

                case 'c': {
                    char *p = va_arg(args, char *);
                    *p = *s++;
                    count++;
                    break;
                }

                case '%':
                    if (*s == '%') {
                        s++;
                    } else {
                        /* Expected % but found something else */
                        va_end(args);
                        return count;
                    }
                    break;

                default:
                    /* Unsupported format specifier */
                    va_end(args);
                    return count;
            }
        } else if (sf_isspace(*format)) {
            /* Skip whitespace in both format and input */
            while (*format && sf_isspace(*format)) format++;
            while (*s && sf_isspace(*s)) s++;
        } else {
            /* Match literal character */
            if (*format != *s) {
                /* Mismatch */
                break;
            }
            format++;
            s++;
        }
    }

    va_end(args);
    return count;
}

char *sf_fgets(char *s, int size, void *stream) {
    if (stream == sf_stdin) {
        /* In minimal mode, provide some test input */
        static const char *test_input = "test input\n";
        static int pos = 0;

        if (test_input[pos] == '\0') {
            /* End of test input */
            return NULL;
        }

        int i;
        for (i = 0; i < size - 1 && test_input[pos]; i++, pos++) {
            s[i] = test_input[pos];
            if (test_input[pos] == '\n') {
                i++;
                pos++;
                break;
            }
        }

        s[i] = '\0';
        return s;
    }

    return NULL;
}

int sf_setvbuf(void *stream, char *buf, int mode, size_t size) {
    /* In minimal mode, this is a no-op */
    return 0;
}

int sf_fclose(void *stream) {
    /* In minimal mode, this is a no-op */
    return 0;
}

void *sf_fopen(const char *filename, const char *mode) {
    /* In minimal mode, file operations aren't supported */
    return NULL;
}

size_t sf_fread(void *ptr, size_t size, size_t nmemb, void *stream) {
    /* In minimal mode, file operations aren't supported */
    return 0;
}

size_t sf_fwrite(const void *ptr, size_t size, size_t nmemb, void *stream) {
    /* In minimal mode, file operations aren't directly supported */
    /* For stdout/stderr, we'll write to the buffer */
    if (stream == sf_stdout) {
        const unsigned char *src = (const unsigned char *) ptr;
        size_t total = size * nmemb;
        size_t written = 0;

        while (written < total && stdout_pos < sizeof(stdout_buffer) - 1) {
            stdout_buffer[stdout_pos++] = src[written++];
        }

        stdout_buffer[stdout_pos] = '\0';
        return written / size;
    } else if (stream == sf_stderr) {
        const unsigned char *src = (const unsigned char *) ptr;
        size_t total = size * nmemb;
        size_t written = 0;

        while (written < total && stderr_pos < sizeof(stderr_buffer) - 1) {
            stderr_buffer[stderr_pos++] = src[written++];
        }

        stderr_buffer[stderr_pos] = '\0';
        return written / size;
    }

    return 0;
}

int sf_fseek(void *stream, long offset, int whence) {
    /* In minimal mode, file operations aren't supported */
    return -1;
}

long sf_ftell(void *stream) {
    /* In minimal mode, file operations aren't supported */
    return -1;
}

int sf_feof(void *stream) {
    /* In minimal mode, file operations aren't fully supported */
    if (stream == sf_stdin) {
        /* End of input when we've consumed all test input */
        return stdin_pos >= stdin_size;
    }
    return 1;
}

void sf_clearerr(void *stream) {
    /* In minimal mode, this is a no-op */
}

int sf_ferror(void *stream) {
    /* In minimal mode, always report no error */
    return 0;
}

int sf_fileno(void *stream) {
    /* In minimal mode, file operations aren't supported */
    if (stream == sf_stdin) return 0;
    if (stream == sf_stdout) return 1;
    if (stream == sf_stderr) return 2;
    return -1;
}

int sf_ungetc(int c, void *stream) {
    /* In minimal mode, this is mostly a no-op */
    return -1;
}

int sf_fgetc(void *stream) {
    /* In minimal mode, this is mostly a no-op */
    if (stream == sf_stdin && stdin_pos < stdin_size) {
        return stdin_buffer[stdin_pos++];
    }
    return -1;
}

int sf_fputc(int c, void *stream) {
    if (stream == sf_stdout) {
        return sf_putchar(c);
    } else if (stream == sf_stderr && stderr_pos < sizeof(stderr_buffer) - 1) {
        stderr_buffer[stderr_pos++] = (char) c;
        stderr_buffer[stderr_pos] = '\0';
        return c;
    }
    return -1;
}

int sf_remove(const char *pathname) {
    /* In minimal mode, file operations aren't supported */
    return -1;
}

int sf_rename(const char *oldpath, const char *newpath) {
    /* In minimal mode, file operations aren't supported */
    return -1;
}

/* ========================================================================
 * TIME FUNCTIONS
 * ======================================================================== */

static sf_time_t current_time = 0;
static sf_tm current_tm = {0, 0, 0, 1, 0, 125, 0, 0, 0}; /* Default to 2025-01-01 00:00:00 */

sf_clock_t sf_clock(void) {
    /* In minimal mode, return a simple incrementing counter */
    static sf_clock_t clock_value = 0;
    return clock_value++;
}

sf_time_t sf_time(sf_time_t *timer) {
    /* In minimal mode, return a simple value */
    if (timer) {
        *timer = current_time;
    }
    return current_time;
}

double sf_difftime(sf_time_t end, sf_time_t beginning) {
    /* Simple time difference */
    return (double) (end - beginning);
}

sf_tm *sf_localtime(const sf_time_t *timer) {
    /* In minimal mode, return a static tm structure */
    return &current_tm;
}

sf_tm *sf_gmtime(const sf_time_t *timer) {
    /* In minimal mode, identical to localtime */
    return &current_tm;
}

sf_time_t sf_mktime(sf_tm *timeptr) {
    /* In minimal mode, this is a simplified implementation */
    /* Just return the current time value */
    return current_time;
}

size_t sf_strftime(char *s, size_t max, const char *format, const sf_tm *tm) {
    /* Simplified implementation that only handles a few format specifiers */
    size_t count = 0;

    while (*format && count < max - 1) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'Y': {
                    /* Year (4 digits) */
                    int year = 1900 + tm->tm_year;
                    char buffer[8];
                    sf_sprintf(buffer, sizeof(buffer), "%04d", year);
                    for (int i = 0; buffer[i] && count < max - 1; i++) {
                        s[count++] = buffer[i];
                    }
                    break;
                }
                case 'm': {
                    /* Month (01-12) */
                    char buffer[4];
                    sf_sprintf(buffer, sizeof(buffer), "%02d", tm->tm_mon + 1);
                    for (int i = 0; buffer[i] && count < max - 1; i++) {
                        s[count++] = buffer[i];
                    }
                    break;
                }
                case 'd': {
                    /* Day of month (01-31) */
                    char buffer[4];
                    sf_sprintf(buffer, sizeof(buffer), "%02d", tm->tm_mday);
                    for (int i = 0; buffer[i] && count < max - 1; i++) {
                        s[count++] = buffer[i];
                    }
                    break;
                }
                case 'H': {
                    /* Hour (00-23) */
                    char buffer[4];
                    sf_sprintf(buffer, sizeof(buffer), "%02d", tm->tm_hour);
                    for (int i = 0; buffer[i] && count < max - 1; i++) {
                        s[count++] = buffer[i];
                    }
                    break;
                }
                case 'M': {
                    /* Minute (00-59) */
                    char buffer[4];
                    sf_sprintf(buffer, sizeof(buffer), "%02d", tm->tm_min);
                    for (int i = 0; buffer[i] && count < max - 1; i++) {
                        s[count++] = buffer[i];
                    }
                    break;
                }
                case 'S': {
                    /* Second (00-59) */
                    char buffer[4];
                    sf_sprintf(buffer, sizeof(buffer), "%02d", tm->tm_sec);
                    for (int i = 0; buffer[i] && count < max - 1; i++) {
                        s[count++] = buffer[i];
                    }
                    break;
                }
                case '%':
                    /* Literal % */
                    s[count++] = '%';
                    break;
                default:
                    /* Unknown format specifier, output as-is */
                    s[count++] = '%';
                    if (count < max - 1) {
                        s[count++] = *format;
                    }
                    break;
            }
        } else {
            s[count++] = *format;
        }

        format++;
    }

    s[count] = '\0';
    return count;
}

void sf_sleep(unsigned seconds) {
    /* In minimal mode, this is a simplified busy-wait */
    sf_clock_t start = sf_clock();
    while (sf_clock() - start < seconds * SF_CLOCKS_PER_SEC / 1000) {
        /* Busy wait */
    }
}

/* ========================================================================
 * MATH FUNCTIONS
 * ======================================================================== */

int sf_abs(int x) {
    return x < 0 ? -x : x;
}

long sf_labs(long x) {
    return x < 0 ? -x : x;
}

double sf_fabs(double x) {
    return x < 0 ? -x : x;
}

double sf_ceil(double x) {
    double i;
    double f = x - (long) x;
    if (x >= 0 && f > 0) {
        i = (long) x + 1;
    } else {
        i = (long) x;
    }
    return i;
}

double sf_floor(double x) {
    double i;
    double f = x - (long) x;
    if (x < 0 && f < 0) {
        i = (long) x - 1;
    } else {
        i = (long) x;
    }
    return i;
}

double sf_fmod(double x, double y) {
    if (y == 0.0) return 0.0;
    double quot = x / y;
    double intpart = quot < 0 ? sf_ceil(quot) : sf_floor(quot);
    return x - intpart * y;
}

double sf_exp(double x) {
    /* Simplified exponential function */
    if (x == 0.0) return 1.0;
    if (x < -20.0) return 0.0;
    if (x > 20.0) return 1.0e20; /* Approximate infinity */

    double result = 1.0;
    double term = 1.0;
    double n = 1.0;

    /* Taylor series approximation */
    for (int i = 1; i <= 20; i++) {
        term *= x / i;
        result += term;
        if (term < 1e-10) break;
    }

    return result;
}

double sf_log(double x) {
    /* Simplified natural logarithm */
    if (x <= 0.0) return -1.0; /* Error case */

    /* Approximate using a simple algorithm */
    double y = (x - 1) / (x + 1);
    double y2 = y * y;
    double result = 0;

    /* Series approximation */
    for (int i = 1; i <= 10; i += 2) {
        result += y / i;
        y *= y2;
    }

    return 2.0 * result;
}

double sf_log10(double x) {
    /* log10(x) = log(x) / log(10) */
    return sf_log(x) / 2.302585092994046; /* ln(10) */
}

double sf_pow(double x, double y) {
    /* Special cases */
    if (y == 0.0) return 1.0;
    if (x == 0.0) return 0.0;
    if (x == 1.0) return 1.0;

    /* Integer powers can be computed efficiently */
    if (y == (int) y) {
        double result = 1.0;
        double base = x;
        int exponent = (int) y;

        if (exponent < 0) {
            base = 1.0 / base;
            exponent = -exponent;
        }

        while (exponent > 0) {
            if (exponent & 1) {
                result *= base;
            }
            base *= base;
            exponent >>= 1;
        }

        return result;
    }

    /* For other cases, use exp(y * log(x)) */
    return sf_exp(y * sf_log(x));
}

double sf_sqrt(double x) {
    if (x < 0.0) return 0.0; /* Error case */
    if (x == 0.0) return 0.0;

    /* Newton's method */
    double guess = x / 2.0;
    double prev;

    do {
        prev = guess;
        guess = (guess + x / guess) / 2.0;
    } while (sf_fabs(guess - prev) > 1e-10);

    return guess;
}

double sf_sin(double x) {
    /* Reduce x to the range [0, 2π) */
    double pi = 3.14159265358979323846;
    double pi2 = 2.0 * pi;
    x = sf_fmod(x, pi2);
    if (x < 0) x += pi2;

    /* Taylor series approximation */
    double x2 = x * x;
    double term = x;
    double sum = x;
    double factorial = 1.0;

    for (int i = 1; i <= 10; i++) {
        factorial *= (2 * i) * (2 * i + 1);
        term = -term * x2;
        sum += term / factorial;
    }

    return sum;
}

double sf_cos(double x) {
    /* cos(x) = sin(x + π/2) */
    double pi = 3.14159265358979323846;
    return sf_sin(x + pi / 2.0);
}

double sf_tan(double x) {
    /* tan(x) = sin(x) / cos(x) */
    double c = sf_cos(x);
    if (sf_fabs(c) < 1e-10) {
        /* Near infinity */
        return x < 0 ? -1.0e10 : 1.0e10;
    }
    return sf_sin(x) / c;
}

double sf_asin(double x) {
    /* Simple implementation for minimal mode */
    if (x < -1.0 || x > 1.0) return 0.0; /* Error case */

    /* Approximation using Taylor series */
    double result = x;
    double x2 = x * x;
    double term = x;

    for (int n = 1; n <= 10; n++) {
        term *= x2 * (2 * n - 1) * (2 * n - 1) / (2 * n * (2 * n + 1));
        result += term;
    }

    return result;
}

double sf_acos(double x) {
    /* acos(x) = π/2 - asin(x) */
    double pi = 3.14159265358979323846;
    return pi / 2.0 - sf_asin(x);
}

double sf_atan(double x) {
    /* Simple approximation for minimal mode */
    if (sf_fabs(x) > 1.0) {
        /* atan(x) = π/2 - atan(1/x) for |x| > 1 */
        double pi = 3.14159265358979323846;
        return x > 0 ? pi / 2.0 - sf_atan(1.0 / x) : -pi / 2.0 - sf_atan(1.0 / x);
    }

    /* Taylor series approximation */
    double x2 = x * x;
    double term = x;
    double sum = x;

    for (int n = 1; n <= 10; n++) {
        term = -term * x2;
        sum += term / (2 * n + 1);
    }

    return sum;
}

double sf_atan2(double y, double x) {
    /* Special cases */
    if (x == 0.0) {
        if (y > 0.0) return 1.5707963267948966; /* π/2 */
        if (y < 0.0) return -1.5707963267948966; /* -π/2 */
        return 0.0; /* y = 0, x = 0, undefined, return 0 */
    }

    double pi = 3.14159265358979323846;

    if (x > 0.0) {
        return sf_atan(y / x);
    } else if (x < 0.0) {
        return y >= 0.0 ? sf_atan(y / x) + pi : sf_atan(y / x) - pi;
    }

    return 0.0; /* Shouldn't reach here */
}

double sf_sinh(double x) {
    /* sinh(x) = (e^x - e^-x) / 2 */
    if (sf_fabs(x) > 20.0) {
        /* Avoid overflow */
        return x > 0 ? 1.0e10 : -1.0e10;
    }
    double ex = sf_exp(x);
    double e_minus_x = 1.0 / ex;
    return (ex - e_minus_x) / 2.0;
}

double sf_cosh(double x) {
    /* cosh(x) = (e^x + e^-x) / 2 */
    if (sf_fabs(x) > 20.0) {
        /* Avoid overflow */
        return 1.0e10;
    }
    double ex = sf_exp(x);
    double e_minus_x = 1.0 / ex;
    return (ex + e_minus_x) / 2.0;
}

double sf_tanh(double x) {
    /* tanh(x) = sinh(x) / cosh(x) = (e^x - e^-x) / (e^x + e^-x) */
    if (x > 20.0) return 1.0;
    if (x < -20.0) return -1.0;

    double ex = sf_exp(x);
    double e_minus_x = 1.0 / ex;
    return (ex - e_minus_x) / (ex + e_minus_x);
}

double sf_frexp(double value, int *exp) {
    if (value == 0.0) {
        *exp = 0;
        return 0.0;
    }

    /* Extract sign */
    int sign = 1;
    if (value < 0) {
        sign = -1;
        value = -value;
    }

    /* Find exponent */
    int e = 0;
    if (value >= 1.0) {
        while (value >= 2.0) {
            value /= 2.0;
            e++;
        }
    } else {
        while (value < 0.5) {
            value *= 2.0;
            e--;
        }
    }

    *exp = e;
    return sign * value;
}

double sf_ldexp(double x, int exp) {
    /* x * 2^exp */
    if (x == 0.0) return 0.0;

    while (exp > 0) {
        x *= 2.0;
        exp--;
    }

    while (exp < 0) {
        x /= 2.0;
        exp++;
    }

    return x;
}

double sf_modf(double value, double *iptr) {
    double int_part;

    if (value >= 0.0) {
        int_part = sf_floor(value);
    } else {
        int_part = sf_ceil(value);
    }

    *iptr = int_part;
    return value - int_part;
}

/* ========================================================================
 * SIGNAL HANDLING
 * ======================================================================== */

/* Signal handling variables */
static sf_sighandler_t signal_handlers[32] = {0};

sf_sighandler_t sf_signal(int signum, sf_sighandler_t handler) {
    if (signum < 0 || signum >= 32) {
        return SF_SIG_ERR;
    }

    sf_sighandler_t old = signal_handlers[signum];
    signal_handlers[signum] = handler;
    return old;
}

int sf_raise(int sig) {
    if (sig < 0 || sig >= 32) {
        return -1;
    }

    sf_sighandler_t handler = signal_handlers[sig];
    if (handler == SF_SIG_DFL) {
        /* Default action is to terminate */
        sf_exit(1);
    } else if (handler != SF_SIG_IGN) {
        /* Call the handler */
        handler(sig);
    }

    return 0;
}

void sf_sigemptyset(void *set) {
    /* In minimal mode, use a simple implementation */
    sf_memset(set, 0, sizeof(int));
}

void sf_sigfillset(void *set) {
    /* In minimal mode, use a simple implementation */
    sf_memset(set, 0xFF, sizeof(int));
}

void sf_sigaddset(void *set, int signum) {
    /* In minimal mode, use a simple implementation */
    if (signum >= 0 && signum < 32) {
        *(int *) set |= (1 << signum);
    }
}

void sf_sigdelset(void *set, int signum) {
    /* In minimal mode, use a simple implementation */
    if (signum >= 0 && signum < 32) {
        *(int *) set &= ~(1 << signum);
    }
}

int sf_sigismember(const void *set, int signum) {
    /* In minimal mode, use a simple implementation */
    if (signum < 0 || signum >= 32) {
        return -1;
    }
    return (*(const int *) set & (1 << signum)) ? 1 : 0;
}

int sf_sigaction(int signum, const sf_sigaction_t *act, sf_sigaction_t *oldact) {
    if (signum < 0 || signum >= 32) {
        return -1;
    }

    if (oldact != NULL) {
        oldact->sa_handler = signal_handlers[signum];
        oldact->sa_sigaction = NULL;
        oldact->sa_flags = 0;
    }

    if (act != NULL) {
        signal_handlers[signum] = act->sa_handler;
    }

    return 0;
}

/* ========================================================================
 * NUMERIC CONVERSION
 * ======================================================================== */

long sf_strtol(const char *nptr, char **endptr, int base) {
    if (nptr == NULL) {
        if (endptr) *endptr = (char *) nptr;
        return 0;
    }

    /* Skip leading whitespace */
    while (sf_isspace(*nptr)) nptr++;

    /* Handle sign */
    int neg = 0;
    if (*nptr == '-') {
        neg = 1;
        nptr++;
    } else if (*nptr == '+') {
        nptr++;
    }

    /* Determine base if not specified */
    if (base == 0) {
        if (*nptr == '0') {
            nptr++;
            if (*nptr == 'x' || *nptr == 'X') {
                base = 16;
                nptr++;
            } else {
                base = 8;
                nptr--;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        /* Skip 0x/0X prefix if present */
        if (*nptr == '0' && (*(nptr + 1) == 'x' || *(nptr + 1) == 'X')) {
            nptr += 2;
        }
    }

    /* Validate base */
    if (base < 2 || base > 36) {
        if (endptr) *endptr = (char *) nptr;
        return 0;
    }

    /* Convert digits */
    long result = 0;
    const char *start = nptr;
    int overflow = 0;

    while (*nptr) {
        int digit;

        if (*nptr >= '0' && *nptr <= '9') {
            digit = *nptr - '0';
        } else if (*nptr >= 'a' && *nptr <= 'z') {
            digit = *nptr - 'a' + 10;
        } else if (*nptr >= 'A' && *nptr <= 'Z') {
            digit = *nptr - 'A' + 10;
        } else {
            break;
        }

        if (digit >= base) break;

        /* Check for overflow */
        if (result > (LONG_MAX - digit) / base) {
            overflow = 1;
        }

        result = result * base + digit;
        nptr++;
    }

    /* Set endptr if provided */
    if (endptr) {
        *endptr = (nptr == start) ? (char *) start : (char *) nptr;
    }

    /* Handle overflow */
    if (overflow) {
        result = neg ? LONG_MIN : LONG_MAX;
    } else if (neg) {
        result = -result;
    }

    return result;
}

unsigned long sf_strtoul(const char *nptr, char **endptr, int base) {
    if (nptr == NULL) {
        if (endptr) *endptr = (char *) nptr;
        return 0;
    }

    /* Skip leading whitespace */
    while (sf_isspace(*nptr)) nptr++;

    /* Handle sign (unsigned, but we still need to handle the - sign) */
    int neg = 0;
    if (*nptr == '-') {
        neg = 1;
        nptr++;
    } else if (*nptr == '+') {
        nptr++;
    }

    /* Determine base if not specified */
    if (base == 0) {
        if (*nptr == '0') {
            nptr++;
            if (*nptr == 'x' || *nptr == 'X') {
                base = 16;
                nptr++;
            } else {
                base = 8;
                nptr--;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        /* Skip 0x/0X prefix if present */
        if (*nptr == '0' && (*(nptr + 1) == 'x' || *(nptr + 1) == 'X')) {
            nptr += 2;
        }
    }

    /* Validate base */
    if (base < 2 || base > 36) {
        if (endptr) *endptr = (char *) nptr;
        return 0;
    }

    /* Convert digits */
    unsigned long result = 0;
    const char *start = nptr;
    int overflow = 0;

    while (*nptr) {
        int digit;

        if (*nptr >= '0' && *nptr <= '9') {
            digit = *nptr - '0';
        } else if (*nptr >= 'a' && *nptr <= 'z') {
            digit = *nptr - 'a' + 10;
        } else if (*nptr >= 'A' && *nptr <= 'Z') {
            digit = *nptr - 'A' + 10;
        } else {
            break;
        }

        if (digit >= base) break;

        /* Check for overflow */
        if (result > (ULONG_MAX - digit) / base) {
            overflow = 1;
        }

        result = result * base + digit;
        nptr++;
    }

    /* Set endptr if provided */
    if (endptr) {
        *endptr = (nptr == start) ? (char *) start : (char *) nptr;
    }

    /* Handle overflow and negative sign */
    if (overflow) {
        result = ULONG_MAX;
    } else if (neg) {
        result = -result;
    }

    return result;
}

double sf_strtod(const char *nptr, char **endptr) {
    if (nptr == NULL) {
        if (endptr) *endptr = (char *) nptr;
        return 0.0;
    }

    /* Skip leading whitespace */
    while (sf_isspace(*nptr)) nptr++;

    /* Handle sign */
    int neg = 0;
    if (*nptr == '-') {
        neg = 1;
        nptr++;
    } else if (*nptr == '+') {
        nptr++;
    }

    /* Convert integer part */
    double result = 0.0;
    const char *start = nptr;

    while (*nptr >= '0' && *nptr <= '9') {
        result = result * 10.0 + (*nptr - '0');
        nptr++;
    }

    /* Convert fractional part */
    if (*nptr == '.') {
        nptr++;

        double fraction = 0.0;
        double divisor = 1.0;

        while (*nptr >= '0' && *nptr <= '9') {
            divisor *= 10.0;
            fraction = fraction * 10.0 + (*nptr - '0');
            nptr++;
        }

        result += fraction / divisor;
    }

    /* Handle exponent */
    if (*nptr == 'e' || *nptr == 'E') {
        nptr++;

        int exp_neg = 0;
        if (*nptr == '-') {
            exp_neg = 1;
            nptr++;
        } else if (*nptr == '+') {
            nptr++;
        }

        int exponent = 0;
        while (*nptr >= '0' && *nptr <= '9') {
            exponent = exponent * 10 + (*nptr - '0');
            nptr++;
        }

        /* Apply exponent */
        if (exponent > 0) {
            double factor = 1.0;
            for (int i = 0; i < exponent; i++) {
                factor *= 10.0;
            }

            if (exp_neg) {
                result /= factor;
            } else {
                result *= factor;
            }
        }
    }

    /* Set endptr if provided */
    if (endptr) {
        *endptr = (nptr == start) ? (char *) start : (char *) nptr;
    }

    /* Apply sign */
    if (neg) {
        result = -result;
    }

    return result;
}

int sf_atoi(const char *nptr) {
    return (int) sf_strtol(nptr, NULL, 10);
}

long sf_atol(const char *nptr) {
    return sf_strtol(nptr, NULL, 10);
}

double sf_atof(const char *nptr) {
    return sf_strtod(nptr, NULL);
}

char *sf_itoa(int value, char *str, int base) {
    /* Validate base */
    if (base < 2 || base > 36 || str == NULL) {
        return NULL;
    }

    int i = 0;
    int neg = 0;

    /* Handle negative numbers */
    if (value < 0 && base == 10) {
        neg = 1;
        value = -value;
    }

    /* Handle special case of zero */
    if (value == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    /* Convert to specified base */
    while (value != 0) {
        int remainder = value % base;
        str[i++] = (remainder < 10) ? remainder + '0' : remainder - 10 + 'a';
        value /= base;
    }

    /* Add negative sign if needed */
    if (neg) {
        str[i++] = '-';
    }

    /* Null-terminate the string */
    str[i] = '\0';

    /* Reverse the string */
    int start = neg ? 1 : 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }

    return str;
}

/* ========================================================================
 * ERROR HANDLING
 * ======================================================================== */

/* Global errno variable */
int sf_errno = 0;

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
    sf_allocator.pool = (uint8_t *) pool;
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
    return *(unsigned char *) s1 - *(unsigned char *) s2;
}

int sf_strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    return n ? *(unsigned char *) s1 - *(unsigned char *) s2 : 0;
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
    unsigned char *p = (unsigned char *) s;
    while (n--) *p++ = (unsigned char) c;
    return s;
}

void *sf_memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *) dest;
    const unsigned char *s = (const unsigned char *) src;
    while (n--) *d++ = *s++;
    return dest;
}

void *sf_memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *) dest;
    const unsigned char *s = (const unsigned char *) src;

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
    const unsigned char *p1 = (const unsigned char *) s1;
    const unsigned char *p2 = (const unsigned char *) s2;
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
__attribute__ ((weak)) int sf_putchar(int c) {
    /* Default: do nothing */
    SF_UNUSED(c);
    return c;
}

__attribute__ ((weak)) int sf_puts(const char *s) {
    while (*s) sf_putchar(*s++);
    sf_putchar('\n');
    return 1;
}

__attribute__ ((weak)) char *sf_fgets(char *s, int size, void *stream) {
    /* Default: return empty string */
    SF_UNUSED(stream);
    if (size > 0) {
        s[0] = '\0';
        return s;
    }
    return 0;
}

__attribute__ ((weak)) int sf_fputs(const char *s, void *stream) {
    SF_UNUSED(stream);
    return sf_puts(s);
}

__attribute__ ((weak)) void sf_fflush(void *stream) {
    SF_UNUSED(stream);
    /* Default: do nothing */
}

__attribute__ ((weak)) sf_clock_t sf_clock(void) {
    /* Default: return 0 */
    return 0;
}

__attribute__ ((weak)) sf_time_t sf_time(sf_time_t *timer) {
    /* Default: return 0 (epoch) */
    sf_time_t t = 0;
    if (timer) *timer = t;
    return t;
}

__attribute__ ((weak)) sf_tm *sf_localtime(const sf_time_t *timer) {
    /* Default: return epoch time broken down */
    static sf_tm tm_buf = {0, 0, 0, 1, 0, 70, 4, 0, 0}; /* Jan 1, 1970 */
    SF_UNUSED(timer);
    return &tm_buf;
}

__attribute__ ((weak)) size_t sf_strftime(char *str, size_t maxsize, const char *format, const sf_tm *timeptr) {
    /* Simple implementation - just return "00:00:00" for time formats */
    SF_UNUSED(format);
    SF_UNUSED(timeptr);

    if (maxsize >= 9) {
        sf_strcpy(str, "00:00:00");
        return 8;
    }
    return 0;
}

__attribute__ ((weak)) sf_time_t sf_time(sf_time_t *tloc) {
    /* Default: return a fake timestamp */
    sf_time_t fake_time = 1609459200; /* 2021-01-01 00:00:00 UTC */
    if (tloc) *tloc = fake_time;
    return fake_time;
}

__attribute__ ((weak)) struct sf_tm *sf_localtime(const sf_time_t *timep) {
    /* Default: return a static fake time structure */
    static struct sf_tm fake_tm = {0, 0, 12, 1, 0, 121, 5, 0, 0}; /* 2021-01-01 12:00:00 */
    SF_UNUSED(timep);
    return &fake_tm;
}

__attribute__ ((weak)) size_t sf_strftime(char *s, size_t max, const char *format, const struct sf_tm *tm) {
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
    sf_allocator.pool = (uint8_t *) pool;
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
    return *(unsigned char *) s1 - *(unsigned char *) s2;
}

int sf_strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    return n ? *(unsigned char *) s1 - *(unsigned char *) s2 : 0;
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
    unsigned char *p = (unsigned char *) s;
    while (n--) *p++ = (unsigned char) c;
    return s;
}

void *sf_memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *) dest;
    const unsigned char *s = (const unsigned char *) src;
    while (n--) *d++ = *s++;
    return dest;
}

void *sf_memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *) dest;
    const unsigned char *s = (const unsigned char *) src;

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
    const unsigned char *p1 = (const unsigned char *) s1;
    const unsigned char *p2 = (const unsigned char *) s2;
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
__attribute__ ((weak)) int sf_putchar(int c) {
    /* Default: do nothing */
    SF_UNUSED(c);
    return c;
}

__attribute__ ((weak)) int sf_puts(const char *s) {
    while (*s) sf_putchar(*s++);
    sf_putchar('\n');
    return 1;
}

__attribute__ ((weak)) char *sf_fgets(char *s, int size, void *stream) {
    /* Default: return empty string */
    SF_UNUSED(stream);
    if (size > 0) {
        s[0] = '\0';
        return s;
    }
    return NULL;
}

__attribute__ ((weak)) int sf_fputs(const char *s, void *stream) {
    SF_UNUSED(stream);
    return sf_puts(s);
}

__attribute__ ((weak)) void sf_fflush(void *stream) {
    SF_UNUSED(stream);
}

__attribute__ ((weak)) sf_clock_t sf_clock(void) {
    return 0;
}

__attribute__ ((weak)) sf_time_t sf_time(sf_time_t *timer) {
    sf_time_t t = 0;
    if (timer) *timer = t;
    return t;
}

__attribute__ ((weak)) sf_tm *sf_localtime(const sf_time_t *timer) {
    static sf_tm tm_buf = {0, 0, 0, 1, 0, 70, 4, 0, 0};
    SF_UNUSED(timer);
    return &tm_buf;
}

__attribute__ ((weak)) size_t sf_strftime(char *str, size_t maxsize, const char *format, const sf_tm *timeptr) {
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
void *sf_stdin = (void *) 1;
void *sf_stdout = (void *) 2;
void *sf_stderr = (void *) 3;

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
                    *buf_ptr++ = (char) c;
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
void *sf_stdin = (void *) 1;
void *sf_stdout = (void *) 2;
void *sf_stderr = (void *) 3;

#endif /* STARFORTH_MINIMAL */