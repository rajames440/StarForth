/* Freestanding sched.h shim — no POSIX scheduling in kernel builds. */
#ifndef FREESTANDING_SCHED_H
#define FREESTANDING_SCHED_H
static inline int sched_yield(void) { return 0; }
#endif /* FREESTANDING_SCHED_H */
