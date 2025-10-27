/*
                                  ***   StarForth   ***

  darwin.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:02.584-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/mlton-20241230-1/x86_64-linux/lib/mlton/include/platform/darwin.h
 */

#include <fenv.h>
#include <inttypes.h>
#include <stdint.h>

#include <unistd.h>

#include <dirent.h>
#include <fcntl.h>
#include <grp.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/ptrace.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/resource.h> /* <sys/resource.h> might not #include <sys/time.h> */
#include <sys/times.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <syslog.h>
#include <termios.h>
#include <utime.h>

#include <crt_externs.h>

#define HAS_FEROUND TRUE
#define HAS_MSG_DONTWAIT TRUE
#define HAS_REMAP FALSE
#define HAS_SHRINK_HEAP TRUE
#define HAS_SIGALTSTACK TRUE
#define NEEDS_SIGALTSTACK_EXEC FALSE
#define HAS_SPAWN FALSE
#define HAS_TIME_PROFILING TRUE

#define MLton_Platform_OS_host "darwin"

// MacOS only defines this if POSIX_C_SOURCE is defined.
// However, defining that breaks half the osx system headers.
// They couldn't possibly change the number at this point anyway.
#ifndef SIGPOLL
#define SIGPOLL 7
#endif

/* for Posix_ProcEnv_environ */
#define environ *_NSGetEnviron()
