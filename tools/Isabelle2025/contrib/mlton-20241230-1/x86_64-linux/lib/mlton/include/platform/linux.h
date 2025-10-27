/*
                                  ***   StarForth   ***

  linux.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:03.667-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/mlton-20241230-1/x86_64-linux/lib/mlton/include/platform/linux.h
 */

#include <inttypes.h>
#include <stdint.h>
#ifdef __UCLIBC__
#include <fpu_control.h>
#else
#include <fenv.h>
#endif

#include <unistd.h>

#include <dirent.h>
#include <fcntl.h>
#include <grp.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/sysinfo.h>
#include <syslog.h>
#include <termios.h>
#include <utime.h>

#ifdef __UCLIBC__
#define HAS_FEROUND FALSE
#else
#define HAS_FEROUND TRUE
#endif
#define HAS_MSG_DONTWAIT TRUE
#define HAS_REMAP TRUE
#define HAS_SHRINK_HEAP TRUE
#define HAS_SIGALTSTACK TRUE
#if (defined (__hppa__))
#define NEEDS_SIGALTSTACK_EXEC TRUE
#else
#define NEEDS_SIGALTSTACK_EXEC FALSE
#endif
#define HAS_SPAWN FALSE
#define HAS_TIME_PROFILING TRUE

#define MLton_Platform_OS_host "linux"

// environ is already defined if _GNU_SOURCE is.
#if !defined(_GNU_SOURCE) && !defined(__ANDROID__)
extern char **environ; /* for Posix_ProcEnv_environ */
#endif

/* The following is compatibility code with older glibc and kernel
   versions. */

#ifdef __GLIBC__
#ifndef __suseconds_t_defined
#include <linux/types.h>
typedef __kernel_suseconds_t suseconds_t;
#define __suseconds_t_defined
#endif

#if __GLIBC__ == 2 && __GLIBC_MINOR__ <= 1
typedef unsigned long int nfds_t;
#endif
#endif

#ifdef __ANDROID__
/* Work around buggy android system libraries */
#undef PRIxPTR
#define PRIxPTR "x"

/* Android is missing these methods: */
#undef tcdrain
#undef ctermid
#define tcdrain MLton_tcdrain
#define ctermid MLton_ctermid

static inline int tcdrain(int fd) {
  return ioctl(fd, TCSBRK, 1);
}

static inline char* ctermid(char* x) {
  static char buf[] = "/dev/tty";
  if (x) {
    strcpy(x, buf);
    return x;
  } else {
    return &buf[0];
  }
}

#endif

#ifndef SO_ACCEPTCONN
#define SO_ACCEPTCONN 30
#endif

#ifdef __UCLIBC__
#define FE_DOWNWARD     _FPU_RC_DOWN
#define FE_TONEAREST    _FPU_RC_NEAREST
#define FE_TOWARDZERO   _FPU_RC_ZERO
#define FE_UPWARD       _FPU_RC_UP
#endif
