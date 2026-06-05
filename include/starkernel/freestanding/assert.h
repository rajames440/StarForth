/* Freestanding assert.h shim — asserts become no-ops in kernel builds. */
#ifndef FREESTANDING_ASSERT_H
#define FREESTANDING_ASSERT_H
#define assert(expr) ((void)(expr))
#define static_assert _Static_assert
#endif /* FREESTANDING_ASSERT_H */
