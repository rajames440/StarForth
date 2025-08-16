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

/* Include only the most basic headers that should be available everywhere */
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef STARFORTH_MINIMAL
#define SF_MINIMAL 1
#else
#define SF_MINIMAL 0
/* System headers for standard mode */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <signal.h>
#include <math.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

/* Standard I/O functions */
#if SF_MINIMAL
/* Input/output functions */
int sf_putchar(int c);
int sf_puts(const char *s);
int sf_printf(const char *fmt, ...);
int sf_fprintf(void *stream, const char *fmt, ...);
int sf_vfprintf(void *stream, const char *fmt, va_list args);
int sf_sprintf(char *str, size_t size, const char *fmt, ...);
int sf_vsprintf(char *str, size_t size, const char *fmt, va_list args);
int sf_sscanf(const char *str, const char *format, ...);
char *sf_fgets(char *s, int size, void *stream);
int sf_fputs(const char *s, void *stream);
void sf_fflush(void *stream);
int sf_setvbuf(void *stream, char *buf, int mode, size_t size);
int sf_fclose(void *stream);
void *sf_fopen(const char *filename, const char *mode);
size_t sf_fread(void *ptr, size_t size, size_t nmemb, void *stream);
size_t sf_fwrite(const void *ptr, size_t size, size_t nmemb, void *stream);
int sf_fseek(void *stream, long offset, int whence);
long sf_ftell(void *stream);
int sf_feof(void *stream);
void sf_clearerr(void *stream);
int sf_ferror(void *stream);
int sf_fileno(void *stream);
int sf_ungetc(int c, void *stream);
int sf_fgetc(void *stream);
int sf_fputc(int c, void *stream);
int sf_remove(const char *pathname);
int sf_rename(const char *oldpath, const char *newpath);

/* Stream handles */
extern void *sf_stdin;
extern void *sf_stdout;
extern void *sf_stderr;

/* Constants for fseek */
#define SF_SEEK_SET 0
#define SF_SEEK_CUR 1
#define SF_SEEK_END 2

/* Constants for setvbuf */
#define SF_IOFBF 0
#define SF_IOLBF 1
#define SF_IONBF 2
#else
/* Map stdio functions to platform-independent names */
#define sf_putchar putchar
#define sf_puts puts
#define sf_printf printf
#define sf_fprintf fprintf
#define sf_vfprintf vfprintf
#define sf_sprintf snprintf
#define sf_vsprintf vsnprintf
#define sf_sscanf sscanf
#define sf_fgets fgets
#define sf_fputs fputs
#define sf_fflush fflush
#define sf_setvbuf setvbuf
#define sf_fclose fclose
#define sf_fopen fopen
#define sf_fread fread
#define sf_fwrite fwrite
#define sf_fseek fseek
#define sf_ftell ftell
#define sf_feof feof
#define sf_clearerr clearerr
#define sf_ferror ferror
#define sf_fileno fileno
#define sf_ungetc ungetc
#define sf_fgetc fgetc
#define sf_fputc fputc
#define sf_remove remove
#define sf_rename rename

/* Stream handles */
#define sf_stdin stdin
#define sf_stdout stdout
#define sf_stderr stderr

/* Constants for fseek */
#define SF_SEEK_SET SEEK_SET
#define SF_SEEK_CUR SEEK_CUR
#define SF_SEEK_END SEEK_END

/* Constants for setvbuf */
#define SF_IOFBF _IOFBF
#define SF_IOLBF _IOLBF
#define SF_IONBF _IONBF
#endif

/* Memory management functions */
#if SF_MINIMAL
void *sf_malloc(size_t size);
void *sf_calloc(size_t nmemb, size_t size);
void *sf_realloc(void *ptr, size_t size);
void sf_free(void *ptr);
void sf_exit(int status);
void sf_abort(void);
int sf_atexit(void (*func)(void));
char *sf_getenv(const char *name);
int sf_system(const char *command);
void sf_qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void *sf_bsearch(const void *key, const void *base, size_t nmemb, size_t size,
                 int (*compar)(const void *, const void *));
