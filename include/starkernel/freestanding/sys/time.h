/* Freestanding sys/time.h shim — no gettimeofday in kernel builds. */
#ifndef FREESTANDING_SYS_TIME_H
#define FREESTANDING_SYS_TIME_H
#include <stddef.h>
typedef long time_t;
typedef long suseconds_t;
struct timeval {
    time_t      tv_sec;
    suseconds_t tv_usec;
};
struct timezone { int tz_minuteswest; int tz_dsttime; };
static inline int gettimeofday(struct timeval *tv, struct timezone *tz) {
    (void)tv; (void)tz; return 0;
}
#endif /* FREESTANDING_SYS_TIME_H */
