/*
 * cmdline.c — kernel boot command-line parser
 *
 * Pure C99 / freestanding.  No UEFI types, no libc.
 * The caller (uefi_loader.c) converts LoadOptions from UCS-2 to ASCII
 * before passing the string here.
 */

#include "starkernel/cmdline.h"
#include "starkernel/kernel_args.h"

/* ---- freestanding string helpers --------------------------------------- */

static int sf_isspace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int sf_strncmp(const char *a, const char *b, unsigned int n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (!n) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

static void sf_strlcpy(char *dst, const char *src, unsigned int n) {
    unsigned int i;
    for (i = 0; i + 1 < n && src[i]; i++) dst[i] = src[i];
    if (n) dst[i] = '\0';
}

/* ---- Size-suffix parser ------------------------------------------------ */

uint64_t cmdline_parse_size(const char *s) {
    uint64_t val = 0;
    if (!s || !(*s >= '0' && *s <= '9')) return 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10U + (uint64_t)(*s - '0');
        s++;
    }
    if      (*s == 'K' || *s == 'k') val *= 1024ULL;
    else if (*s == 'M' || *s == 'm') val *= 1024ULL * 1024ULL;
    else if (*s == 'G' || *s == 'g') val *= 1024ULL * 1024ULL * 1024ULL;
    return val;
}

/* ---- Log-level parser -------------------------------------------------- */

static int parse_log_level(const char *s) {
    if (sf_strncmp(s, "debug", 5) == 0 || sf_strncmp(s, "DEBUG", 5) == 0)
        return KARGS_LOG_DEBUG;
    if (sf_strncmp(s, "info",  4) == 0 || sf_strncmp(s, "INFO",  4) == 0)
        return KARGS_LOG_INFO;
    if (sf_strncmp(s, "warn",  4) == 0 || sf_strncmp(s, "WARN",  4) == 0)
        return KARGS_LOG_WARN;
    if (sf_strncmp(s, "error", 5) == 0 || sf_strncmp(s, "ERROR", 5) == 0)
        return KARGS_LOG_ERROR;
    return KARGS_LOG_INFO;  /* unknown → info (safe default) */
}

/* ---- Main parser ------------------------------------------------------- */

void cmdline_parse_ascii(const char *cmdline, KernelArgs *args) {
    const char *p;
    const char *tok;
    unsigned int tok_len;

    /* Fill defaults first */
    args->stack_size = 0;
    args->heap_size  = 0;
    args->run_doe    = KARGS_DEFAULT_RUN_DOE;
    args->log_level  = KARGS_DEFAULT_LOG_LEVEL;
    args->raw[0]     = '\0';

    if (!cmdline || !*cmdline) return;

    sf_strlcpy(args->raw, cmdline, KERNEL_ARGS_CMDLINE_MAX);

    p = cmdline;
    while (*p) {
        /* Skip leading whitespace */
        while (*p && sf_isspace(*p)) p++;
        if (!*p) break;

        /* Measure token */
        tok = p;
        while (*p && !sf_isspace(*p)) p++;
        tok_len = (unsigned int)(p - tok);

        /* --doe */
        if (tok_len == 5 && sf_strncmp(tok, "--doe", 5) == 0) {
            args->run_doe = 1;

        /* --log-level=<level>  (prefix = 12 chars) */
        } else if (tok_len > 12 && sf_strncmp(tok, "--log-level=", 12) == 0) {
            args->log_level = parse_log_level(tok + 12);

        /* --stack=<size>  (prefix = 8 chars) */
        } else if (tok_len > 8 && sf_strncmp(tok, "--stack=", 8) == 0) {
            args->stack_size = cmdline_parse_size(tok + 8);

        /* --heap=<size>  (prefix = 7 chars) */
        } else if (tok_len > 7 && sf_strncmp(tok, "--heap=", 7) == 0) {
            args->heap_size = cmdline_parse_size(tok + 7);
        }
        /* Unknown flags are silently ignored (forward-compatibility) */
    }
}