long sf_strtol(const char *nptr, char **endptr, int base);
unsigned long sf_strtoul(const char *nptr, char **endptr, int base);
double sf_strtod(const char *nptr, char **endptr);
int sf_atoi(const char *nptr);
long sf_atol(const char *nptr);
double sf_atof(const char *nptr);
char *sf_itoa(int value, char *str, int base);

/* Memory allocator initialization */
void sf_init_allocator(void *pool, size_t pool_size);
#else
#define sf_malloc malloc
#define sf_calloc calloc
#define sf_realloc realloc
#define sf_free free
#define sf_exit exit
#define sf_abort abort
#define sf_atexit atexit
#define sf_getenv getenv
#define sf_system system
#define sf_qsort qsort
#define sf_bsearch bsearch
#define sf_strtol strtol
#define sf_strtoul strtoul
#define sf_strtod strtod
#define sf_atoi atoi
#define sf_atol atol
#define sf_atof atof

/* itoa is not standard C, so we need to provide it */
char *sf_itoa(int value, char *str, int base);

/* No-op for standard mode */
#define sf_init_allocator(pool, size) ((void)0)
#endif

/* String functions */
#if SF_MINIMAL
size_t sf_strlen(const char *s);
int sf_strcmp(const char *s1, const char *s2);
int sf_strncmp(const char *s1, const char *s2, size_t n);
char *sf_strcpy(char *dest, const char *src);
char *sf_strncpy(char *dest, const char *src, size_t n);
char *sf_strcat(char *dest, const char *src);
char *sf_strncat(char *dest, const char *src, size_t n);
char *sf_strchr(const char *s, int c);
char *sf_strrchr(const char *s, int c);
char *sf_strstr(const char *haystack, const char *needle);
char *sf_strtok(char *str, const char *delim);
size_t sf_strspn(const char *s, const char *accept);
size_t sf_strcspn(const char *s, const char *reject);
char *sf_strpbrk(const char *s, const char *accept);
int sf_strcoll(const char *s1, const char *s2);
size_t sf_strxfrm(char *dest, const char *src, size_t n);
char *sf_strerror(int errnum);
void *sf_memset(void *s, int c, size_t n);
void *sf_memcpy(void *dest, const void *src, size_t n);
void *sf_memmove(void *dest, const void *src, size_t n);
int sf_memcmp(const void *s1, const void *s2, size_t n);
void *sf_memchr(const void *s, int c, size_t n);
#else
#define sf_strlen strlen
#define sf_strcmp strcmp
#define sf_strncmp strncmp
#define sf_strcpy strcpy
#define sf_strncpy strncpy
#define sf_strcat strcat
#define sf_strncat strncat
#define sf_strchr strchr
#define sf_strrchr strrchr
#define sf_strstr strstr
#define sf_strtok strtok
#define sf_strspn strspn
#define sf_strcspn strcspn
#define sf_strpbrk strpbrk
#define sf_strcoll strcoll
#define sf_strxfrm strxfrm
#define sf_strerror strerror
#define sf_memset memset
#define sf_memcpy memcpy
#define sf_memmove memmove
#define sf_memcmp memcmp
#define sf_memchr memchr
#endif

/* Character classification functions */
#if SF_MINIMAL
int sf_isalnum(int c);
int sf_isalpha(int c);
int sf_iscntrl(int c);
int sf_isdigit(int c);
int sf_isgraph(int c);
int sf_islower(int c);
int sf_isprint(int c);
int sf_ispunct(int c);
int sf_isspace(int c);
int sf_isupper(int c);
int sf_isxdigit(int c);
int sf_tolower(int c);
int sf_toupper(int c);
#else
#define sf_isalnum isalnum
#define sf_isalpha isalpha
#define sf_iscntrl iscntrl
#define sf_isdigit isdigit
#define sf_isgraph isgraph
#define sf_islower islower
#define sf_isprint isprint
#define sf_ispunct ispunct
#define sf_isspace isspace
#define sf_isupper isupper
#define sf_isxdigit isxdigit
#define sf_tolower tolower
#define sf_toupper toupper
#endif

