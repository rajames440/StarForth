/*
 * kernel_args.h — boot command-line argument structure and defaults
 *
 * KernelArgs is populated by the UEFI loader from LoadOptions (or the
 * one-shot StarForthBootArgs NVRAM variable) and carried to kernel_main
 * via BootInfo.args.  All fields have safe defaults so the kernel boots
 * normally when no arguments are present.
 */

#ifndef STARKERNEL_KERNEL_ARGS_H
#define STARKERNEL_KERNEL_ARGS_H

#include <stdint.h>

/* ---- Buffer size -------------------------------------------------------- */
/* Shared by BootInfo.args.raw, the NVRAM variable payload, and cmdline.c.  */
#define KERNEL_ARGS_CMDLINE_MAX  512

/* ---- Defaults ----------------------------------------------------------- */
#define KARGS_DEFAULT_STACK_SIZE   (2ULL  * 1024ULL * 1024ULL)            /* 2 MB  */
#define KARGS_DEFAULT_HEAP_SIZE    (2ULL  * 1024ULL * 1024ULL * 1024ULL)  /* 2 GB  */
#define KARGS_DEFAULT_LOG_LEVEL    1    /* info */
#define KARGS_DEFAULT_RUN_DOE      0

/* ---- Log level constants ------------------------------------------------ */
#define KARGS_LOG_DEBUG  0
#define KARGS_LOG_INFO   1
#define KARGS_LOG_WARN   2
#define KARGS_LOG_ERROR  3

/* ---- REBOOT escape hatch ------------------------------------------------ */
/* Max consecutive REBOOT attempts before the escape hatch fires.
 * On the Nth boot the counter is checked; if >= REBOOT_MAX_TRIES the
 * REBOOT word drops to REPL instead of calling ResetSystem. */
#define REBOOT_MAX_TRIES  3

/* ---- Struct ------------------------------------------------------------ */
typedef struct {
    /* Memory sizing — 0 means "use default" */
    uint64_t stack_size;   /* --stack=<N>[KMG] */
    uint64_t heap_size;    /* --heap=<N>[KMG]  */

    /* Runtime behaviour */
    int      run_doe;      /* --doe            (0 = off) */
    int      log_level;    /* --log-level=...  (KARGS_LOG_*)  */

    /* Raw unparsed cmdline; always NUL-terminated */
    char     raw[KERNEL_ARGS_CMDLINE_MAX];
} KernelArgs;

#endif /* STARKERNEL_KERNEL_ARGS_H */
