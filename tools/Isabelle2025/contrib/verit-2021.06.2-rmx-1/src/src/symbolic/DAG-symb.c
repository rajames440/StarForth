/*
  --------------------------------------------------------------
  Symbol stuff
  --------------------------------------------------------------
*/
#include "symbolic/DAG-symb.h"

#include "limits.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-sort-pm.h"
#include "utils/general.h"
#include "utils/h-util.h"
#include "utils/nonce.h"
#include "utils/stack.h"

#include <string.h>

Tstack_Ssymb DAG_symb_stack;

/**
   \brief Hook functions called when symb table is resized
   and when symb is freed */
TSstack(_hook_resize, TDAG_symb_hook_resize);
TSstack(_hook_free, TDAG_symb_hook_free);
static Tstack_hook_resize stack_hook_resize;
static Tstack_hook_free stack_hook_free;

/**
   \brief Accesses the name of the symbol
   \param symb
   \return The name of symb */
#define DAG_symb_name(symb) (__DAG_SYMB_DATA(symb).value.name)

/** Assumes use of unsigned long long int */
#define MAX_SIZE 64

unsigned long long int* symb_mask;
unsigned* symb_precedence;
unsigned* symb_weight;

/*
  --------------------------------------------------------------
  Fresh name generators
  --------------------------------------------------------------
*/

static Tnonce nonce_const; /*< constant symbols */
static Tnonce nonce_func; /*< function symbols */
static Tnonce nonce_pred; /*< predicate symbols */
static Tnonce nonce_skolem; /*< skolem symbols */
static Tnonce nonce_var; /*< variables */

/*
  --------------------------------------------------------------
  Named symbols
  --------------------------------------------------------------
*/

/**
   \brief Structure to store a set of homonym symbols */
typedef struct Tsymb_homonym
{
	char* name; /*< the name of the symbols */
	Tstack_symb symbs; /*< homonym symbols */
} Tsymb_homonym;

typedef Tsymb_homonym* TPsymb_homonym;

static inline bool
symb_homonym_cmp(TPsymb_homonym Ph1, TPsymb_homonym Ph2)
{
	return !strcmp(Ph1->name, Ph2->name);
}

static inline unsigned
symb_homonym_hash(TPsymb_homonym Ph1)
{
	return hash_one_at_a_time(Ph1->name);
}

static inline void
symb_homonym_free(TPsymb_homonym Ph)
{
	if (!Ph) return;
	stack_free(Ph->symbs);
	free(Ph);
}

#define TYPE_EXT symb_homonym
#define TYPE TPsymb_homonym
#define TYPE_NULL NULL
#define HA_CMP symb_homonym_cmp
#define HA_HASH symb_homonym_hash
#define HA_FREE symb_homonym_free
#define HA_AUTO_RESIZE

#include "utils/ha.c.tpl"

#undef TYPE_EXT
#undef TYPE
#undef TYPE_NULL
#undef HA_CMP
#undef HA_HASH
#undef HA_FREE
#undef HA_AUTO_RESIZE

/**
   \brief Hash table to store sets of homonyms */
static Tha_symb_homonym symb_homonym;

/* PF retrieve homonym structure for name */
static inline Tsymb_homonym*
homonym_query(char* name)
{
	Tsymb_homonym tmp;
	tmp.name = name;
	return ha_symb_homonym_query(symb_homonym, &tmp);
}

/* PF insert symb in symb_by_name, symb_table */
static void
homonym_enter(Tsymb symb)
{
	TPsymb_homonym Psymb_homonym = homonym_query(DAG_symb_name(symb));
	if (!Psymb_homonym) {
		MY_MALLOC(Psymb_homonym, sizeof(Tsymb_homonym));
		stack_INIT(Psymb_homonym->symbs);
		Psymb_homonym->name = DAG_symb_name(symb);
		stack_push(Psymb_homonym->symbs, symb);
		symb_homonym = ha_symb_homonym_enter(symb_homonym, Psymb_homonym);
		assert(homonym_query(DAG_symb_name(symb)));
		return;
	}
	stack_push(Psymb_homonym->symbs, symb);
}

static void
DAG_symb_hook_resize(unsigned old_alloc, unsigned new_alloc)
{
	unsigned i;
	for (i = 0; i < stack_hook_resize->size; i++)
		stack_hook_resize->data[i](old_alloc, new_alloc);
}

/* For symb mask */

/* TODO: fix this workaround */
extern Tstack_DAG* sort_symbols;

static inline void
set_symb_mask(Tsymb symb, Tsort sort)
{
	MY_REALLOC(symb_mask, (symb + 1) * MAX_SIZE);
	symb_mask[symb] = 0;
	/* Only function symbols with arity bigger than one may have mask */
	if (sort && sort != SORT_BOOLEAN && DAG_sort_arity(sort)) {
		/* Workaround for intepreted symbols */
		sort = DAG_sort_arity(sort) == -1
			? DAG_sort_sub(sort, 1)
			: DAG_sort_sub(sort, DAG_sort_arity(sort) - 1);
		if (!sort_symbols[sort]) stack_INIT(sort_symbols[sort]);
		stack_push(sort_symbols[sort], symb);
		/* Only symbols below threshold have their masks set. The value
         corresponds to the number of symbols of a given sort: 2^0 to first, 2^1
         to second and so on */
		if (stack_size(sort_symbols[sort]) < MAX_SIZE)
			symb_mask[symb] = 1 << (stack_size(sort_symbols[sort]) - 1);
	}
}

