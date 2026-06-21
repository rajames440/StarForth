/*
 * StarKernel Q48.16 fixed-point stubs
 *
 * Minimal implementations for kernel build - floating point
 * conversion is not needed for core VM operation.
 */

#include "q48_16.h"

/**
 * @brief Convert a @c double to Q48.16 fixed-point (kernel stub — always 0).
 *
 * Floating-point arithmetic is unavailable in the freestanding kernel build,
 * so this function is a deliberate no-op stub. It discards @p d and returns
 * a zero-valued @c q48_16_t. Shared VM code that calls @c q48_from_double()
 * only uses it for logging and diagnostic output (e.g., @c WORD-ENTROPY),
 * which are suppressed in the kernel build, so the zero return is safe.
 *
 * @param d Floating-point value to convert (ignored).
 * @return Zero-valued @c q48_16_t.
 */
q48_16_t q48_from_double(double d)
{
    (void)d;
    q48_16_t zero = {0};
    return zero;
}

/**
 * @brief Convert a Q48.16 fixed-point value to @c double (kernel stub — always 0.0).
 *
 * Floating-point arithmetic is unavailable in the freestanding kernel build,
 * so this function is a deliberate no-op stub. It discards @p q and returns
 * @c 0.0. Shared VM code that calls @c q48_to_double() only uses the result
 * for logging and RStudio/CSV export paths that are inactive in the kernel
 * build, so the zero return is safe.
 *
 * @param q Q48.16 value to convert (ignored).
 * @return @c 0.0 always.
 */
double q48_to_double(q48_16_t q)
{
    (void)q;
    return 0.0;
}
