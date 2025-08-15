/*

                                 ***   StarForth   ***
  starforth_platform.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/14/25, 8:47 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#ifndef STARFORTH_PLATFORM_H
#define STARFORTH_PLATFORM_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef STARFORTH_MINIMAL
    #define SF_MINIMAL 1
#else
    #define SF_MINIMAL 0
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <time.h>
    #include <ctype.h>
#endif

#if SF_MINIMAL
    int sf_putchar(int c);
    int sf_puts(const char *s);
    int sf_printf(const char *fmt, ...);
    int sf_fprintf(void *stream, const char *fmt, ...);
    int sf_vfprintf(void *stream, const char *fmt, va_list args);
    int sf_sprintf(char *str, size_t size, const char *fmt, ...);
    char *sf_fgets(char *s, int size, void *stream);
    int sf_fputs(const char *s, void *stream);
    void sf_fflush(void *stream);

    extern void *sf_stdin;
    extern void *sf_stdout;
    extern void *sf_stderr;
#else
    #define sf_putchar putchar
    #define sf_puts puts
    #define sf_printf printf
    #define sf_fprintf fprintf
    #define sf_vfprintf vfprintf
    #define sf_sprintf snprintf
    #define sf_fgets fgets
    #define sf_fputs fputs
    #define sf_fflush fflush
    #define sf_stdin stdin
    #define sf_stdout stdout
    #define sf_stderr stderr
#endif

#if SF_MINIMAL
    void *sf_malloc(size_t size);
    void sf_free(void *ptr);
    void sf_init_allocator(void *pool, size_t pool_size);
#else
    #define sf_malloc malloc
    #define sf_free free
    #define sf_init_allocator(pool, size) ((void)0)
#endif

#if SF_MINIMAL
    size_t sf_strlen(const char *s);
    int sf_strcmp(const char *s1, const char *s2);
    int sf_strncmp(const char *s1, const char *s2, size_t n);
    char *sf_strcpy(char *dest, const char *src);
    char *sf_strncpy(char *dest, const char *src, size_t n);
    void *sf_memset(void *s, int c, size_t n);
    void *sf_memcpy(void *dest, const void *src, size_t n);
    void *sf_memmove(void *dest, const void *src, size_t n);
    int sf_memcmp(const void *s1, const void *s2, size_t n);
    int sf_isspace(int c);
    int sf_isdigit(int c);
    int sf_isalpha(int c);
    int sf_tolower(int c);
    int sf_toupper(int c);
#else
    #define sf_strlen strlen
    #define sf_strcmp strcmp
    #define sf_strncmp strncmp
    #define sf_strcpy strcpy
    #define sf_strncpy strncpy
    #define sf_memset memset
    #define sf_memcpy memcpy
    #define sf_memmove memmove
    #define sf_memcmp memcmp
    #define sf_isspace isspace
    #define sf_isdigit isdigit
    #define sf_isalpha isalpha
    #define sf_tolower tolower
    #define sf_toupper toupper
#endif

#if SF_MINIMAL
    typedef uint64_t sf_clock_t;
    typedef uint64_t sf_time_t;
    typedef struct {
        int tm_sec, tm_min, tm_hour;
        int tm_mday, tm_mon, tm_year;
        int tm_wday, tm_yday, tm_isdst;
    } sf_tm;

    sf_clock_t sf_clock(void);
    sf_time_t sf_time(sf_time_t *timer);
    sf_tm *sf_localtime(const sf_time_t *timer);
    size_t sf_strftime(char *str, size_t maxsize, const char *format, const sf_tm *timeptr);
    #define SF_CLOCKS_PER_SEC 1000000000ULL
#else
    #include <time.h>
    #define sf_clock clock
    #define sf_clock_t clock_t
    #define sf_time time
    #define sf_localtime localtime  
    #define sf_strftime strftime
    #define SF_CLOCKS_PER_SEC CLOCKS_PER_SEC
    typedef time_t sf_time_t;
    #define sf_tm tm
#endif

#if SF_MINIMAL
    int sf_abs(int x);
    long sf_labs(long x);
    double sf_fabs(double x);
#else
    #include <stdlib.h>
    #include <math.h>
    #define sf_abs abs
    #define sf_labs labs
    #define sf_fabs fabs
#endif

#define SF_UNUSED(x) ((void)(x))

#endif