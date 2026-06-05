/* Freestanding errno.h shim — no OS errno in kernel builds. */
#ifndef FREESTANDING_ERRNO_H
#define FREESTANDING_ERRNO_H
static int _freestanding_errno = 0;
#define errno _freestanding_errno
#define EPERM   1
#define ENOENT  2
#define EINVAL  22
#define ENOMEM  12
#endif /* FREESTANDING_ERRNO_H */
