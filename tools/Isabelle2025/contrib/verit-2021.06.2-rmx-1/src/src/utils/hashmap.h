/*
  --------------------------------------------------------------
  Maps through hashing
  --------------------------------------------------------------
*/
#ifndef __HASHMAP_H
#define __HASHMAP_H

#ifdef HASH_STAT
#include <stdio.h>
#endif

#include "utils/assoc.h"
#include "utils/types.h"

typedef struct TShashmap* Thashmap;

extern Thashmap hashmap_new(
	Tunsigned size, TFhash, TFequal, TFfree key_free, TFfree value_free);
extern void hashmap_free(Thashmap* Phashmap);
extern void hashmap_erase(Thashmap hashmap);

extern void** hashmap_lookup(Thashmap hashmap, void* key);
extern void hashmap_insert(Thashmap hashmap, void* key, void* value);
extern void hashmap_remove(Thashmap hashmap, void* key);
extern void hashmap_apply(Thashmap hashmap, TFapply2 fun);

#ifdef HASH_STAT
extern void hashmap_fprint_stats(FILE* file, Thashmap hashmap);
#endif

#endif /* __HASHMAP_H */
