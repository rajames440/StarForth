/**
   Module for representing formulas and terms
*/

/* -------------------------------------------------------------- */
/* #define DEBUG_TYPE_VARIABLES */
/* #define DEBUG_DAG */
#ifdef DEBUG
#define DAG_CHECK
#endif

#include "symbolic/DAG.h"

#include "symbolic/DAG-flag.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-prop.h"
#include "symbolic/DAG-ptr.h"
#include "symbolic/DAG-symb-DAG.h"
#include "symbolic/DAG-tmp.h"
#include "utils/general.h"
#include "utils/h-util.h"
#include "utils/options.h"
#include "veriT-config.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* #define DMEM */
#ifdef MEM
#include "symbolic/DAG-stat.h"
#warning compiling with memory checking
#endif
#define QUICK_MEM
#ifdef QUICK_MEM
#include "utils/statistics.h"
#endif

/* PF option to disable taking symmetry of equality into account */
/**
   \addtogroup arguments_user

   - --disable-sym-eq

   Disables symmetry of equality (EXPERIMENTAL - DO NOT USE).*/
static bool disable_sym_eq = false;

Tsymb PREDICATE_EQ = DAG_SYMB_NULL;
const char* const PREDICATE_EQ_STR = "=";

Tsymb QUANTIFIER_EXISTS = DAG_SYMB_NULL;
Tsymb QUANTIFIER_FORALL = DAG_SYMB_NULL;
/** \brief String of the predefined symbol for existential quantification. */
const char* const QUANTIFIER_EXISTS_STR = "exists";
/** \brief String of the predefined symbol for universal quantification. */
const char* const QUANTIFIER_FORALL_STR = "forall";

/**
   \brief The DAG table: stored in a single chunk of memory */
Tstack_SDAG DAG_table;
/**
   \brief Reference counter for DAGS */
uint32_t* gc_count = NULL;

/**
   \brief Hook functions called when DAG table is resized
   and when DAG is freed */
TSstack(_hook_resize, TDAG_hook_resize);
TSstack(_hook_free, TDAG_hook_free);
static Tstack_hook_resize stack_hook_resize;
static Tstack_hook_free stack_hook_free;

/** \brief Freed entry in the DAG table */
static TDAG DAG_freed = DAG_NULL;

#ifdef MEM
/* #define DAG_SPY(DAG) (DAG==230 || DAG==133) */
/* #define DAG_SPY(DAG) (DAG == 40) */
#define DAG_SPY(DAG) (false)
#endif

struct DAG_attr DAG_attr;

static void
DAG_attr_hook_resize(unsigned old_alloc, unsigned new_alloc)
{
#ifdef DMEM
	MY_REALLOC_DMEM(
		DAG_attr.sort, new_alloc * sizeof(*DAG_attr.sort),
		old_alloc * sizeof(*DAG_attr.sort));
	MY_REALLOC_DMEM(
		DAG_attr.misc, new_alloc * sizeof(*DAG_attr.misc),
		old_alloc * sizeof(*DAG_attr.misc));
	MY_REALLOC_DMEM(
		DAG_attr.hash_key, new_alloc * sizeof(*DAG_attr.hash_key),
		old_alloc * sizeof(*DAG_attr.hash_key));
	MY_REALLOC_DMEM(
		gc_count, new_alloc * sizeof(*gc_count), old_alloc * sizeof(*gc_count));
#else
	MY_REALLOC(DAG_attr.sort, new_alloc * sizeof(*DAG_attr.sort));
	MY_REALLOC(DAG_attr.misc, new_alloc * sizeof(*DAG_attr.misc));
	MY_REALLOC(DAG_attr.hash_key, new_alloc * sizeof(*DAG_attr.hash_key));
	MY_REALLOC(gc_count, new_alloc * sizeof(*gc_count));
#endif
	if (new_alloc <= old_alloc) return;
	memset(
		DAG_attr.sort + old_alloc, 0,
		(new_alloc - old_alloc) * sizeof(*DAG_attr.sort));
	memset(
		DAG_attr.misc + old_alloc, 0,
		(new_alloc - old_alloc) * sizeof(*DAG_attr.misc));
	memset(
		DAG_attr.hash_key + old_alloc, 0,
		(new_alloc - old_alloc) * sizeof(*DAG_attr.hash_key));
	memset(gc_count + old_alloc, 0, (new_alloc - old_alloc) * sizeof(*gc_count));
}

