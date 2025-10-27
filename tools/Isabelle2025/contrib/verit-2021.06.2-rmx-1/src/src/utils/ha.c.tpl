/* -*- mode: C -*- */
/*
  --------------------------------------------------------------
  Hash arrays
  --------------------------------------------------------------
*/

/*
  Required definitions
  TYPE_EXT
  extension for types and function names (e.g. DAG for TDAG)
  TYPE
  type of elements in the hash array (should never be 0)
  TYPE_NULL
  NULL element for TYPE.  Should be 0 in memory
  bool HA_CMP(TYPE, TYPE)
  test function for equality of two elements
  unsigned HA_HASH(TYPE)
  hash key from element

  Optional definitions
  HA_AUTO_RESIZE
  if set (defined) the hash array is auto resized
  !!! if so, ha_enter returns a possible new hash array
  HA_DEBUG
  if set (defined) assertions are checked and ha_print is compiled
  void HA_PRINT(TYPE)
  output TYPE on stderr (for ha_print and debugging purposes)
  HA_STAT
  if set (defined) some stats are collected
  HA_FREE
  if set, elements are freed when hash array is freed
*/

#ifdef HA_DEBUG
#include <stdio.h>
#endif
#ifdef HA_STAT
#include "utils/statistics.h"
#endif

#define CC2(A, B) A##B
/* force prescan */
#define CC(A, B) CC2(A, B)
#define CC3(A, B, C) CC(CC(A, B), C)
#define EXT(A) CC3(A, _, TYPE_EXT)
#define EXTHA(A) CC3(ha_, TYPE_EXT, A)

#define Tbucket EXT(Tbucket)
#define TSha EXT(TSha)
#define Tha EXT(Tha)

#define ha_resize EXTHA(_resize)
#define ha_query EXTHA(_query)
#define ha_enter EXTHA(_enter)
#define ha_densify EXTHA(_densify)
#define ha_del EXTHA(_del)
#define ha_remove EXTHA(_remove)
#define ha_apply EXTHA(_apply)
#define ha_new EXTHA(_new)
#define ha_free EXTHA(_free)
#define ha_print EXTHA(_print)

typedef struct Tbucket
{
	unsigned k;
	TYPE e;
} Tbucket;

typedef struct TSha
{
	unsigned mask;
#ifdef HA_AUTO_RESIZE
	unsigned n;
#endif
	Tbucket bucket[];
} TSha;

typedef struct TSha* Tha;

/*
  --------------------------------------------------------------
  Private functions
  --------------------------------------------------------------
*/

/**
   \brief resize the hash array
   \param ha the hash array
   \param size the new size, should be a power of two
   \return the new hash array
   \remark private function */
static inline Tha
ha_resize(Tha ha, unsigned new_alloc)
{
	unsigned i;
	Tha ha_old = ha;
	unsigned n = ha->mask ? ha->mask + 1 : 0;
	if (new_alloc <= n) /* never decrease size */
		return ha;
	assert(((new_alloc - 1) & new_alloc) == 0); /* checking that power of two */
#ifdef HA_AUTO_RESIZE
	MY_MALLOC(ha, 2 * sizeof(unsigned) + new_alloc * sizeof(Tbucket));
	ha->n = ha_old->n;
#else
	MY_MALLOC(ha, sizeof(unsigned) + new_alloc * sizeof(Tbucket));
#endif
	ha->mask = new_alloc - 1;
	memset(&(ha->bucket), 0, new_alloc * sizeof(Tbucket));
	for (i = 0; i < n; i++)
		if (ha_old->bucket[i].e) /* e, not k: k might be 0 */
		{
			unsigned j = ha_old->bucket[i].k & ha->mask;
			while (ha->bucket[j].e) j = (j + 1) & ha->mask;
			ha->bucket[j].k = ha_old->bucket[i].k;
			ha->bucket[j].e = ha_old->bucket[i].e;
		}
	free(ha_old);
	return ha;
}

/**
   \brief densify hash array
   \param ha the hash array
   \param i the position of the element to be removed
   \remark private function */
static inline void
ha_densify(Tha ha, unsigned i)
{
	unsigned j, k;
	for (j = (i + 1) & ha->mask; ha->bucket[j].e; j = (j + 1) & ha->mask) {
		/* invariants: i is free. j != i (because table is not full)
         All l between j and i (cycle) could not occupy i */
		k = ha->bucket[j].k & ha->mask;
		/* determine if k lies cyclically in ]i,j]
         | i.k.j |, |.j i.k.| or |.k.j i.| */
		if ((k <= i) + (i <= j) + (j < k) <= 1) /* In fact == 1 */
			continue;
		ha->bucket[i].k = ha->bucket[j].k;
		ha->bucket[i].e = ha->bucket[j].e;
		i = j;
	}
	ha->bucket[i].k = 0;
	ha->bucket[i].e = TYPE_NULL;
#ifdef HA_AUTO_RESIZE
	ha->n--;
#endif
}

/*
  --------------------------------------------------------------
  Public functions
  --------------------------------------------------------------
*/

/**
   \brief get the element that is HA_CMP to e
   \param ha the hash array
   \param e the element
   \return the element HA_CMP to e, TYPE_NULL if none
   \remark public function */