/* Time functions */
#if SF_MINIMAL
typedef uint64_t sf_clock_t;
typedef uint64_t sf_time_t;

typedef struct {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
} sf_tm;

sf_clock_t sf_clock(void);
sf_time_t sf_time(sf_time_t *timer);
double sf_difftime(sf_time_t end, sf_time_t beginning);
sf_tm *sf_localtime(const sf_time_t *timer);
sf_tm *sf_gmtime(const sf_time_t *timer);
sf_time_t sf_mktime(sf_tm *timeptr);
size_t sf_strftime(char *s, size_t max, const char *format, const sf_tm *tm);
void sf_sleep(unsigned seconds);

#define SF_CLOCKS_PER_SEC 1000000ULL
#else
#define sf_clock clock
#define sf_clock_t clock_t
#define sf_time time
#define sf_difftime difftime
#define sf_localtime localtime
#define sf_gmtime gmtime
#define sf_mktime mktime
#define sf_strftime strftime

void sf_sleep(unsigned seconds);

#define SF_CLOCKS_PER_SEC CLOCKS_PER_SEC
typedef time_t sf_time_t;
#define sf_tm struct tm
#endif

/* Math functions */
#if SF_MINIMAL
int sf_abs(int x);
long sf_labs(long x);
double sf_fabs(double x);
double sf_ceil(double x);
double sf_floor(double x);
double sf_fmod(double x, double y);
double sf_exp(double x);
double sf_log(double x);
double sf_log10(double x);
double sf_pow(double x, double y);
double sf_sqrt(double x);
double sf_sin(double x);
double sf_cos(double x);
double sf_tan(double x);
double sf_asin(double x);
double sf_acos(double x);
double sf_atan(double x);
double sf_atan2(double y, double x);
double sf_sinh(double x);
double sf_cosh(double x);
double sf_tanh(double x);
double sf_frexp(double value, int *exp);
double sf_ldexp(double x, int exp);
double sf_modf(double value, double *iptr);
#else
#define sf_abs abs
#define sf_labs labs
#define sf_fabs fabs
#define sf_ceil ceil
#define sf_floor floor
#define sf_fmod fmod
#define sf_exp exp
#define sf_log log
#define sf_log10 log10
#define sf_pow pow
#define sf_sqrt sqrt
#define sf_sin sin
#define sf_cos cos
#define sf_tan tan
#define sf_asin asin
#define sf_acos acos
#define sf_atan atan
#define sf_atan2 atan2
#define sf_sinh sinh
#define sf_cosh cosh
#define sf_tanh tanh
#define sf_frexp frexp
#define sf_ldexp ldexp
#define sf_modf modf
#endif

/* Signal handling */
#if SF_MINIMAL
typedef void (*sf_sighandler_t)(int);

sf_sighandler_t sf_signal(int signum, sf_sighandler_t handler);
int sf_raise(int sig);

/* Signal constants */
#define SF_SIGINT  2
#define SF_SIGILL  4
#define SF_SIGABRT 6
#define SF_SIGFPE  8
#define SF_SIGSEGV 11
#define SF_SIGTERM 15

/* Signal handlers */
#define SF_SIG_DFL ((sf_sighandler_t)0)
#define SF_SIG_IGN ((sf_sighandler_t)1)
#define SF_SIG_ERR ((sf_sighandler_t)-1)

/* sigaction structure and functions */
typedef struct {
    sf_sighandler_t sa_handler;

    void (*sa_sigaction)(int, void *, void *);

    int sa_flags;
    /* sa_mask is platform-specific, minimal implementation simplifies */
} sf_sigaction_t;