#ifdef DEBUG
void
DAG_table_print(void)
{
	TDAG DAG;
	fprintf(
		stderr, "DAG_table: size = %u, address = %p\n", DAG_table->size,
		(void*)DAG_table);
	fprintf(stderr, " DAG symb sort refs args\n");
	for (DAG = 1; DAG < DAG_table->size; ++DAG) {
		if (DAG_table->data[DAG].symb != DAG_SYMB_NULL) {
			unsigned i;
			fprintf(
				stderr, "%4u %4u %4u %4u", DAG, DAG_table->data[DAG].symb,
				DAG_attr.sort[DAG], gc_count[DAG]);
			fprintf(stderr, " {");
			for (i = 0; i < DAG_table->data[DAG].arity; ++i)
				fprintf(stderr, i ? ",%i" : "%i", DAG_arg(DAG, i));
			fprintf(stderr, "}\n");
		} else
			fprintf(stderr, "%4u **** **** ****\n", DAG);
	}
	if (DAG_freed != DAG_NULL) {
		TDAG DAG = DAG_freed;
		fprintf(stderr, "Freed positions:");
		do {
			fprintf(stderr, " %u", DAG);
			DAG = (TDAG)DAG_attr.hash_key[DAG];
		} while (DAG != DAG_NULL);
		fprintf(stderr, "\n");
	}
}
#endif

/*
  --------------------------------------------------------------
  hash tables for DAG stuff
  --------------------------------------------------------------
*/

static inline bool
ha_DAG_cmp(TDAG DAG1, TDAG DAG2)
{
	/* DD+PF in principle, this function will never be called with DAGs
     with different top symbols */
	assert(DAG_symb(DAG1) == DAG_symb(DAG2));
	if (DAG_hash(DAG1) != DAG_hash(DAG2) || DAG_arity(DAG1) != DAG_arity(DAG2))
		return false;
	switch (DAG_arity(DAG1)) {
		case 0: return true;
		case 1: return DAG_arg0(DAG1) == DAG_arg0(DAG2);
		case 2:
			return DAG_arg0(DAG1) == DAG_arg0(DAG2) &&
				DAG_arg1(DAG1) == DAG_arg1(DAG2);
		default:
			return memcmp(
							 DAG_args(DAG1), DAG_args(DAG2),
							 DAG_arity(DAG1) * sizeof(TDAG)) == 0;
	}
}

static inline unsigned
ha_DAG_hash(TDAG DAG)
{
	return DAG_attr.hash_key[DAG];
}

#define TYPE_EXT DAG
#define TYPE TDAG
#define TYPE_NULL DAG_NULL
#define HA_CMP ha_DAG_cmp
#define HA_HASH ha_DAG_hash
#define HA_AUTO_RESIZE

#include "utils/ha.c.tpl"

#undef TYPE_EXT
#undef TYPE
#undef TYPE_NULL
#undef HA_CMP
#undef HA_HASH
#undef HA_AUTO_RESIZE

typedef union
{
	Tha_DAG ha;
	TDAG DAG;
} Tsymb_to_DAG;

static Tsymb_to_DAG* symb_to_DAG = NULL;

static inline TDAG
symb_to_DAG_get(Tsymb symb)
{
	assert(DAG_symb_type(symb) & SYMB_NULLARY);
	return symb_to_DAG[symb].DAG;
}

static inline void
symb_to_DAG_set(Tsymb symb, TDAG DAG)
{
	assert(DAG_symb_type(symb) & SYMB_NULLARY);
	symb_to_DAG[symb].DAG = DAG;
}

static inline TDAG
symb_to_DAG_lookup(Tsymb symb, TDAG DAG)
{
	assert(!(DAG_symb_type(symb) & SYMB_NULLARY));
	if (!symb_to_DAG[symb].ha) return DAG_NULL;
	return ha_DAG_query(symb_to_DAG[symb].ha, DAG);
}

static inline void
symb_to_DAG_enter(Tsymb symb, TDAG DAG)
{
	assert(!(DAG_symb_type(symb) & SYMB_NULLARY));
	if (!symb_to_DAG[symb].ha) symb_to_DAG[symb].ha = ha_DAG_new(4);
	symb_to_DAG[symb].ha = ha_DAG_enter(symb_to_DAG[symb].ha, DAG);
}

static inline void
symb_to_DAG_remove(Tsymb symb, TDAG DAG)
{
	if (DAG_symb_type(symb) & SYMB_NULLARY) {
		assert(symb_to_DAG[symb].DAG);
		symb_to_DAG[symb].DAG = DAG_NULL;
		return;
	};
	assert(symb_to_DAG[symb].ha);
	ha_DAG_del(symb_to_DAG[symb].ha, DAG);
}

