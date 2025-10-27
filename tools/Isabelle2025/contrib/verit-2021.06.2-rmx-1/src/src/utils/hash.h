/*
  --------------------------------------------------------------
  Hash tables
  --------------------------------------------------------------
*/

#ifndef __HASH_H
#define __HASH_H

#include "utils/types.h"

#include <stdio.h>

/* PF do the size of hash tables grow? */
#define HASH_ADAPTIVE_SIZE

/* PF print statistics about hash tables  */
/* #define HASH_STAT */

typedef struct TShash* Thash;

/* PF creates a new hash table,
   initially of size given in the first argument,
   hash_function is used to get the hash key from objects
   equal is the equality function between objects
   free function is the function used to free objects when table released */
Thash hash_new(
	unsigned size, TFhash hash_function, TFequal equal, TFfree free_function);
/* PF Release table and apply free_function to all objects in table */
void hash_free(Thash* Phash);

/* PF look for an object that is equal to P */
void* hash_lookup(Thash hash, void* P);
/* PF insert object P */
void hash_insert(Thash hash, void* P);
/* PF remove object P */
void hash_remove(Thash hash, void* P);
/* PF Empty table contents */
void hash_erase(Thash hash);

/* PF apply function f on every pointer in hash table */
void hash_apply(Thash hash, TFapply f);
/* PF print some statistics about hash table use */
void hash_print_stats(Thash hash);

#endif /* __HASH_H */
