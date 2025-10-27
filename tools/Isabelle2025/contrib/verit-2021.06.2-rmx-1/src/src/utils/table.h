/*
  --------------------------------------------------------------
  table data structure
  --------------------------------------------------------------
*/

#ifndef __TABLE_H
#define __TABLE_H
#include "utils/types.h"

/* PF A table is basically a growing array */

#define TABLE_MACRO

#ifdef TABLE_MACRO
struct TStable
{
	unsigned last;
	unsigned size;
	void** P;
	unsigned increment;
#ifdef PEDANTIC
	unsigned unused;
#endif
};
#endif

typedef struct TStable* Ttable;

/* PF builds a table with initial size, and growing by adding increment */
Ttable table_new(unsigned size, unsigned increment);
void table_free(Ttable* Ptable);

/* PF initialize table such that it contains length elements, null initially */
void table_init(Ttable table, unsigned length);
/* PF resize table such that it contains length elements.
   New elements are set to null initially */
void table_resize(Ttable table, unsigned length);

/* PF returns number of elements in table */
#ifdef TABLE_MACRO
#define table_length(T) T->last
#else
int table_length(Ttable table);
#endif
/* PF returns the increment size of table */
unsigned table_increment(Ttable table);
/* PF adds P on top of table */
void table_push(Ttable table, void* P);
/* PF returns top of table, and remove element from table */
void* table_pop(Ttable table);
/* PF returns top of table, and let table unchanged */
void* table_top(Ttable table);
/* PF returns 1 iff table is empty, 0 else */
#ifdef TABLE_MACRO
#define table_empty(T) (T->last == 0)
#else
int table_empty(Ttable table);
#endif
/* PF returns i-th element */
#ifdef TABLE_MACRO
#define table_get(T, I) T->P[I]
#else
void* table_get(Ttable table, unsigned i);
#endif
/* PF set i-th element */
#ifdef TABLE_MACRO
#define table_set(T, I, E) (T)->P[I] = (void*)(E)
#else
void table_set(Ttable table, unsigned i, void* P);
#endif
/* PF inserts P at position i */
void table_insert(Ttable table, unsigned i, void* P);
/* PF delete i-th element */
void* table_del(Ttable table, unsigned i);
/* PF table is set to have 0 elements */
void table_erase(Ttable table);
/* PF applies f to every element in table */
void table_apply(Ttable table, void (*f)(void*));
/* PF size of table is set to be exactly the number of elements,
   further adding only one element will imply a realloc */
void table_shrink(Ttable table);

/* PF inserts P in a sorted table */
void table_insert_sort(Ttable table, void* P, TFcmp compare);
/* PF sort table so that lookup would be in ln n */
void table_sort(Ttable table, TFcmp compare);
/* PF return element such that lookup_function(element, criteria) = 0
   DD or NULL if no such element exist.
   Linear */
void* table_lookup(Ttable table, TFcmp lookup, void* criteria);
/* PF return element such that compare(element, criteria) = 0.
   For sorted tables only! n ln n. */
void* table_lookup_sort(Ttable table, TFcmp compare, void* criteria);

#endif /* __TABLE_H */