static void
symb_to_DAG_resize(unsigned old, unsigned new)
{
	unsigned i;
	if (new < old) {
		for (i = new; i < old; i++) assert(!symb_to_DAG[i].ha);
		MY_REALLOC(symb_to_DAG, new * sizeof(Tsymb_to_DAG));
		return;
	}
	MY_REALLOC(symb_to_DAG, new * sizeof(Tsymb_to_DAG));
	memset(symb_to_DAG + old, 0, (new - old) * sizeof(Tsymb_to_DAG));
}

static void
symb_to_DAG_free_symb(Tsymb symb)
{
	if (DAG_symb_type(symb) & SYMB_NULLARY)
		symb_to_DAG[symb].DAG = DAG_NULL;
	else
		ha_DAG_free(&symb_to_DAG[symb].ha);
}

/*
  --------------------------------------------------------------
  DAG stuff
  --------------------------------------------------------------
*/

int
DAG_equal(const TDAG DAG1, const TDAG DAG2)
{
	return DAG1 == DAG2;
}

unsigned
DAG_hash(const TDAG DAG)
{
	return DAG_attr.hash_key[DAG];
}

int
DAG_cmp(const TDAG DAG1, const TDAG DAG2)
/* PF ordering function on DAGs: returns -1, 0, 1
   for qsort, one more dereferencing needed: see DAG_cmp_q */
{
  return (DAG1 > DAG2) - (DAG1 < DAG2);
}

int
DAG_cmp_q(const TDAG *PDAG1, const TDAG *PDAG2)
/* PF ordering function on DAGs: returns -1, 0, 1
   like a compare for qsort */
{
	return DAG_cmp(*PDAG1, *PDAG2);
}

bool
stack_DAG_contains(Tstack_DAG stack, TDAG DAG)
{
	if (stack_is_empty(stack)) return false;
	int imid, imin = 0, imax = stack_size(stack) - 1;
	while (imin <= imax) {
		imid = imin + (imax - imin) / 2;
		if (DAG == stack_get(stack, imid)) return true;
		if (DAG < stack_get(stack, imid))
			imax = imid - 1;
		else if (DAG > stack_get(stack, imid))
			imin = imid + 1;
	}
	return false;
}

int
stack_DAG_find(Tstack_DAG stack, TDAG DAG)
{
	if (stack_is_empty(stack)) return -1;
	int imid, imin = 0, imax = stack_size(stack) - 1;
	while (imin <= imax) {
		imid = imin + (imax - imin) / 2;
		if (DAG == stack_get(stack, imid)) return imid;
		if (DAG < stack_get(stack, imid))
			imax = imid - 1;
		else if (DAG > stack_get(stack, imid))
			imin = imid + 1;
	}
	return -1;
}

Tstack_DAG
stack_DAG_intersect(Tstack_DAG stack0, Tstack_DAG stack1)
{
	unsigned i = 0, j = 0;
	Tstack_DAG result;
	stack_INIT(result);
	while (i < stack_size(stack0) && j < stack_size(stack1))
		if (stack_get(stack0, i) < stack_get(stack1, j))
			i++;
		else if (stack_get(stack1, j) < stack_get(stack0, i))
			j++;
		else /* if stack_get(stack0, i) == stack_get(stack1, j) */
		{
			stack_push(result, stack_get(stack1, j++));
			i++;
		}
	return result;
}

bool
stack_DAG_equal(Tstack_DAG stack0, Tstack_DAG stack1)
{
	unsigned i = 0, j = 0;
	while (i < stack_size(stack0) && j < stack_size(stack1))
		if (stack_get(stack0, i) < stack_get(stack1, j))
			return false;
		else if (stack_get(stack1, j) < stack_get(stack0, i))
			return false;
		else /* if stack_get(stack0, i) == stack_get(stack1, j) */
		{
			++i;
			++j;
		}
	return true;
}

bool
stack_DAG_subset(Tstack_DAG stack0, Tstack_DAG stack1)
{
	unsigned i = 0, j = 0;
	while (i < stack_size(stack0))
		if (j == stack_size(stack1) || stack_get(stack0, i) < stack_get(stack1, j))
			return false;
		else if (stack_get(stack0, i) == stack_get(stack1, j))
			++i;
		else
			j++;
	return true;
}

