/* Freestanding signal.h shim — no POSIX signals in kernel builds. */
#ifndef FREESTANDING_SIGNAL_H
#define FREESTANDING_SIGNAL_H
#define SIGINT  2
#define SIGABRT 6
#define SIGSEGV 11
#define SIGTERM 15
typedef void (*sighandler_t)(int);
#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
struct sigaction { sighandler_t sa_handler; int sa_flags; };
#define SA_RESETHAND 0
static inline sighandler_t signal(int sig, sighandler_t handler) { (void)sig; (void)handler; return SIG_DFL; }
static inline int sigaction(int sig, const struct sigaction *act, struct sigaction *old)
    { (void)sig; (void)act; (void)old; return 0; }
#endif /* FREESTANDING_SIGNAL_H */
