/*
 * cmdline.h — kernel command-line parser API
 *
 * Pure C99 / freestanding — no UEFI types, no libc.
 * The UEFI loader converts LoadOptions (UCS-2) to ASCII before calling
 * cmdline_parse_ascii(); the parser itself never sees CHAR16.
 */

#ifndef STARKERNEL_CMDLINE_H
#define STARKERNEL_CMDLINE_H

#include "kernel_args.h"

/*
 * cmdline_parse_ascii — parse a NUL-terminated ASCII cmdline into *args.
 *
 * Fills all default values first; unknown flags are silently ignored so
 * future kernels can add flags without breaking old boot entries.
 * Safe to call with NULL or empty cmdline (returns all defaults).
 *
 * Recognised flags:
 *   --doe                    set run_doe = 1
 *   --log-level=<level>      debug / info / warn / error
 *   --stack=<N>[KMG]         stack_size in bytes
 *   --heap=<N>[KMG]          heap_size in bytes
 */
void cmdline_parse_ascii(const char *cmdline, KernelArgs *args);

/*
 * cmdline_parse_size — parse a size token with optional K/M/G suffix.
 *
 * Examples: "2M" -> 2*1024*1024, "4G" -> 4*1024^3, "65536" -> 65536.
 * Returns 0 on parse error (empty string, non-digit start, etc.).
 */
uint64_t cmdline_parse_size(const char *s);

#endif /* STARKERNEL_CMDLINE_H */