Tstack_DAG
stack_DAG_difference(Tstack_DAG stack0, Tstack_DAG stack1)
{
	unsigned i, j;
	Tstack_DAG result;
	stack_INIT(result);
	i = j = 0;
	while (i < stack_size(stack0) && j < stack_size(stack1))
		if (stack_get(stack0, i) < stack_get(stack1, j)) {
			stack_push(result, stack_get(stack0, i));
			++i;
		} else if (stack_get(stack0, i) > stack_get(stack1, j))
			j++;
		else {
			i++;
			j++;
		}
	while (i < stack_size(stack0)) stack_push(result, stack_get(stack0, i++));
	return result;
}

static TDAG
DAG_gc_inc(TDAG DAG)
{
	if (gc_count[DAG] == UINT32_MAX) my_error("DAG_gc_inc: limit reached\n");
	++gc_count[DAG];
	return DAG;
}

static TDAG
DAG_gc_dec(TDAG DAG)
{
	unsigned i;
#ifdef DEBUG_DAG
	my_DAG_message("DAG_gc_dec %u:%D\n", DAG, DAG);
#endif
	if (gc_count[DAG] == 0) my_error("DAG_gc_dec: under limit\n");
	if (--gc_count[DAG] > 0) return DAG;
#ifdef DEBUG_DAG
	my_DAG_message("freeing DAG %u:%D\n", DAG, DAG);
#endif
	assert(!DAG_attr.misc[DAG]);
	for (i = 0; i < DAG_arity(DAG); i++) {
#ifdef MEM
		if (DAG_SPY(DAG_arg(DAG, i))) {
			my_DAG_message("Released %d from %d\n", DAG_arg(DAG, i), DAG);
			breakpoint();
		}
#endif
		DAG_gc_dec(DAG_arg(DAG, i));
	}
	symb_to_DAG_remove(DAG_symb(DAG), DAG);
	if (DAG_arity(DAG) > 2) free(DAG_args(DAG));
#ifdef DAG_CHECK
	memset(&DAG_table->data[DAG], 0, sizeof(struct TSDAG));
#endif
	DAG_table->data[DAG].symb = DAG_SYMB_NULL;
	DAG_attr.hash_key[DAG] = (unsigned)DAG_freed;
	DAG_freed = DAG;
	for (i = 0; i < stack_hook_free->size; i++) stack_hook_free->data[i](DAG);
	return DAG_NULL;
}

static void
DAG_hook_resize(unsigned old_alloc, unsigned new_alloc)
{
	unsigned i;
	for (i = 0; i < stack_hook_resize->size; i++)
		stack_hook_resize->data[i](old_alloc, new_alloc);
}

/*
  --------------------------------------------------------------
  Constructors
  --------------------------------------------------------------
*/

static Tstack_sort sort_stack = NULL;

/** \brief Specialised TDAG constructor for null-ary terms */

TDAG
DAG_new_nullary(Tsymb symb)
{
	TDAG DAG1, DAG2;
	unsigned key;
	if (!symb) my_error("DAG_new: null symbol\n");
	if (DAG_symb_type(symb) & SYMB_NULLARY) {
		DAG1 = symb_to_DAG_get(symb);
		if (DAG1) return DAG1;
		if (!DAG_freed) {
			/* IMPROVE do not call hooks on every resize */
			DAG_freed = DAG_table->size;
			stack_inc_hook(DAG_table, DAG_hook_resize);
			DAG_attr.hash_key[DAG_freed] = (unsigned)DAG_NULL;
		}
		DAG1 = DAG_freed;
		DAG_freed = (TDAG)DAG_attr.hash_key[DAG1];
		DAG_table->data[DAG1].symb = symb;
		DAG_attr.sort[DAG1] = DAG_symb_check(symb, 0, NULL);
		if (!DAG_attr.sort[DAG1]) my_error("DAG_new: unable to determine sort\n");
		DAG_table->data[DAG1].arity = 0;
		gc_count[DAG1] = 0;
		key = hash_one_at_a_time_u_inc(0, DAG_symb_key(symb));
		key = hash_one_at_a_time_u_inc(key, 0);
		DAG_attr.hash_key[DAG1] = hash_one_at_a_time_inc_end(key);
		DAG_attr.misc[DAG1] = 0;
		DAG_table->data[DAG1].quant = quantifier(symb);
		symb_to_DAG_set(symb, DAG1);
		return DAG1;
	}
	if (!DAG_freed) {
		/* IMPROVE do not call hooks on every resize */
		DAG_freed = DAG_table->size;
		stack_inc_hook(DAG_table, DAG_hook_resize);
		DAG_attr.hash_key[DAG_freed] = (unsigned)DAG_NULL;
	}
	DAG1 = DAG_freed;
	DAG_freed = (TDAG)DAG_attr.hash_key[DAG1];
	DAG_table->data[DAG1].symb = symb;
	DAG_attr.sort[DAG1] = DAG_symb_check(symb, 0, NULL);
	if (!DAG_attr.sort[DAG1]) my_error("DAG_new: unable to determine sort\n");
	DAG_table->data[DAG1].arity = 0;
	gc_count[DAG1] = 0;
	key = hash_one_at_a_time_u_inc(0, DAG_symb_key(symb));
	key = hash_one_at_a_time_u_inc(key, 0);
	DAG_attr.hash_key[DAG1] = hash_one_at_a_time_inc_end(key);
	DAG2 = symb_to_DAG_lookup(symb, DAG1);
	if (DAG2) {
		DAG_attr.hash_key[DAG1] = (unsigned)DAG_freed;
		DAG_table->data[DAG1].symb = DAG_SYMB_NULL;
		DAG_freed = DAG1;
		return DAG2;
	}
	DAG_attr.misc[DAG1] = 0;
	DAG_table->data[DAG1].quant = quantifier(symb);
	symb_to_DAG_enter(symb, DAG1);
#ifdef DEBUG_TYPE_VARIABLES
	my_DAG_message("DAG_new: %D has sort %S.\n", DAG1, DAG_sort(DAG1));
#endif
	return DAG1;
}