static inline void
set_symb_orderings(Tsymb symb, Tsort sort)
{
	MY_REALLOC(symb_precedence, (symb + 1) * sizeof(unsigned));
	MY_REALLOC(symb_weight, (symb + 1) * sizeof(unsigned));
	symb_precedence[symb] = UINT_MAX;
	symb_weight[symb] = 0;
	/* Only function symbols with arity bigger than one may have precedence */
	if (sort && sort != SORT_BOOLEAN && DAG_sort_arity(sort)) {
		/* Workaround for intepreted symbols */
		sort = DAG_sort_arity(sort) == -1
			? DAG_sort_sub(sort, 1)
			: DAG_sort_sub(sort, DAG_sort_arity(sort) - 1);
		symb_precedence[symb] = stack_size(sort_symbols[sort]) - 1;
	}
}

Tsymb
DAG_symb_new(const char* name, Tsymb_type type, Tsort sort)
{
	Tsymb symb = DAG_SYMB_NULL;
	Tsymb_homonym* Psymb_homonym = homonym_query((char*)name);
	type |= SYMB_NAMED;
	if (sort && DAG_sort_arity(sort) == 0 && !(type & SYMB_QUANTIFIER))
		type |= SYMB_NULLARY;
	if (Psymb_homonym) {
		unsigned i;
		/* PF Check if other symbols are compatible so that sort of
         every DAG may be deduced by symbol and subDAGs */
		for (i = 0; i < Psymb_homonym->symbs->size; i++)
			if (
				DAG_symb_sort(Psymb_homonym->symbs->data[i]) == sort &&
				DAG_symb_type(Psymb_homonym->symbs->data[i]) == type)
				return Psymb_homonym->symbs->data[i];
	}
	symb = stack_size(DAG_symb_stack);
	stack_inc_hook(DAG_symb_stack, DAG_symb_hook_resize);
	DAG_symb_stack->data[symb].value.name = strmake(name);
	DAG_symb_stack->data[symb].type = type;
	DAG_symb_stack->data[symb].sort = sort;
	DAG_symb_stack->data[symb].misc = 0;
	DAG_symb_stack->data[symb].hash_key =
		hash_one_at_a_time_inc(0, DAG_symb_name(symb));
	DAG_symb_stack->data[symb].hash_key = hash_one_at_a_time_u_inc(
		DAG_symb_stack->data[symb].hash_key,
		Psymb_homonym ? stack_size(Psymb_homonym->symbs) : 0);
	DAG_symb_stack->data[symb].hash_key =
		hash_one_at_a_time_inc_end(DAG_symb_stack->data[symb].hash_key);
	homonym_enter(symb);
	/* Setting symbol mask */
	set_symb_mask(symb, sort);
	/* Setting orderings */
	set_symb_orderings(symb, sort);
	return symb;
}

static void
DAG_symb_free(Tsymb symb)
/* PF Free memory of symbol Pvoid */
{
	unsigned i;
	/* TODO: one should free the symbol on the hash tables too */
	if (symb == DAG_SYMB_NULL) return;
	assert(symb < stack_size(DAG_symb_stack));
	for (i = 0; i < stack_hook_free->size; i++) stack_hook_free->data[i](symb);
	if (DAG_symb_type(symb) & SYMB_NAMED)
		free(DAG_symb_stack->data[symb].value.name);
	else if (DAG_symb_type(symb) & SYMB_INTEGER)
		mpz_clear(DAG_symb_stack->data[symb].value.mpz);
	else if (DAG_symb_type(symb) & SYMB_NUMBER)
		mpq_clear(DAG_symb_stack->data[symb].value.mpq);
}

Tsymb
DAG_symb_lookup_sort(char* name, Tsort sort)
{
	unsigned i;
	Tsymb found_symb = DAG_SYMB_NULL;
	Tsymb_homonym* homonym = homonym_query(name);
	if (!homonym) return DAG_SYMB_NULL;
	if (sort == DAG_SORT_NULL) return DAG_SYMB_NULL;
	for (i = 0; i < homonym->symbs->size; i++) {
		if (!DAG_sort_subsumes(DAG_symb_sort(homonym->symbs->data[i]), sort))
			continue;
		/*PF2DD I am a bit puzzled:
        why the order in homonym->symbs should define the behavior here */
		if (
			found_symb &&
			!DAG_sort_subsumes(
				DAG_symb_sort(homonym->symbs->data[i]), DAG_symb_sort(found_symb)))
			return DAG_SYMB_NULL;
		found_symb = homonym->symbs->data[i];
	}
	return found_symb;
}

Tsymb
DAG_symb_skolem(Tsort sort)
{
	nonce_next(&nonce_skolem);
	return DAG_symb_new(nonce_skolem.buffer, 0, sort);
}

Tsymb
DAG_symb_const(Tsort sort)
{
	Tsymb_type type = (sort == SORT_BOOLEAN) ? SYMB_PREDICATE : 0;
	nonce_next(&nonce_const);
	return DAG_symb_new(nonce_const.buffer, type, sort);
}

Tsymb
DAG_symb_variable(Tsort sort)
{
	nonce_next(&nonce_var);
	return DAG_symb_new(nonce_var.buffer, SYMB_VARIABLE, sort);
}

