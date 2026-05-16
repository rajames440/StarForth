/* Freestanding stdlib.h shim for aarch64 COFF kernel builds.
 * malloc/free/calloc/realloc provided by shim.c; abort by shim.c. */
#ifndef FREESTANDING_STDLIB_H
#define FREESTANDING_STDLIB_H
#include <stddef.h>
void *malloc(size_t size);
void  free(void *ptr);
void *calloc(size_t n, size_t size);
void *realloc(void *ptr, size_t size);
void  abort(void);
void  qsort(void *base, size_t nmemb, size_t size,
            int (*compar)(const void *, const void *));
long  strtol(const char *str, char **endptr, int base);
unsigned long strtoul(const char *str, char **endptr, int base);
long long strtoll(const char *str, char **endptr, int base);
int   atoi(const char *str);
long  atol(const char *str);
#endif /* FREESTANDING_STDLIB_H */