TDAG
DAG_new_unary(Tsymb symb, TDAG arg)
{
	TDAG DAG1, DAG2;
	unsigned key;
	if (!symb) my_error("DAG_new_unary: null symbol\n");
#ifdef DAG_CHECK
	if (!arg) my_error("DAG_new_unary: unused subdag\n");
#endif
	if (!DAG_freed) {
		/* IMPROVE do not call hooks on every resize */
		DAG_freed = DAG_table->size;
		stack_inc_hook(DAG_table, DAG_hook_resize);
		DAG_attr.hash_key[DAG_freed] = (unsigned)DAG_NULL;
	}
	DAG1 = DAG_freed;
	DAG_freed = (TDAG)DAG_attr.hash_key[DAG1];
	DAG_table->data[DAG1].symb = symb;
	DAG_attr.sort[DAG1] = DAG_symb_check(symb, 1, &DAG_sort(arg));
	if (!DAG_attr.sort[DAG1])
		my_error("DAG_new_unary: unable to determine sort\n");
	DAG_table->data[DAG1].arity = 1;
	DAG_table->data[DAG1].args.bin.DAG0 = arg;
	gc_count[DAG1] = 0;
	key = hash_one_at_a_time_u_inc(0, DAG_symb_key(symb));
	key = hash_one_at_a_time_u_inc(key, 1);
	key = hash_one_at_a_time_u_inc(key, DAG_attr.hash_key[arg]);
	DAG_attr.hash_key[DAG1] = hash_one_at_a_time_inc_end(key);
	DAG2 = symb_to_DAG_lookup(symb, DAG1);
	if (DAG2) {
		DAG_attr.hash_key[DAG1] = (unsigned)DAG_freed;
		DAG_table->data[DAG1].symb = DAG_SYMB_NULL;
		DAG_freed = DAG1;
		return DAG2;
	}
	DAG_attr.misc[DAG1] = 0;
	DAG_table->data[DAG1].quant = quantifier(symb);
	DAG_table->data[DAG1].quant |= DAG_table->data[arg].quant;
	DAG_gc_inc(arg);
#ifdef MEM
	if (DAG_SPY(arg)) {
		my_DAG_message("Used %d in %d\n", arg, DAG1);
		breakpoint();
	}
#endif
	symb_to_DAG_enter(symb, DAG1);
#ifdef DEBUG_TYPE_VARIABLES
	my_DAG_message("DAG_new_unary: %D has sort %S.\n", DAG1, DAG_sort(DAG1));
#endif
	return DAG1;
}