Tsymb
DAG_symb_function(Tsort sort)
{
	nonce_next(&nonce_func);
	return DAG_symb_new(nonce_func.buffer, 0, sort);
}

/**
   \brief returns sort of application of symb to arguments of sort
   Psort[0] ...Psort[n-1]
   \param symb the symbol
   \param n the number of arguments
   \param Psort the argument sorts
   \return DAG_SORT_NULL if it cannot be applied */
Tsort
DAG_symb_check(Tsymb symb, unsigned n, Tsort* Psort)
{
	unsigned i;
	Tsort sort;
	/* PF special cases (mainly overloaded symbols) */
	if (n == 0) return DAG_symb_sort(symb);
	if (symb == PREDICATE_EQ) {
		if (
			n == 2 && DAG_sort_unif_pair(Psort[0], Psort[1])
			/* && Psort[0] != SORT_BOOLEAN*/)
			return SORT_BOOLEAN;
		return DAG_SORT_NULL;
	}
	if (symb == PREDICATE_DISTINCT) {
		unsigned j;
		for (i = 0; i < n; i++)
			for (j = i + 1; j < n; j++)
				if (!DAG_sort_unif_pair(Psort[i], Psort[j])) return DAG_SORT_NULL;
		/* PF TODO: Accept boolean sort in distinct */
		return SORT_BOOLEAN;
	}
	if (symb == FUNCTION_ITE) {
		if (
			n == 3 && Psort[0] == SORT_BOOLEAN && Psort[1] != SORT_BOOLEAN &&
			Psort[2] != SORT_BOOLEAN)
			return DAG_sort_unif_pair(Psort[1], Psort[2]);
		return DAG_SORT_NULL;
	}
	if (symb == CONNECTOR_ITE) {
		if (
			n == 3 && Psort[0] == SORT_BOOLEAN && Psort[1] == SORT_BOOLEAN &&
			Psort[2] == SORT_BOOLEAN)
			return SORT_BOOLEAN;
		return DAG_SORT_NULL;
	}
	if (quantifier(symb)) {
		if (n > 1 && Psort[n - 1] == SORT_BOOLEAN) return SORT_BOOLEAN;
		return DAG_SORT_NULL;
	}
	if (symb == LET) {
		if (n < 3) return DAG_SORT_NULL;
		for (i = 0; i < n - 1; i++, i++)
			if (Psort[i] != Psort[i + 1]) return DAG_SORT_NULL;
		return Psort[n - 1];
	}
	if (symb == LAMBDA) {
		if (n == 1) /* lambda-term without arguments */
			return Psort[0];
		else {
			Tsort* sub;
			MY_MALLOC(sub, n * sizeof(Tsort));
			for (i = 0; i < n; i++) sub[i] = Psort[i];
			return DAG_sort_new(NULL, n, sub);
		}
	}
	if (symb == APPLY_LAMBDA) {
		if (n < 2) return DAG_SORT_NULL;
		sort = Psort[0];
		assert(DAG_sort_arity(sort) != DAG_SORT_NARY);
		if (!sort || DAG_sort_arity(sort) != n)
			my_error("Sort error in lambda application.\n");
		return DAG_sort_unif_apply(
			Psort + 1, DAG_sort_subs(sort), n - 1, DAG_sort_sub(sort, n - 1));
	}
	if (symb == CHOICE_FUNCTION) {
		if (n != 2) return DAG_SORT_NULL;
		/* Sort of function is sort of variable */
		return Psort[0];
	}
	if (arith_function(symb)) {
		Tsort sort;
		if (
			(symb == FUNCTION_SUM && n < 2) || (symb == FUNCTION_PROD && n < 2) ||
			(unary_minus(symb) && n != 1) || (symb == FUNCTION_MINUS && n != 2) ||
			(symb == FUNCTION_DIV && n != 2))
			return DAG_SORT_NULL;
		sort = Psort[0];
		for (i = 1; i < n; i++) sort = DAG_sort_combine(sort, Psort[i]);
		if (sort == SORT_INTEGER || sort == SORT_REAL) return sort;
		return DAG_SORT_NULL;
	}
	if (arith_comparison(symb)) {
		Tsort sort;
		if (n != 2) return DAG_SORT_NULL;
		sort = DAG_sort_combine(Psort[0], Psort[1]);
		if (DAG_sort_combine(sort, SORT_INTEGER)) return SORT_BOOLEAN;
		return DAG_SORT_NULL;
	}
	/* DD when symb is an instance of a polymorphic symbol */
	/* if (symb->origin != NULL) */
	/*   { */
	/*     i = 0; */
	/*     while (i < n && DAG_symb_sort(symb)->sub[i] == Psort[i]) ++i; */
	/*     if (i == n) */
	/*    return DAG_symb_sort(symb)->sub[n]; */
	/*     else */
	/*    return NULL; */
	/*   } */
	/* PF general treatment */
	sort = DAG_symb_sort(symb);
	if (!n) return sort;
	if (DAG_sort_arity(sort) == DAG_SORT_NARY)
		return DAG_sort_unif_apply_polyadic(
			Psort, DAG_sort_sub(sort, 0), n, DAG_sort_sub(sort, 1));
	if (DAG_sort_arity(sort) != n + 1u) /* nary handled just above */
		return DAG_SORT_NULL;
	return DAG_sort_unif_apply(
		Psort, DAG_sort_subs(sort), n, DAG_sort_sub(sort, n));
}

