/*
 * StarKernel Q48.16 fixed-point stubs
 *
 * Minimal implementations for kernel build - floating point
 * conversion is not needed for core VM operation.
 */

#include "q48_16.h"

/* Stub: convert double to Q48.16 (returns 0) */
q48_16_t q48_from_double(double d)
{
    (void)d;
    q48_16_t zero = {0};
    return zero;
}

/* Stub: convert Q48.16 to double (returns 0.0) */
double q48_to_double(q48_16_t q)
{
    (void)q;
    return 0.0;
}