TDAG
DAG_new_binary(Tsymb symb, TDAG arg0, TDAG arg1)
{
	TDAG DAG1, DAG2;
	unsigned key;
	if (!symb)
		my_error("DAG_new_binary: null symbol\n");
#ifdef DAG_CHECK
	if (arg0 == DAG_NULL || arg1 == DAG_NULL)
		my_error("DAG_new_binary: unused subdag\n");
#endif
	if (!DAG_freed) {
		/* IMPROVE do not call hooks on every resize */
		DAG_freed = DAG_table->size;
		stack_inc_hook(DAG_table, DAG_hook_resize);
		DAG_attr.hash_key[DAG_freed] = (unsigned)DAG_NULL;
	}
	DAG1 = DAG_freed;
	DAG_freed = (TDAG)DAG_attr.hash_key[DAG1];
	DAG_table->data[DAG1].symb = symb;
	stack_inc_n(sort_stack, 1);
	sort_stack->data[0] = DAG_sort(arg0);
	sort_stack->data[1] = DAG_sort(arg1);
	DAG_attr.sort[DAG1] = DAG_symb_check(symb, 2, sort_stack->data);
	if (!DAG_attr.sort[DAG1])
		my_error("DAG_new_binary: unable to determine sort\n");
	stack_reset(sort_stack);
	if (symb == PREDICATE_EQ && !disable_sym_eq && DAG_cmp(arg0, arg1) > 0)
		SWAP(arg0, arg1);
	DAG_table->data[DAG1].arity = 2;
	DAG_table->data[DAG1].args.bin.DAG0 = arg0;
	DAG_table->data[DAG1].args.bin.DAG1 = arg1;
	gc_count[DAG1] = 0;
	key = hash_one_at_a_time_u_inc(0, DAG_symb_key(symb));
	key = hash_one_at_a_time_u_inc(key, 2);
	key = hash_one_at_a_time_u_inc(key, DAG_attr.hash_key[arg0]);
	key = hash_one_at_a_time_u_inc(key, DAG_attr.hash_key[arg1]);
	DAG_attr.hash_key[DAG1] = hash_one_at_a_time_inc_end(key);
	DAG2 = symb_to_DAG_lookup(symb, DAG1);
	if (DAG2) {
		DAG_attr.hash_key[DAG1] = (unsigned)DAG_freed;
		DAG_table->data[DAG1].symb = DAG_SYMB_NULL;
		DAG_freed = DAG1;
		return DAG2;
	}
	DAG_attr.misc[DAG1] = 0;
	DAG_table->data[DAG1].quant = quantifier(symb);
	DAG_table->data[DAG1].quant |= DAG_table->data[arg0].quant;
	DAG_table->data[DAG1].quant |= DAG_table->data[arg1].quant;
	DAG_gc_inc(arg0);
	DAG_gc_inc(arg1);
#ifdef MEM
	if (DAG_SPY(arg0)) {
		my_DAG_message("Used %d in %d\n", arg0, DAG1);
		breakpoint();
	}
	if (DAG_SPY(arg1)) {
		my_DAG_message("Used %d in %d\n", arg1, DAG1);
		breakpoint();
	}
#endif
	symb_to_DAG_enter(symb, DAG1);
#ifdef DEBUG_TYPE_VARIABLES
	my_DAG_message("DAG_new_binary: %D has sort %S.\n", DAG1, DAG_sort(DAG1));
#endif
	return DAG1;
}

