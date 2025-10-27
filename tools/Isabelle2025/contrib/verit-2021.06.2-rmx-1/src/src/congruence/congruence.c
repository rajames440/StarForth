/*
  --------------------------------------------------------------
  Module for congruence closure
  --------------------------------------------------------------
*/

/* TODO
   backfire is really dirty.  Couldn't we avoid that?
   Late addition of terms
   SYMB_VARIABLE: does this still makes sense?
   Lists or arrays? pred seems OK
   Ttable in models

   SYMB_VARIABLE is used in DAG_add_q.  This is obsolete, and I believe could
   lead to mistakes.  Check if we can trust the "ground" bit.

   CHECK:
   * new predicate and value
   * propagating equalities and inequalities
   * proofs

   200 Signature table
   700 Path finding for explanations
   1000 Conflict analysis
   1200 Core union
   1750 Asserting =, !=, and pred values
   1900 Backtracking data structures
   2050 Hook to increase CC data structures according to DAG numbers
   2150 Interface functions for assertions
   2200 Notifying new terms/formulas to CC
   2500 Frontend for conflict and proof production
   2800 Model output

   The module is not tolerant towards non clean terms (ite, lambdas, let)
   However it accepts quantifiers.

   In the predecessor lists, maybe not recopy predecessors that were already there

   CC_union does not care for its argument.  So maybe ensure first is largest class, to avoid to swap things?

   Injective functions: when merging two classes about a same function, also
   merge the children.  Needs a new kind of explanation. (for the moment, only
   literal and congruence)

   Random remarks
   * hint_CC third argument is necessary because in case of
   a != b being set as a hint because of some other inequality b' != a',
   a, a', and b, b' being pairwise in the same class,
   it may happen that, when querying the reason of this hint,
   all a, a', b, b' are in the same class, and the proof would then involve
   literals that were not present at the time the hint was generated, causing
   a bug.
   * Adding new term/predicate in CC should only be done before first assert.
   Otherwise, the backtracking stack is all messed up.  Predecessors list may
   also have to be modified in a way that ensures consistency after
   backtracking
   * One could try to add feature to propagate inequalities, e.g.
   f(a) != f(b) --> a != b
   f(a) != f(b) && b = c --> a != c
   f(a) != b && b = f(c) --> a != c
   This could be quite complicated.
   The signature table could be used to find out if a equality a = b exists
   with a, b in the same class as some given a', b' respectively
   But finding out the inequalities such that X!=Y with X, Y in the same
   class as some f(a), f(b) respectively is difficult.
   It is not only when an equality is to false that the finding of such
   inequalities should run, but also whenever the class of one member of an
   inequality is extended (because of merges)
   * Trivial equalities/inequalities are OK, but no theory propagation is done
   for them
   * for pred classes, maybe it is useless to update class for elements
   */

#ifdef DEBUG
#define DEBUG_CC 1
#else
#define DEBUG_CC 0
#endif

#include "veriT-config.h"

/* #define HASH_STAT */
#if STATS_LEVEL >= 2
#endif

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#if DEBUG_CC >= 2
#include <stdio.h>
#endif

#include "bool/bool.h"
#include "congruence/congruence.h"
#include "instantiation/free-vars.h"
#include "symbolic/DAG-symb.h"
#include "symbolic/recursion.h"
#include "undo.h"
#include "utils/general.h"
#include "utils/options.h"
#include "utils/stack.h"
#include "utils/statistics.h"
#ifdef CC_CLASS_LIST
#include "utils/dll-DAG.h"
#endif
#include "bool/literal.h"
#include "hint.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/DAG.h"
#include "veriT-state.h"

/* typedef enum Tboolean_value {FALSE = 0, TRUE = 1, UNDEFINED} Tboolean_value; */

/* One hash table for each symbol
   DAG : use hash_keys.
   How does it react with a != a
   p(a), !p(a) */

/**
   \brief specifies things that are not handled by congruence closure */
#define DEAD_NODE(DAG) \
	(quantifier(DAG_symb(DAG)) || DAG_symb(DAG) == LAMBDA || \
	 DAG_symb(DAG) == APPLY_LAMBDA || DAG_symb(DAG) == LET || \
	 boolean_connector(DAG_symb(DAG)))
/* IMPROVE it is still possible DEAD_NODE (APPLY_)LAMBDA are forced into CC
   (causing an error) in case they occur in quantified formulas */

typedef TDAG Tclass;
/* TSstack(_Tclass, Tclass); /\* typedefs Tstack_Tclass *\/ */

/**
   \brief table of class indexed by DAG */
Tclass* class;

typedef struct Tclass_info_f
{
	unsigned elem_nb; /*< nb of elements in class */
	TDAG elem; /*< next element in class */
	Tstack_DAG stack_pred; /*< stack of predecessors */
	/* PF Do not change place in struct because this is the only
     non-zero element by default.  It could then interfere with boolean_value
     in union */
	TDAG DAG; /*< representing DAG for subDAGs in sigs */
	TDAG DAG_arith; /*< representative for arithmetic, if any */
	unsigned long long int symbols; /*< sort array of bitmasks for fsymbols */
	Tstack_DAG diseqs; /*< all classes it is disequal to */
} Tclass_info_f;

typedef struct Tclass_info_p
{
	unsigned elem_nb; /*< nb of elements in class */
	TDAG elem; /*< next element in class */
	Tboolean_value boolean_value; /*< boolean value */
} Tclass_info_p;

typedef union Tclass_info
{
	Tclass_info_f f;
	Tclass_info_p p;
} Tclass_info;

static Tclass_info* class_info = NULL;

#define CC_find(a) ((Tclass_info_f*)(class_info + class[a]))
#define CC_find_p(a) ((Tclass_info_p*)(class_info + class[a]))

typedef struct TCC
{
	TDAG next; /*< higher element in the class tree */
	Tlit lit; /*< reason to climb class tree or
                                  literal setting value to a predicate */
} TCC;

static TCC* CC = NULL;

Tstatus CC_status = SAT;

/*
  --------------------------------------------------------------
  Class list
  --------------------------------------------------------------
*/

/* Turning this on has a very minor effect responsible for something like a 2-3% linear efficiency decrease */
/* #define CC_CLASS_LIST */

#ifdef CC_CLASS_LIST

#warning CC_CLASS_LIST set

/**
   \brief list of classes (one DAG per class, the representative) */
Tdll_DAG class_list = DLL_DAG_NULL;
/**
   \brief For each DAG, points to its element in the list of classes */
Tdll_DAG* in_class_list;

#define CLASS_LIST_RM(class1) \
	assert(in_class_list[class1]); \
	assert(dll_DAG_car(in_class_list[class1]) == class1); \
	class_list = dll_DAG_remove(in_class_list[class1]); \
	in_class_list[class1] = DLL_DAG_NULL;

#define CLASS_LIST_ADD(class1) \
	assert(!in_class_list[class1]); \
	class_list = dll_DAG_cons(class1, class_list); \
	in_class_list[class1] = class_list;

#else

#define CLASS_LIST_RM(class1)
#define CLASS_LIST_ADD(class1)

#endif

/*
  --------------------------------------------------------------
  Tclass (congruence classes)
  --------------------------------------------------------------
*/

#if DEBUG_CC >= 2
static void
class_print(Tclass class0)
/* PF print class information */
{
	unsigned i;
	if (DAG_sort(class0) != SORT_BOOLEAN) {
		Tclass_info_f* Pclass = (Tclass_info_f*)(class_info + class0);
		my_DAG_message(
			"F-Class %d, with repr %D (Arith %D):\n", class0, Pclass->DAG,
			Pclass->DAG_arith);
		my_DAG_message("  Predecessors (%d)\n", stack_size(Pclass->stack_pred));
		for (i = 0; i < stack_size(Pclass->stack_pred); i++)
			my_DAG_message("  %D\n", stack_get(Pclass->stack_pred, i));
	} else {
		Tclass_info_p* Pclass = (Tclass_info_p*)(class_info + class0);
		my_DAG_message("P-Class %d, value %d:\n", class0, Pclass->boolean_value);
	}
	my_DAG_message(" Class members\n");
	while (class0) {
		my_DAG_message("  %D\n", class0);
		class0 = ((Tclass_info_f*)class_info + class0)->elem;
	}
	my_DAG_message("Class end\n");
}
#endif

/*
  --------------------------------------------------------------
  Signature table and storage of terms
  --------------------------------------------------------------
*/

/* PF hash key is not computed from the classes of subterms
   but rather from a given representative associated to those classes.
   When merging, this allows to either
   - change the representative of the surviving class and reenter
   signatures of the parents of this class
   - reenter the parents of the disappearing class
   according to the number of parents in both classes */

/**
   \brief storage for zero arity DAGs
   \remark needed because these are not maintained in hash table */
Tstack_DAG zero_arity = NULL;
/**
   \brief storage for quantified formulas
   \remark needed because these are not maintained in hash table */
Tstack_DAG CC_quantified = NULL;

#if DEBUG_CC >= 3
#define SIG_DEBUG
#endif

typedef struct Tbucket
{
	unsigned key;
	TDAG DAG;
} Tbucket;

static unsigned hash_mask = 0;
static Tbucket* hash_bucket = NULL;
#ifdef HASH_STAT
static unsigned stat_hash_collision = 0;
static unsigned stat_hash_insert = 0;
static unsigned stat_hash_max_collision = 0;
static unsigned stat_hash_eq_key = 0;
#endif /* HASH_STAT */

static void
sig_resize(unsigned new_alloc)
{
	unsigned i;
	Tbucket* old_hash_bucket = hash_bucket;
	unsigned n = hash_mask ? hash_mask + 1 : 0;
	if (new_alloc <= n) /* never decrease size */
		return;
#ifdef SIG_DEBUG
	my_DAG_message("sig_resize\n");
#endif
	assert(((new_alloc - 1) & new_alloc) == 0); /* checking that power of two */
	hash_mask = new_alloc - 1;
	MY_MALLOC(hash_bucket, new_alloc * sizeof(Tbucket));
	memset(hash_bucket, 0, new_alloc * sizeof(Tbucket));
	for (i = 0; i < n; i++)
		if (old_hash_bucket[i].DAG) {
			unsigned j = old_hash_bucket[i].key & hash_mask;
			while (hash_bucket[j].DAG) j = (j + 1) & hash_mask;
			hash_bucket[j].key = old_hash_bucket[i].key;
			hash_bucket[j].DAG = old_hash_bucket[i].DAG;
		}
	free(old_hash_bucket);
}

#define hash_one_at_a_time_u_inc(hash, u) \
	((((hash) + (u)) << 10) ^ (((hash) + (u)) >> 6))

#define EQ_COM

/**
   \brief computes hash function associated to DAG
   \param DAG the DAG
   \return the hash of the DAG
   \remark private function */
static inline unsigned
sig_hash(TDAG DAG)
{
	unsigned i;
	unsigned key;
	assert(DAG);
	key = DAG_symb_key(DAG_symb(DAG));
#ifdef EQ_COM
	if (DAG_symb(DAG) == PREDICATE_EQ) {
		TDAG tmp0 = CC_find(DAG_arg0(DAG))->DAG;
		TDAG tmp1 = CC_find(DAG_arg1(DAG))->DAG;
		if (tmp0 > tmp1) SWAP(tmp0, tmp1);
		key = hash_one_at_a_time_u_inc(key, DAG_key(tmp0));
		key = hash_one_at_a_time_u_inc(key, DAG_key(tmp1));
		key += (key << 3);
		key ^= (key >> 11);
		key += (key << 15);
		return key;
	}
#endif /* EQ_COM */
	key = hash_one_at_a_time_u_inc(key, DAG_arity(DAG));
	for (i = 0; i < DAG_arity(DAG); i++) {
		TDAG tmp = CC_find(DAG_arg(DAG, i))->DAG;
		key = hash_one_at_a_time_u_inc(key, DAG_key(tmp));
	}
	key += (key << 3);
	key ^= (key >> 11);
	key += (key << 15);
	return key;
}