void sf_sigemptyset(void *set);
void sf_sigfillset(void *set);
void sf_sigaddset(void *set, int signum);
void sf_sigdelset(void *set, int signum);
int sf_sigismember(const void *set, int signum);
int sf_sigaction(int signum, const sf_sigaction_t *act, sf_sigaction_t *oldact);

/* sigaction flags */
#define SF_SA_NOCLDSTOP 1
#define SF_SA_NOCLDWAIT 2
#define SF_SA_SIGINFO   4
#define SF_SA_RESTART   0x10000000
#define SF_SA_NODEFER   0x40000000
#define SF_SA_RESETHAND 0x80000000
#define SF_SA_ONSTACK   0x08000000
#else
/* Map signal functions to platform-independent names */
#define sf_signal signal
#define sf_raise raise
#define sf_sighandler_t sighandler_t

/* Signal constants */
#define SF_SIGINT  SIGINT
#define SF_SIGILL  SIGILL
#define SF_SIGABRT SIGABRT
#define SF_SIGFPE  SIGFPE
#define SF_SIGSEGV SIGSEGV
#define SF_SIGTERM SIGTERM

/* Signal handlers */
#define SF_SIG_DFL SIG_DFL
#define SF_SIG_IGN SIG_IGN
#define SF_SIG_ERR SIG_ERR

/* sigaction structure and functions */
typedef struct sigaction sf_sigaction_t;

void sf_sigemptyset(void *set);

void sf_sigfillset(void *set);

void sf_sigaddset(void *set, int signum);

void sf_sigdelset(void *set, int signum);

int sf_sigismember(const void *set, int signum);

int sf_sigaction(int signum, const sf_sigaction_t *act, sf_sigaction_t *oldact);

/* sigaction flags - map if available */
#ifdef SA_NOCLDSTOP
#define SF_SA_NOCLDSTOP SA_NOCLDSTOP
#else
#define SF_SA_NOCLDSTOP 0
#endif

#ifdef SA_NOCLDWAIT
#define SF_SA_NOCLDWAIT SA_NOCLDWAIT
#else
#define SF_SA_NOCLDWAIT 0
#endif

#ifdef SA_SIGINFO
#define SF_SA_SIGINFO SA_SIGINFO
#else
#define SF_SA_SIGINFO 0
#endif

#ifdef SA_RESTART
#define SF_SA_RESTART SA_RESTART
#else
#define SF_SA_RESTART 0
#endif

#ifdef SA_NODEFER
#define SF_SA_NODEFER SA_NODEFER
#else
#define SF_SA_NODEFER 0
#endif

#ifdef SA_RESETHAND
#define SF_SA_RESETHAND SA_RESETHAND
#else
#define SF_SA_RESETHAND 0
#endif

#ifdef SA_ONSTACK
#define SF_SA_ONSTACK SA_ONSTACK
#else
#define SF_SA_ONSTACK 0
#endif
#endif

/* File and directory functions */
#if SF_MINIMAL
int sf_open(const char *pathname, int flags, ...);
int sf_close(int fd);
ssize_t sf_read(int fd, void *buf, size_t count);
ssize_t sf_write(int fd, const void *buf, size_t count);
off_t sf_lseek(int fd, off_t offset, int whence);
int sf_stat(const char *pathname, void *statbuf);
int sf_fstat(int fd, void *statbuf);
int sf_access(const char *pathname, int mode);
int sf_mkdir(const char *pathname, int mode);
int sf_rmdir(const char *pathname);
int sf_unlink(const char *pathname);
int sf_chdir(const char *path);
char *sf_getcwd(char *buf, size_t size);

/* File open flags */
#define SF_O_RDONLY 0
#define SF_O_WRONLY 1
#define SF_O_RDWR   2
#define SF_O_CREAT  64
#define SF_O_EXCL   128
#define SF_O_TRUNC  512
#define SF_O_APPEND 1024

/* File seek constants */
#define SF_SEEK_SET 0
#define SF_SEEK_CUR 1
#define SF_SEEK_END 2