TDAG
DAG_new(Tsymb symb, unsigned arity, TDAG* PDAG)
{
	TDAG DAG1, DAG2;
	unsigned i;
	unsigned key;
	if (arity == 0) {
		TDAG result = DAG_new_nullary(symb);
		free(PDAG);
		return result;
	}
	if (arity == 1) {
		TDAG result = DAG_new_unary(symb, PDAG[0]);
		free(PDAG);
		return result;
	}
	if (arity == 2) {
		TDAG result = DAG_new_binary(symb, PDAG[0], PDAG[1]);
		free(PDAG);
		return result;
	}
	assert(arity > 2);
	assert(symb);
#ifdef DAG_CHECK
	for (i = 0; i < arity; i++) assert(PDAG[i]);
#endif
	assert(arity <= UINT_MAX);
	if (!DAG_freed) {
		/* TODO: IMPROVE do not call hooks on every resize */
		DAG_freed = DAG_table->size;
		stack_inc_hook(DAG_table, DAG_hook_resize);
		DAG_attr.hash_key[DAG_freed] = (unsigned)DAG_NULL;
	}
	DAG1 = DAG_freed;
	DAG_freed = (TDAG)DAG_attr.hash_key[DAG1];
	DAG_table->data[DAG1].symb = symb;
	stack_inc_n(sort_stack, arity);
	for (i = 0; i < arity; i++) sort_stack->data[i] = DAG_sort(PDAG[i]);
	DAG_attr.sort[DAG1] = DAG_symb_check(symb, arity, sort_stack->data);
	if (!DAG_attr.sort[DAG1]) my_error("DAG_new: unable to determine sort\n");
	stack_reset(sort_stack);
	/* TODO: This might be to strict after removing DAG_ground */
	if (arity >= 1u << 31) my_error("DAG arity too large\n");

	DAG_table->data[DAG1].arity = arity;
	DAG_table->data[DAG1].args.PDAG = PDAG;
	gc_count[DAG1] = 0;
	key = hash_one_at_a_time_u_inc(0, DAG_symb_key(symb));
	key = hash_one_at_a_time_u_inc(key, arity);
	for (i = 0; i < arity; i++)
		key = hash_one_at_a_time_u_inc(key, DAG_attr.hash_key[PDAG[i]]);
	DAG_attr.hash_key[DAG1] = hash_one_at_a_time_inc_end(key);
	DAG2 = symb_to_DAG_lookup(symb, DAG1);
	if (DAG2) {
		DAG_attr.hash_key[DAG1] = (unsigned)DAG_freed;
		DAG_table->data[DAG1].symb = DAG_SYMB_NULL;
		DAG_freed = DAG1;
		free(PDAG);
		return DAG2;
	}
	DAG_attr.misc[DAG1] = 0;
	DAG_table->data[DAG1].quant = quantifier(symb);
	for (i = 0; i < DAG_table->data[DAG1].arity; i++) {
		DAG_table->data[DAG1].quant |= DAG_table->data[PDAG[i]].quant;
		DAG_gc_inc(PDAG[i]);
#ifdef MEM
		if (DAG_SPY(PDAG[i])) {
			my_DAG_message("Used %d in %d\n", PDAG[i], DAG1);
			breakpoint();
		}
#endif
	}
	symb_to_DAG_enter(symb, DAG1);

#ifdef DEBUG_TYPE_VARIABLES
	my_DAG_message("DAG_new: %D has sort %S.\n", DAG1, DAG_sort(DAG1));
#endif
	return DAG1;
}

TDAG
DAG_new_args(Tsymb symb, ...)
{
	va_list adpar;
	TDAG DAG0, DAG1, DAG2;
	unsigned arity;
	TDAG* DAGs = NULL;
	va_start(adpar, symb);
	if ((DAG0 = va_arg(adpar, TDAG)) == DAG_NULL) {
		va_end(adpar);
		return DAG_new_nullary(symb);
	}
	if ((DAG1 = va_arg(adpar, TDAG)) == DAG_NULL) {
		va_end(adpar);
		return DAG_new_unary(symb, DAG0);
	}
	if ((DAG2 = va_arg(adpar, TDAG)) == DAG_NULL) {
		va_end(adpar);
		return DAG_new_binary(symb, DAG0, DAG1);
	}
	arity = 3;
	MY_REALLOC(DAGs, arity * sizeof(TDAG));
	DAGs[0] = DAG0;
	DAGs[1] = DAG1;
	DAGs[2] = DAG2;
	while ((DAG0 = va_arg(adpar, TDAG)) != DAG_NULL) {
		++arity;
		MY_REALLOC(DAGs, arity * sizeof(TDAG));
		DAGs[arity - 1] = DAG0;
	}
	va_end(adpar);
	return DAG_new(symb, arity, DAGs);
}

TDAG
DAG_new_stack(Tsymb symb, Tstack_DAG stack_arg)
{
	switch (stack_arg->size) {
		case 0: return DAG_new_nullary(symb);
		case 1: return DAG_new_unary(symb, stack_arg->data[0]);
		case 2: return DAG_new_binary(symb, stack_arg->data[0], stack_arg->data[1]);
		default: {
			TDAG* PDAG = NULL;
			MY_MALLOC(PDAG, stack_arg->size * sizeof(TDAG));
			memcpy(PDAG, stack_arg->data, stack_arg->size * sizeof(TDAG));
			return DAG_new(symb, stack_arg->size, PDAG);
		}
	}
}

#ifdef MEM
static unsigned count = 0;
#endif

TDAG
DAG_dup(TDAG DAG)
{
#ifdef DEBUG_DAG
	my_DAG_message("DAG_dup(%u) %u:%D\n", gc_count[DAG], DAG, DAG);
#endif
#ifdef MEM
	if (DAG_SPY(DAG)) {
		my_DAG_message("Count+ %d: %d, %d\n", DAG, ++count, gc_count[DAG] + 1);
		breakpoint();
	}
#endif
	return DAG_gc_inc(DAG);
}