Tsymb
DAG_symb_lookup(char* name, unsigned arity, Tsort* Psort, Tsort sort)
{
	unsigned i;
	Tsymb found_symb = DAG_SYMB_NULL;
	Tsymb_homonym* homonym = homonym_query(name);
	if (!homonym) return DAG_SYMB_NULL;
	for (i = 0; i < homonym->symbs->size; i++) {
		if (sort && !DAG_sort_combine(sort, DAG_symb_sort(homonym->symbs->data[i])))
			continue;
		if (Psort && !DAG_symb_check(homonym->symbs->data[i], arity, Psort))
			continue;
		if (DAG_symb_type(homonym->symbs->data[i]) & SYMB_VARIABLE) continue;
		if (found_symb) return DAG_SYMB_NULL;
		found_symb = homonym->symbs->data[i];
	}
	return found_symb;
}

/*
  --------------------------------------------------------------
  Integer symbols
  --------------------------------------------------------------
*/

static inline bool
symb_int_cmp(Tsymb s1, Tsymb s2)
{
	return !mpz_cmp(
		DAG_symb_stack->data[s1].value.mpz, DAG_symb_stack->data[s2].value.mpz);
}

static inline unsigned
symb_int_hash(Tsymb s)
{
	return DAG_symb_stack->data[s].hash_key;
}

#define TYPE_EXT symb_int
#define TYPE Tsymb
#define TYPE_NULL DAG_SYMB_NULL
#define HA_CMP symb_int_cmp
#define HA_HASH symb_int_hash
#define HA_AUTO_RESIZE

#include "utils/ha.c.tpl"

#undef TYPE_EXT
#undef TYPE
#undef TYPE_NULL
#undef HA_CMP
#undef HA_HASH
#undef HA_AUTO_RESIZE

static Tha_symb_int ha_symb_int;

Tsymb
DAG_symb_integer(long value)
{
	char* str;
	Tsymb symb = stack_size(DAG_symb_stack);
	Tsymb symb2;
	stack_inc_hook(DAG_symb_stack, DAG_symb_hook_resize);
	mpz_init_set_si(DAG_symb_stack->data[symb].value.mpz, value);
	DAG_symb_stack->data[symb].type = SYMB_INTERPRETED | SYMB_PREDEFINED |
		SYMB_NUMBER | SYMB_INTEGER | SYMB_NULLARY;
	DAG_symb_stack->data[symb].sort = SORT_NUMERAL;
	DAG_symb_stack->data[symb].misc = 0;
	str = mpz_get_str(NULL, 10, DAG_symb_stack->data[symb].value.mpz);
	DAG_symb_stack->data[symb].hash_key =
		hash_one_at_a_time_inc_end(hash_one_at_a_time_inc(0, str));
	free(str);
	/* Setting symbol mask */
	set_symb_mask(symb, SORT_NUMERAL);
	/* Setting orderings */
	set_symb_orderings(symb, SORT_NUMERAL);

	symb2 = ha_symb_int_query(ha_symb_int, symb);
	if (!symb2) {
		ha_symb_int = ha_symb_int_enter(ha_symb_int, symb);
		return symb;
	}
	mpz_clear(DAG_symb_stack->data[symb].value.mpz);
	stack_dec(DAG_symb_stack);
	return symb2;
}

Tsymb
DAG_symb_integer_mpz(mpz_t mpz)
{
	char* str;
	Tsymb symb = stack_size(DAG_symb_stack);
	Tsymb symb2;
	stack_inc_hook(DAG_symb_stack, DAG_symb_hook_resize);
	mpz_init_set(DAG_symb_stack->data[symb].value.mpz, mpz);
	DAG_symb_stack->data[symb].type = SYMB_INTERPRETED | SYMB_PREDEFINED |
		SYMB_NUMBER | SYMB_INTEGER | SYMB_NULLARY;
	DAG_symb_stack->data[symb].sort = SORT_NUMERAL;
	DAG_symb_stack->data[symb].misc = 0;
	str = mpz_get_str(NULL, 10, DAG_symb_stack->data[symb].value.mpz);
	DAG_symb_stack->data[symb].hash_key =
		hash_one_at_a_time_inc_end(hash_one_at_a_time_inc(0, str));
	free(str);
	/* Setting symbol mask */
	set_symb_mask(symb, SORT_NUMERAL);
	/* Setting orderings */
	set_symb_orderings(symb, SORT_NUMERAL);

	symb2 = ha_symb_int_query(ha_symb_int, symb);
	if (!symb2) {
		ha_symb_int = ha_symb_int_enter(ha_symb_int, symb);
		return symb;
	}
	mpz_clear(DAG_symb_stack->data[symb].value.mpz);
	stack_dec(DAG_symb_stack);
	return symb2;
}

/**
   \brief Tsymb constructor
   \param value textual representation of an integer [\-]?(0|[1-9][0-9]*)
   \return Creates (if new) and returns Tsymb representing integer value.
   \remark The given string is checked for conformance. */