static inline bool
sig_equal(TDAG DAG1, TDAG DAG2)
{
	unsigned i;
	assert(DAG1 && DAG2);
	if (DAG_symb(DAG1) != DAG_symb(DAG2) || DAG_arity(DAG1) != DAG_arity(DAG2))
		return false;
#ifdef EQ_COM
	if (DAG_symb(DAG1) == PREDICATE_EQ)
		return (class[DAG_arg0(DAG1)] == class[DAG_arg0(DAG2)] &&
						class[DAG_arg1(DAG1)] == class[DAG_arg1(DAG2)]) ||
			(class[DAG_arg0(DAG1)] == class[DAG_arg1(DAG2)] &&
			 class[DAG_arg1(DAG1)] == class[DAG_arg0(DAG2)]);
#endif /* EQ_COM */
	for (i = 0; i < DAG_arity(DAG1); i++)
		if (class[DAG_arg(DAG1, i)] != class[DAG_arg(DAG2, i)]) return false;
	return true;
}

/**
   \brief get the DAG that is signature equivalent to argument
   \param DAG the term
   \return the DAG equivalent to argument, DAG_NULL if none
   \remark public function */
static inline TDAG
sig_query(TDAG DAG)
{
	unsigned key = sig_hash(DAG);
	unsigned i = key & hash_mask;
	while (hash_bucket[i].DAG &&
				 (key != hash_bucket[i].key || !sig_equal(DAG, hash_bucket[i].DAG)))
		i = (i + 1) & hash_mask;
	return hash_bucket[i].DAG;
}

static inline bool
sig_equal_params(Tsymb symb, Tstack_DAG params, TDAG DAG2)
{
	unsigned i;
	if (symb != DAG_symb(DAG2) || stack_size(params) != DAG_arity(DAG2))
		return false;
	for (i = 0; i < stack_size(params); ++i)
		if (class[stack_get(params, i)] != class[DAG_arg(DAG2, i)]) return false;
	return true;
}

TDAG
sig_query_params(Tsymb symb, Tstack_DAG params)
{
	unsigned i, key;
	key = DAG_symb_key(symb);
	key = hash_one_at_a_time_u_inc(key, stack_size(params));
	for (i = 0; i < stack_size(params); ++i) {
		/* TODO: break loop if CC_find is NULL? */
		TDAG tmp = CC_find(stack_get(params, i))->DAG;
		key = hash_one_at_a_time_u_inc(key, DAG_key(tmp));
	}
	key += (key << 3);
	key ^= (key >> 11);
	key += (key << 15);
	i = key & hash_mask;
	while (hash_bucket[i].DAG &&
				 (key != hash_bucket[i].key ||
					!sig_equal_params(symb, params, hash_bucket[i].DAG)))
		i = (i + 1) & hash_mask;
	return hash_bucket[i].DAG;
}

/**
   \brief adds a DAG to signature table
   \param DAG the term
   \remark public function */
static inline void
sig_enter(TDAG DAG)
{
	unsigned key = sig_hash(DAG);
	unsigned i = key & hash_mask;
	while (hash_bucket[i].DAG) {
		assert(key != hash_bucket[i].key || !sig_equal(DAG, hash_bucket[i].DAG));
		i = (i + 1) & hash_mask;
	}
	hash_bucket[i].key = key;
	hash_bucket[i].DAG = DAG;
#ifdef SIG_DEBUG
	my_DAG_message("sig_enter %D\n", DAG);
#endif
#ifdef HASH_STAT
	stats_counter_inc(stat_hash_insert);
	if ((key & hash_mask) == i) return;
	stats_counter_inc(stat_hash_collision);
	if (i < (key & hash_mask)) i += hash_mask + 1;
	i -= key & hash_mask;
	if (stats_counter_get(stat_hash_max_collision) < (int)i)
		stats_counter_set(stat_hash_max_collision, (int)i);
	i = key & hash_mask;
	while (hash_bucket[i].DAG) {
		if (hash_bucket[i].key == key && hash_bucket[i].DAG != DAG)
			stats_counter_inc(stat_hash_eq_key);
		i = (i + 1) & hash_mask;
	}
#endif
}

/**
   \brief remove DAG, with key, from signature table
   \param key the key
   \param DAG the term
   \remark it is mandatory that DAG is present
   \remark public function */
static inline void
sig_densify(unsigned i)
{
	unsigned j, k;
#ifdef SIG_DEBUG
	my_DAG_message("sig_densify %D\n", hash_bucket[i].DAG);
#endif
	for (j = (i + 1) & hash_mask; hash_bucket[j].DAG; j = (j + 1) & hash_mask) {
		/* invariants: i is free. j != i (because table is not full)
         All l between j and i (cycle) could not occupy i */
		k = hash_bucket[j].key & hash_mask;
		/* determine if k lies cyclically in ]i,j]
         | i.k.j |, |.j i.k.| or |.k.j i.| */
		if ((k <= i) + (i <= j) + (j < k) <= 1) /* In fact == 1 */
			continue;
		hash_bucket[i].key = hash_bucket[j].key;
		hash_bucket[i].DAG = hash_bucket[j].DAG;
		i = j;
	}
	hash_bucket[i].key = 0;
	hash_bucket[i].DAG = DAG_NULL;
}

/**
   \brief remove DAG from signature table
   \param DAG the term
   \remark it is mandatory that DAG is present
   \remark public function */
static inline void
sig_del(TDAG DAG)
{
	unsigned key = sig_hash(DAG);
	unsigned i = key & hash_mask;
	while (hash_bucket[i].DAG != DAG) i = (i + 1) & hash_mask;
	sig_densify(i);
}

/**
   \brief remove signature of DAG from signature table
   \param DAG the term
   \remark it is mandatory that DAG is present
   \remark public function */
static inline TDAG
sig_remove(TDAG DAG)
{
	unsigned i, key = sig_hash(DAG);
	for (i = key & hash_mask; hash_bucket[i].DAG; i = (i + 1) & hash_mask)
		if (key == hash_bucket[i].key && sig_equal(hash_bucket[i].DAG, DAG)) {
			TDAG DAG = hash_bucket[i].DAG;
			sig_densify(i);
			return DAG;
		}
	return DAG_NULL;
}

/**
   \brief remove the signature of DAG and add updated signature
   \remark rep2 is absorbed by rep1 */
static inline TDAG
sig_reenter(TDAG DAG)
{
	unsigned i, key = sig_hash(DAG);
	for (i = key & hash_mask; hash_bucket[i].DAG; i = (i + 1) & hash_mask)
		if (key == hash_bucket[i].key && sig_equal(hash_bucket[i].DAG, DAG))
			return hash_bucket[i].DAG;
#ifdef SIG_DEBUG
	my_DAG_message("sig_reenter add %D\n", DAG);
#endif
	hash_bucket[i].key = key;
	hash_bucket[i].DAG = DAG;
	return DAG_NULL;
}

void
CC_sig_apply(void (*f)(TDAG))
{
	unsigned i;
	for (i = 0; i <= hash_mask; i++)
		if (hash_bucket[i].DAG) f(hash_bucket[i].DAG);
}

/**
   \brief initialization of signature table */