void
DAG_free(TDAG DAG)
{
#ifdef DEBUG_DAG
	my_DAG_message("DAG_free(%u) %u:%D\n", gc_count[DAG], DAG, DAG);
#endif
#ifdef MEM
	if (DAG_SPY(DAG)) {
		my_DAG_message("Count- %d: %d, %d\n", DAG, --count, gc_count[DAG] - 1);
		breakpoint();
	}
#endif
	DAG_gc_dec(DAG);
}

/*
  --------------------------------------------------------------
  Checking memory
  --------------------------------------------------------------
*/

#ifdef MEM
static int
DAG_dec_size(TDAG* PDAG1, TDAG* PDAG2)
{
	if (DAG_attr.misc[*PDAG2] == DAG_attr.misc[*PDAG1])
		return DAG_count_nodes(*PDAG2) - DAG_count_nodes(*PDAG1);
	if (DAG_attr.misc[*PDAG1]) return 1;
	return -1;
}

static void
mark_indirect_aux(TDAG DAG)
{
	unsigned i;
	DAG_attr.misc[DAG] = 1;
	for (i = 0; i < DAG_arity(DAG); i++) mark_indirect_aux(DAG_arg(DAG, i));
}

static void
mark_indirect(TDAG DAG)
{
	unsigned i;
	for (i = 0; i < DAG_arity(DAG); i++) mark_indirect_aux(DAG_arg(DAG, i));
}

static void
DAG_check_mem(void)
{
	unsigned i;
	TDAG DAG;
	Tstack_DAG unfreed;
	stack_INIT(unfreed);
	for (DAG = 0; DAG < DAG_table->size; DAG++)
		if (gc_count[DAG]) stack_push(unfreed, DAG);
	if (unfreed->size) {
		for (i = 0; i < unfreed->size; i++) mark_indirect(unfreed->data[i]);
		stack_sort(unfreed, DAG_dec_size);
		my_DAG_message(
			"Largest unfreed DAG %d: %D\n", unfreed->data[0], unfreed->data[0]);
		for (i = 0; i < unfreed->size; i++) {
			DAG = unfreed->data[i];
			my_DAG_message(
				"Unfreed DAG %d: %d, gc:%d, %D %s\n", i, DAG, gc_count[DAG], DAG,
				DAG_attr.misc[DAG] ? "(indirect)" : "");
			DAG_attr.misc[DAG] = 0;
		}
	}
	stack_free(unfreed);
}
#endif

#ifdef QUICK_MEM
static int
DAG_check_mem_quick(void)
{
	int c = 0;
	TDAG DAG;
	for (DAG = 0; DAG < DAG_table->size; DAG++) c += (gc_count[DAG] ? 1 : 0);
	return c;
}
#endif

/*
  --------------------------------------------------------------
  Initialisation and release
  --------------------------------------------------------------
*/

void
DAG_init(void)
{
	stack_INIT(DAG_table);
	DAG_freed = DAG_NULL;
	stack_inc0(DAG_table);

	options_new(
		0, "disable-sym-eq",
		"Disable symmetry of equality (EXPERIMENTAL - don't use that)",
		&disable_sym_eq);

	stack_INIT(stack_hook_resize);
	stack_INIT(stack_hook_free);

	DAG_set_hook_resize(DAG_attr_hook_resize);

	DAG_sort_init();
	DAG_symb_init();
	DAG_prop_init();
	DAG_flag_init();
	DAG_tmp_init();
	DAG_symb_DAG_init();

	stack_INIT(sort_stack);

	DAG_symb_set_hook_free(symb_to_DAG_free_symb);
	DAG_symb_set_hook_resize(symb_to_DAG_resize);
}

void
DAG_done(void)
{
	DAG_symb_DAG_done();
#ifdef MEM
	DAG_check_mem();
#endif
#ifdef QUICK_MEM
	stats_easy_set(
		"DAG_unfreed", "number of unfreed DAGs", "%6d", DAG_check_mem_quick());
#endif
	stack_free(sort_stack);
	DAG_tmp_done();
	DAG_flag_done();
	DAG_sort_done();
	DAG_symb_done();
	DAG_prop_done();
	DAG_hook_resize(DAG_table->alloc, 0);
	stack_free(DAG_table);
	stack_free(stack_hook_resize);
	stack_free(stack_hook_free);
}

void
DAG_set_hook_resize(TDAG_hook_resize hook_resize)
{
	hook_resize(0, DAG_table->alloc);
	stack_push(stack_hook_resize, hook_resize);
}

void
DAG_set_hook_free(TDAG_hook_free hook_free)
{
	stack_push(stack_hook_free, hook_free);
}