Tsymb
DAG_symb_integer_str(char* value)
{
	size_t i, n, start;
	char* str;
	Tsymb symb = stack_size(DAG_symb_stack);
	Tsymb symb2;
	n = strlen(value);
	start = (value[0] == '-') ? 1 : 0;
	if (n <= start || (n > 1 && value[start] == '0'))
		my_error("DAG_symb_numeral_str: ill-formed integer\n");
	for (i = start; i < n; i++)
		if (value[i] < '0' || value[i] > '9')
			my_error("DAG_symb_numeral_str: ill-formed integer\n");
	stack_inc_hook(DAG_symb_stack, DAG_symb_hook_resize);
	mpz_init_set_str(DAG_symb_stack->data[symb].value.mpz, value, 10);
	DAG_symb_stack->data[symb].type = SYMB_INTERPRETED | SYMB_PREDEFINED |
		SYMB_NUMBER | SYMB_INTEGER | SYMB_NULLARY;
	DAG_symb_stack->data[symb].sort = SORT_NUMERAL;
	DAG_symb_stack->data[symb].misc = 0;
	str = mpz_get_str(NULL, 10, DAG_symb_stack->data[symb].value.mpz);
	DAG_symb_stack->data[symb].hash_key =
		hash_one_at_a_time_inc_end(hash_one_at_a_time_inc(0, str));
	free(str);
	/* Setting symbol mask */
	set_symb_mask(symb, SORT_NUMERAL);
	/* Setting orderings */
	set_symb_orderings(symb, SORT_NUMERAL);

	symb2 = ha_symb_int_query(ha_symb_int, symb);
	if (!symb2) {
		ha_symb_int = ha_symb_int_enter(ha_symb_int, symb);
		return symb;
	}
	mpz_clear(DAG_symb_stack->data[symb].value.mpz);
	stack_dec(DAG_symb_stack);
	return symb2;
}

/**
   \brief Tsymb constructor
   \param value textual representation of a binary \c #b[01]+ \c
   \return Creates (if new) and returns Tsymb representing binary value.

   The given string is checked for conformance.
*/
Tsymb
DAG_symb_binary_str(char* value)
{
	size_t i, n;
	char* str;
	Tsymb symb = stack_size(DAG_symb_stack);
	Tsymb symb2;
	n = strlen(value);
	if (n < 3 || value[0] != '#' || value[1] != 'b')
		my_error("DAG_symb_binary_str: ill-formed binary\n");
	for (i = 2; i < n; i++)
		if (value[i] != '0' && value[i] != '1')
			my_error("DAG_symb_binary_str: ill-formed binary\n");
	stack_inc_hook(DAG_symb_stack, DAG_symb_hook_resize);
	mpz_init_set_str(DAG_symb_stack->data[symb].value.mpz, value + 2, 2);
	DAG_symb_stack->data[symb].type = SYMB_INTERPRETED | SYMB_PREDEFINED |
		SYMB_NUMBER | SYMB_INTEGER | SYMB_NULLARY;
	DAG_symb_stack->data[symb].sort = SORT_NUMERAL;
	DAG_symb_stack->data[symb].misc = 0;
	str = mpz_get_str(NULL, 10, DAG_symb_stack->data[symb].value.mpz);
	DAG_symb_stack->data[symb].hash_key =
		hash_one_at_a_time_inc_end(hash_one_at_a_time_inc(0, str));
	free(str);
	/* Setting symbol mask */
	set_symb_mask(symb, SORT_NUMERAL);
	/* Setting orderings */
	set_symb_orderings(symb, SORT_NUMERAL);

	symb2 = ha_symb_int_query(ha_symb_int, symb);
	if (!symb2) {
		ha_symb_int = ha_symb_int_enter(ha_symb_int, symb);
		return symb;
	}
	mpz_clear(DAG_symb_stack->data[symb].value.mpz);
	stack_dec(DAG_symb_stack);
	return symb2;
}

/**
   \brief Tsymb constructor
   \param value textual representation of an hexadecimal \c #x[0-9A-Fa-f]+ \c
   \return Tsymb representing hexadecimal value (creating it if necessary);
   the given string is checked for conformance.
*/
Tsymb
DAG_symb_hex_str(char* value)
{
	size_t i, n;
	char* str;
	Tsymb symb = stack_size(DAG_symb_stack);
	Tsymb symb2;
	n = strlen(value);
	if (n < 3 || value[0] != '#' || value[1] != 'h')
		my_error("DAG_symb_hex_str: ill-formed hexadecimal\n");
	for (i = 2; i < n; i++)
		if (!((value[i] >= 'A' && value[i] <= 'F') ||
					(value[i] >= 'a' && value[i] <= 'f') ||
					(value[i] >= '0' && value[i] <= '9')))
			my_error("DAG_symb_hex_str: ill-formed hexadecimal\n");
	stack_inc_hook(DAG_symb_stack, DAG_symb_hook_resize);
	mpz_init_set_str(DAG_symb_stack->data[symb].value.mpz, value + 2, 16);
	DAG_symb_stack->data[symb].type = SYMB_INTERPRETED | SYMB_PREDEFINED |
		SYMB_NUMBER | SYMB_INTEGER | SYMB_NULLARY;
	DAG_symb_stack->data[symb].sort = SORT_NUMERAL;
	DAG_symb_stack->data[symb].misc = 0;
	str = mpz_get_str(NULL, 10, DAG_symb_stack->data[symb].value.mpz);
	DAG_symb_stack->data[symb].hash_key =
		hash_one_at_a_time_inc_end(hash_one_at_a_time_inc(0, str));
	free(str);
	/* Setting symbol mask */
	set_symb_mask(symb, SORT_NUMERAL);
	/* Setting orderings */
	set_symb_orderings(symb, SORT_NUMERAL);

	symb2 = ha_symb_int_query(ha_symb_int, symb);
	if (!symb2) {
		ha_symb_int = ha_symb_int_enter(ha_symb_int, symb);
		return symb;
	}
	mpz_clear(DAG_symb_stack->data[symb].value.mpz);
	stack_dec(DAG_symb_stack);
	return symb2;
}

