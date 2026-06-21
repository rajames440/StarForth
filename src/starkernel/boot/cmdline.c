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

/**
 * @brief Test whether a character is ASCII whitespace.
 *
 * Returns non-zero for space (@c ' '), horizontal tab (@c '\t'),
 * carriage return (@c '\r'), and line feed (@c '\n'). Used by
 * @c cmdline_parse_ascii() to skip inter-token whitespace and by
 * @c load_cfg_from_esp() to strip trailing CR/LF from the config file.
 * Replaces the libc @c isspace() which is not available in a freestanding
 * build; this version does not consider locale.
 *
 * @param c Character to test.
 * @return Non-zero if @p c is ASCII whitespace, 0 otherwise.
 */
static int sf_isspace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/**
 * @brief Compare at most @p n characters of two strings (freestanding).
 *
 * Iterates up to @p n bytes, stopping at the first differing character
 * or when either string's null terminator is reached. Returns 0 if the
 * first @p n characters are equal (or both strings end before @p n),
 * positive if @p a is lexicographically greater, negative if @p b is.
 * Comparison is on @c unsigned @c char values per C standard convention.
 *
 * Used exclusively by @c cmdline_parse_ascii() and @c parse_log_level()
 * to match fixed-length flag prefixes without a libc dependency.
 *
 * @param a First string.
 * @param b Second string.
 * @param n Maximum number of characters to compare.
 * @return 0 if equal, positive if @p a > @p b, negative if @p a < @p b.
 */
static int sf_strncmp(const char *a, const char *b, unsigned int n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (!n) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

/**
 * @brief Copy a string into a fixed-size buffer with guaranteed NUL termination.
 *
 * Copies at most @p n-1 bytes from @p src into @p dst and always writes
 * a NUL terminator at @c dst[@p n-1] (if @p n > 0). This is the
 * BSD @c strlcpy() semantics without returning the source length.
 * If @p n is 0 the function is a no-op (no write, no UB).
 *
 * Used by @c cmdline_parse_ascii() to copy the raw command-line string
 * into @c KernelArgs.raw with a guaranteed NUL, avoiding the truncation
 * risk of @c strncpy().
 *
 * @param dst  Destination buffer; receives at most @p n-1 bytes of @p src
 *             followed by a NUL terminator.
 * @param src  NUL-terminated source string.
 * @param n    Size of @p dst in bytes (including the NUL slot).
 */
static void sf_strlcpy(char *dst, const char *src, unsigned int n) {
    unsigned int i;
    for (i = 0; i + 1 < n && src[i]; i++) dst[i] = src[i];
    if (n) dst[i] = '\0';
}

/* ---- Size-suffix parser ------------------------------------------------ */

/**
 * @brief Parse a decimal integer with optional K/M/G size suffix.
 *
 * Reads a non-negative decimal integer from @p s and multiplies it by:
 * - 1024 for 'K' or 'k' (kibibytes)
 * - 1024² for 'M' or 'm' (mebibytes)
 * - 1024³ for 'G' or 'g' (gibibytes)
 * - 1 for any other or absent suffix
 *
 * Returns 0 if @p s is NULL, empty, or does not begin with a digit.
 * Overflow is not checked; caller must validate the resulting value
 * is sensible for its intended use (stack size, heap size, etc.).
 *
 * Used by @c cmdline_parse_ascii() to decode the @c --stack=@<size@> and
 * @c --heap=@<size@> arguments from the boot command line.
 *
 * @param s NUL-terminated string beginning with a decimal integer
 *          and optionally followed by a size suffix character.
 * @return Parsed byte count (0 on parse failure or zero input).
 */
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

/**
 * @brief Map a log-level name string to a @c KARGS_LOG_* constant.
 *
 * Performs case-insensitive prefix matching against the known level names:
 * - "debug" / "DEBUG" → @c KARGS_LOG_DEBUG
 * - "info"  / "INFO"  → @c KARGS_LOG_INFO
 * - "warn"  / "WARN"  → @c KARGS_LOG_WARN
 * - "error" / "ERROR" → @c KARGS_LOG_ERROR
 *
 * Any unrecognised string (including empty) maps to @c KARGS_LOG_INFO
 * as a safe default. Called from @c cmdline_parse_ascii() after
 * recognising the @c --log-level= prefix.
 *
 * @param s String beginning at the first character of the level name
 *          (i.e., after the @c "=" separator has been skipped by the caller).
 * @return One of the @c KARGS_LOG_* constants defined in @c kernel_args.h;
 *         defaults to @c KARGS_LOG_INFO for unknown input.
 */
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

/**
 * @brief Parse a NUL-terminated ASCII command-line string into @c KernelArgs.
 *
 * Populates @p args from space-separated tokens in @p cmdline. Always
 * initialises all fields to their defaults first (so partial or empty
 * command lines produce safe values):
 * - @c stack_size = 0 (use BSS static stack)
 * - @c heap_size  = 0 (use built-in kmalloc arena)
 * - @c run_doe    = @c KARGS_DEFAULT_RUN_DOE
 * - @c log_level  = @c KARGS_DEFAULT_LOG_LEVEL
 * - @c raw[0]     = '\0'
 *
 * Recognised flags:
 * - @c --doe                 — sets @c args->run_doe = 1.
 * - @c --log-level=@<level@> — sets @c args->log_level via
 *                              @c parse_log_level().
 * - @c --stack=@<size@>      — sets @c args->stack_size via
 *                              @c cmdline_parse_size().
 * - @c --heap=@<size@>       — sets @c args->heap_size via
 *                              @c cmdline_parse_size().
 *
 * Unknown flags are silently ignored for forward-compatibility with
 * flags introduced in future kernel versions. The raw command-line string
 * is copied verbatim into @c args->raw (truncated to
 * @c KERNEL_ARGS_CMDLINE_MAX - 1) for diagnostic logging.
 *
 * Called from @c efi_main() (in @c uefi_loader.c) after assembling the
 * ASCII command line from the three priority sources: NVRAM variable,
 * @c starforth.cfg on the ESP, and EFI @c LoadOptions.
 *
 * @param cmdline NUL-terminated ASCII command line; may be NULL or empty.
 * @param args    Output @c KernelArgs structure; must not be NULL.
 */
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
