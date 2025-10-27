#ifndef __VERIT_QSORT_H
#define __VERIT_QSORT_H
#include <stdlib.h> /* for size_t */

#ifdef PEDANTIC
#define veriT_qsort qsort
#else
#ifdef NATIVE_QSORT
#define veriT_qsort qsort
#else
/**
   To avoid difference caused by different implementations of 
   the standard C library qsort, we single out one (Next/Apple)
   for its permissive license and copy it here. */
extern void veriT_qsort(
	void*, size_t, size_t, int (*)(const void*, const void*));
#endif
#endif
#endif /* __VERIT_QSORT_H */