/*
  --------------------------------------------------------------
  Rational symbols
  --------------------------------------------------------------
*/

static inline bool
symb_rat_cmp(Tsymb s1, Tsymb s2)
{
	return mpq_equal(
		DAG_symb_stack->data[s1].value.mpq, DAG_symb_stack->data[s2].value.mpq);
}

static inline unsigned
symb_rat_hash(Tsymb s)
{
	return DAG_symb_stack->data[s].hash_key;
}

#define TYPE_EXT symb_rat
#define TYPE Tsymb
#define TYPE_NULL DAG_SYMB_NULL
#define HA_CMP symb_rat_cmp
#define HA_HASH symb_rat_hash
#define HA_AUTO_RESIZE

#include "utils/ha.c.tpl"

#undef TYPE_EXT
#undef TYPE
#undef TYPE_NULL
#undef HA_CMP
#undef HA_HASH
#undef HA_AUTO_RESIZE

static Tha_symb_rat ha_symb_rat;

Tsymb
DAG_symb_rational(long num, unsigned long den)
/* PF the user is responsible for not generating overflows */
/* DC fraction representation of rationals */
{
	char* str;
	Tsymb symb = stack_size(DAG_symb_stack);
	Tsymb symb2;
	stack_inc_hook(DAG_symb_stack, DAG_symb_hook_resize);
	mpq_init(DAG_symb_stack->data[symb].value.mpq);
	mpq_set_si(DAG_symb_stack->data[symb].value.mpq, num, den);
	mpq_canonicalize(DAG_symb_stack->data[symb].value.mpq);
	DAG_symb_stack->data[symb].type =
		SYMB_INTERPRETED | SYMB_PREDEFINED | SYMB_NUMBER | SYMB_NULLARY;
	DAG_symb_stack->data[symb].sort = SORT_REAL;
	DAG_symb_stack->data[symb].misc = 0;
	str = mpq_get_str(NULL, 10, DAG_symb_stack->data[symb].value.mpq);
	DAG_symb_stack->data[symb].hash_key =
		hash_one_at_a_time_inc_end(hash_one_at_a_time_inc(0, str));
	free(str);
	/* Setting symbol mask */
	set_symb_mask(symb, SORT_REAL);
	/* Setting orderings */
	set_symb_orderings(symb, SORT_REAL);

	symb2 = ha_symb_rat_query(ha_symb_rat, symb);
	if (!symb2) {
		ha_symb_rat = ha_symb_rat_enter(ha_symb_rat, symb);
		return symb;
	}
	mpq_clear(DAG_symb_stack->data[symb].value.mpq);
	stack_dec(DAG_symb_stack);
	return symb2;
}

Tsymb
DAG_symb_rational_mpq(mpq_t mpq)
/* PF the user is responsible for not generating overflows */
/* DC fraction representation of rationals */
{
	char* str;
	Tsymb symb = stack_size(DAG_symb_stack);
	Tsymb symb2;
	stack_inc_hook(DAG_symb_stack, DAG_symb_hook_resize);
	mpq_init(DAG_symb_stack->data[symb].value.mpq);
	mpq_set(DAG_symb_stack->data[symb].value.mpq, mpq);
	DAG_symb_stack->data[symb].type =
		SYMB_INTERPRETED | SYMB_PREDEFINED | SYMB_NUMBER | SYMB_NULLARY;
	DAG_symb_stack->data[symb].sort = SORT_REAL;
	DAG_symb_stack->data[symb].misc = 0;
	str = mpq_get_str(NULL, 10, DAG_symb_stack->data[symb].value.mpq);
	DAG_symb_stack->data[symb].hash_key =
		hash_one_at_a_time_inc_end(hash_one_at_a_time_inc(0, str));
	free(str);
	/* Setting symbol mask */
	set_symb_mask(symb, SORT_REAL);
	/* Setting orderings */
	set_symb_orderings(symb, SORT_REAL);

	symb2 = ha_symb_rat_query(ha_symb_rat, symb);
	if (!symb2) {
		ha_symb_rat = ha_symb_rat_enter(ha_symb_rat, symb);
		return symb;
	}
	mpq_clear(DAG_symb_stack->data[symb].value.mpq);
	stack_dec(DAG_symb_stack);
	return symb2;
}

/**
   \brief Tsymb constructor
   \param value textual representation of a rational [\-]?[1-9][0-9]* / [0-9]+[1-9] or [\-]?[1-9][0-9]*
   \return Creates (if new) and returns Tsymb representing rational value.
   \remark given string checked for conformance */
