/* Freestanding sys/types.h shim — POSIX types for kernel builds. */
#ifndef FREESTANDING_SYS_TYPES_H
#define FREESTANDING_SYS_TYPES_H
#include <stddef.h>
#include <stdint.h>
typedef long          ssize_t;
typedef long          off_t;
typedef unsigned int  mode_t;
typedef unsigned int  uid_t;
typedef unsigned int  gid_t;
typedef int           pid_t;
#endif /* FREESTANDING_SYS_TYPES_H */