/* File access mode constants */
#define SF_F_OK 0
#define SF_R_OK 4
#define SF_W_OK 2
#define SF_X_OK 1
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Map file functions to platform-independent names */
#define sf_open open
#define sf_close close
#define sf_read read
#define sf_write write
#define sf_lseek lseek
#define sf_stat stat
#define sf_fstat fstat
#define sf_access access
#define sf_mkdir mkdir
#define sf_rmdir rmdir
#define sf_unlink unlink
#define sf_chdir chdir
#define sf_getcwd getcwd

/* File open flags */
#define SF_O_RDONLY O_RDONLY
#define SF_O_WRONLY O_WRONLY
#define SF_O_RDWR   O_RDWR
#define SF_O_CREAT  O_CREAT
#define SF_O_EXCL   O_EXCL
#define SF_O_TRUNC  O_TRUNC
#define SF_O_APPEND O_APPEND

/* File seek constants */
#define SF_SEEK_SET SEEK_SET
#define SF_SEEK_CUR SEEK_CUR
#define SF_SEEK_END SEEK_END

/* File access mode constants */
#define SF_F_OK F_OK
#define SF_R_OK R_OK
#define SF_W_OK W_OK
#define SF_X_OK X_OK
#endif

/* Error handling */
#if SF_MINIMAL
extern int sf_errno;

/* Common error codes */
#define SF_EPERM        1
#define SF_ENOENT       2
#define SF_ESRCH        3
#define SF_EINTR        4
#define SF_EIO          5
#define SF_ENXIO        6
#define SF_E2BIG        7
#define SF_ENOEXEC      8
#define SF_EBADF        9
#define SF_ECHILD      10
#define SF_EAGAIN      11
#define SF_ENOMEM      12
#define SF_EACCES      13
#define SF_EFAULT      14
#define SF_EBUSY       16
#define SF_EEXIST      17
#define SF_EXDEV       18
#define SF_ENODEV      19
#define SF_ENOTDIR     20
#define SF_EISDIR      21
#define SF_EINVAL      22
#define SF_ENFILE      23
#define SF_EMFILE      24
#define SF_ENOTTY      25
#define SF_EFBIG       27
#define SF_ENOSPC      28
#define SF_ESPIPE      29
#define SF_EROFS       30
#define SF_EMLINK      31
#define SF_EPIPE       32
#else
#include <errno.h>

/* Map errno to platform-independent name */
#define sf_errno errno

/* Error codes */
#define SF_EPERM   EPERM
#define SF_ENOENT  ENOENT
#define SF_ESRCH   ESRCH
#define SF_EINTR   EINTR
#define SF_EIO     EIO
#define SF_ENXIO   ENXIO
#define SF_E2BIG   E2BIG
#define SF_ENOEXEC ENOEXEC
#define SF_EBADF   EBADF
#define SF_ECHILD  ECHILD
#define SF_EAGAIN  EAGAIN
#define SF_ENOMEM  ENOMEM
#define SF_EACCES  EACCES
#define SF_EFAULT  EFAULT
#define SF_EBUSY   EBUSY
#define SF_EEXIST  EEXIST
#define SF_EXDEV   EXDEV
#define SF_ENODEV  ENODEV
#define SF_ENOTDIR ENOTDIR
#define SF_EISDIR  EISDIR
#define SF_EINVAL  EINVAL
#define SF_ENFILE  ENFILE
#define SF_EMFILE  EMFILE
#define SF_ENOTTY  ENOTTY
#define SF_EFBIG   EFBIG
#define SF_ENOSPC  ENOSPC
#define SF_ESPIPE  ESPIPE
#define SF_EROFS   EROFS
#define SF_EMLINK  EMLINK
#define SF_EPIPE   EPIPE
#endif

/* Utility macro to silence unused parameter warnings */
#define SF_UNUSED(x) ((void)(x))

#endif /* STARFORTH_PLATFORM_H */