Tsymb
DAG_symb_rational_str(char* value)
{
	size_t i, d, n, start;
	char* str;
	Tsymb symb = stack_size(DAG_symb_stack);
	Tsymb symb2;
	n = strlen(value);
	d = 0;
	for (i = 0; i < n; i++)
		if (value[i] == '/') {
			d = i;
			break;
		}
	start = (value[0] == '-') ? 1 : 0;
	if (d == start || d == n - 1) /*|| (d > 1 && value[start] == '0') 00 is OK */
		my_error("DAG_symb_rational_str: ill-formed rational %s\n", value);
	for (i = start; i < d; i++)
		if (value[i] < '0' || value[i] > '9')
			my_error("DAG_symb_rational_str: ill-formed rational %s\n", value);
	for (i = d + 1; i < n; i++)
		if (value[i] < '0' || value[i] > '9')
			my_error("DAG_symb_rational_str: ill-formed rational %s\n", value);

	stack_inc_hook(DAG_symb_stack, DAG_symb_hook_resize);
	mpq_init(DAG_symb_stack->data[symb].value.mpq);
	mpq_set_str(DAG_symb_stack->data[symb].value.mpq, value, 10);
	mpq_canonicalize(DAG_symb_stack->data[symb].value.mpq);
	DAG_symb_stack->data[symb].type =
		SYMB_INTERPRETED | SYMB_PREDEFINED | SYMB_NUMBER | SYMB_NULLARY;
	DAG_symb_stack->data[symb].sort = SORT_REAL;
	DAG_symb_stack->data[symb].misc = 0;
	str = mpq_get_str(NULL, 10, DAG_symb_stack->data[symb].value.mpq);
	DAG_symb_stack->data[symb].hash_key =
		hash_one_at_a_time_inc_end(hash_one_at_a_time_inc(0, str));
	free(str);
	/* Setting symbol mask */
	set_symb_mask(symb, SORT_REAL);
	/* Setting orderings */
	set_symb_orderings(symb, SORT_REAL);

	symb2 = ha_symb_rat_query(ha_symb_rat, symb);
	if (!symb2) {
		ha_symb_rat = ha_symb_rat_enter(ha_symb_rat, symb);
		return symb;
	}
	mpq_clear(DAG_symb_stack->data[symb].value.mpq);
	stack_dec(DAG_symb_stack);
	return symb2;
}

/**
   \brief Tsymb constructor
   \param value textual representation of a decimal [\-]?(0|[1-9][0-9]*)\.[0-9]+
   \return Creates (if new) and returns Tsymb representing decimal value.
   The given string is checked for conformance. */
Tsymb
DAG_symb_decimal_str(char* value)
{
	char* str;
	size_t i, d, n, start;
	Tsymb symb;
	n = strlen(value);
	d = 0;
	for (i = 0; i < n; i++)
		if (value[i] == '.') {
			d = i;
			break;
		}
	start = (value[0] == '-') ? 1 : 0;
	if (d == start || d == n - 1 || (d > 1 && value[start] == '0'))
		my_error("DAG_symb_decimal_str: ill-formed decimal\n");

	for (i = start; i < d; i++)
		if (value[i] < '0' || value[i] > '9')
			my_error("DAG_symb_decimal_str: ill-formed decimal\n");
	for (i = d + 1; i < n; i++)
		if (value[i] < '0' || value[i] > '9')
			my_error("DAG_symb_decimal_str: ill-formed decimal\n");
	MY_MALLOC(
		str,
		(n + 1 /* NULL term */ + 1 /*/*/ + 1 /*1*/ + (n - d - 1) /*0*/) *
			sizeof(char));
	strcpy(str, value);
	for (i = d; i < n - 1; i++) str[i] = str[i + 1];
	str[i++] = '/'; /* n - 1 */
	str[i++] = '1'; /* n */
	for (i = n + 1; i < n + 1 + (n - d - 1); i++) str[i] = '0';
	str[i] = 0;
	symb = DAG_symb_rational_str(str);
	free(str);
	return symb;
}

/**
   \brief Tsymb constructor
   \param value string
   \return Creates (if new) and returns Tsymb representing string */
Tsymb
DAG_symb_str(char* value)
{
	my_error("DAG_symb_str: undefined %s\n", value);
	return DAG_SYMB_NULL;
}

Tsymb
DAG_symb_predicate(Tsort sort)
{
	nonce_next(&nonce_pred);
	return DAG_symb_new(nonce_pred.buffer, SYMB_PREDICATE, sort);
}

void
DAG_symb_set_hook_resize(TDAG_symb_hook_resize hook_resize)
{
	hook_resize(0, DAG_table->alloc);
	stack_push(stack_hook_resize, hook_resize);
}

void
DAG_symb_set_hook_free(TDAG_symb_hook_free hook_free)
{
	stack_push(stack_hook_free, hook_free);
}

/*
  --------------------------------------------------------------
  Initialisation and release
  --------------------------------------------------------------
*/

void
DAG_symb_init(void)
{
	stack_INIT(DAG_symb_stack);
	/* Reserve id 0 */
	stack_inc(DAG_symb_stack);
	/* Ensure the hash key of DAG_SYMB_NULL is will defined such that
	   it can be used as  a placeholder for variables in discrimination
	   trees */
	DAG_symb_stack->data[0].hash_key = 0;
	symb_homonym = ha_symb_homonym_new(64);
	ha_symb_int = ha_symb_int_new(64);
	ha_symb_rat = ha_symb_rat_new(64);
	stack_INIT(stack_hook_resize);
	stack_INIT(stack_hook_free);
	nonce_init(&nonce_const, "@ct");
	nonce_init(&nonce_func, "@fn");
	nonce_init(&nonce_pred, "@pr");
	nonce_init(&nonce_skolem, "@sk");
	nonce_init(&nonce_var, "@vr");

	/* For symbol mask */
	MY_MALLOC(symb_mask, MAX_SIZE);
	symb_mask[0] = 0;
	/* For orderings */
	MY_MALLOC(symb_precedence, sizeof(unsigned));
	MY_MALLOC(symb_weight, sizeof(unsigned));
	symb_precedence[0] = UINT_MAX;
	symb_weight[0] = 0;
}

