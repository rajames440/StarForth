/* Freestanding time.h shim — no POSIX time in kernel builds. */
#ifndef FREESTANDING_TIME_H
#define FREESTANDING_TIME_H
#include <stddef.h>
#include "sys/time.h"   /* provides time_t, suseconds_t, struct timeval */
struct timespec {
    time_t tv_sec;
    long   tv_nsec;
};
int nanosleep(const struct timespec *req, struct timespec *rem);
#endif /* FREESTANDING_TIME_H */
