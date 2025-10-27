/*        Hash tables                                           */

/* #define DEBUG_HASH */

#include "utils/hash.h"

#include "utils/general.h"
#include "utils/types.h"
#include "veriT-config.h"

#include <stdlib.h>
#include <string.h>

typedef struct Tbucket
{
	unsigned key;
	void* P;
	struct Tbucket* next;
} Tbucket;

struct TShash
{
	unsigned size;
#ifdef HASH_ADAPTIVE_SIZE
	unsigned nb;
#endif
	Tbucket** Pbucket;
	TFhash hash_function;
	TFfree free_function;
	TFequal equal;
#ifdef HASH_STAT
	TFunsigned insert_collision;
	TFunsigned insert_total;
	TFunsigned max_collision;
	TFunsigned eq_key;
#endif
};

Thash
hash_new(
	unsigned size, TFhash hash_function, TFequal equal, TFfree free_function)
{
	Thash hash;
	MY_MALLOC(hash, sizeof(struct TShash));
	hash->size = size;
	MY_MALLOC(hash->Pbucket, size * sizeof(Tbucket*));
	/* PF caution: keep following line even if duplicate in SAFE mode */
	memset(hash->Pbucket, 0, size * sizeof(Tbucket*));
	hash->hash_function = hash_function;
	hash->equal = equal;
	hash->free_function = free_function;
#ifdef HASH_ADAPTIVE_SIZE
	hash->nb = 0;
#endif
#ifdef HASH_STAT
	hash->insert_collision = 0;
	hash->insert_total = 0;
	hash->max_collision = 0;
	hash->eq_key = 0;
#endif
	return hash;
}

void
hash_free(Thash* Phash)
{
	hash_erase(*Phash);
	free((*Phash)->Pbucket);
	free(*Phash);
	(*Phash) = NULL;
}

void
hash_erase(Thash hash)
{
	unsigned i;
	Tbucket *Pbucket1, *Pbucket2;

	if (!hash) return;

	for (i = 0; i < hash->size; i++)
		for (Pbucket1 = hash->Pbucket[i]; Pbucket1; Pbucket1 = Pbucket2) {
			Pbucket2 = Pbucket1->next;
			if (hash->free_function) hash->free_function(Pbucket1->P);
			free(Pbucket1);
		}
	memset(hash->Pbucket, 0, hash->size * sizeof(Tbucket*));
}

#ifdef HASH_ADAPTIVE_SIZE
static void
hash_double_size(Thash hash)
{
	register unsigned i;
	register Tbucket **PPbucket, *Pbucket1, *Pbucket2;
	register unsigned key;

	MY_REALLOC(hash->Pbucket, 2 * hash->size * sizeof(Tbucket*));
	memset(hash->Pbucket + hash->size, 0, hash->size * sizeof(Tbucket*));
	hash->size *= 2;
	for (i = 0; i < hash->size / 2; i++) {
		PPbucket = &hash->Pbucket[i];
		for (Pbucket1 = *PPbucket; Pbucket1;)
			if ((key = Pbucket1->key % hash->size) != i) {
				Pbucket2 = Pbucket1->next;
				Pbucket1->next = hash->Pbucket[key];
				hash->Pbucket[key] = Pbucket1;
				*PPbucket = Pbucket2;
				Pbucket1 = Pbucket2;
			} else {
				PPbucket = &Pbucket1->next;
				Pbucket1 = Pbucket1->next;
			}
	}
}
#endif

void*
hash_lookup(Thash hash, void* P)
{
	Tbucket* Pbucket;
	unsigned key = hash->hash_function(P);

	for (Pbucket = hash->Pbucket[key % hash->size]; Pbucket;
			 Pbucket = Pbucket->next)
		if ((key == Pbucket->key) && (hash->equal(P, Pbucket->P))) {
#ifdef DEBUG_HASH
			my_message("hash_lookup %8x, found %8x\n", P, Pbucket->P);
#endif
			return Pbucket->P;
		}
	return NULL;
}

void
hash_insert(Thash hash, void* P)
{
	Tbucket* Pbucket;
	unsigned key = hash->hash_function(P);
#ifdef DEBUG_HASH
	my_message("hash_insert %8x\n", P);
#endif
#ifdef HASH_ADAPTIVE_SIZE
	hash->nb++;
	if (hash->nb > hash->size >> HASH_ADAPTIVE_RATIO) hash_double_size(hash);
#endif
	MY_MALLOC(Pbucket, sizeof(Tbucket));
	Pbucket->key = key;
	Pbucket->P = P;
	Pbucket->next = hash->Pbucket[key % hash->size];
	hash->Pbucket[key % hash->size] = Pbucket;
#ifdef HASH_STAT
	{
		int i = 1;
		hash->insert_total++;
		if (Pbucket->next) hash->insert_collision++;
		while ((Pbucket = Pbucket->next)) {
			i++;
			if (Pbucket->key == key) hash->eq_key++;
		}
		if (hash->max_collision < i) hash->max_collision = i;
	}
#endif
}

void
hash_remove(Thash hash, void* P)
{
	Tbucket** PPbucket;
	unsigned key = hash->hash_function(P);
#ifdef DEBUG_HASH
	my_message("hash_remove %8x\n", P);
#endif
	for (PPbucket = &hash->Pbucket[key % hash->size]; *PPbucket;
			 PPbucket = &(*PPbucket)->next)
		if ((key == (*PPbucket)->key) && (hash->equal(P, (*PPbucket)->P))) {
			Tbucket* Pbucket2 = *PPbucket;
			*PPbucket = Pbucket2->next;
			if (hash->free_function) hash->free_function(Pbucket2->P);
			free(Pbucket2);
#ifdef HASH_ADAPTIVE_SIZE
			hash->nb--;
#endif
			return;
		}
	my_error("hash_remove: object not found\n");
}

void
hash_apply(Thash hash, void (*f)(void*))
{
	unsigned i;
	Tbucket* Pbucket;
	for (i = 0; i < hash->size; i++)
		for (Pbucket = hash->Pbucket[i]; Pbucket; Pbucket = Pbucket->next)
			f(Pbucket->P);
}

void
hash_print_stats(Thash hash)
{
#ifdef HASH_STAT
	my_message(
		"Hash stats: %d, %d/%d (%d-%d)\n", hash->size, hash->insert_total,
		hash->insert_collision, hash->max_collision, hash->eq_key);
#else
#ifdef PEDANTIC
	/* PF avoid a warning when compiling in pedantic mode */
	printf("%p\n", (void*)hash);
#endif
#endif
}