void
DAG_symb_done(void)
{
	Tsymb symb;
	/*
    for (symb = 1; symb < stack_size(DAG_symb_stack); ++symb)
    if (SYMB_DAG(symb))
    DAG_free(SYMB_DAG(symb));
  */
	nonce_free(&nonce_const);
	nonce_free(&nonce_func);
	nonce_free(&nonce_pred);
	nonce_free(&nonce_skolem);
	nonce_free(&nonce_var);
	for (symb = 1; symb < stack_size(DAG_symb_stack); ++symb) DAG_symb_free(symb);
	DAG_symb_hook_resize(DAG_symb_stack->alloc, 0);
	stack_free(DAG_symb_stack);
	ha_symb_homonym_free(&symb_homonym);
	ha_symb_int_free(&ha_symb_int);
	ha_symb_rat_free(&ha_symb_rat);
	stack_free(stack_hook_resize);
	stack_free(stack_hook_free);

	/* For symbol mask */
	free(symb_mask);
	/* For orderings */
	free(symb_precedence);
	free(symb_weight);
}

/*
  --------------------------------------------------------------
  Debugging and printing
  --------------------------------------------------------------
*/

void
DAG_symb_snprint(Tsymb symb, unsigned n, char* str)
{
	int sign;
	const mp_limb_t* limbs;
	size_t limbs_size;
	mpq_t* rat;

	if (DAG_symb_type(symb) & SYMB_NAMED) {
		if (strlen(DAG_symb_name(symb)) > n)
			my_error("DAG_symb_snprint: symbol name too large to be printed\n");
		else
			strncpy(str, DAG_symb_name(symb), n);
		return;
	}

	if (DAG_symb_type(symb) & SYMB_INTEGER)
		sign = mpz_sgn(DAG_symb_stack->data[symb].value.mpz);
	else if (DAG_symb_type(symb) & SYMB_NUMBER)
		sign = mpq_sgn(DAG_symb_stack->data[symb].value.mpq);

	if (DAG_symb_sort(symb) == SORT_REAL) {
		if (DAG_symb_type(symb) & SYMB_INTEGER) {
			limbs = mpz_limbs_read(DAG_symb_stack->data[symb].value.mpz);
			limbs_size = mpz_size(DAG_symb_stack->data[symb].value.mpz);
			if (
				gmp_snprintf(
					str, n, sign < 0 ? "(- %Nd.0)" : "%Nd.0", limbs, limbs_size) >= n)
				my_error("DAG_symb_snprint: symbol name too large to be printed\n");
			return;
		}
		if (DAG_symb_type(symb) & SYMB_NUMBER) {
			rat = &DAG_symb_stack->data[symb].value.mpq;
			limbs = mpz_limbs_read(mpq_numref(*rat));
			limbs_size = mpz_size(mpq_numref(*rat));
			/* mpz_cmp_ui returns 0 if the two values are equal */
			if (mpz_cmp_ui(mpq_denref(*rat), 1) == 0) {
				if (
					gmp_snprintf(
						str, n, sign < 0 ? "(- %Nd.0)" : "%Nd.0", limbs, limbs_size) >= n)
					my_error("DAG_symb_snprint: symbol name too large to be printed\n");
			} else {
				if (
					gmp_snprintf(
						str, n, sign < 0 ? "(- (/ %Nd.0 %Zd.0))" : "(/ %Nd.0 %Zd.0)", limbs,
						limbs_size, mpq_denref(*rat)) >= n)
					my_error("DAG_symb_snprint: symbol name too large to be printed\n");
			}
			return;
		}
	} else {
		assert(DAG_symb_sort(symb) == SORT_INTEGER);
		if (DAG_symb_type(symb) & SYMB_INTEGER) {
			limbs = mpz_limbs_read(DAG_symb_stack->data[symb].value.mpz);
			limbs_size = mpz_size(DAG_symb_stack->data[symb].value.mpz);
			if (
				gmp_snprintf(str, n, sign < 0 ? "(- %Nd)" : "%Nd", limbs, limbs_size) >=
				n)
				my_error("DAG_symb_snprint: symbol name too large to be printed\n");
			return;
		}
		if (DAG_symb_type(symb) & SYMB_NUMBER) {
			assert(
				mpz_cmp_ui(mpq_denref(DAG_symb_stack->data[symb].value.mpq), 1) == 0);
			rat = &DAG_symb_stack->data[symb].value.mpq;
			limbs = mpz_limbs_read(mpq_numref(*rat));
			limbs_size = mpz_size(mpq_numref(*rat));
			if (
				gmp_snprintf(str, n, sign < 0 ? "(- %Nd)" : "%Nd", limbs, limbs_size) >=
				n)
				my_error("DAG_symb_snprint: symbol name too large to be printed\n");
			return;
		}
	}
}

#ifndef PEDANTIC
#ifdef DEBUG
void
DAG_symb_table_print(void)
{
	unsigned i;
	fprintf(stderr, "%5s %15s %5s\n", "index", "name", "sort");
	for (i = 0; i < stack_size(DAG_symb_stack); ++i)
		fprintf(stderr, "%5i %15s %5i\n", i, DAG_symb_name(i), DAG_symb_sort(i));
}
#endif
#endif
