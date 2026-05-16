/* Freestanding inttypes.h shim — PRI* format macros for kernel builds. */
#ifndef FREESTANDING_INTTYPES_H
#define FREESTANDING_INTTYPES_H
#include <stdint.h>
#define PRId8   "d"
#define PRId16  "d"
#define PRId32  "d"
#define PRId64  "lld"
#define PRIu8   "u"
#define PRIu16  "u"
#define PRIu32  "u"
#define PRIu64  "llu"
#define PRIx8   "x"
#define PRIx16  "x"
#define PRIx32  "x"
#define PRIx64  "llx"
#define PRIX8   "X"
#define PRIX16  "X"
#define PRIX32  "X"
#define PRIX64  "llX"
#define PRIi8   "d"
#define PRIi16  "d"
#define PRIi32  "d"
#define PRIi64  "lld"
#endif /* FREESTANDING_INTTYPES_H */
