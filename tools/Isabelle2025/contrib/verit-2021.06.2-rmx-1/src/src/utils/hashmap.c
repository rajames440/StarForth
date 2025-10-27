/*        Hashmap tables                                        */

/* #define DEBUG_HASHMAP */

#ifdef HASH_STAT
#include <stdio.h>
#endif
#include "utils/general.h"
#include "utils/hashmap.h"

#include <stdlib.h>
#include <string.h>

typedef struct Tbucket
{
	Tunsigned hash;
	void* key;
	void* value;
	struct Tbucket* next;
} Tbucket;

struct TShashmap
{
	Tunsigned size;
#ifdef HASH_ADAPTIVE_SIZE
	Tunsigned nb;
#endif
	Tbucket** Pbucket;
	TFhash hash_function;
	TFfree key_free_function;
	TFfree value_free_function;
	TFequal equal_function;
#ifdef HASH_STAT
	Tunsigned insert_collision;
	Tunsigned insert_total;
	Tunsigned max_collision;
#endif
};

Thashmap
hashmap_new(
	Tunsigned size, TFhash hash_function, TFequal equal_function,
	TFfree key_free_function, TFfree value_free_function)
{
	Thashmap hashmap;
	MY_MALLOC(hashmap, sizeof(struct TShashmap));
	hashmap->size = size;
	MY_MALLOC(hashmap->Pbucket, size * sizeof(Tbucket*));
	/* PF caution: keep following line even if duplicate in SAFE mode */
	memset(hashmap->Pbucket, 0, size * sizeof(Tbucket*));
	hashmap->hash_function = hash_function;
	hashmap->equal_function = equal_function;
	hashmap->key_free_function = key_free_function;
	hashmap->value_free_function = value_free_function;
#ifdef HASH_ADAPTIVE_SIZE
	hashmap->nb = 0;
#endif
#ifdef HASH_STAT
	hashmap->insert_collision = 0;
	hashmap->insert_total = 0;
	hashmap->max_collision = 0;
#endif
	return hashmap;
}

void
hashmap_free(Thashmap* Phashmap)
{
	hashmap_erase(*Phashmap);
	free((*Phashmap)->Pbucket);
	free(*Phashmap);
	(*Phashmap) = NULL;
}

void
hashmap_erase(Thashmap hashmap)
{
	Tunsigned i;
	Tbucket *Pbucket1, *Pbucket2;

	if (!hashmap) return;

	for (i = 0; i < hashmap->size; i++)
		for (Pbucket1 = hashmap->Pbucket[i]; Pbucket1; Pbucket1 = Pbucket2) {
			Pbucket2 = Pbucket1->next;
			if (hashmap->key_free_function) hashmap->key_free_function(Pbucket1->key);
			if (hashmap->value_free_function)
				hashmap->value_free_function(Pbucket1->value);
			free(Pbucket1);
		}
	memset(hashmap->Pbucket, 0, hashmap->size * sizeof(Tbucket*));
}

#ifdef HASH_ADAPTIVE_SIZE
static void
hashmap_double_size(Thashmap hashmap)
{
	register Tunsigned i;
	register Tbucket **PPbucket, *Pbucket1, *Pbucket2;
	register Tunsigned hash;

	MY_REALLOC(hashmap->Pbucket, 2 * hashmap->size * sizeof(Tbucket*));
	memset(hashmap->Pbucket + hashmap->size, 0, hashmap->size * sizeof(Tbucket*));
	hashmap->size *= 2;
	for (i = 0; i < hashmap->size / 2; i++) {
		PPbucket = &hashmap->Pbucket[i];
		for (Pbucket1 = *PPbucket; Pbucket1;)
			if ((hash = Pbucket1->hash % hashmap->size) != i) {
				Pbucket2 = Pbucket1->next;
				Pbucket1->next = hashmap->Pbucket[hash];
				hashmap->Pbucket[hash] = Pbucket1;
				*PPbucket = Pbucket2;
				Pbucket1 = Pbucket2;
			} else {
				PPbucket = &Pbucket1->next;
				Pbucket1 = Pbucket1->next;
			}
	}
}
#endif

void**
hashmap_lookup(Thashmap hashmap, void* key)
{
	Tbucket* Pbucket;
	Tunsigned hash = hashmap->hash_function(key);

	for (Pbucket = hashmap->Pbucket[hash % hashmap->size]; Pbucket;
			 Pbucket = Pbucket->next)
		if (
			(hash == Pbucket->hash) && (hashmap->equal_function(key, Pbucket->key))) {
#ifdef DEBUG_HASH
			fprintf(stderr, "hash_lookup %8x, found %8x\n", key, Pbucket->key);
#endif
			return &(Pbucket->value);
		}
	return NULL;
}

void
hashmap_insert(Thashmap hashmap, void* key, void* value)
{
	Tbucket* Pbucket;
	Tunsigned hash = hashmap->hash_function(key);
#ifdef DEBUG_HASH
	fprintf(stderr, "hashmap_insert %8x\n", key);
#endif
#ifdef HASH_ADAPTIVE_SIZE
	++hashmap->nb;
	if (hashmap->nb > hashmap->size >> 1) hashmap_double_size(hashmap);
#endif
	MY_MALLOC(Pbucket, sizeof(Tbucket));
	Pbucket->hash = hash;
	Pbucket->key = key;
	Pbucket->value = value;
	Pbucket->next = hashmap->Pbucket[hash % hashmap->size];
	hashmap->Pbucket[hash % hashmap->size] = Pbucket;
#ifdef HASH_STAT
	{
		Tunsigned i = 1;
		hashmap->insert_total++;
		if (Pbucket->next) hashmap->insert_collision++;
		while ((Pbucket = Pbucket->next)) i++;
		if (hashmap->max_collision < i) hashmap->max_collision = i;
	}
#endif
}

void
hashmap_remove(Thashmap hashmap, void* key)
{
	Tbucket** PPbucket;
	Tunsigned hash = hashmap->hash_function(key);
#ifdef DEBUG_HASH
	fprintf(stderr, "hashmap_remove %8x\n", key);
#endif
	for (PPbucket = &hashmap->Pbucket[hash % hashmap->size]; *PPbucket;
			 PPbucket = &(*PPbucket)->next)
		if (
			(hash == (*PPbucket)->hash) &&
			(hashmap->equal_function(key, (*PPbucket)->key))) {
			Tbucket* Pbucket2 = *PPbucket;
			*PPbucket = Pbucket2->next;
			if (hashmap->key_free_function) hashmap->key_free_function(Pbucket2->key);
			if (hashmap->value_free_function)
				hashmap->value_free_function(Pbucket2->value);
			free(Pbucket2);
#ifdef HASH_ADAPTIVE_SIZE
			hashmap->nb--;
#endif
			return;
		}
	my_error("hashmap_remove: object not found\n");
}

void
hashmap_apply(Thashmap hashmap, TFapply2 f)
{
	Tunsigned i;
	Tbucket* Pbucket;
	for (i = 0; i < hashmap->size; i++)
		for (Pbucket = hashmap->Pbucket[i]; Pbucket; Pbucket = Pbucket->next)
			f(Pbucket->key, Pbucket->value);
}

#ifdef HASH_STAT
void
hashmap_fprint_stats(FILE* file, Thashmap hashmap)
{
	fprintf(file, "Size %lu\n", hashmap->size);
	fprintf(file, "Insert collisions %lu\n", hashmap->insert_collision);
	fprintf(file, "Insert total %lu\n", hashmap->insert_total);
	fprintf(file, "Max collision %lu\n", hashmap->max_collision);
}
#endif