static inline void
sig_init(void)
{
	sig_resize(64);
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
sig_done(void)
{
#if DEBUG_CC >= 1
	unsigned i;
	for (i = 0; i <= hash_mask; i++) assert(!hash_bucket[i].DAG);
#endif
	hash_mask = 0;
	free(hash_bucket);
}

#ifdef SIG_DEBUG
void
sig_print(void)
{
	unsigned i;
	fprintf(stderr, "sig_print begin\n");
	for (i = 0; i <= hash_mask; i++) {
		TDAG DAG = hash_bucket[i].DAG;
		unsigned j;
		if (!DAG) continue;
		fprintf(stderr, "(%s", DAG_symb_name2(DAG_symb(DAG)));
		for (j = 0; j < DAG_arity(DAG); j++) {
			fprintf(stderr, " ");
			DAG_fprint(stderr, CC_find(DAG_arg(DAG, j))->DAG);
		}
		fprintf(stderr, ") : ");
		DAG_fprint(stderr, DAG);
		fprintf(
			stderr, " %s (%u)\n", (sig_hash(DAG) != hash_bucket[i].key) ? "X" : "",
			hash_bucket[i].key);
	}
	fprintf(stderr, "sig_print end\n");
}
#endif

#if 0
void
sig_check(void)
{
  unsigned i;
  DAG_tmp_reserve();
  for (i = 0; i <= hash_mask; i++)
    if (hash_bucket[i].DAG)
      (DAG_tmp_unsigned[hash_bucket[i].DAG])++;
  for (i = 0; i <= hash_mask; i++)
    {
      assert(DAG_tmp_unsigned[hash_bucket[i].DAG] <= 1);
      DAG_tmp_unsigned[hash_bucket[i].DAG] = 0;
    }
  DAG_tmp_release();
}
#endif

/*
  --------------------------------------------------------------
  Get equalities and all that
  --------------------------------------------------------------
*/

static Tstack_DAG explain_pending;
static TDAG DAG_delay = DAG_NULL;

/**
   \brief computes the nearest ancestor of DAG0 and DAG1
   \param DAG0 a term
   \param DAG1 a term in the same congruence class as DAG0
   \return the lowest DAG ancestor of DAG0 and DAG1 in the tree relation
   stored in the "next" field of DAGs */
static inline TDAG
explain_nearest_ancestor(TDAG DAG0, TDAG DAG1)
{
	TDAG D0, D1;
	assert(DAG0);
	assert(DAG1);
	DAG_tmp_reserve();
	D0 = DAG0;
	D1 = DAG1;
	/* PF climb alternatively from DAG0 or DAG1 to root and
     stop when twice visited node */
	while (!DAG_tmp_bool[D0]) {
		DAG_tmp_bool[D0] = 1;
		D0 = CC[D0].next;
		if (D1) SWAP(D0, D1);
		assert(D0);
	}
	/* PF clean */
	for (; DAG0 && DAG_tmp_bool[DAG0]; DAG0 = CC[DAG0].next)
		DAG_tmp_bool[DAG0] = 0;
	for (; DAG1 && DAG_tmp_bool[DAG1]; DAG1 = CC[DAG1].next)
		DAG_tmp_bool[DAG1] = 0;
	DAG_tmp_release();
	return D0;
}

/**
   \brief stores in explain_pending the pairs of DAGs for which to
   compute congruence
   \param DAG0 a term
   \param DAG1 a term congruent to DAG0 */
static inline void
explain_congruence(TDAG DAG0, TDAG DAG1)
{
	unsigned i;
	assert(DAG_symb(DAG0) == DAG_symb(DAG1));
	assert(DAG_arity(DAG0) == DAG_arity(DAG1));
	for (i = 0; i < DAG_arity(DAG0); i++) {
		TDAG tmp0 = DAG_arg(DAG0, i);
		TDAG tmp1 = DAG_arg(DAG1, i);
		if (tmp0 == tmp1) continue;
		stack_push(explain_pending, tmp0);
		stack_push(explain_pending, tmp1);
	}
}

static void DAG_add_late(TDAG DAG);

/**
   \brief collect all literals from DAG0 to DAG1 in veriT_conflict
   \param DAG0 a term
   \param DAG1 a term in the same congruence class as DAG0
   \remark the congruence leading to DAG1 in the path are not recorded but the
   first congruent term in the path congruent to DAG1 is stored in DAG_delay */
static inline void
explain_path(TDAG DAG0, TDAG DAG1)
{
	while (DAG0 != DAG1) {
		if (CC[DAG0].lit) {
			if (DAG_delay) /* PF there is a congruence pending */
			{
				explain_congruence(DAG_delay, DAG0);
				DAG_delay = DAG_NULL;
			}
			if (CC[DAG0].lit == LIT_MODEL_EQ) {
				TDAG DAG2 = DAG_eq(DAG0, CC[DAG0].next);
				DAG_add_late(DAG2);
				CC[DAG0].lit = DAG_to_lit(DAG2);
			}
			stack_push(veriT_conflict, CC[DAG0].lit);
		} else if (!DAG_delay)
			DAG_delay = DAG0;
		DAG0 = CC[DAG0].next;
	}
}

/**
   \brief collect all literals to explain that DAG0 = DAG1 in veriT_conflict
   \param DAG0 a term
   \param DAG1 a term in the same congruence class as DAG0 */
static inline void
explain_eq(TDAG DAG0, TDAG DAG1)
{
	assert(class[DAG0] == class[DAG1]);
	stack_push(explain_pending, DAG0);
	stack_push(explain_pending, DAG1);
	while (stack_size(explain_pending)) {
		TDAG ancestor;
		TDAG tmp;
		DAG1 = stack_pop(explain_pending);
		DAG0 = stack_pop(explain_pending);
		ancestor = explain_nearest_ancestor(DAG0, DAG1);
		explain_path(DAG0, ancestor);
		tmp = DAG_delay;
		DAG_delay = DAG_NULL;
		explain_path(DAG1, ancestor);
		if (tmp || DAG_delay) {
			explain_congruence(
				DAG_delay ? DAG_delay : ancestor, tmp ? tmp : ancestor);
			DAG_delay = DAG_NULL;
		}
	}
}

static Tproof explain_eq_proof(TDAG DAG0, TDAG DAG1);

/**
   \brief collect all literals from DAG0 to DAG1 in veriT_conflict
   \param DAG0 a term
   \param DAG1 a term in the same congruence class as DAG0
   \param trans a stack of DAGs that are successively equal by congruence or
   external equalities
   \param cong a stack of DAGs that are pairwise equal by congruence, and that
   collect all congruences in trans
   \remark the congruence leading to DAG1 in the path are not recorded but the
   first congruent term in the path congruent to DAG1 is stored in DAG_delay
   \remark in case the end of the path leading to DAG1 is linked by congruences,
   there is an even number of DAGs in cong, and the last one is the first term
   in the congruences */
static inline void
explain_path_proof(TDAG DAG0, TDAG DAG1, Tstack_DAG* trans, Tstack_DAG* cong)
{
	stack_push(*trans, DAG0);
	while (DAG0 != DAG1) {
		if (CC[DAG0].lit) {
			if (DAG_delay) /* PF there is a congruence pending */
			{
				stack_push(*trans, DAG0);
				stack_push(*cong, DAG_delay);
				stack_push(*cong, DAG0);
				DAG_delay = DAG_NULL;
			}
			if (CC[DAG0].lit == LIT_MODEL_EQ) {
				TDAG DAG2 = DAG_eq(DAG0, CC[DAG0].next);
				DAG_add_late(DAG2);
				CC[DAG0].lit = DAG_to_lit(DAG2);
			}
			stack_push(veriT_conflict, CC[DAG0].lit);
			stack_push(*trans, CC[DAG0].next);
		} else if (!DAG_delay)
			DAG_delay = DAG0;
		DAG0 = CC[DAG0].next;
	}
}

/* PF The same congruence may be generated again and again with this
   thing */

static Tproof
explain_congruence_proof(TDAG DAG0, TDAG DAG1)
{
	unsigned i;
	Tstack_DAG lits;
	Tproof proof;
	assert(DAG0 && DAG1 && class[DAG0] == class[DAG1]);
	assert(DAG_symb(DAG0) == DAG_symb(DAG1));
	stack_INIT(lits);
	for (i = 0; i < DAG_arity(DAG0); i++)
		stack_push(lits, DAG_dup(DAG_neq(DAG_arg(DAG0, i), DAG_arg(DAG1, i))));
	stack_push(lits, DAG_dup(DAG_eq(DAG0, DAG1)));
	proof = proof_step_valid_stack(ps_type_eq_congruent, lits);
	DAG_free(stack_pop(lits));
	stack_sort(lits, DAG_cmp_q);
	stack_uniq_f(lits, DAG_free);
	for (i = 0; i < stack_size(lits); i++) {
		TDAG eq = DAG_arg(stack_get(lits, i), 0);
		Tproof proof2 = explain_eq_proof(DAG_arg0(eq), DAG_arg1(eq));
		if (proof2) proof = proof_bin_resolve(proof, proof2);
	}
	stack_apply(lits, DAG_free);
	stack_free(lits);
	return proof;
}

/* PF IMPROVE
   The same congruence could be generated/reproved again and again */

/* PF IMPROVE
   There is no sharing of transitive parts */
static Tproof
explain_eq_proof(TDAG DAG0, TDAG DAG1)
{
	unsigned i;
	TDAG ancestor;
	TDAG tmp;
	Tstack_DAG trans, cong, lits;
	Tproof proof = 0;
	if (DAG0 == DAG1) /* PF reflexivity */
		return proof_step_valid(
			ps_type_eq_reflexive, 1, DAG_dup(DAG_eq(DAG0, DAG1)));
	ancestor = explain_nearest_ancestor(DAG0, DAG1);
	stack_INIT(trans);
	stack_INIT(cong);
	explain_path_proof(DAG0, ancestor, &trans, &cong);
	stack_INIT(lits);
	for (i = 0; i + 1 < stack_size(trans); i++)
		stack_push(
			lits, DAG_dup(DAG_neq(stack_get(trans, i), stack_get(trans, i + 1))));
	assert(!DAG_delay || DAG_delay == stack_get(trans, stack_size(trans) - 1));
	assert(DAG_delay || ancestor == stack_get(trans, stack_size(trans) - 1));
	stack_reset(trans);
	tmp = DAG_delay;
	DAG_delay = DAG_NULL;
	explain_path_proof(DAG1, ancestor, &trans, &cong);
	if (tmp || DAG_delay) {
		stack_push(cong, DAG_delay ? DAG_delay : ancestor);
		stack_push(cong, tmp ? tmp : ancestor);
		if (DAG_delay)
			stack_push(trans, tmp ? tmp : ancestor);
		else
			stack_push(trans, tmp);
		DAG_delay = DAG_NULL;
	}
	for (i = stack_size(trans); i-- > 1;)
		stack_push(
			lits, DAG_dup(DAG_neq(stack_get(trans, i), stack_get(trans, i - 1))));
	stack_push(lits, DAG_dup(DAG_eq(DAG0, DAG1)));
	if (stack_size(lits) > 2)
		proof = proof_step_valid_stack(ps_type_eq_transitive, lits);
	else
		assert(
			stack_size(lits) == 2 &&
			DAG_arg(stack_get(lits, 0), 0) == stack_get(lits, 1) &&
			stack_size(cong) <= 2);
	stack_apply(lits, DAG_free);
	stack_free(lits);
	stack_free(trans);
	/* PF resolve with congruences */
	while (stack_size(cong)) {
		TDAG DAG0 = stack_pop(cong);
		TDAG DAG1 = stack_pop(cong);
		Tproof proof2 = explain_congruence_proof(DAG0, DAG1);
		assert(proof2);
		if (proof)
			proof = proof_bin_resolve(proof, proof2);
		else /* equality is between two terms */
			proof = proof2;
	}
	stack_free(cong);
	return proof;
}

static void
explain_init(void)
{
	stack_INIT(explain_pending);
}

static void
explain_done(void)
{
	stack_free(explain_pending);
}

/**
   \brief find the DAG in the same class as arg that has its value set directly
   \param DAG is the first DAG in class (DAG == class[DAG])
   \return the literal setting the value to class */
static TDAG
get_value_p(TDAG DAG)
{
	assert(DAG && DAG == class[DAG]);
	for (; DAG; DAG = class_info[DAG].p.elem)
		if (CC[DAG].lit) return DAG;
	return DAG_NULL;
}

/*
  --------------------------------------------------------------
  Backtrack prototypes
  --------------------------------------------------------------
*/

static void backtrack_union(TDAG DAG0, TDAG DAG1, TDAG DAG_arith);
static void backtrack_union_p(Tclass class0, Tboolean_value boolean_value);
static void backtrack_sig_del(TDAG DAG);
static void backtrack_sig_add(TDAG DAG);
static void backtrack_undef_p(TDAG DAG);
static void backtrack_store_quantified(void);
static void backtrack_status(void);

/*
  --------------------------------------------------------------
  Conflicts
  --------------------------------------------------------------
*/

static TDAG cDAG0 = DAG_NULL, cDAG1 = DAG_NULL;

/**
   \brief store information for conflict on predicates
   \param DAG0 first predicate
   \param DAG1 second predicate
   \remark DAG0 occurs positively, DAG1 negatively */
static inline void
conflict(TDAG DAG0, TDAG DAG1)
{
	cDAG0 = DAG0;
	cDAG1 = DAG1;
	CC_status = UNSAT;
#if DEBUG_CC >= 2
	my_DAG_message("CC: status changed to UNSAT\n");
#endif
	backtrack_status();
}

/*
  --------------------------------------------------------------
  Watch
  --------------------------------------------------------------
*/

static inline void
hint_p(TDAG DAG, bool bool_value, TDAG src)
{
	for (; DAG; DAG = class_info[DAG].p.elem)
		hint_CC(
			lit_make(DAG_to_var(DAG), bool_value), src,
			DAG_symb(DAG) == PREDICATE_EQ &&
				class[DAG_arg0(DAG)] != class[DAG_arg0(src)]);
}

/*
  --------------------------------------------------------------
  merge queue handling
  --------------------------------------------------------------
*/

static bool backfire;

static Tstack_DAG merge_queue;

static inline void
merge_push(TDAG DAG0, TDAG DAG1)
{
	stack_push(merge_queue, DAG0);
	stack_push(merge_queue, DAG1);
}

/*
  --------------------------------------------------------------
  Congruence classes functions
  --------------------------------------------------------------
*/

#if DEBUG_CC >= 2
static inline void
CC_union_debug_begin(
	Tclass_info_f* Pclass0, Tclass_info_f* Pclass1, Tclass class0, Tclass class1)
{
	unsigned i;
#ifdef SIG_DEBUG
	sig_print();
#endif
	my_message("CC: CC_union %d - %d\n", class0, class1);
	class_print(class0);
	class_print(class1);
	for (i = 0; i < stack_size(Pclass0->stack_pred); i++)
		assert(sig_query(stack_get(Pclass0->stack_pred, i)));
	for (i = 0; i < stack_size(Pclass1->stack_pred); i++)
		assert(sig_query(stack_get(Pclass1->stack_pred, i)));
#ifdef PEDANTIC
	printf("%d %d\n", class0, class1);
#endif
}
#endif

static inline void
CC_set_eq(TDAG DAG)
{
	Tboolean_value* Pbool = &(CC_find_p(DAG)->boolean_value);
	assert(DAG_symb(DAG) == PREDICATE_EQ);
	if (*Pbool == BOOL_UNDEFINED) {
#if DEBUG_CC >= 2
		my_DAG_message("CC_set_eq: setting equality %D\n", DAG);
#endif
		(*Pbool) = BOOL_TRUE;
		hint_p(class[DAG], true, DAG);
		CC[DAG].lit = LIT_UNDEF;
		backtrack_undef_p(DAG);
		return;
	}
	if ((*Pbool) == BOOL_TRUE) return;
#if DEBUG_CC >= 2
	my_DAG_message("CC_set_eq: conflict %D\n", DAG);
#endif
	conflict(DAG_NULL, get_value_p(class[DAG]));
}

/**
   \brief Merges classes for DAG0 and DAG1
   \param DAG0 first term to merge
   \param DAG1 second term to merge
   \param lit a literal or placeholder for reason of merging
   \remark top symbol is a function
   \remark new (congruence) merges may be stacked by merge_push calls */
static void
CC_union(TDAG DAG0, TDAG DAG1, Tlit lit)
{
	unsigned i;
	TDAG tmp_DAG;
	Tclass class0 = class[DAG0];
	Tclass class1 = class[DAG1];
	/* IMPROVE ASK Filbois about using Pclass[2] rather
     It may simplify code lateron, and avoid conditionals */
	Tclass_info_f* Pclass0 = CC_find(DAG0);
	Tclass_info_f* Pclass1 = CC_find(DAG1);
	if (class0 == class1) return;
#if DEBUG_CC >= 2
	CC_union_debug_begin(Pclass0, Pclass1, class0, class1);
#endif

	/* Check for inequalities to propagate */
	if ((!Pclass0->DAG_arith) != (!Pclass1->DAG_arith)) {
		if (Pclass1->DAG_arith) {
			SWAP(Pclass0, Pclass1);
			SWAP(class0, class1);
			SWAP(DAG0, DAG1);
		}
		for (i = 0; i < stack_size(Pclass1->stack_pred); i++) {
			TDAG eq = stack_get(Pclass1->stack_pred, i);
			TDAG other;
			if (
				DAG_symb(eq) != PREDICATE_EQ ||
				CC_find_p(eq)->boolean_value != BOOL_FALSE)
				continue;
			other = (class[DAG_arg0(eq)] == class1) ? DAG_arg1(eq) : DAG_arg0(eq);
			if (CC_find(other)->DAG_arith) {
				veriT_xenqueue_type(XTYPE_CC_INEQ);
				assert(
					!CC_find(DAG_arg0(eq))->DAG_arith ||
					!CC_find(DAG_arg1(eq))->DAG_arith);
				veriT_xenqueue_DAG(Pclass0->DAG_arith);
				veriT_xenqueue_DAG(CC_find(other)->DAG_arith);
			}
		}
	}

	/* PF Pclass0 is the largest class */
	if (Pclass0->elem_nb < Pclass1->elem_nb) {
		SWAP(Pclass0, Pclass1);
		SWAP(class0, class1);
		SWAP(DAG0, DAG1);
	}

	/* PF reenter signatures of the smallest predecessor list */
	if (stack_size(Pclass0->stack_pred) < stack_size(Pclass1->stack_pred)) {
		SWAP(Pclass0->stack_pred, Pclass1->stack_pred);
		for (i = 0; i < stack_size(Pclass1->stack_pred); i++)
			backtrack_sig_del(sig_remove(stack_get(Pclass1->stack_pred, i)));
		SWAP(Pclass0->DAG, Pclass1->DAG);
	} else
		for (i = 0; i < stack_size(Pclass1->stack_pred); i++)
			backtrack_sig_del(sig_remove(stack_get(Pclass1->stack_pred, i)));

	/* PF set the new spanning tree
     invert the path from DAG1 to its root (so DAG1 becomes the new root) */
	{
		TDAG orig = DAG1, next = DAG0;
		Tlit lit2 = lit;
		backtrack_union(orig, next, Pclass0->DAG_arith);
		do {
			TDAG tmp;
			SWAP(lit2, CC[orig].lit);
			tmp = CC[orig].next;
			CC[orig].next = next;
			next = orig;
			orig = tmp;
		} while (orig);
	}

	/* PF update class for every element in Pclass1 */
	tmp_DAG = class1;
	while (class_info[tmp_DAG].f.elem) {
		class[tmp_DAG] = class0;
		tmp_DAG = class_info[tmp_DAG].f.elem;
	}
	class[tmp_DAG] = class0;
	class_info[tmp_DAG].f.elem = Pclass0->elem;
	Pclass0->elem = class1;
	Pclass0->elem_nb += Pclass1->elem_nb;
	CLASS_LIST_RM(class1);

	/* PF reenter signatures */
	for (i = 0; i < stack_size(Pclass1->stack_pred); i++) {
		TDAG DAG0 = stack_get(Pclass1->stack_pred, i);
		TDAG DAG1 = sig_reenter(DAG0);
		if (!DAG1)
			backtrack_sig_add(DAG0);
		else if (class[DAG0] != class[DAG1]) /* TODO: this test is unnecessary */
			merge_push(DAG0, DAG1);
		if (
			DAG_symb(DAG0) == PREDICATE_EQ &&
			class[DAG_arg0(DAG0)] == class[DAG_arg1(DAG0)])
			CC_set_eq(DAG0);
	}

	/* PF inform DPs of new equality */
	if (Pclass0->DAG_arith && Pclass1->DAG_arith && backfire) {
		veriT_xenqueue_type(XTYPE_CC_EQ);
		veriT_xenqueue_DAG(Pclass1->DAG_arith);
		veriT_xenqueue_DAG(Pclass0->DAG_arith);
	}
	/* PF previously I was not doing this test, and systematically did the
     affectation.  It appeared to be wrong, when
     Pclass0->DAG_arith && !Pclass1->DAG_arith */
	if (!Pclass0->DAG_arith) Pclass0->DAG_arith = Pclass1->DAG_arith;

	stack_merge(Pclass0->stack_pred, Pclass1->stack_pred);

	assert(CC_find(Pclass0->DAG) == Pclass0);
#ifdef SIG_DEBUG
	sig_print();
#endif
#if DEBUG_CC >= 1
	for (i = 0; i < stack_size(Pclass0->stack_pred); i++)
		if (DAG_symb(stack_get(Pclass0->stack_pred, i)) != PREDICATE_EQ)
			assert(sig_query(stack_get(Pclass0->stack_pred, i)));
#endif
#if DEBUG_CC >= 2
	my_message("CC: CC_union Final\n");
	class_print(class0);
#endif
}

static void
CC_disunion(TDAG DAG0, TDAG DAG1, TDAG DAG_arith)
{
	unsigned i;
	TDAG tmp_DAG;
	Tclass class0 = class[DAG0];
	Tclass class1 = class_info[class0].f.elem;
	Tclass_info_f* Pclass0 = (Tclass_info_f*)(class_info + class0);
	Tclass_info_f* Pclass1 = (Tclass_info_f*)(class_info + class1);
	assert(class[Pclass0->DAG] == class0);
	assert(class[DAG0] == class0);
	assert(Pclass0->elem_nb >= Pclass1->elem_nb);
#if DEBUG_CC >= 2
	my_message("CC_disunion: initial:\n");
#ifdef SIG_DEBUG
	sig_print();
#endif
	class_print(class0);
	class_print(class1);
#if DEBUG_CC >= 3
	tmp_DAG = Pclass1->elem;
	for (tmp_DAG = class1, i = Pclass1->elem_nb; i;
			 i--, tmp_DAG = class_info[tmp_DAG].f.elem)
		my_DAG_message("CC: class %d->%d - %D\n", class0, class1, tmp_DAG);
#endif
#endif
	/* remember that class_info[class0] = class1
     (the very class that disappeared) */
	tmp_DAG = class0;
	i = Pclass1->elem_nb;
	do {
		tmp_DAG = class_info[tmp_DAG].f.elem;
		class[tmp_DAG] = class1;
	} while (--i);
	Pclass0->elem = class_info[tmp_DAG].f.elem;
	class_info[tmp_DAG].f.elem = DAG_NULL;
	assert(Pclass0->elem_nb > Pclass1->elem_nb);
	Pclass0->elem_nb -= Pclass1->elem_nb;
	CLASS_LIST_ADD(class1);

	/* PF edge between DAG0 and DAG1 may have been reversed */
	if (CC[DAG0].next == DAG1) {
		CC[DAG0].next = DAG_NULL;
		CC[DAG0].lit = LIT_UNDEF;
	} else {
		assert(CC[DAG1].next == DAG0);
		CC[DAG1].next = DAG_NULL;
		CC[DAG1].lit = LIT_UNDEF;
	}

	assert(stack_size(Pclass0->stack_pred) >= stack_size(Pclass1->stack_pred));
	stack_dec_n(Pclass0->stack_pred, stack_size(Pclass1->stack_pred));
	/* PF may not be the case anymore with late adding of DAGs
     assert(stack_size(Pclass0->stack_pred) >= stack_size(Pclass1->stack_pred));*/

	if (class[Pclass0->DAG] == class1) {
		SWAP(Pclass0->DAG, Pclass1->DAG);
		SWAP(Pclass0->stack_pred, Pclass1->stack_pred);
	}

	Pclass0->DAG_arith = DAG_arith;
	assert(Pclass0 != Pclass1);
	assert(class[Pclass0->DAG] == class0);
	assert(class[Pclass1->DAG] == class1);
#if DEBUG_CC >= 2
	my_message("final:\n");
	class_print(class0);
	class_print(class1);
	my_message("CC: CC_disunion %d - %d\n", class0, class1);
#endif
#ifdef SIG_DEBUG
	sig_print();
#endif
}

/**
   \brief Merges classes for DAG0 and DAG1
   \param DAG0 first term to merge
   \param DAG1 second term to merge
   \remark merging is always due to congruence
   \remark top symbol is a predicate (it is not a subterm, may have bool value)
   \remark new (congruence) merges may be stacked by merge_push calls */
static void
CC_union_p(TDAG DAG0, TDAG DAG1)
{
	TDAG tmp_DAG;
	Tclass class0 = class[DAG0];
	Tclass class1 = class[DAG1];
	/* IMPROVE ASK Filbois about using Pclass[2] rather
     It may simplify code lateron, and avoid conditionals */
	Tclass_info_p* Pclass0 = CC_find_p(DAG0);
	Tclass_info_p* Pclass1 = CC_find_p(DAG1);
	if (class0 == class1) return;

	if (Pclass0->boolean_value != Pclass1->boolean_value) {
		if (Pclass0->boolean_value == BOOL_UNDEFINED)
			hint_p(class0, Pclass1->boolean_value == BOOL_TRUE, get_value_p(class1));
		else if (Pclass1->boolean_value == BOOL_UNDEFINED)
			hint_p(class1, Pclass0->boolean_value == BOOL_TRUE, get_value_p(class0));
		else if (Pclass0->boolean_value == BOOL_TRUE)
			conflict(get_value_p(class0), get_value_p(class1));
		else
			conflict(get_value_p(class1), get_value_p(class0));
	}

	/* PF Pclass0 is the largest class */
	if (Pclass0->elem_nb < Pclass1->elem_nb) {
		SWAP(Pclass0, Pclass1);
		SWAP(class0, class1);
	}
	backtrack_union_p(class0, Pclass0->boolean_value);

	/* PF update class for every element in Pclass1 */
	tmp_DAG = class1;
	while (tmp_DAG) {
		class[tmp_DAG] = class0;
		if (!class_info[tmp_DAG].p.elem) break;
		tmp_DAG = class_info[tmp_DAG].p.elem;
	}
	class_info[tmp_DAG].p.elem = Pclass0->elem;
	Pclass0->elem = class1;
	Pclass0->elem_nb += Pclass1->elem_nb;
	CLASS_LIST_RM(class1);
	if (Pclass0->boolean_value == BOOL_UNDEFINED)
		Pclass0->boolean_value = Pclass1->boolean_value;
}

static void
CC_disunion_p(Tclass class0, Tboolean_value boolean_value)
{
	unsigned i;
	TDAG tmp_DAG;
	Tclass class1 = class_info[class0].p.elem;
	Tclass_info_p* Pclass0 = (Tclass_info_p*)(class_info + class0);
	Tclass_info_p* Pclass1 = (Tclass_info_p*)(class_info + class1);
	assert(Pclass0->elem_nb >= Pclass1->elem_nb);
#ifdef SIG_DEBUG
	sig_print();
#endif
#if DEBUG_CC >= 2
	my_message("CC_disunion: initial:\n");
	class_print(class0);
	class_print(class1);
#if DEBUG_CC >= 3
	tmp_DAG = Pclass1->elem;
	for (tmp_DAG = Pclass1->elem, i = Pclass1->elem_nb; i;
			 i--, tmp_DAG = class_info[tmp_DAG].p.elem)
		my_DAG_message("CC: class %d->%d - %D\n", class0, class1, tmp_DAG);
#endif
#endif
	/* remember that class_info[class0] = class1
     (the very class that disappeared) */
	tmp_DAG = class0;
	i = Pclass1->elem_nb;
	do {
		tmp_DAG = class_info[tmp_DAG].p.elem;
		class[tmp_DAG] = class1;
	} while (--i);
	Pclass0->elem = class_info[tmp_DAG].p.elem;
	class_info[tmp_DAG].p.elem = DAG_NULL;
	assert(Pclass0->elem_nb > Pclass1->elem_nb);
	Pclass0->elem_nb -= Pclass1->elem_nb;
	CLASS_LIST_ADD(class1);

	Pclass0->boolean_value = boolean_value;
	assert(Pclass0 != Pclass1);
#if DEBUG_CC >= 2
	my_message("final:\n");
	class_print(class0);
	class_print(class1);
	my_message("CC: CC_disunion %d - %d\n", class0, class1);
#endif
#ifdef SIG_DEBUG
	sig_print();
#endif
}

/*
  --------------------------------------------------------------
  Main functions
  --------------------------------------------------------------
*/

static inline void
CC_merge(TDAG DAG0, TDAG DAG1, Tlit lit)
{
	unsigned i = 0;
	assert(DAG_sort(DAG0) != SORT_BOOLEAN);
	CC_union(DAG0, DAG1, lit);
	backfire = true;
	while (i < stack_size(merge_queue) && CC_status != UNSAT) {
		TDAG DAG0 = stack_get(merge_queue, i);
		TDAG DAG1 = stack_get(merge_queue, i + 1);
		if (DAG_sort(DAG0) == SORT_BOOLEAN)
			CC_union_p(DAG0, DAG1);
		else
			CC_union(DAG0, DAG1, 0);
		i += 2;
	}
	stack_reset(merge_queue);
}

static inline void
CC_set_p(TDAG DAG, bool pol, Tlit lit)
{
	Tboolean_value* Pbool = &(CC_find_p(DAG)->boolean_value);
	assert(!boolean_connector(DAG_symb(DAG)));
	assert(!pol || DAG_symb(DAG) != PREDICATE_EQ);
	if (*Pbool == BOOL_UNDEFINED) {
		(*Pbool) = pol ? BOOL_TRUE : BOOL_FALSE;
		hint_p(class[DAG], pol, DAG);
		CC[DAG].lit = lit;
		backtrack_undef_p(DAG);
		if (
			pol || DAG_symb(DAG) != PREDICATE_EQ ||
			!CC_find(DAG_arg0(DAG))->DAG_arith || !CC_find(DAG_arg1(DAG))->DAG_arith)
			return;
		veriT_xenqueue_type(XTYPE_CC_INEQ);
		assert(
			CC_find(DAG_arg0(DAG))->DAG_arith != CC_find(DAG_arg1(DAG))->DAG_arith);
		veriT_xenqueue_DAG(CC_find(DAG_arg0(DAG))->DAG_arith);
		veriT_xenqueue_DAG(CC_find(DAG_arg1(DAG))->DAG_arith);
		return;
	}
	/* leave this order for pedantic warnings */
	if ((pol ? (*Pbool) == BOOL_TRUE : (*Pbool) == BOOL_FALSE)) return;
	if (pol)
		conflict(DAG, get_value_p(class[DAG]));
	else
		conflict(get_value_p(class[DAG]), DAG);
}

static inline void
CC_unset_p_undef(TDAG DAG)
{
	CC[DAG].lit = LIT_UNDEF;
	CC_find_p(DAG)->boolean_value = BOOL_UNDEFINED;
}

/*
  --------------------------------------------------------------
  Backtracking functions
  --------------------------------------------------------------
*/

enum
{
	CC_UNION = UNDO_CC,
	CC_UNION_P,
	CC_SIG_ADD,
	CC_SIG_DEL,
	CC_NOTIFY,
	CC_DAG_ADD,
	CC_P_UNDEF,
	CC_STORE_QUANTIFIED,
	CC_STATUS
#if DEBUG_CC >= 2
	,
	CC_ASSERT,
	CC_ASSERT_EQ
#endif
};

#if DEBUG_CC >= 2
static void
CC_hook_assert(void* ignored)
{
	my_DAG_message("CC_assert backtracked\n");
}

static void
CC_hook_assert_eq(void* ignored)
{
	my_DAG_message("CC_assert_eq backtracked\n");
}
#endif

typedef struct Tunion_BTcell
{
	TDAG DAG0;
	TDAG DAG1;
	TDAG DAG_arith;
} Tunion_BTcell;

static inline void
backtrack_union(TDAG DAG0, TDAG DAG1, TDAG DAG_arith)
{
	Tunion_BTcell* PBTcell = (Tunion_BTcell*)undo_push(CC_UNION);
	PBTcell->DAG0 = DAG0;
	PBTcell->DAG1 = DAG1;
	PBTcell->DAG_arith = DAG_arith;
}

static void
CC_hook_union(Tunion_BTcell* PBTcell)
{
	CC_disunion(PBTcell->DAG0, PBTcell->DAG1, PBTcell->DAG_arith);
}

static inline void
backtrack_sig_add(TDAG DAG)
{
	(*(TDAG*)undo_push(CC_SIG_ADD)) = DAG;
}

static inline void
CC_hook_sig_add(TDAG* PDAG)
{
	sig_del(*PDAG);
}

static inline void
backtrack_sig_del(TDAG DAG)
{
	if (DAG) (*(TDAG*)undo_push(CC_SIG_DEL)) = DAG;
}

static inline void
CC_hook_sig_del(TDAG* PDAG)
{
	sig_enter(*PDAG);
}

typedef struct Tunion_p_BTcell
{
	Tclass class0;
	Tboolean_value boolean_value;
} Tunion_p_BTcell;

static inline void
backtrack_union_p(Tclass class0, Tboolean_value boolean_value)
{
	Tunion_p_BTcell* PBTcell = (Tunion_p_BTcell*)undo_push(CC_UNION_P);
	PBTcell->class0 = class0;
	PBTcell->boolean_value = boolean_value;
}

static void
CC_hook_union_p(Tunion_p_BTcell* PBTcell)
{
	CC_disunion_p(PBTcell->class0, PBTcell->boolean_value);
}

static inline void
backtrack_undef_p(TDAG DAG)
{
	(*(TDAG*)undo_push(CC_P_UNDEF)) = DAG;
}

static void
CC_hook_set_p_undef(TDAG* DAG)
{
	CC_unset_p_undef(*DAG);
}

static inline void
backtrack_store_quantified(void)
{
	undo_push(CC_STORE_QUANTIFIED);
}

static void
CC_hook_store_quantified(void* ignored)
{
	DAG_free(stack_pop(CC_quantified));
}

static inline void
backtrack_status(void)
{
	undo_push(CC_STATUS);
}

static void
CC_hook_status(void* ignored)
{
#if DEBUG_CC >= 1
	cDAG0 = cDAG1 = DAG_NULL;
#endif
#if DEBUG_CC >= 2
	my_DAG_message("CC: status changed to SAT\n");
#endif
	CC_status = SAT;
}

/*
  --------------------------------------------------------------
  Resize
  --------------------------------------------------------------
*/

static void
CC_DAG_hook_resize(unsigned old_alloc, unsigned new_alloc)
{
	TDAG i;
	for (i = new_alloc; i < old_alloc; i++) {
#if DEBUG_CC >= 1
		TCC* PCC = CC + i;
		Tclass_info* Pclass = class_info + i;
		assert(!class[i]);
		assert(!PCC->next);
		assert(PCC->lit == LIT_UNDEF);
		assert(!Pclass->f.elem_nb);
		assert(!Pclass->f.elem);
		assert(Pclass->f.DAG == i);
		assert(!Pclass->f.stack_pred);
		/*IMPROVE assert(!Pclass->f.DAG_arith); */
		assert(!Pclass->p.elem_nb);
		assert(!Pclass->p.elem);
		assert(Pclass->p.boolean_value == BOOL_FALSE);
#ifdef CC_CLASS_LIST
		assert(!in_class_list[i]);
#endif
#endif
	}
	MY_REALLOC(class, new_alloc * sizeof(TCC));
	for (i = old_alloc; i < new_alloc; i++) class[i] = 0;
	MY_REALLOC(CC, new_alloc * sizeof(TCC));
	for (i = old_alloc; i < new_alloc; i++) {
		TCC* PCC = CC + i;
		PCC->next = DAG_NULL;
		PCC->lit = LIT_UNDEF;
	}
	MY_REALLOC(class_info, new_alloc * sizeof(Tclass_info));
	if (new_alloc > old_alloc)
		memset(
			class_info + old_alloc, 0, (new_alloc - old_alloc) * sizeof(Tclass_info));
	for (i = old_alloc; i < new_alloc; i++) {
		class_info[i].f.DAG = i;
		assert(class_info[i].f.elem_nb == 0);
		assert(class_info[i].f.elem == 0);
		assert(class_info[i].f.DAG == i);
		assert(class_info[i].f.stack_pred == NULL);
		assert(class_info[i].f.DAG_arith == DAG_NULL);
		assert(class_info[i].f.symbols == 0);
		assert(class_info[i].f.diseqs == NULL);
		assert(class_info[i].p.elem_nb == 0);
		assert(class_info[i].p.elem == 0);
		assert(class_info[i].p.boolean_value == BOOL_FALSE);
	}
#ifdef CC_CLASS_LIST
	MY_REALLOC(in_class_list, new_alloc * sizeof(Tdll_DAG));
	for (i = old_alloc; i < new_alloc; i++) in_class_list[i] = DLL_DAG_NULL;
#endif
	sig_resize(new_alloc << HASH_ADAPTIVE_RATIO);
}

/*
  --------------------------------------------------------------
  Interface
  --------------------------------------------------------------
*/

void
CC_DAG_arith(TDAG DAG)
{
	Tclass_info_f* Pclass = CC_find(DAG);
	assert(DAG_sort(DAG) != SORT_BOOLEAN);
	if (Pclass->DAG_arith) return;
#if DEBUG_CC >= 2
	my_DAG_message("CC_DAG_arith %D\n", DAG);
#endif
	assert(Pclass->elem_nb == 1);
	Pclass->DAG_arith = DAG;
}

Tstatus
CC_assert(Tlit lit)
{
	TDAG DAG = var_to_DAG(lit_var(lit));
	if (CC_status == UNSAT) return CC_status;
	backfire = true;
#if DEBUG_CC >= 2
	undo_push(
		CC_ASSERT); /* backtracking assert is safe to leave only in debug mode */
	my_DAG_message(
		"CC_assert(%d) : %s%D%s\n", lit, lit_pol(lit) ? "" : "(not ", DAG,
		lit_pol(lit) ? "" : ")");
#endif
	assert(!boolean_connector(DAG_symb(DAG)));
	assert(DAG_arity(DAG));
	if (DAG_quant(DAG)) {
		assert(quantifier(DAG_symb(DAG)));
		/* Skolemization should have occurred upstream,
         we can safely discard if should be quantified
         (basically has been asserted with this polarity because SAT solver
         does not handle the pure literal rule */
		if (
			(lit_pol(lit) && DAG_symb(DAG) == QUANTIFIER_EXISTS) ||
			(!lit_pol(lit) && DAG_symb(DAG) == QUANTIFIER_FORALL))
			return CC_status;
		stack_push(CC_quantified, DAG_dup(DAG));
		backtrack_store_quantified();
		return CC_status;
	}
	assert(class[DAG]);
	if (DAG_symb(DAG) == PREDICATE_EQ && lit_pol(lit))
		CC_merge(DAG_arg0(DAG), DAG_arg1(DAG), lit);
	else
		CC_set_p(DAG, lit_pol(lit), lit);
	/* PF IMPROVE this is a hack...
     is it necessary (efficiency, completeness)?
     where is it best located? */
	/*
    if ((!lit_pol(lit) && DAG_symb(DAG) == PREDICATE_LEQ) ||
    (lit_pol(lit) && DAG_symb(DAG) == PREDICATE_LESS))
    CC_anti_merge(clue_new_implied_inequality(clue)); */
	return CC_status;
}

Tstatus
CC_assert_eq(TDAG DAG0, TDAG DAG1, Tlit lit)
{
	/* IMPROVE we used to return when class[DAG0] == class[DAG1]
     check if occurs often or not */
	if (CC_status == UNSAT) return CC_status;
#if DEBUG_CC >= 2
	undo_push(
		CC_ASSERT_EQ); /* backtracking assert_eq is safe to leave only in debug mode */
	my_DAG_message("CC_assert_eq(%d) : (= %D %D)\n", lit, DAG0, DAG1);
#endif
	assert(class[DAG0]);
	assert(class[DAG1]);
	if (class[DAG0] == class[DAG1]) return CC_status;
	backfire = (lit == LIT_MODEL_EQ);
	CC_merge(DAG0, DAG1, lit);
	return CC_status;
}

/*
  --------------------------------------------------------------
  Adding/removing terms
  --------------------------------------------------------------
*/

static void DAG_add(TDAG DAG);
static void DAG_remove(TDAG DAG);

static inline void
backtrack_top_notify(TDAG DAG)
{
	*(TDAG*)undo_top_push(CC_NOTIFY) = DAG;
}

static void
CC_hook_top_notify(TDAG* PDAG)
{
	DAG_free(*PDAG);
}

static inline void
backtrack_top_DAG_add(TDAG DAG)
{
	*(TDAG*)undo_top_push(CC_DAG_ADD) = DAG;
}

static void
CC_hook_top_DAG_add(TDAG* PDAG)
{
	DAG_remove(*PDAG);
}

/**
   \brief Sets bit to the masks of the classes of (largest) ground terms nested
   in quantified formulas and to function and predicate symbols that are
   directly or indirectly applied to quantified variables
   \note Backtrackable */
static void
DAG_add_q(TDAG DAG)
{
	unsigned i;
	if (ground(DAG) && !DAG_quant(DAG) && DAG_sort(DAG) != SORT_BOOLEAN) {
		DAG_add(DAG);
		return;
	}
	if (quantifier(DAG_symb(DAG)))
		DAG_add_q(DAG_arg_last(DAG));
	else /* for any non-ground term (boolean structure, =, ...)
          explore subterms */
		for (i = 0; i < DAG_arity(DAG); i++) DAG_add_q(DAG_arg(DAG, i));
}

static void
DAG_add(TDAG DAG)
{
	unsigned i;
	assert(!quantifier(DAG_symb(DAG)));
	assert(
		DAG_symb(DAG) != LAMBDA && DAG_symb(DAG) != APPLY_LAMBDA &&
		DAG_symb(DAG) != LET && !boolean_connector(DAG_symb(DAG)));
	if (class[DAG]) return;
	/* PF could discard propositions.  Anyway, minimal gain, maximal risks */
	assert(DAG_sort(DAG) == SORT_BOOLEAN || class_info[DAG].f.DAG == DAG);
	class_info[DAG].f.elem_nb = 1;
	class_info[DAG].f.elem = DAG_NULL;
	if (DAG_sort(DAG) == SORT_BOOLEAN && DAG_arity(DAG))
		class_info[DAG].p.boolean_value =
			(DAG_symb(DAG) == PREDICATE_EQ && DAG_arg0(DAG) == DAG_arg1(DAG))
			? BOOL_TRUE
			: BOOL_UNDEFINED;
	else
		stack_INIT_s(class_info[DAG].f.stack_pred, 1);
	class[DAG] = DAG;
	/* First add all subDAGs.  Keep 2 loops separated otherwise mess up for BT */
	for (i = 0; i < DAG_arity(DAG); i++) DAG_add(DAG_arg(DAG, i));
	/* PF could avoid adding in parent lists, if signatures already exist
     However would never occur in practice: DAG are mostly added before
     first equalities */
	for (i = 0; i < DAG_arity(DAG); i++)
		stack_push(CC_find(DAG_arg(DAG, i))->stack_pred, DAG);
	CLASS_LIST_ADD(DAG);
	backtrack_top_DAG_add(DAG);
	assert(!sig_query(DAG));
	if (!DAG_arity(DAG))
		stack_push(zero_arity, DAG);
	else
		sig_enter(DAG);
}

static inline void
DAG_remove(TDAG DAG)
{
	unsigned i;
	assert(class[DAG]);
	if (DAG_arity(DAG)) {
		/* TODO: dirty hack because of DAG_add_late */
		if (DAG_symb(DAG) != PREDICATE_EQ || sig_query(DAG)) {
			assert(sig_query(DAG) == DAG);
			sig_del(DAG);
		}
		for (i = DAG_arity(DAG); i-- != 0;) {
			Tclass_info_f* Pclass = CC_find(DAG_arg(DAG, i));
			assert(
				stack_size(Pclass->stack_pred) && stack_top(Pclass->stack_pred) == DAG);
			stack_dec(Pclass->stack_pred);
		}
	} else {
		assert(stack_top(zero_arity) == DAG);
		stack_dec(zero_arity);
	}
	CLASS_LIST_RM(DAG);
	if (DAG_sort(DAG) == SORT_BOOLEAN && DAG_arity(DAG)) {
		Tclass_info_p* Pclass = CC_find_p(DAG);
		assert(Pclass->elem_nb == 1);
		assert(Pclass->elem == DAG_NULL);
		assert(
			(DAG_symb(DAG) == PREDICATE_EQ && DAG_arg0(DAG) == DAG_arg1(DAG)) ||
			Pclass->boolean_value == BOOL_UNDEFINED);
		Pclass->boolean_value = BOOL_UNDEFINED;
		Pclass->elem_nb = 0;
		Pclass->boolean_value = BOOL_FALSE;
	} else {
		Tclass_info_f* Pclass = CC_find(DAG);
		assert(Pclass->elem_nb == 1);
		assert(Pclass->elem == DAG_NULL);
		Pclass->elem_nb = 0;
		assert(stack_size(Pclass->stack_pred) == 0);
		stack_free(Pclass->stack_pred);
		/* IMPROVE assert(Pclass->DAG_arith == DAG_NULL); */
	}
	class[DAG] = 0;
}

static inline void
CC_notify_atom(TDAG DAG)
{
	if (CC_status == UNSAT) return;
	if (DAG_quant(DAG))
		DAG_add_q(DAG);
	else /* Add equalities, predicates and terms */
		DAG_add(DAG);
}

/**
   \brief stores arithmetic definition for arithmetic atoms,
   and all arithmetic terms in DAG
   \param DAG a DAG */
static void
CC_notify_formula_aux(TDAG DAG)
{
	unsigned i;
	Tsymb symb = DAG_symb(DAG);
	if (DAG_tmp_bool[DAG]) return;
	DAG_tmp_bool[DAG] = 1;
	if (boolean_connector(symb))
		for (i = 0; i < DAG_arity(DAG); i++) CC_notify_formula_aux(DAG_arg(DAG, i));
	else if (quantifier(symb))
		return;
	else if (
		symb == LET || symb == LAMBDA || symb == APPLY_LAMBDA ||
		symb == FUNCTION_ITE)
		my_error("CC: unaccepted symbol, internal error\n");
	else
		CC_notify_atom(DAG);
}

/**
   \brief stores arithmetic definition for arithmetic atoms,
   and all arithmetic terms in DAG
   \param DAG a DAG */
void
CC_notify_formula(TDAG DAG)
{
	DAG_dup(DAG);
	backtrack_top_notify(DAG);
	DAG_tmp_reserve();
	CC_notify_formula_aux(DAG);
	DAG_tmp_reset_bool(DAG);
	DAG_tmp_release();
}

#if 0
static bool
check_in_class(Tclass class, TDAG DAG)
{
  unsigned i;
  for (i = class_info[class].f.elem_nb; i;
       i--, class = class_info[class].f.elem)
    if (DAG == class)
      return true;
  return false;
}
#endif

/*
  PF given a DAG, the elements in the class are organised as follow
  class[DAG] is the representative surviving the class.
  The e stack contains class[DAG] and all succeeding DAGs in class_info_f.elem.
  The n stack contains the successive f.elem_nb
  The e and n stack contain, starting position 1 (not 0) the e and n
  stack of the class "disappearing" in the last merge.  The n stack informs
  of how many they are.
  Given a DAG, it is thus easy to check if it was in the disappearing class in
  this large merge: just check if it is in e with index between 1 and n[1].
  When classes A and B are merged, the representative for the new class is
  either the A or the B one.
*/
static void
add_pred_late(TDAG DAG, TDAG pred)
{
	unsigned i = UINT_MAX;
	TDAG class1 = class[DAG];
	Tstack_DAG stack_e;
	Tstack_DAG stack_n;
	TDAG r = class_info[class1].f.DAG;
	Tstack_DAG* Pstack_pred = &class_info[class1].f.stack_pred;
	unsigned pred_n = stack_size(*Pstack_pred);
	unsigned DAG_pos = 0;
	stack_INIT(stack_e);
	stack_INIT(stack_n);
	do {
		if (class1 == DAG) DAG_pos = stack_size(stack_e);
		stack_push(stack_e, class1);
		stack_push(stack_n, class_info[class1].f.elem_nb);
		class1 = class_info[class1].f.elem;
	} while (class1);
	assert(stack_get(stack_n, 0) == stack_size(stack_e));
	while (stack_get(stack_n, 0) > 1) {
		unsigned j, m = stack_get(stack_n, 1);
		if (i == UINT_MAX)
			for (i = 0; i <= stack_get(stack_n, 0); i++)
				if (r == stack_get(stack_e, i)) break;
		assert(r == stack_get(stack_e, i));
		if (DAG_pos && DAG_pos <= m) { /* DAG was in last merged class */
			if (i && i <= m) { /* Representative has been preserved,
                 current list is old pred list modified */
				assert(
					pred_n >= stack_size(class_info[stack_get(stack_e, 1)].f.stack_pred));
				pred_n -= stack_size(class_info[stack_get(stack_e, 1)].f.stack_pred);
				i--;
			} else {
				stack_insert(*Pstack_pred, pred, pred_n);
				r = class_info[stack_get(stack_e, 1)].f.DAG;
				Pstack_pred = &class_info[stack_get(stack_e, 1)].f.stack_pred;
				pred_n = stack_size(*Pstack_pred);
				i = UINT_MAX;
			}
			DAG_pos--;
			for (j = 0; j < m; j++) {
				stack_set(stack_e, j, stack_get(stack_e, j + 1));
				stack_set(stack_n, j, stack_get(stack_n, j + 1));
			}
		} else {
			unsigned n = stack_get(stack_n, 0) - m;
			if (i && i <= m) { /* Representative has been swapped.
                 Modify current list, and take the old pred */
				stack_insert(*Pstack_pred, pred, pred_n);
				r = class_info[stack_get(stack_e, 1)].f.DAG;
				Pstack_pred = &class_info[stack_get(stack_e, 1)].f.stack_pred;
				pred_n = stack_size(*Pstack_pred);
				i = UINT_MAX;
			} else {
				assert(
					pred_n >= stack_size(class_info[stack_get(stack_e, 1)].f.stack_pred));
				pred_n -= stack_size(class_info[stack_get(stack_e, 1)].f.stack_pred);
				if (i) i -= m;
			}
			DAG_pos -= m;
			for (j = 1; j < n; j++) {
				stack_set(stack_e, j, stack_get(stack_e, j + m));
				stack_set(stack_n, j, stack_get(stack_n, j + m));
			}
			stack_set(stack_n, 0, n);
		}
	}
	stack_insert(*Pstack_pred, pred, pred_n);
	stack_free(stack_e);
	stack_free(stack_n);
}

/*
  Add DAGs in CC datastructures, even when some equalities are asserted and
  sub-terms may not be in their own class alone.  It is mandatory however that
  such terms are in their own class when added.  Otherwise, at some point, there
  should have been a merge.  And this merge should be put in the right place in
  the backtrack stack.
  Do not have a clear idea of what to do if not alone in class
  TODO: solve this question!!!!
*/
static void
DAG_add_late(TDAG DAG)
{
	unsigned i;
	assert(!quantifier(DAG_symb(DAG)));
	assert(
		DAG_symb(DAG) != LAMBDA && DAG_symb(DAG) != APPLY_LAMBDA &&
		DAG_symb(DAG) != LET && !boolean_connector(DAG_symb(DAG)));
	if (class[DAG]) return;
	/* PF could discard propositions.  Anyway, minimal gain, maximal risks */
	assert(DAG_sort(DAG) == SORT_BOOLEAN || class_info[DAG].f.DAG == DAG);
	class_info[DAG].f.elem_nb = 1;
	class_info[DAG].f.elem = DAG_NULL;
	if (DAG_sort(DAG) == SORT_BOOLEAN)
		class_info[DAG].p.boolean_value =
			(DAG_symb(DAG) == PREDICATE_EQ && DAG_arg0(DAG) == DAG_arg1(DAG))
			? BOOL_TRUE
			: BOOL_UNDEFINED;
	else
		stack_INIT_s(class_info[DAG].f.stack_pred, 1);
	class[DAG] = DAG;
	/* First add all subDAGs.  Keep 2 loops separated otherwise mess up for BT */
	for (i = 0; i < DAG_arity(DAG); i++) DAG_add_late(DAG_arg(DAG, i));
	/* PF could avoid adding in parent lists, if signatures already exist
     However would never occur in practice: DAG are mostly added before
     first equalities */
	for (i = 0; i < DAG_arity(DAG); i++) add_pred_late(DAG_arg(DAG, i), DAG);
	CLASS_LIST_ADD(DAG);
	backtrack_top_DAG_add(DAG);
	if (!DAG_arity(DAG)) stack_push(zero_arity, DAG);
	/* PF TODO: we do not care much yet about having it in the sig tables
     DAG_add_late is used only for eqs. */
	/*  else if (!sig_query(DAG))
      sig_enter(DAG); */
}

/*
  --------------------------------------------------------------
  Public functions for instantiation
  --------------------------------------------------------------
*/

static Tstack_DAG CC_get_sig_aux_stack;
static Tsymb CC_get_sig_aux_symb;
static void
CC_get_sig_aux(TDAG DAG)
{
	if (DAG_symb(DAG) == CC_get_sig_aux_symb)
		stack_push(CC_get_sig_aux_stack, DAG);
}

Tstack_DAG
CC_get_sig(Tsymb symb)
{
	Tstack_DAG result;
	stack_INIT(result);
	if (symb == PREDICATE_EQ) return result;
	CC_get_sig_aux_symb = symb;
	CC_get_sig_aux_stack = result;
	CC_sig_apply(CC_get_sig_aux);
	result = CC_get_sig_aux_stack;
	return result;
}

Tstack_DAG
CC_get_sig_DAG(Tsymb symb, TDAG DAG)
{
	Tstack_DAG result;
	stack_INIT(result);
	if (symb == PREDICATE_EQ) return result;
	do {
		if (DAG_symb(DAG) == symb) stack_push(result, DAG);
		DAG = class_info[DAG].p.elem;
	} while (DAG);
	return result;
}

Tstack_DAG
CC_get_sort_classes(Tsort sort)
{
	unsigned i;
	Tstack_DAG result;
	stack_INIT(result);
	DAG_tmp_reserve();
	for (i = 1; i < stack_size(DAG_table); i++)
		if (class[i] && !DAG_tmp_bool[class[i]] && DAG_sort(class[i]) == sort) {
			stack_push(result, class[i]);
			DAG_tmp_bool[class[i]] = 1;
		}
	for (i = 0; i < stack_size(result); i++)
		DAG_tmp_bool[stack_get(result, i)] = 0;
	DAG_tmp_release();
	return result;
}

/*
  --------------------------------------------------------------
  Set symbols bitmasks in CC
  --------------------------------------------------------------
*/

#define DEBUG_SYMB 0

#define MAX_SIZE 64

extern unsigned long long int* symb_mask;
Tstack_DAG class_DAGs;

bool
class_has_symbol(TDAG DAG, Tsymb symbol)
{
	assert(symb_mask[symbol] || !symb_mask[symbol]);
	if (!symb_mask[symbol]) return true;
	return (symb_mask[symbol] & class_info[class[DAG]].f.symbols) == 0 ? false
																																		 : true;
}

/* TODO: Do it when inserting elements in my own CC */
void
CC_set_symbols(TDAG DAG)
{
	Tclass tmp_class = class[DAG];
	if (class_info[tmp_class].f.symbols) return;
	stack_push(class_DAGs, tmp_class);
	class_info[tmp_class].f.symbols =
		class_info[tmp_class].f.symbols | symb_mask[DAG_symb(tmp_class)];
	while (class_info[tmp_class].f.elem) {
		class_info[class[tmp_class]].f.symbols =
			class_info[class[tmp_class]].f.symbols |
			symb_mask[DAG_symb(class_info[tmp_class].f.elem)];
		tmp_class = class_info[tmp_class].f.elem;
	}
#if DEBUG_SYMB
	char buff[MAX_SIZE + 1];
	my_DAG_message(
		"Class {%d}%D has bitmask:\n", class[tmp_class], class[tmp_class]);
	printf(
		"\t%lld -> %s\n", class_info[class[tmp_class]].f.symbols,
		binrep(class_info[class[tmp_class]].f.symbols, buff, MAX_SIZE));
	printf("================================================\n");
#endif
}

/** TODO: Do it when backtracking in the CC */
void
CC_reset_symbols(void)
{
	unsigned i;
	for (i = 0; i < stack_size(class_DAGs); ++i)
		class_info[class[stack_get(class_DAGs, i)]].f.symbols = 0;
	stack_reset(class_DAGs);
}

/*
  --------------------------------------------------------------
  Set disequalities per class in CC
  --------------------------------------------------------------
*/

#define DEBUG_DISEQ 0

Tstack_DAG diseq_DAGs;

static int
cmp_q_class(TDAG* PDAG1, TDAG* PDAG2)
{
	return (int)class[*PDAG1] - (int)class[*PDAG2];
}

static bool
stack_DAG_contains_class(Tstack_DAG stack, TDAG DAG)
{
	if (stack_is_empty(stack)) return false;
	int imid, imin = 0, imax = stack_size(stack) - 1;
	while (imin <= imax) {
		imid = imin + (imax - imin) / 2;
		if (class[DAG] == class[stack_get(stack, imid)]) return true;
		if (class[DAG] < class[stack_get(stack, imid)])
			imax = imid - 1;
		else if (class[DAG] > class[stack_get(stack, imid)])
			imin = imid + 1;
	}
	return false;
}

/* TODO: Do it when inserting literals in my own CC */
void
CC_set_diseqs(TDAG DAG)
{
	unsigned i;
	for (i = 0; i < DAG_arity(DAG); ++i) {
		if (!class_info[class[DAG_arg(DAG, i)]].f.diseqs) {
			stack_INIT(class_info[class[DAG_arg(DAG, i)]].f.diseqs);
			stack_push(
				class_info[class[DAG_arg(DAG, i)]].f.diseqs,
				class[DAG_arg(DAG, 1 - i)]);
			/* TODO: eventually this shoud disappear, only necessary because
             hacking ground CC */
			stack_push(diseq_DAGs, DAG_arg(DAG, i));
#if DEBUG_DISEQ
			my_DAG_message(
				"diseq_DAGs: push {%d}%D\n", class[DAG_arg(DAG, i)], DAG_arg(DAG, i));
#endif
			continue;
		}
		if (stack_DAG_contains_class(
					class_info[class[DAG_arg(DAG, i)]].f.diseqs, DAG_arg(DAG, 1 - i)))
			continue;
		stack_push(
			class_info[class[DAG_arg(DAG, i)]].f.diseqs, class[DAG_arg(DAG, 1 - i)]);
		stack_sort(class_info[class[DAG_arg(DAG, i)]].f.diseqs, cmp_q_class);
#if DEBUG_DISEQ
		my_DAG_message(
			"Diseqs of class {%d}%D:\n", class[DAG_arg(DAG, i)], DAG_arg(DAG, i));
		print_Tstack_DAG(class_info[class[DAG_arg(DAG, i)]].f.diseqs);
		printf("-----------------------------------\n");
#endif
	}
}

/* TODO: Do it when backtracking in the CC */
void
CC_free_diseqs(void)
{
	unsigned i;
	/* TODO: it should be sorted/uniq by construction... */
	stack_sort(diseq_DAGs, DAG_cmp_q);
	stack_uniq(diseq_DAGs);
	for (i = 0; i < stack_size(diseq_DAGs); ++i) {
#if DEBUG_DISEQ
		my_DAG_message(
			"diseq_DAGs: free {%d}%D\n", class[stack_get(diseq_DAGs, i)],
			stack_get(diseq_DAGs, i));
		print_Tstack_DAG(class_info[class[stack_get(diseq_DAGs, i)]].f.diseqs);
#endif
		stack_free(class_info[class[stack_get(diseq_DAGs, i)]].f.diseqs);
	}
	stack_reset(diseq_DAGs);
}

/*
  --------------------------------------------------------------
  Public functions: DPs
  --------------------------------------------------------------
*/

TDAG
CC_abstract(TDAG DAG)
{
	return CC_find(DAG)->DAG;
}

Tboolean_value
CC_abstract_p(TDAG DAG)
{
	if (!class[DAG]) return BOOL_UNDEFINED;
	return CC_find_p(DAG)->boolean_value;
}

bool
CC_disequal(TDAG D0, TDAG D1)
{
	Tstack_DAG diseqs0 = CC_find(D0)->diseqs;
	Tstack_DAG diseqs1 = CC_find(D1)->diseqs;
	if (!diseqs0 || !diseqs1) return false;
	/* HB Check in smaller stack the presence of the class */
	if (stack_size(diseqs0) > stack_size(diseqs1)) {
		if (!stack_DAG_contains(diseqs1, class[D0])) return false;
	} else if (!stack_DAG_contains(diseqs0, class[D1]))
		return false;
	return true;
}

Tstack_DAG
CC_diseqs(TDAG DAG)
{
	return CC_find(DAG)->diseqs;
}

/*
  --------------------------------------------------------------
  Public functions: proof
  --------------------------------------------------------------
*/

Tproof
CC_conflict_proof(void)
{
	unsigned i;
	Tstack_DAG lits;
	Tproof proof;
	assert(
		cDAG0 != cDAG1 && cDAG1 && !boolean_connector(DAG_symb(cDAG1)) &&
		((!cDAG0 && DAG_symb(cDAG1) == PREDICATE_EQ &&
			class[DAG_arg0(cDAG1)] == class[DAG_arg1(cDAG1)]) ||
		 (cDAG0 && class[cDAG0] == class[cDAG1] &&
			DAG_symb(cDAG0) == DAG_symb(cDAG1))));
	if (
		DAG_symb(cDAG1) == PREDICATE_EQ) { /* because of a CC_set_eq, or a merge */
		stack_push(veriT_conflict, lit_make(DAG_to_var(cDAG1), 0));
		return explain_eq_proof(DAG_arg0(cDAG1), DAG_arg1(cDAG1));
	}
	stack_push(veriT_conflict, lit_make(DAG_to_var(cDAG0), 1));
	stack_push(veriT_conflict, lit_make(DAG_to_var(cDAG1), 0));
	stack_INIT(lits);
	for (i = 0; i < DAG_arity(cDAG0); i++)
		stack_push(lits, DAG_dup(DAG_neq(DAG_arg(cDAG0, i), DAG_arg(cDAG1, i))));
	stack_push(lits, DAG_dup(DAG_not(cDAG0)));
	stack_push(lits, DAG_dup(cDAG1));
	proof = proof_step_valid_stack(ps_type_eq_congruent_pred, lits);
	DAG_free(stack_pop(lits));
	DAG_free(stack_pop(lits));
	stack_sort(lits, DAG_cmp_q);
	stack_uniq_f(lits, DAG_free);
	for (i = 0; i < stack_size(lits); i++) {
		TDAG eq = DAG_arg0(stack_get(lits, i));
		Tproof proof2 = explain_eq_proof(DAG_arg0(eq), DAG_arg1(eq));
		if (proof2) proof = proof_bin_resolve(proof, proof2);
	}
	stack_apply(lits, DAG_free);
	stack_free(lits);
	return proof;
}

void
CC_conflict(void)
{
	unsigned i;
	assert(
		cDAG0 != cDAG1 && cDAG1 && !boolean_connector(DAG_symb(cDAG1)) &&
		((!cDAG0 && DAG_symb(cDAG1) == PREDICATE_EQ &&
			class[DAG_arg0(cDAG1)] == class[DAG_arg1(cDAG1)]) ||
		 (cDAG0 && class[cDAG0] == class[cDAG1] &&
			DAG_symb(cDAG0) == DAG_symb(cDAG1))));
	if (
		DAG_symb(cDAG1) == PREDICATE_EQ) { /* because of a CC_set_eq, or a merge */
		stack_push(veriT_conflict, lit_make(DAG_to_var(cDAG1), 0));
		explain_eq(DAG_arg0(cDAG1), DAG_arg1(cDAG1));
		return;
	}
	/* An equality may be set to false by CC_set_predicate, leading to a
     conflict, and then the above also works

     It is never the case that CC_set_predicate sets an equality true leading to
     a conflict, since this conflict should be handled by the merge occuring
     just before */
	stack_push(veriT_conflict, lit_make(DAG_to_var(cDAG0), 1));
	stack_push(veriT_conflict, lit_make(DAG_to_var(cDAG1), 0));
	for (i = 0; i < DAG_arity(cDAG0); i++)
		explain_eq(DAG_arg(cDAG0, i), DAG_arg(cDAG1, i));
}

#define DAG_pol(A, B) ((A) ? B : DAG_not(B))

void
CC_hint_explain(Tlit lit)
{
	TDAG DAG0 = var_to_DAG(lit_var(lit));
	TDAG DAG1 = hint_CC_cause(lit);
	unsigned i;
	assert(class[DAG0]);
	if (lit_pol(lit) && DAG_symb(DAG0) == PREDICATE_EQ) {
		assert(class[DAG_arg0(DAG0)] == class[DAG_arg1(DAG0)]);
		stack_push(veriT_conflict, lit_neg(lit)); /* later double negated */
		explain_eq(DAG_arg0(DAG0), DAG_arg1(DAG0));
		return;
	}
	assert(class[DAG0] == class[DAG1]);
	assert(DAG0 != DAG1);
	assert(DAG_symb(DAG0) == DAG_symb(DAG1));
	stack_push(veriT_conflict, lit_neg(lit));
	stack_push(veriT_conflict, lit_make(DAG_to_var(DAG1), lit_pol(lit)));
	if (hint_CC_reversed(lit)) {
		assert(DAG_symb(DAG0) == PREDICATE_EQ);
		explain_eq(DAG_arg0(DAG0), DAG_arg1(DAG1));
		explain_eq(DAG_arg1(DAG0), DAG_arg0(DAG1));
		return;
	}
	for (i = 0; i < DAG_arity(DAG0); i++)
		explain_eq(DAG_arg(DAG0, i), DAG_arg(DAG1, i));
}

Tproof
CC_hint_explain_proof(Tlit lit)
{
	TDAG DAG0 = var_to_DAG(lit_var(lit));
	TDAG DAG1 = hint_CC_cause(lit);
	Tstack_DAG lits;
	Tproof proof;
	unsigned i;
	assert(class[DAG0]);
	if (DAG_symb(DAG0) == PREDICATE_EQ) {
		Tproof proof1, proof2;
		if (lit_pol(lit)) {
			assert(class[DAG_arg0(DAG0)] == class[DAG_arg1(DAG0)]);
			assert(lit_pol(lit)); /* A = B */
			stack_push(veriT_conflict, lit_neg(lit)); /* later be double negated */
			return explain_eq_proof(DAG_arg0(DAG0), DAG_arg1(DAG0));
		}
		stack_push(veriT_conflict, lit_neg(lit));
		stack_push(veriT_conflict, lit_make(DAG_to_var(DAG1), lit_pol(lit)));
		stack_INIT(lits);
		if (hint_CC_reversed(lit)) {
			proof1 = explain_eq_proof(DAG_arg0(DAG0), DAG_arg1(DAG1));
			stack_push(lits, DAG_dup(DAG_neq(DAG_arg0(DAG0), DAG_arg1(DAG1))));
			stack_push(lits, DAG_dup(DAG_not(DAG0)));
			proof2 = explain_eq_proof(DAG_arg1(DAG0), DAG_arg0(DAG1));
			stack_push(lits, DAG_dup(DAG_neq(DAG_arg1(DAG0), DAG_arg0(DAG1))));
		} else {
			proof1 = explain_eq_proof(DAG_arg0(DAG0), DAG_arg0(DAG1));
			stack_push(lits, DAG_dup(DAG_neq(DAG_arg0(DAG0), DAG_arg0(DAG1))));
			stack_push(lits, DAG_dup(DAG_not(DAG0)));
			proof2 = explain_eq_proof(DAG_arg1(DAG0), DAG_arg1(DAG1));
			stack_push(lits, DAG_dup(DAG_neq(DAG_arg1(DAG0), DAG_arg1(DAG1))));
		}
		stack_push(lits, DAG_dup(DAG1));
		proof = proof_step_valid_stack(ps_type_eq_transitive, lits);
		if (proof1) proof = proof_bin_resolve(proof, proof1);
		if (proof2) proof = proof_bin_resolve(proof, proof2);
		stack_apply(lits, DAG_free);
		stack_free(lits);
		return proof;
	}
	assert(class[DAG0] == class[DAG1]);
	assert(DAG0 != DAG1);
	assert(DAG_symb(DAG0) == DAG_symb(DAG1));
	stack_push(veriT_conflict, lit_neg(lit));
	stack_push(veriT_conflict, lit_make(DAG_to_var(DAG1), lit_pol(lit)));
	stack_INIT(lits);
	for (i = 0; i < DAG_arity(DAG0); i++)
		stack_push(lits, DAG_dup(DAG_neq(DAG_arg(DAG0, i), DAG_arg(DAG1, i))));
	stack_push(lits, DAG_dup(DAG_pol(lit_pol(lit), DAG0)));
	stack_push(lits, DAG_dup(DAG_pol(1 - lit_pol(lit), DAG1)));
	proof = proof_step_valid_stack(ps_type_eq_congruent_pred, lits);
	DAG_free(stack_pop(lits));
	DAG_free(stack_pop(lits));
	stack_sort(lits, DAG_cmp_q);
	stack_uniq_f(lits, DAG_free);
	for (i = 0; i < stack_size(lits); i++) {
		TDAG eq = DAG_arg0(stack_get(lits, i));
		Tproof proof2 = explain_eq_proof(DAG_arg0(eq), DAG_arg1(eq));
		/* PF to justify arity <= 2 and taking DAG_arg0 for the above */
		assert(DAG_symb(stack_get(lits, i)) == CONNECTOR_NOT);
		if (proof2) proof = proof_bin_resolve(proof, proof2);
	}
	stack_apply(lits, DAG_free);
	stack_free(lits);
	return proof;
}

void
CC_premisses_eq(TDAG DAG0, TDAG DAG1)
{
	explain_eq(DAG0, DAG1);
}

Tproof
CC_premisses_eq_proof(TDAG DAG0, TDAG DAG1)
{
	return explain_eq_proof(DAG0, DAG1);
}

/*
  --------------------------------------------------------------
  Model output
  --------------------------------------------------------------
*/

/*
  IMPROVE:
  use stack rather than tables
  ask other DPs, when interpreted domains, for a value */
static Tstack_DAG* model_symb = NULL;

static void
CC_model_aux(TDAG DAG)
{
	Tstack_DAG table_sig = model_symb[DAG_symb(DAG)];
	if (table_sig == NULL) {
		stack_INIT(table_sig);
		model_symb[DAG_symb(DAG)] = table_sig;
	}
	stack_push(table_sig, DAG);
}

static unsigned class_id = 0;

static unsigned
CC_model_get_class_id(TDAG DAG)
{
	TDAG DAG2 = CC_find(DAG)->DAG;
	if (!DAG_tmp_unsigned[DAG2]) DAG_tmp_unsigned[DAG2] = ++class_id;
	return DAG_tmp_unsigned[DAG2];
}

static void
CC_model_reset_class_id(TDAG DAG)
{
	TDAG DAG2 = CC_find(DAG)->DAG;
	DAG_tmp_unsigned[DAG2] = 0;
}

void
CC_model(void(out)(char* format, ...))
{
	Tsymb symb;
	unsigned i, t = 0;
	class_id = 0;
	MY_MALLOC(model_symb, sizeof(*model_symb) * stack_size(DAG_symb_stack));
	memset(model_symb, '\0', sizeof(*model_symb) * stack_size(DAG_symb_stack));
	DAG_tmp_reserve();
	out("(");
	for (i = 0; i < stack_size(zero_arity); i++) {
		char buffer[SYMB_SIZE_LIMIT];
		TDAG DAG = stack_get(zero_arity, i);
		if (!(DAG_symb_type(DAG_symb(DAG)) & SYMB_NAMED)) continue;
		DAG_symb_snprint(DAG, SYMB_SIZE_LIMIT, buffer);
		out("%s(%s cc_%d)\n", t++ ? " " : "", buffer, CC_model_get_class_id(DAG));
	}
	CC_sig_apply(CC_model_aux);
	for (symb = 1; symb < stack_size(DAG_symb_stack); symb++) {
		unsigned j;
		char buffer[SYMB_SIZE_LIMIT];
		Tstack_DAG table_sig = model_symb[symb];
		TDAG DAG;
		if (table_sig == NULL) continue;
		DAG = stack_get(table_sig, 0);
		if (!(DAG_symb_type(DAG_symb(DAG)) & SYMB_NAMED)) continue;
		DAG_symb_snprint(DAG, SYMB_SIZE_LIMIT, buffer);
		out("%s(%s ", t++ ? " " : "", buffer);
		for (j = 0; j < stack_size(table_sig); j++) {
			TDAG DAG = stack_get(table_sig, j);
			unsigned k;
			out("\n   (");
			for (k = 0; k < DAG_arity(DAG); k++)
				out(k ? " cc_%d" : "cc_%d", CC_model_get_class_id(DAG_arg(DAG, k)));
			if (DAG_symb_type(DAG_symb(DAG)) & SYMB_PREDICATE)
				out(
					(CC_find_p(DAG)->boolean_value == BOOL_TRUE) ? " true)" : " false)");
			else
				out(" cc_%d)", CC_model_get_class_id(DAG));
		}
		out(")\n");
	}
	out(")\n");
	for (symb = 1; symb < stack_size(DAG_symb_stack); symb++) {
		unsigned j;
		Tstack_DAG table_sig = model_symb[symb];
		if (table_sig == NULL) continue;
		for (j = 0; j < stack_size(table_sig); j++) {
			TDAG DAG = stack_get(table_sig, j);
			unsigned k;
			for (k = 0; k < DAG_arity(DAG); k++)
				CC_model_reset_class_id(DAG_arg(DAG, k));
			CC_model_reset_class_id(DAG);
		}
		stack_free(table_sig);
	}
	free(model_symb);
	for (i = 0; i < stack_size(zero_arity); i++)
		CC_model_reset_class_id(stack_get(zero_arity, i));
	DAG_tmp_release();
}

/*
  --------------------------------------------------------------
  Public functions
  --------------------------------------------------------------
*/

void
CC_init(void)
{
	backfire = true;
	CC_status = SAT;
#ifdef HASH_STAT
	stat_hash_insert =
		stats_counter_new("CC_insert", "Number of sig insertions in CC", "%9d");
	stat_hash_collision =
		stats_counter_new("CC_collision", "Number of collisions in CC", "%9d");
	stat_hash_max_collision = stats_counter_new(
		"CC_max_collision", "Max number of collisions in CC", "%9d");
	stat_hash_eq_key =
		stats_counter_new("CC_eq_key", "Key collisions in CC", "%9d");
#endif
	sig_init();
	stack_INIT(zero_arity);
	stack_INIT(CC_quantified);
	stack_INIT(class_DAGs);
	stack_INIT(diseq_DAGs);
	undo_set_hook(CC_UNION, (Tundo_hook)CC_hook_union, sizeof(Tunion_BTcell));
	undo_set_hook(
		CC_UNION_P, (Tundo_hook)CC_hook_union_p, sizeof(Tunion_p_BTcell));
	undo_set_hook(CC_P_UNDEF, (Tundo_hook)CC_hook_set_p_undef, sizeof(TDAG));
	undo_set_hook(CC_SIG_ADD, (Tundo_hook)CC_hook_sig_add, sizeof(TDAG));
	undo_set_hook(CC_SIG_DEL, (Tundo_hook)CC_hook_sig_del, sizeof(TDAG));
	undo_set_hook(CC_STORE_QUANTIFIED, (Tundo_hook)CC_hook_store_quantified, 0);
	undo_set_hook(CC_STATUS, (Tundo_hook)CC_hook_status, 0);
#if DEBUG_CC >= 2
	undo_set_hook(CC_ASSERT, (Tundo_hook)CC_hook_assert, 0);
	undo_set_hook(CC_ASSERT_EQ, (Tundo_hook)CC_hook_assert_eq, 0);
#endif

	undo_set_hook(CC_NOTIFY, (Tundo_hook)CC_hook_top_notify, sizeof(TDAG));
	undo_set_hook(CC_DAG_ADD, (Tundo_hook)CC_hook_top_DAG_add, sizeof(TDAG));

	stack_INIT(merge_queue);
	DAG_set_hook_resize(CC_DAG_hook_resize);
	explain_init();
}

void
CC_done(void)
{
	backfire = true;
	assert(!stack_size(CC_quantified));
	explain_done();
	stack_free(merge_queue);
	stack_free(class_DAGs);
	stack_free(diseq_DAGs);
	stack_free(CC_quantified);
	stack_free(zero_arity);
	sig_done();
}
