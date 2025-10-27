/*
  -------------------------------------------------------------------
  Bit-vector based set representation.
  -------------------------------------------------------------------
*/
#ifndef __BITSET_H
#define __BITSET_H

#include <stdbool.h>

typedef struct TSbs
{
	unsigned size;
	unsigned char v[];
} * Tbs;

extern Tbs bitset_new(unsigned size);
extern void bitset_free(Tbs set);
extern void bitset_insert(Tbs set, unsigned val);
extern void bitset_remove(Tbs set, unsigned val);
extern void bitset_union(Tbs res, Tbs set1, Tbs set2);
extern void bitset_diff(Tbs res, Tbs set1, Tbs set2);
extern bool bitset_empty(Tbs set);
extern bool bitset_equal(Tbs set1, Tbs set2);
extern bool bitset_subseteq(Tbs set1, Tbs set2);
extern bool bitset_in(Tbs set, unsigned val);
extern unsigned bitset_card(Tbs set);
#ifdef DEBUG
extern void bitset_fprint(FILE* file, Tbs set);
#endif

#endif