static inline TYPE
ha_query(Tha ha, TYPE e)
{
	unsigned k = HA_HASH(e);
	unsigned i = k & ha->mask;
	while (ha->bucket[i].e && /* e, not k: k might be 0 */
				 (k != ha->bucket[i].k || !HA_CMP(e, ha->bucket[i].e)))
		i = (i + 1) & ha->mask;
	return ha->bucket[i].e;
}

/**
   \brief adds an elment to hash array
   \param ha the hash array
   \param e the elemnt
   \return for autoresize case, return the new hash array
   \remark public function */
static inline
#ifdef HA_AUTO_RESIZE
	Tha
#else
	void
#endif
	ha_enter(Tha ha, TYPE e)
{
	unsigned k = HA_HASH(e);
	unsigned i;
#ifdef HA_AUTO_RESIZE
	ha->n++;
	if (ha->n > (ha->mask >> 1)) ha = ha_resize(ha, (ha->mask + 1) << 1);
#endif
	i = k & ha->mask;
	while (ha->bucket[i].e) /* e, not k: k might be 0 */
	{
		assert(k != ha->bucket[i].k || !HA_CMP(e, ha->bucket[i].e));
		i = (i + 1) & ha->mask;
	}
	ha->bucket[i].k = k;
	ha->bucket[i].e = e;
#ifdef HA_STAT
	stats_easy_inc("ha_enter", "number of insertion in ha", "%6d");
	if ((k & ha->mask) == i) return;
	stats_easy_inc("ha_collision", "number of collisions in ha", "%6d");
	if (i < (k & ha->mask)) i += ha->mask + 1;
	i -= k & ha->mask;
	stats_easy_max(
		"ha_max_collision", "max length of conflicting chunk", "%6d", (int)i);
	i = k & ha->mask;
	while (ha->bucket[i].e) {
		if (ha->bucket[i].k == k && !HA_CMP(hash_bucket[i].e, e))
			stats_easy_inc("ha_equal", "number of equal keys", "%6d");
		i = (i + 1) & hash_mask;
	}
#endif
#ifdef HA_AUTO_RESIZE
	return ha;
#endif
}

/**
   \brief remove element from hash array
   \param ha the hash array
   \param e the element
   \remark it is mandatory that e is present as such (=) in the hash array
   \see ha_remove
   \remark public function */
static inline void
ha_del(Tha ha, TYPE e)
{
	unsigned k = HA_HASH(e);
	unsigned i = k & ha->mask;
	while (ha->bucket[i].e != e) i = (i + 1) & ha->mask;
	ha_densify(ha, i);
}

/**
   \brief remove element equal to e in hash array
   \param ha the hash array
   \param e the element
   \return the element equal to e
   \remark the element might not belong to the hash array
   \remark public function */
static inline TYPE
ha_remove(Tha ha, TYPE e)
{
	unsigned i, k = HA_HASH(e);
	for (i = k & ha->mask; ha->bucket[i].e; i = (i + 1) & ha->mask)
		if (k == ha->bucket[i].k && HA_CMP(ha->bucket[i].e, e)) {
			TYPE e = ha->bucket[i].e;
			ha_densify(ha, i);
			return e;
		}
	return TYPE_NULL;
}

/**
   \brief applies f to every element in the signature table
   \param ha the hash array
   \param f a function from TYPE to void
   \remark public function */
static inline void
ha_apply(Tha ha, void (*f)(TYPE))
{
	unsigned i;
	for (i = 0; i <= ha->mask; i++)
		if (ha->bucket[i].e) f(ha->bucket[i].e);
}

/**
   \brief initialization of signature table */
static inline Tha
ha_new(unsigned size)
{
	Tha ha;
	MY_MALLOC(ha, sizeof(Tha));
	ha->mask = 0;
#ifdef HA_AUTO_RESIZE
	ha->n = 0;
#endif
	ha = ha_resize(ha, size);
	return ha;
#ifdef HASH_STAT
	stats_counter_set(stat_hash_collision, 0);
	stats_counter_set(stat_hash_insert, 0);
	stats_counter_set(stat_hash_max_collision, 0);
	stats_counter_set(stat_hash_eq_key, 0);
#endif
}

/**
   \brief release signature table */
static inline void
ha_free(Tha* Pha)
{
#ifdef HA_FREE
	unsigned i;
	for (i = 0; i <= (*Pha)->mask; i++) HA_FREE((*Pha)->bucket[i].e);
#else
#ifdef HA_DEBUG
	unsigned i;
	for (i = 0; i <= (*Pha)->mask; i++) assert(!(*Pha)->bucket[i].e);
#endif
#endif
	free(*Pha);
	*Pha = NULL;
}

#ifdef HA_DEBUG
void
ha_print(Tha ha)
{
	unsigned i;
	fprintf(stderr, "ha_print begin\n");
	for (i = 0; i <= ha->mask; i++) {
		TYPE e = ha->bucket[i].e;
		unsigned j;
		if (!e) continue;
		HA_PRINT(e);
		fprintf(
			stderr, " %s (%u)\n", (HA_HASH(e) != ha->bucket[i].key) ? "X" : "",
			ha->bucket[i].key);
	}
	fprintf(stderr, "ha_print end\n");
}
#endif

#undef CC2
#undef CC
#undef CC3
#undef EXT
#undef EXTHA

#undef Tbucket
#undef TSha
#undef Tha

#undef ha_resize
#undef ha_query
#undef ha_enter
#undef ha_densify
#undef ha_del
#undef ha_remove
#undef ha_apply
#undef ha_init
#undef ha_done
#undef ha_print
