/* -*- mode: C -*- */

/**
   \file LA-hw.c.tpl

   \brief Implementation of the linear arithmetics module API

*/

#include "arith/LA-hw.h"
#include "arith/eq-store.h"
#include "arith/numbers-hw.h"
#include "arith/simplex-hw.h"
#include "arith/totality.h"
#include "bool/literal.h"
#include "congruence/congruence.h"
#include "hint.h"
#include "symbolic/DAG-arith.h"
#include "symbolic/DAG.h"
#include "symbolic/veriT-status.h"
#include "undo.h"
#include "utils/hk.h"
#include "utils/stack.h"
#include "utils/statistics.h"
#include "utils/veriT-qsort.h"
#include "veriT-state.h"

#include <stdbool.h>

#define HW_VERSION

#if DEBUG_LA
#include "symbolic/DAG-print.h"
#endif

#ifdef DEBUG_LA_PROPAGATE
#include "symbolic/DAG-print.h"
#endif

extern bool LA_enable_theory_propagation;

#define MAX_BRANCH_DEPTH ((unsigned)1000)

/*
  --------------------------------------------------------------
  Backtracking: Declarations
  --------------------------------------------------------------
*/

enum
{
	LA_HINT = UNDO_LA,
	LA_ASSERT
};

static void backtrack_hint(Tlit lit);
static void LA_hook_hint(Tlit* Plit);
#if DEBUG_LA
#ifdef HW_VERSION
static void
LA_hook_assert(void* ignored)
{
	my_DAG_message("LA_assert backtracked\n");
}
#endif
#endif
static void backtrack_init(void);
static void backtrack_done(void);

/*
  --------------------------------------------------------------
  Data structures for proofs
  --------------------------------------------------------------
*/

/* Stores the coefficient of disequalities are multiplied with
   before they are sent to simplex */
static TLArational_hw* prior_coefficient_var = 0;
static unsigned prior_var_alloc = 0;

/*
  --------------------------------------------------------------
  Lemma Handling
  --------------------------------------------------------------
*/

static unsigned LA_lemma_n = 0; /*< counts lemmas generated */

/** \brief factors out instructions executed on generated lemmas */
static inline void
record_lemma(TDAG lemma)
{
	stack_push(veriT_lemmas, DAG_dup(lemma));
	proof_set_lemma_id(lemma, proof_add_la_tautology(DAG_dup(lemma)));
	LA_lemma_n++;
}

/*
  --------------------------------------------------------------
  Theory Propagation: Declarations
  --------------------------------------------------------------
*/

/* Theory propagation sends hints to the SAT solver that takes them into account
   to take decisions.  A hint is a non-asserted literal that is a consequence of
   the asserted literals (asserted means received through a LA_hw_assert call).
   When a literal is received through a LA_hw_assert call, and it has been
   previously sent as a hint, it needs not be processed.  The array LA_hinted is
   responsible for storing all such hints. It is accessed through LA_hint
   (setting entries) and LA_hook_hint (unsetting entries when backtracking).
   When a literal has been sent has a hint, and is then received as an
   assertion, nothing needs. */

/** \brief Classification of bounds.

    \remark Bounds are classified for efficient computation of
    hints. */

#ifdef HW_VERSION /* Shared between versions */
Tvar* LA_bound_ranking;
size_t LA_bound_ranking_n; /*< size of LA_bound_ranking */
#else
extern Tvar* LA_bound_ranking;
extern size_t LA_bound_ranking_n;
#endif

static void LA_bound_rank(void);

static void LA_do_hint(const Tlit lit);
static void LA_hint(const Tlit, const Tlit);
static bool LA_is_hint(const Tlit lit);
static void LA_hook_hint(Tlit*);

/*
  --------------------------------------------------------------
  Store
  --------------------------------------------------------------
*/

/** \struct store

    \brief the temporary GMP data

    \details this structure factors the memory allocation and disallocation for
    storing the parameters and results of the calls to libgmp. Each element of
    the structure is named according to a sub-routine, and is itself a structure
    with one field for each variable used. In the case of recursive sub-routines
    this field is an array.
*/
static struct store
{
	struct
	{
		TLAdelta_hw delta;
	} assert_eq;
	struct
	{
		size_t size;
		TLAdelta_hw* xi;
	} solve_aux;
} store;

/**
   \brief initializes the store
*/
static inline void
store_init(void)
{
	unsigned d;
	LAdelta_hw_init(&store.assert_eq.delta);
	store.solve_aux.size = MAX_BRANCH_DEPTH + 1;
	MY_MALLOC(store.solve_aux.xi, store.solve_aux.size * sizeof(TLAdelta_hw));
	for (d = 0; d < store.solve_aux.size; ++d)
		LAdelta_hw_init(&store.solve_aux.xi[d]);
}

/**
   \brief frees resources used in the store
*/
static inline void
store_done(void)
{
	unsigned d;
	LAdelta_hw_clear(&store.assert_eq.delta);
	for (d = 0; d < store.solve_aux.size; ++d)
		LAdelta_hw_clear(&store.solve_aux.xi[d]);
	free(store.solve_aux.xi);
}

/**
   \brief returns address of TLAdelta_hw value corresponding
   to recursive call at given depth
*/
static inline void
store_solve_aux_fit(unsigned depth)
{
	while (depth >= store.solve_aux.size) {
		size_t d = store.solve_aux.size;
		store.solve_aux.size *= 2;
		MY_REALLOC(store.solve_aux.xi, store.solve_aux.size * sizeof(TLAdelta_hw));
		while (d < store.solve_aux.size) {
			LAdelta_hw_init(&store.solve_aux.xi[d]);
			++d;
		}
	}
}

/*
  --------------------------------------------------------------
  linking DAGs and LA variables
  --------------------------------------------------------------
*/

/**
   \var DAG_var_table
   \brief Maps DAGs to simplex variables.
   \details Records all simplex variables that have been created in
   a table indexed by the corresponding DAG.
   \see LA_DAG_var, LA_DAG_is_var */
static Tstack_simplex_var DAG_var_table;

/**
   \typedef TLA_DAG_info
   \brief flags for linear processing of DAGs. */
typedef struct
{
#ifdef PEDANTIC
	bool shared;
	bool visited;
#else
	bool shared : 1; /*!< DAG is shared with other reasoning engines */
	bool visited : 1; /*!< DAG has been processed by notification routines */
#endif
} TLA_DAG_info;

/**
   \var TLA_DAG_info
   \brief Maps DAGs to flags. */
TSstack(_DAG_info, TLA_DAG_info);

/**
   \var DAG_info
   \brief Records if DAG is in DAG_var_share
   \see LA_DAG_var, LA_DAG_is_var */
static Tstack_DAG_info DAG_info;

/**
   \var DAG_var_share
   \brief Shared DAGs
   \details Records all DAGs that are shared with other decision
   procedures and for which equalities may need to be generated.
   \note For linear complexity when processing shared DAGs. */
static Tstack_DAG DAG_var_share;

/**
   \brief Returns a simplex variable representing the given DAG
   \param DAG a DAG
   \see DAG_var_table
   \note A new variable is created if one did not exist before */
static inline Tsimplex_var
LA_DAG_var(TDAG DAG)
{
	assert(DAG);
	if (!DAG_var_table->data[DAG])
		DAG_var_table->data[DAG] = simplex_hw_var_new(
			DAG_sort(DAG) == SORT_INTEGER ||
			(DAG_sort(DAG) == SORT_BOOLEAN && DAG_arity(DAG) == 2 &&
			 DAG_sort(DAG_arg0(DAG)) == SORT_INTEGER &&
			 DAG_sort(DAG_arg1(DAG)) == SORT_INTEGER));
#if DEBUG_LA >= 2
	my_DAG_message("DAG %D associated to v_%d\n", DAG, DAG_var_table->data[DAG]);
#endif
	return DAG_var_table->data[DAG];
}

#define LA_DAG_is_var(DAG) (DAG_var_table->data[DAG])

#if DEBUG_LA >= 2
static void
DAG_var_share_print(void)
{
	unsigned i;
	printf("%u shared variable(s):\n", stack_size(DAG_var_share));
	for (i = 0; i < stack_size(DAG_var_share); ++i)
		my_DAG_message(
			"%D (simplex var v_%u)\n", stack_get(DAG_var_share, i),
			DAG_var_table->data[stack_get(DAG_var_share, i)]);
}
#endif

/**
   \brief Set DAG as shared with other decision procedures
   \param DAG a DAG */
static inline void
LA_DAG_var_share(TDAG DAG)
{
	assert(DAG && DAG_var_table->data[DAG]);
	if (DAG_info->data[DAG].shared) return;
	stack_push(DAG_var_share, DAG);
	DAG_info->data[DAG].shared = true;
	CC_DAG_arith(DAG);
}

/**
   \brief check if DAG is an arithmetic constant or not
   \param DAG a DAG
   \return true if DAG is an arithmetic constant */
static inline bool
LA_DAG_is_number(TDAG DAG)
{
	return (DAG_symb_type(DAG_symb(DAG)) & SYMB_NUMBER) ||
		(DAG_symb(DAG) == FUNCTION_UNARY_MINUS &&
		 LA_DAG_is_number(DAG_arg0(DAG))) ||
		(DAG_symb(DAG) == FUNCTION_DIV && DAG_arity(DAG) == 2 &&
		 LA_DAG_is_number(DAG_arg0(DAG)) && LA_DAG_is_number(DAG_arg1(DAG)));
}

/**
   \brief Conversion from TDAG to a TLArational_hw
   \param[in] DAG the DAG representing a number
   \param[out] rat address where the TLArational_hw value is stored
   rat is normalized
   \pre LA_DAG_is_number(DAG) is true
   \note this function is recursive as a term such as
   "(/ (- (- 1)) (/ 2 3))" passes the test LA_DAG_is_number. */
static void
LA_hw_DAG_number(TDAG DAG, TLArational_hw rat)
{
	if (DAG_symb_type(DAG_symb(DAG)) & SYMB_INTEGER)
		LArational_hw_mpz(rat, DAG_symb_mpz(DAG_symb(DAG)));
	else if (DAG_symb_type(DAG_symb(DAG)) & SYMB_NUMBER)
		LArational_hw_mpq(rat, DAG_symb_mpq(DAG_symb(DAG)));
	else if (
		DAG_symb(DAG) == FUNCTION_UNARY_MINUS && LA_DAG_is_number(DAG_arg0(DAG))) {
		LA_hw_DAG_number(DAG_arg0(DAG), rat);
		LArational_hw_neg(rat);
	} else if (
		DAG_symb(DAG) == FUNCTION_DIV && DAG_arity(DAG) == 2 &&
		LA_DAG_is_number(DAG_arg0(DAG)) && LA_DAG_is_number(DAG_arg1(DAG))) {
		TLArational_hw rat2;
		LArational_hw_init(rat2);
		LA_hw_DAG_number(DAG_arg0(DAG), rat);
		LA_hw_DAG_number(DAG_arg1(DAG), rat2);
		LArational_hw_div(rat, rat2);
		LArational_hw_clear(rat2);
	}
}

/*
  --------------------------------------------------------------
  Support for conflicts
  --------------------------------------------------------------
*/

#ifdef HW_VERSION /* Shared between versions */
Tstack_lit conflict_lits; /*!< conflict literals */
Tstack_simplex_var conflict_eqs; /*!< conflict equality variables */
#else
extern Tstack_lit conflict_lits;
extern Tstack_simplex_var conflict_eqs;
#endif

/*
  --------------------------------------------------------------
  Support for conflicts caused by integrality constraints
  --------------------------------------------------------------
*/

static bool LA_hw_intg_conflict; /*!< conflict involving integer variable */

/*
  --------------------------------------------------------------
  Creating a linear expression
  --------------------------------------------------------------
*/

/**
   \typedef Tmonom
   \brief data structure to represent monoms */
typedef struct Tmonom
{
	TLArational_hw coef; /*!< the coefficient is a rational number */
	Tsimplex_var var; /*!< integer identifying the variable in the simplex */
} Tmonom;

/**
   \typedef Tstack_monom
   \brief a single memory area stores all the monom structures */
TSstack(_monom, Tmonom);
/**
   \var monoms
   \brief a single memory area stores all the monom structures */
static Tstack_monom monoms;

/**
   \brief comparison function for type Tmonom
   \pre assumes that arguments are the same or are different variables
*/
static int
cmp_monom(const Tmonom* Pmonom1, const Tmonom* Pmonom2)
{
	return ((int)Pmonom1->var) - ((int)Pmonom2->var);
}

/*
  --------------------------------------------------------------
  Recording bounds
  --------------------------------------------------------------
*/

/**
   \typedef Tbound
   \brief data structure to represent variable bounds */
typedef struct Tbound
{
	TLAdelta_hw delta; /*!< the bound */
	TLAdelta_hw delta2; /*!< the bound, when atom is negated */
	Tsimplex_var var; /*!< the bounded variable */
	unsigned rank; /*!< position in ranking for theory propagation */
#ifndef PEDANTIC
	bool upper : 1; /*!< indicates if the bound is upper (or lower) */
	unsigned hint : 2;
#else
	bool upper; /*!< indicates if the bound is upper (or lower) */
	unsigned hint;
#endif
} Tbound;

/**
   \typedef Tstack_bound
   \brief a single memory area stores all the bounds */
TSstack(_bound, Tbound);

/**
   \var bounds
   \brief a single memory area stores all the bounds */
static Tstack_bound bounds;

#if DEBUG_LA >= 2
static void
bound_print(Tbound bound)
{
	if (bound.var == 0) return;
	printf("v_%u", bound.var);
	printf(" %c= ", bound.upper ? '<' : '>');
	LAdelta_hw_print(&bound.delta);
}

static void
bounds_print(void)
{
	unsigned i;
	printf("bounds:\n");
	for (i = 0; i < stack_size(bounds); ++i) {
		bound_print(bounds->data[i]);
		printf("\n");
	}
	printf("integer variables:\n");
	for (i = simplex_integer_var_begin(); i != simplex_integer_var_end(); ++i) {
		printf("\tv_%u\n", simplex_integer_var_get(i));
	}
}
#else
#ifdef DEBUG_LA_PROPAGATE
static void
bound_print(Tbound bound)
{
	if (bound.var == 0) return;
	printf("v_%u", bound.var);
	printf(" %c= ", bound.upper ? '<' : '>');
	LAdelta_hw_print(&bound.delta);
}
#endif
#endif

/*
  --------------------------------------------------------------
  Maps linear expressions to slack variables
  --------------------------------------------------------------
*/

/**
   \typedef Tvar_assoc
   \brief a data structure to represent association between slack variables
   and linear expressions without indeterminate */
typedef struct TSvar_assoc
{
	Tsimplex_var var; /*!< the slack variable */
	unsigned n; /*!< number of monoms in the expression */
	TLAsigned_hw* coefs; /*!< coefficients of the monoms */
	Tsimplex_var* vars; /*!< variables of the monoms */
} * Tvar_assoc;

/**
   \brief hash key for type Tvar_assoc
*/
static inline unsigned
var_assoc_key(Tvar_assoc var_assoc)
{
	unsigned i, k = 0;
	for (i = 0; i < var_assoc->n; i++) {
		k = h_unsigned_inc(k, var_assoc->vars[i]);
		k = h_unsigned_inc(k, LAsigned_hw_key(var_assoc->coefs[i]));
	}
	return h_inc_end(k);
}

static Tvar_assoc
var_assoc_new(const unsigned n)
{
	Tvar_assoc result;
	MY_MALLOC(result, sizeof(struct TSvar_assoc));
	result->n = n;
	MY_MALLOC(result->coefs, (n + 1) * sizeof(TLAsigned_hw));
	MY_MALLOC(result->vars, (n + 1) * sizeof(Tsimplex_var));
	return result;
}

static inline void
var_assoc_free(Tvar_assoc var_assoc)
{
	unsigned i;
	for (i = 0; i < var_assoc->n; i++) LAsigned_hw_clear(var_assoc->coefs[i]);
	free(var_assoc->vars);
	free(var_assoc->coefs);
	free(var_assoc);
}

static inline bool
var_assoc_eq(Tvar_assoc var_assoc1, Tvar_assoc var_assoc2)
{
	unsigned i;
	if (var_assoc1->n != var_assoc2->n) return false;
	for (i = 0; i < var_assoc1->n; i++) {
		if (var_assoc1->vars[i] != var_assoc2->vars[i]) return false;
		if (LAsigned_hw_neq(var_assoc1->coefs[i], var_assoc2->coefs[i]))
			return false;
	}
	return true;
}

/* set up definitions to create hash table for var_assoc's */
#define TYPE Tvar_assoc
#define TYPE_EXT var_assoc
#define TYPE_KEY var_assoc_key
#define TYPE_EQ var_assoc_eq
#define TYPE_NULL NULL

#include "utils/h.h"

/* unset definitions */
#undef TYPE
#undef TYPE_EXT
#undef TYPE_KEY
#undef TYPE_EQ
#undef TYPE_NULL

static Th_var_assoc h_var_assoc = NULL;

/**
   \brief adds equation to the Simplex module
   \param DAG the DAG for an inequality
   \pre the terms of the inequality have been processed by notify_term
   \details if necessary creates a slack variable and adds equation relating
   this variable to the linear expression corresponding to the (subtraction
   of) the two terms of the inequality
   \note if the inequality has no variable term, it is either trivially
   valid or unsatisfiable, and a lemma is created.
   \note no slack variable is created if there is a single variable in the
   linear expression, or if the linear expression is colinear with another
   linear expression already associated with a slack variable. */
static void
LA_constraint_push2(TDAG DAG)
{
	unsigned i, j;
	Tvar_assoc var_assoc, var_assoc2;
	TLAsigned_hw tmp, tmp2;
	TLArational_hw indep_term;
	Tsimplex_var var;
	Tvar atom = DAG_to_var(DAG);
	bool upper =
		DAG_symb(DAG) == PREDICATE_LESS || DAG_symb(DAG) == PREDICATE_LEQ;
	bool strict =
		DAG_symb(DAG) == PREDICATE_GREATER || DAG_symb(DAG) == PREDICATE_LESS;
	if (bounds->data[atom].var) return;
	/* sort and remove monoms on same variable */
	veriT_qsort(
		monoms->data, stack_size(monoms), sizeof(Tmonom), (TFcmp)cmp_monom);
	for (i = 0, j = 1; j < stack_size(monoms); j++) {
		if (monoms->data[j].var == monoms->data[i].var)
			LArational_hw_add(monoms->data[i].coef, monoms->data[j].coef);
		else {
			i++;
			if (i != j) {
				LArational_hw_set(monoms->data[i].coef, monoms->data[j].coef);
				monoms->data[i].var = monoms->data[j].var;
			}
		}
	}
	for (j = i + 1; j < stack_size(monoms); j++)
		LArational_hw_clear(monoms->data[j].coef);
	stack_resize(monoms, i + 1);

	/* remove monoms with null coefficient */
	for (i = 0, j = 0; j < stack_size(monoms); j++)
		if (!LArational_hw_is_zero(monoms->data[j].coef)) {
			if (i != j) {
				LArational_hw_set(monoms->data[i].coef, monoms->data[j].coef);
				monoms->data[i].var = monoms->data[j].var;
			}
			i++;
		}
	for (j = i; j < stack_size(monoms); j++)
		LArational_hw_clear(monoms->data[j].coef);
	stack_resize(monoms, i);

	/* remove monom corresponding to constant, store value in indep_term */
	LArational_hw_init(indep_term);
	if (stack_size(monoms) && monoms->data[0].var == 0) {
		LArational_hw_set(indep_term, monoms->data[0].coef);
		for (i = 0; i < stack_size(monoms) - 1; i++) {
			LArational_hw_set(monoms->data[i].coef, monoms->data[i + 1].coef);
			monoms->data[i].var = monoms->data[i + 1].var;
		}
		LArational_hw_clear(monoms->data[i].coef);
		stack_resize(monoms, stack_size(monoms) - 1);
	} else
		LArational_hw_set_zero(indep_term);

	/* Handle variable-free constraint: either valid or inconsistent */
	if (stack_size(monoms) == 0) {
		bool valid;
		/* PF This is not efficiency-critical code,
         so no custom rational operation will be introduced just for this
         even if following tests are a bit dirty */
		if (LArational_hw_is_neg(indep_term))
			valid = upper;
		else if (LArational_hw_is_zero(indep_term))
			valid = !strict;
		else
			valid = !upper;
		LArational_hw_clear(indep_term);
		for (i = 0; i < stack_size(monoms); i++)
			LArational_hw_clear(stack_get(monoms, i).coef);
		stack_reset(monoms);
		record_lemma(valid ? DAG : DAG_not(DAG));
		return;
	}

	TLArational_hw prior_coeff;

	/* Handle constraint on a single variable */
	if (stack_size(monoms) == 1) {
		/* do not introduce another variable */
		LArational_hw_div(indep_term, monoms->data[0].coef);
		if (LArational_hw_is_neg(monoms->data[0].coef)) upper = !upper;
		var = monoms->data[0].var;
		if (proof_on) {
			LArational_hw_init(prior_coeff);
			LArational_hw_set_one(prior_coeff);
			LArational_hw_div(prior_coeff, monoms->data[0].coef);
		}
		goto allocate;
	}

	/* computes the LCM of the coefficient denominators */
	LAsigned_hw_init(tmp);
	LAsigned_hw_set_si(tmp, 1L);
	for (i = 0; i < stack_size(monoms); i++)
		LArational_hw_lcm(tmp, stack_get(monoms, i).coef);
	assert(!LAsigned_hw_is_zero(tmp));
	var_assoc = var_assoc_new(stack_size(monoms));
	for (i = 0; i < var_assoc->n; i++) {
		LAsigned_hw_init(var_assoc->coefs[i]);
		LAsigned_hw_set(var_assoc->coefs[i], tmp);
		LArational_hw_mult_to_signed(
			var_assoc->coefs[i], stack_get(monoms, i).coef);
		var_assoc->vars[i] = stack_get(monoms, i).var;
	}
	LArational_hw_mult_s(indep_term, tmp);

	if (proof_on) {
		LArational_hw_init(prior_coeff);
		LArational_hw_set_si(prior_coeff, tmp);
	}

	LAsigned_hw_clear(tmp);

	/* Now all coefficients are integers, divide by gcd */
	assert(var_assoc->n);
	LAsigned_hw_init(tmp);
	LAsigned_hw_init(tmp2);
	LAsigned_hw_abs(tmp, var_assoc->coefs[0]);
	/* compute gcd */
	for (i = 1; LAsigned_hw_notone(tmp) && i < var_assoc->n; i++) {
		LAsigned_hw_abs(tmp2, var_assoc->coefs[i]);
		LAsigned_hw_gcd(tmp, tmp2);
	}
	if (LAsigned_hw_is_neg(var_assoc->coefs[0])) {
		LAsigned_hw_neg(tmp, tmp);
		upper = !upper;
	}
	/* divide by gcd */
	if (LAsigned_hw_notone(tmp))
		for (i = 0; i < var_assoc->n; i++)
			LAsigned_hw_divex(var_assoc->coefs[i], tmp);
	LArational_hw_div_s(indep_term, tmp);

	if (proof_on) {
		LArational_hw_div_s(prior_coeff, tmp);
	}

	LAsigned_hw_clear(tmp);
	LAsigned_hw_clear(tmp2);
	/* now check for a variable defining the expression */
	var_assoc2 = h_var_assoc_get(h_var_assoc, var_assoc);
	if (var_assoc2) {
		var_assoc_free(var_assoc);
		var = var_assoc2->var;
	} else {
		var_assoc->var = var_assoc->vars[var_assoc->n] = LA_DAG_var(DAG);
		var = var_assoc->var;
		/* DD improve: create once the representation of -1L */
		LAsigned_hw_init(var_assoc->coefs[var_assoc->n]);
		LAsigned_hw_set_si(var_assoc->coefs[var_assoc->n], -1L);
		simplex_hw_constraint_push(
			var_assoc->n + 1, var_assoc->vars, var_assoc->coefs);
		LAsigned_hw_clear(var_assoc->coefs[var_assoc->n]);
		h_var_assoc_push(h_var_assoc, var_assoc);
	}

allocate:
	/* store the prior coefficient to retrieve when we construct proof steps */
	if (proof_on) {
		if (prior_var_alloc <= var_max) {
			MY_REALLOC(prior_coefficient_var, 2 * var_max * sizeof(TLArational_hw));
			for (unsigned i = prior_var_alloc; i < 2 * var_max; ++i) {
				memset(prior_coefficient_var[i], 0, sizeof(TLArational_hw));
			}
			prior_var_alloc = 2 * var_max;
		}
		LArational_hw_init(prior_coefficient_var[atom]);
		if (LArational_hw_is_neg(prior_coeff))
			LArational_hw_neg(prior_coeff);
		LArational_hw_set(prior_coefficient_var[atom], prior_coeff);
		LArational_hw_clear(prior_coeff);
	}

	simplex_hw_var_bounded(var);
	bounds->data[atom].var = var;
	LAdelta_hw_init(&bounds->data[atom].delta);
	LAdelta_hw_init(&bounds->data[atom].delta2);
	LArational_hw_neg(indep_term);
	if (simplex_hw_var_integer(var)) {
		/* The current constraint is on integer variables, and this block
		   tightens the bound for the constraint (+) and its negation (-).

		   Given a constraint v op c, where v is the slack variable, op the
		   relational operator (<, <=, >, >=) and c a numberic constant,
		   tightening follows the rules:

		   * If c is an integer:

		   | op | upper | strict | (+) | (-) | explanation                    |
		   +----+-------+--------+-----+-----|--------------------------------+
		   | >  | false | true   | c+1 | c   | v > c (+) v >= c+1 (-) v <= c  |
		   | >= | false | false  | c   | c-1 | v >= c (+) v >= c (-) v <= c-1 |
		   | <  | true  | true   | c-1 | c   | v < c (+) v <= c-1 (-) v >= c  |
		   | <= | true  | false  | c   | c+1 | v <= c (+) v <= c (-) v >= c+1 |


		   * If c is not an integer:

		   Let f = floor(c) in

		   | op | upper | strict | (+) | (-) | explanation                    |
		   +----+-------+--------+-----+-----|--------------------------------+
		   | >  | false | true   | f+1 | f   | v > c (+) v >= f+1 (-) v <= f  |
		   | >= | false | false  | f+1 | f   | v >= c (+) v >= f+1 (-) v <= f |
		   | <  | true  | true   | f   | f+1 | v < c (+) v <= f (-) v >= f+1  |
		   | <= | true  | false  | f   | f+1 | v <= c (+) v <= f (-) v >= f+1 |

		   In the following (+) and (-) are stored in the delta and delta2
		   fields of atom_bounds->data[atom].
		*/
		if (LArational_hw_is_int(indep_term)) /* implements the first table */
		{
			LAdelta_hw_set_rat(&bounds->data[atom].delta, &indep_term);
			LAdelta_hw_set_delta(&bounds->data[atom].delta, 0);
			LAdelta_hw_set(&bounds->data[atom].delta2, &bounds->data[atom].delta);
			if (upper)
				if (strict)
					LAdelta_hw_decrement(&bounds->data[atom].delta);
				else
					LAdelta_hw_increment(&bounds->data[atom].delta2);
			else if (strict)
				LAdelta_hw_increment(&bounds->data[atom].delta);
			else
				LAdelta_hw_decrement(&bounds->data[atom].delta2);
		} else /* implements the second table */
		{
			LAdelta_hw_set_rat(&bounds->data[atom].delta, &indep_term);
			LAdelta_hw_floor(&bounds->data[atom].delta);
			LAdelta_hw_set(&bounds->data[atom].delta2, &bounds->data[atom].delta);
			LAdelta_hw_set_delta(&bounds->data[atom].delta, 0);
			LAdelta_hw_set_delta(&bounds->data[atom].delta2, 0);
			if (upper)
				LAdelta_hw_increment(&bounds->data[atom].delta2);
			else
				LAdelta_hw_increment(&bounds->data[atom].delta);
		}
	} else {
		LAdelta_hw_set_rat(&bounds->data[atom].delta, &indep_term);
		LAdelta_hw_set_rat(&bounds->data[atom].delta2, &indep_term);
		LAdelta_hw_set_delta(&bounds->data[atom].delta, upper ? -strict : strict);
		LAdelta_hw_set_delta(
			&bounds->data[atom].delta2, upper ? (!strict) : -(!strict));
	}
	bounds->data[atom].upper = upper;

	LArational_hw_clear(indep_term);
	for (i = 0; i < stack_size(monoms); i++)
		LArational_hw_clear(stack_get(monoms, i).coef);
	stack_reset(monoms);
}

/**
   \brief adds equation to the Simplex module
   \param DAG the DAG for a term
   \pre the term is an argument of a non-arithmetic symbol.
   \details no slack variable is created here: the term is abstracted to
   a simplex variable and its definition is recorded as an equation in the
   Simplex module.
   \note the simplex variables abstracting arithmetic subterms are
   recorded in DAG_var_table
   \todo TODO: unify var_assoc and DAG_var_table?
*/
static void
LA_constraint_push(void)
{
	unsigned i, j;
	TLAsigned_hw lcm;
	TLAsigned_hw* coefs;
	Tsimplex_var* vars;
	/* sort and remove monoms on same variable */
	veriT_qsort(
		monoms->data, stack_size(monoms), sizeof(Tmonom), (TFcmp)cmp_monom);
	for (i = 0, j = 1; j < stack_size(monoms); j++)
		if (monoms->data[j].var == monoms->data[i].var) {
			LArational_hw_add(monoms->data[i].coef, monoms->data[j].coef);
		} else {
			LArational_hw_set(monoms->data[++i].coef, monoms->data[j].coef);
			monoms->data[i].var = monoms->data[j].var;
		}
	for (j = i + 1; j < stack_size(monoms); j++)
		LArational_hw_clear(monoms->data[j].coef);
	stack_resize(monoms, i + 1);
	/* remove monoms with null coefficient */
	for (i = 0, j = 0; j < stack_size(monoms); j++)
		if (!LArational_hw_is_zero(monoms->data[j].coef)) {
			LArational_hw_set(monoms->data[i].coef, monoms->data[j].coef);
			monoms->data[i++].var = monoms->data[j].var;
		}
	for (j = i; j < stack_size(monoms); j++)
		LArational_hw_clear(monoms->data[j].coef);
	stack_resize(monoms, i);
	/* computes the LCM of the coefficient denominators */
	LAsigned_hw_init(lcm);
	LAsigned_hw_set_si(lcm, 1);
	for (i = 0; i < stack_size(monoms); i++)
		LArational_hw_lcm(lcm, stack_get(monoms, i).coef);
	assert(!LAsigned_hw_is_zero(lcm));
	MY_MALLOC(coefs, stack_size(monoms) * sizeof(TLAsigned_hw));
	MY_MALLOC(vars, stack_size(monoms) * sizeof(Tsimplex_var));
	for (i = 0; i < stack_size(monoms); i++) {
		LAsigned_hw_init(coefs[i]);
		LAsigned_hw_set(coefs[i], lcm);
		LArational_hw_mult_to_signed(coefs[i], stack_get(monoms, i).coef);
		vars[i] = stack_get(monoms, i).var;
	}
	simplex_hw_constraint_push(stack_size(monoms), vars, coefs);
	free(vars);
	for (i = 0; i < stack_size(monoms); i++) {
		LAsigned_hw_clear(coefs[i]);
		LArational_hw_clear(stack_get(monoms, i).coef);
	}
	LAsigned_hw_clear(lcm);
	free(coefs);
	stack_reset(monoms);
}

/*
  --------------------------------------------------------------
  Collecting terms in formulas
  --------------------------------------------------------------
*/

typedef bool Tpol_b;

#define POL_NEG false
#define POL_POS true
#define inv_pol(a) (!(a))

/**
   \brief accumulates rat * var with polarity pol
   \param rat a rational number
   \param var a simplex variable
   \param pol a polarity */
static inline void
LA_hw_notify_monom(
	const TLArational_hw rat, const Tsimplex_var var, const Tpol_b pol)
{
	stack_inc(monoms);
	LArational_hw_init(stack_top(monoms).coef);
	LArational_hw_set(stack_top(monoms).coef, rat);
	if (pol == POL_NEG) LArational_hw_neg(stack_top(monoms).coef);
	stack_top(monoms).var = var;
}

/**
   \var DAG_todo
   \brief Queue for the DAGs to be notified. */
static Tstack_DAG DAG_todo;

static void LA_hw_notify_term(const TDAG, const Tpol_b, const TLArational_hw);

/**
   \brief accumulates all monoms in DAG, with polarity pol.
   \param[in] DAG a DAG
   \param[in] pol a polarity
   \param[in] fact a factor
   \remark do not introduce variable if not linear term
   \remark adds (non arithmetic) subterms for deep inspection
   \remark (sub)terms are set the visited tag
   \see LA_hw_notify_term, LA_hw_notify_term_top */
static inline bool
LA_hw_notify_term_top(
	const TDAG DAG, const Tpol_b pol, const TLArational_hw fact)
{
	TLArational_hw rat;
	unsigned i;
#if DEBUG_LA >= 2
	my_DAG_message("LA_notify_term_top %D, %d\n", DAG, pol);
#endif
	if (DAG_symb(DAG) == FUNCTION_SUM) {
		for (i = 0; i < DAG_arity(DAG); i++)
			LA_hw_notify_term(DAG_arg(DAG, i), pol, fact);
		return true;
	}
	if (DAG_symb(DAG) == FUNCTION_PROD && DAG_arity(DAG) == 2) {
		if (LA_DAG_is_number(DAG_arg0(DAG))) {
			LArational_hw_init(rat);
			LA_hw_DAG_number(DAG_arg0(DAG), rat);
			LArational_hw_mult(rat, fact);
			LA_hw_notify_term(DAG_arg1(DAG), pol, rat);
			LArational_hw_clear(rat);
			return true;
		}
		if (LA_DAG_is_number(DAG_arg1(DAG))) {
			LArational_hw_init(rat);
			LA_hw_DAG_number(DAG_arg1(DAG), rat);
			LArational_hw_mult(rat, fact);
			LA_hw_notify_term(DAG_arg0(DAG), pol, rat);
			LArational_hw_clear(rat);
			return true;
		}
	} else if (
		DAG_symb(DAG) == FUNCTION_UNARY_MINUS ||
		DAG_symb(DAG) == FUNCTION_UNARY_MINUS_ALT) {
		assert(DAG_arity(DAG) == 1);
		LA_hw_notify_term(DAG_arg0(DAG), inv_pol(pol), fact);
		return true;
	} else if (DAG_symb(DAG) == FUNCTION_MINUS) {
		LA_hw_notify_term(DAG_arg(DAG, 0), pol, fact);
		for (i = 1; i < DAG_arity(DAG); i++)
			LA_hw_notify_term(DAG_arg(DAG, i), inv_pol(pol), fact);
		return true;
	} else if (LA_DAG_is_number(DAG)) {
		LArational_hw_init(rat);
		LA_hw_DAG_number(DAG, rat);
		LArational_hw_mult(rat, fact);
		LA_hw_notify_monom(rat, 0, pol);
		LArational_hw_clear(rat);
		return true;
	}
	if ((DAG_symb(DAG) == FUNCTION_PROD || DAG_symb(DAG) == FUNCTION_DIV ||
			 DAG_symb(DAG) == FUNCTION_MOD || DAG_symb(DAG) == FUNCTION_ABS))
		my_error("Non linear expression\n");
	return false;
}

/**
   \brief accumulates all monoms in DAG, with polarity pol.
   \param[in] DAG a DAG
   \param[in] pol a polarity
   \param[in] fact a factor
   \remark introduce a variable if not linear term
   \remark adds its subterms for deep inspection
   \remark (sub)terms are set the visited tag
   \see LA_hw_notify_term, LA_hw_notify_term_top */
static void
LA_hw_notify_term(const TDAG DAG, const Tpol_b pol, const TLArational_hw fact)
{
	unsigned i;
	if (LA_DAG_is_var(DAG)) {
		LA_hw_notify_monom(fact, LA_DAG_var(DAG), pol);
		return;
	}
	if (LA_hw_notify_term_top(DAG, pol, fact)) return;
	LA_hw_notify_monom(fact, LA_DAG_var(DAG), pol);
	LA_DAG_var_share(DAG);
	for (i = 0; i < DAG_arity(DAG); i++) stack_push(DAG_todo, DAG_arg(DAG, i));
}

/**
   \brief introduces arithmetic definition for all arithmetic subterms in DAG
   \param[in] DAG a DAG */
static void
LA_hw_notify_deep_terms(const TDAG DAG)
{
	unsigned i;
	Tsymb symb = DAG_symb(DAG);
#if DEBUG_LA >= 2
	my_DAG_message("LA_notify_deep_terms: %D\n", DAG);
#endif
	if (LA_DAG_is_var(DAG)) /* TODO: Check if shared */
		return;
	if (
		symb == FUNCTION_SUM || symb == FUNCTION_PROD ||
		symb == FUNCTION_UNARY_MINUS || symb == FUNCTION_UNARY_MINUS_ALT ||
		symb == FUNCTION_MINUS || symb == FUNCTION_DIV || LA_DAG_is_number(DAG)) {
		/* Arithmetic term found */
		TLArational_hw rat;
		LArational_hw_init(rat);
		LArational_hw_set_one(rat);
		if (LA_hw_notify_term_top(DAG, POL_POS, rat)) {
			LA_hw_notify_monom(rat, LA_DAG_var(DAG), POL_NEG);
			LA_DAG_var_share(DAG);
			LArational_hw_clear(rat);
			LA_constraint_push();
#if DEBUG_LA >= 2
			my_DAG_message("Translating %D\n", DAG);
#endif
			return;
		}
		/* Condition only false if non-linear term: handle like any term */
		LArational_hw_clear(rat);
	}
	if (DAG_info->data[DAG].visited) return;
	DAG_info->data[DAG].visited = true;
	for (i = 0; i < DAG_arity(DAG); i++) {
		LA_hw_notify_deep_terms(DAG_arg(DAG, i));
		RETURN_IF_OVERFLOW();
	}
}

/**
   \brief stores arithmetic definition for the atom,
   and all arithmetic terms in DAG
   \param[in] DAG a DAG */
static inline void
LA_hw_notify_atom(const TDAG DAG)
{
	if (DAG_info->data[DAG].visited) return;
#if DEBUG_LA >= 2
	my_DAG_message("LA_notify_atom: %D\n", DAG);
#endif
	if (
		DAG_symb(DAG) == PREDICATE_LESS || DAG_symb(DAG) == PREDICATE_LEQ ||
		DAG_symb(DAG) == PREDICATE_GREATER ||
		DAG_symb(DAG) == PREDICATE_GREATEREQ) {
		TLArational_hw rat;
		LArational_hw_init(rat);
		LArational_hw_set_one(rat);
		LA_hw_notify_term(DAG_arg0(DAG), POL_POS, rat);
		LA_hw_notify_term(DAG_arg1(DAG), POL_NEG, rat);
		LArational_hw_clear(rat);
		/* IMPROVE asserts DAG has never been introduced */
		/* IMPROVE tighten constant using gcd of coefficients */
		assert(!DAG_var_table->data[DAG]);
		LA_constraint_push2(DAG);
		RETURN_IF_OVERFLOW();
	} else if (DAG_symb(DAG) == PREDICATE_ISINT)
		my_error("DAG2LA: not implemented\n");
	else {
		unsigned i;
		for (i = 0; i < DAG_arity(DAG); i++) stack_push(DAG_todo, DAG_arg(DAG, i));
	}
	while (stack_size(DAG_todo)) {
		LA_hw_notify_deep_terms(stack_pop(DAG_todo));
		RETURN_IF_OVERFLOW();
	}
	DAG_info->data[DAG].visited = true;
}

/**
   \brief adds to the Simplex arithmetic constraints found in DAG
   \param[in] DAG a DAG
   \details The arithmetic constraints are arithmetic atoms and the
   definition of arithmetic sub-terms of non-arithmetic symbols. For most
   constraints, an equation is added to the Simplex. For most inequalities,
   slack variables are created.
   This function is responsible for recursing over the propositional
   structure of the DAG. When an atom is found, the processing is handed
   over to LA_hw_notify_atom. */
static void
LA_hw_notify_formula_aux(const TDAG DAG)
{
	unsigned i;
	Tsymb symb = DAG_symb(DAG);
	if (DAG_info->data[DAG].visited) return;
#if DEBUG_LA >= 2
	my_DAG_message("LA_notify_formula_aux: %D\n", DAG);
#endif
	if (boolean_connector(symb))
		for (i = 0; i < DAG_arity(DAG); i++) {
			LA_hw_notify_formula_aux(DAG_arg(DAG, i));
			RETURN_IF_OVERFLOW();
		}
	else if (quantifier(symb))
		return;
	else if (
		symb == LET || symb == LAMBDA || symb == APPLY_LAMBDA ||
		symb == FUNCTION_ITE)
		my_error("DAG2LA: not implemented\n");
	else
		LA_hw_notify_atom(DAG);
	DAG_info->data[DAG].visited = true;
}

/*
  --------------------------------------------------------------
  Public functions
  --------------------------------------------------------------
*/

/**
   \brief stores arithmetic definition for arithmetic atoms,
   and all arithmetic terms in DAG
   \param DAG a DAG */
void
LA_hw_notify_formula(TDAG DAG)
{
	if (var_max + 1 > stack_size(bounds)) stack_resize(bounds, var_max + 1);
	LA_hw_notify_formula_aux(DAG);
	RETURN_IF_OVERFLOW();
	simplex_hw_simp_unbound();
	RETURN_IF_OVERFLOW();
	simplex_hw_solve();
	RETURN_IF_OVERFLOW();
#if DEBUG_LA >= 2
	bounds_print();
	DAG_var_share_print();
#endif
	assert(simplex_status == SAT);
	if (LA_enable_theory_propagation) {
#ifdef DEBUG_LA_PROPAGATE
		my_DAG_message("arith theory propagation is enabled.\n");
#endif
		LA_bound_rank();
	}
}

/**
   \brief comparison function used to classify atoms that
   correspond to arithmetic bounds.
   \remark groups together bounds on the same variable;
   within one such group, places first upper bounds in
   increasing order, and then lower bounds in decreasing
   order.
   \remark assumes that atoms are arithmetic bounds.
*/
static int
cmp_bound_hw(const void* P1, const void* P2)
{
	unsigned i1 = *(unsigned*)P1;
	unsigned i2 = *(unsigned*)P2;
	Tbound* Pbound1 = bounds->data + i1;
	Tbound* Pbound2 = bounds->data + i2;
	assert(Pbound1->var && Pbound2->var);
	if (Pbound1->var < Pbound2->var) return -1;
	if (Pbound1->var > Pbound2->var) return 1;
	if (Pbound1->upper < Pbound2->upper) return 1;
	if (Pbound1->upper > Pbound2->upper) return -1;
	if (Pbound1->upper)
		return LAdelta_hw_cmp(&Pbound1->delta, &Pbound2->delta);
	else
		return LAdelta_hw_cmp(&Pbound2->delta, &Pbound1->delta);
}

static unsigned*
LA_hw_sort_bounds(size_t* Psize)
{
	unsigned i, j, n, *tmp;
	/* Sort the literal bounds using lexicographical ordering on
     tuples (var, bound, upper).
     The array bounds is not changed: sorting is applied
     to the indices.
  */
	n = stack_size(bounds);
	MY_MALLOC(tmp, n * sizeof(unsigned));
	for (i = 0, j = 0; i < n; ++i) {
		if (bounds->data[i].var) {
			tmp[j] = i;
			++j;
		}
	}
	veriT_qsort(tmp, j, sizeof(unsigned), cmp_bound_hw);
	*Psize = j;
	return tmp;
}

extern bool LA_enable_lemmas_totality;

void
LA_hw_unate_propagation(void)
{
	unsigned i, *atom;
	size_t n;
	/* create a classification of atoms in "atom", "n" being assigned its size */
	atom = LA_hw_sort_bounds(&n);
	/* compare consecutive entries; if they are related, generate lemma */
	i = 0;
	while (i + 1 < n) {
		/* one execution of the loop body generates lemmas for one variable */
		/* i : v <= b_i, ..., i+n: v <= b_{i+n},
         i+n+1: v >= b_{i+n+1}, i+n+m: v >= b_{i+n+m}
         where:
         for 0 <= k < n : b_k <= b_{k+1}
         for n+1 <= k < n+m : b_k >= b_{k+1} */
		unsigned start = i, next;
		unsigned j = i + 1;
		Tvar ai = atom[i], aj = atom[j];
		Tsimplex_var v = bounds->data[ai].var;
		/* generate lemmas between upper bounds */
		while (bounds->data[aj].var == v && bounds->data[aj].upper) {
			/* (i : v <= b_i), (j : v <= b_j), b_i <= b_j
             |= (i) => (j) -- lemma */
			record_lemma(DAG_or2(DAG_not(var_to_DAG(ai)), var_to_DAG(aj)));
			if (LAdelta_hw_eq(&bounds->data[ai].delta, &bounds->data[aj].delta))
				/* (i : v <= b_i), (j : v <= b_j), b_i = b_j
               |= (j) => (i) -- lemma */
				record_lemma(DAG_or2(var_to_DAG(ai), DAG_not(var_to_DAG(aj))));
			if (j + 1 == n) /* no more bounds */
				return;
			i = j;
			ai = aj;
			j = i + 1;
			aj = atom[j];
		}
		assert(j == i + 1);
		i = j;
		ai = aj;
		assert(bounds->data[ai].var != v || !bounds->data[ai].upper);
		/* only upper bounds on vi, go to next variable */
		if (bounds->data[ai].var != v) continue;
		/* generate lemmas between lower bounds */
		assert(bounds->data[ai].var == v && !bounds->data[ai].upper);
		j += 1;
		aj = atom[j];
		while (i + 1 < n && bounds->data[aj].var == v) {
			assert(j == i + 1);
			assert(bounds->data[ai].var == v);
			assert(!bounds->data[ai].upper);
			assert(bounds->data[aj].var == v);
			assert(!bounds->data[aj].upper);
			/* (i : v >= b_i), (j : v >= b_j), b_i >= b_j
             |= (i) => (j) -- lemma */
			record_lemma(DAG_or2(DAG_not(var_to_DAG(ai)), var_to_DAG(aj)));
			if (LAdelta_hw_eq(&bounds->data[ai].delta, &bounds->data[aj].delta))
				/* (i : v >= b_i), (j : v >= b_j), b_i = b_j
               |= (j) => (i) -- lemma */
				record_lemma(DAG_or2(var_to_DAG(ai), DAG_not(var_to_DAG(aj))));
			i = j;
			ai = aj;
			j = i + 1;
			aj = atom[j];
		}
		/* generate lemmas between lower and upper bounds */
		assert(bounds->data[ai].var == v && !bounds->data[ai].upper);
		assert(j == i + 1);
		assert(j == n || bounds->data[aj].var != v);
		next = j;
		j = i;
		aj = atom[j];
		i = start;
		ai = atom[i];
		while (bounds->data[ai].upper && !bounds->data[aj].upper) {
			assert(bounds->data[ai].var == v);
			assert(bounds->data[aj].var == v);
			if (LAdelta_hw_less(&bounds->data[ai].delta, &bounds->data[aj].delta)) {
				/* (i : v <= b_i), (j : v >= b_j), b_i < b_j
                 |= (i) => !(j) -- lemma */
				if (
					!LA_enable_lemmas_totality ||
					!totality_check(lit_make(ai, 1), lit_make(aj, 1)))
					record_lemma(
						DAG_or2(DAG_not(var_to_DAG(ai)), DAG_not(var_to_DAG(aj))));
				i += 1;
				ai = atom[i];
			} else {
				j -= 1;
				aj = atom[j];
			}
		}
		i = next;
	}
	free(atom);
}

/* PF
   pol   strict upper  eps  simplex_lower/upper
   0     0      0      -1   up
   0     0      1       1   low
   0     1      0       0   up
   0     1      1       0   low
   1     0      0       0   low
   1     0      1       0   up
   1     1      0       1   low
   1     1      1      -1   up
*/
Tstatus
LA_hw_assert(Tlit lit)
{
	Tstatus status;
	Tvar atom = lit_var(lit);
	Tstatus (*assert_fn)(Tsimplex_var, TLAdelta_hw, Tlit);
	TLAdelta_hw delta;
	RETURN_IF_OVERFLOW(UNDEF);
	/* TODO: this is a quick fix for model equalities.  When model equalities
     are generated, var_bounds should be resized */
	if (var_max + 1 > stack_size(bounds)) stack_resize(bounds, var_max + 1);
	if (!bounds->data[atom].var) return SAT;
	if (LA_enable_theory_propagation && LA_is_hint(lit)) return SAT;
#if DEBUG_LA
	undo_push(LA_ASSERT);
	my_DAG_message(
		"LA_hw_assert(%d) : %s%D%s\n", lit, lit_pol(lit) ? "" : "(not ",
		var_to_DAG(atom), lit_pol(lit) ? "" : ")");
#endif
	if (SAT_lit_pol(lit) ^ bounds->data[atom].upper)
		assert_fn = simplex_hw_assert_lower_bound;
	else
		assert_fn = simplex_hw_assert_upper_bound;
	if (lit_pol(lit))
		delta = bounds->data[atom].delta;
	else
		delta = bounds->data[atom].delta2;
	status = assert_fn(bounds->data[atom].var, delta, lit);
	if (LA_enable_theory_propagation) LA_do_hint(lit);
	return status;
}

Tstatus
LA_hw_assert_eq(TDAG DAG0, TDAG DAG1)
{
	Tsimplex_var svar;
#if DEBUG_LA
	undo_push(LA_ASSERT);
	my_DAG_message("LA_hw_assert_eq (= %D %D)\n", DAG0, DAG1);
#endif
	assert(DAG0 != DAG1);
	if (DAG0 > DAG1) { SWAP(DAG0, DAG1); }
	svar = eq_get_from_DAG(DAG0, DAG1);
	if (!svar) {
		TLArational_hw rat;
		/* Never add variables in inconsistent state
         We should have somewhere a valid model */
		if (simplex_hw_solve() != SAT) return UNSAT;
		LArational_hw_init(rat);
		LArational_hw_set_one(rat);
		LA_hw_notify_monom(rat, LA_DAG_var(DAG0), POL_POS);
		LA_hw_notify_monom(rat, LA_DAG_var(DAG1), POL_NEG);
		/* IMPROVE
         reuse, if any, a variable defining c1 * (DAG0 - DAG1) + c2 */
		svar = simplex_hw_var_new(
			DAG_sort(DAG0) == SORT_INTEGER && DAG_sort(DAG1) == SORT_INTEGER);
		simplex_hw_var_bounded(svar);
#if DEBUG_LA >= 2
		my_DAG_message(
			"LA_hw_assert_eq %D = %D associated to %u\n", DAG0, DAG1, svar);
#endif
		eq_store(DAG0, DAG1, svar);
		LA_hw_notify_monom(rat, svar, POL_NEG);
		LA_constraint_push();
		LArational_hw_clear(rat);
		RETURN_IF_OVERFLOW(UNDEF);
		simplex_hw_solve();
		RETURN_IF_OVERFLOW(UNDEF);
		assert(simplex_status == SAT);
	}
	LAdelta_hw_set_zero(&store.assert_eq.delta);
	if (
		simplex_hw_assert_lower_bound(svar, store.assert_eq.delta, 0) == UNSAT ||
		simplex_hw_assert_upper_bound(svar, store.assert_eq.delta, 0) == UNSAT)
		return UNSAT;
	return UNDEF;
}

#define DAG_leq(a, b) DAG_new_binary(PREDICATE_LEQ, a, b)

Tstatus
LA_hw_assert_neq(TDAG DAG0, TDAG DAG1)
{
	TDAG lemma;
#if DEBUG_LA
	undo_push(LA_ASSERT);
	my_DAG_message("LA_hw_assert_neq (not (= %D %D))\n", DAG0, DAG1);
#endif
	assert(DAG0 != DAG1);
	if (DAG0 > DAG1) { SWAP(DAG0, DAG1); }
	/* IMPROVE: for inequalities between constants c!=d,
     just introduce the unit clause */
	if (eq_get_lemma_generated(DAG0, DAG1)) return UNDEF;
	eq_set_lemma_generated(DAG0, DAG1);
	/* IMPROVE:
     there is no need to generate a lemma for two numbers
     if (LA_DAG_is_number(DAG0) && LA_DAG_is_number(DAG1))
     return UNDEF */
	lemma = DAG_dup(DAG_or3(
		DAG_eq(DAG0, DAG1), DAG_not(DAG_leq(DAG0, DAG1)),
		DAG_not(DAG_leq(DAG1, DAG0))));
	stack_push(veriT_lemmas, lemma);
	proof_set_lemma_id(lemma, proof_add_disequality_lemma(DAG_dup(lemma)));
#if DEBUG_LA >= 2
	my_DAG_message("Lemma_hw: %D\n", stack_top(veriT_lemmas));
#endif
	return UNDEF;
}

void
LA_hw_conflict(void)
{
	unsigned i;
	assert(!LA_hw_intg_conflict);
	assert(stack_size(conflict_lits) == 0);
	assert(stack_size(conflict_eqs) == 0);
	simplex_hw_conflict(&conflict_lits, &conflict_eqs);
	RETURN_IF_OVERFLOW();
	for (i = 0; i < stack_size(conflict_lits); ++i)
		stack_push(veriT_conflict, stack_get(conflict_lits, i));
	stack_reset(conflict_lits);
	for (i = 0; i < stack_size(conflict_eqs); i++)
		if (eq_test(stack_get(conflict_eqs, i))) {
			TDAG DAG0, DAG1;
			eq_get_from_var(stack_get(conflict_eqs, i), &DAG0, &DAG1);
			stack_push(veriT_conflict_eqs, DAG0);
			stack_push(veriT_conflict_eqs, DAG1);
		}
	stack_reset(conflict_eqs);
}

Tproof
LA_hw_conflict_proof(void)
{
	Tstack_DAG lits;
	Tproof proof;

	assert(!LA_hw_intg_conflict);
	Tstack_LAsigned_hw conflict_coeffs;
	stack_INIT(conflict_coeffs);
	simplex_hw_conflict_proof(&conflict_lits, &conflict_eqs, &conflict_coeffs);
	stack_INIT(lits);

	Tstack_DAG args;
	stack_INIT(args);

	unsigned coef_idx = 0;
	for (unsigned i = 0; i < stack_size(conflict_lits); ++i) {
		Tlit lit = stack_get(conflict_lits, i);
		TDAG DAG = lit_to_DAG(lit);
		stack_push(lits, DAG_dup(lit_pol(lit) ? DAG_not(DAG) : DAG));
		stack_push(veriT_conflict, lit);

		TLArational_hw c;
		LArational_hw_init(c);
		if (!LAsigned_hw_is_zero(stack_get(conflict_coeffs, coef_idx)))
			LArational_hw_set_si(c, stack_get(conflict_coeffs, coef_idx));
		else
			LArational_hw_set_one(c);

		assert(!LArational_hw_is_zero(prior_coefficient_var[SAT_lit_var(lit)]));
		LArational_hw_mult(c, prior_coefficient_var[SAT_lit_var(lit)]);

		TDAG arg;
		if (SORT_REAL != DAG_SORT_NULL) {
#ifdef HW_VERSION
			arg = DAG_dup(DAG_new_rational(c->num, c->den));
#else
			arg = DAG_dup(DAG_new_rational_mpq(c));
#endif
		} else {
			assert(SORT_INTEGER != DAG_SORT_NULL);
#ifdef HW_VERSION
			if (LArational_hw_is_int(c))
				arg = DAG_dup(DAG_new_integer(c->num));
			else {
				TDAG n = DAG_dup(DAG_new_integer(c->num));
				TDAG d = DAG_dup(DAG_new_integer(c->den));
				arg = DAG_dup(DAG_new_binary(FUNCTION_DIV, n, d));
				DAG_free(n);
				DAG_free(d);
			}
#else
			if (LArational_hw_is_int(c))
				arg = DAG_dup(DAG_new_integer_mpz(mpq_numref(c)));
			else {
				TDAG n = DAG_dup(DAG_new_integer_mpz(mpq_numref(c)));
				TDAG d = DAG_dup(DAG_new_integer_mpz(mpq_denref(c)));
				arg = DAG_dup(DAG_new_binary(FUNCTION_DIV, n, d));
				DAG_free(n);
				DAG_free(d);
			}
#endif
		}
		stack_push(args, arg);

		coef_idx++;
	}
	stack_reset(conflict_lits);
	for (unsigned i = 0; i < stack_size(conflict_eqs); ++i) {
		Tsimplex_var var;
		var = stack_get(conflict_eqs, i);
		/* if var is not an equality then it has no reason */
		if (eq_test(var)) {
			TDAG DAG0, DAG1;
			eq_get_from_var(stack_get(conflict_eqs, i), &DAG0, &DAG1);
			stack_push(veriT_conflict_eqs, DAG0);
			stack_push(veriT_conflict_eqs, DAG1);
			stack_push(lits, DAG_dup(DAG_neq(DAG0, DAG1)));

#ifdef HW_VERSION
			TDAG arg = DAG_dup(DAG_new_integer(stack_get(conflict_coeffs, coef_idx)));
#else
			TDAG arg = DAG_dup(DAG_new_integer_mpz(stack_get(conflict_coeffs, coef_idx)));
#endif
			stack_push(args, arg);

			coef_idx++;
		}
	}
	stack_reset(conflict_eqs);

	proof = proof_step_valid_stack_args(ps_type_la_generic, lits, args);

	stack_apply(args, DAG_free);
	stack_free(args);
	stack_apply(lits, DAG_free);
	stack_free(lits);

	/* I can't use stack_apply, because LAsigned_hw_clear is a macro */
	for (unsigned i = 0; i < stack_size(conflict_coeffs); ++i) {
		LAsigned_hw_clear(stack_get(conflict_coeffs, i));
	}
	stack_free(conflict_coeffs);
	return proof;
}

static int
cmp_simplex_assign(const void* P1, const void* P2)
{
	TDAG DAG1 = *(TDAG*)P1;
	TDAG DAG2 = *(TDAG*)P2;
	/* IMPROVE: by grouping together terms that are = and in the same CC class
     we may decrease the amount of generated model equalities */
	return simplex_hw_var_cmp(
		stack_get(DAG_var_table, DAG1), stack_get(DAG_var_table, DAG2));
}

bool
LA_hw_model_eq(void)
{
	unsigned n = stack_size(DAG_var_share);
	unsigned i, j;

	if (n == 0) return false;
	/* TODO: be careful that DAG representing numbers should also be in the game */

	simplex_hw_update_assign();

	assert(stack_size(xqueue) == 0);
	stack_sort(DAG_var_share, cmp_simplex_assign);
	for (i = 0; i < n - 1;) {
		TDAG DAGi = stack_get(DAG_var_share, i);
		Tsimplex_var sv_i = stack_get(DAG_var_table, DAGi);
		j = i + 1;
		do {
			TDAG DAGj = stack_get(DAG_var_share, j);
			Tsimplex_var sv_j = stack_get(DAG_var_table, DAGj);
			if (!simplex_hw_var_eq(sv_i, sv_j)) break;
			veriT_xenqueue_type(XTYPE_LA_MODEL_EQ);
			veriT_xenqueue_DAG(DAGi);
			veriT_xenqueue_DAG(DAGj);
			++j;
		} while (j < n);
		i = j;
	}
	return stack_size(xqueue) != 0;
}

Tstatus
LA_hw_solve_r(void)
{
	Tstatus r;

	RETURN_IF_OVERFLOW(UNDEF);
	r = simplex_hw_solve();
	RETURN_IF_OVERFLOW(UNDEF);
	return r;
}

/*
  --------------------------------------------------------------
  Integer Reasoning
  --------------------------------------------------------------
*/

/*
  --------------------------------------------------------------
  Branch and Bound Variable Selection Heuristics
  --------------------------------------------------------------
*/

static Tsimplex_var bb_var; /** variable selected for branching */
static enum { LOW = 0, UPP = 1 } bb_dir; /** last branch taken */

/** data used to score and select variables */
typedef struct Sbb_data
{
	double low; /* number of times low branch taken */
	double count_low; /* conflict variables after taking low branch */
	double upp; /* number of times upp branch taken */
	double count_upp; /* conflict variables after taking upp branch */
} Tbb_data;

#ifdef HW_VERSION /* Shared between versions */
Tbb_data* bb_data; /* table containing the variable data */
#else
extern Tbb_data* bb_data; /* table containing the variable data */
#endif

static size_t bb_var_n; /* number of variables */

/**
   \brief initializes data for branch and bound variable selection heuristics.
   \note this is called for each LA_hw_solve, which is not the most efficient
   solution. */
static void
bb_data_init(void)
{
	bb_var_n = 128; /* any positive value is fine */
	MY_MALLOC(bb_data, bb_var_n * sizeof(struct Sbb_data));
	memset(bb_data, 0, bb_var_n * sizeof(struct Sbb_data));
	bb_var = 0;
}

/**
   \brief initializes data for branch and bound variable selection heuristics.
   \note this is called for each LA_hw_solve. */
static void
bb_data_reset(void)
{
	unsigned n = 1 + simplex_hw_var_n();
	if (bb_var_n < n) {
		bb_var_n = n;
		MY_REALLOC(bb_data, n * sizeof(struct Sbb_data));
	}
	memset(bb_data, 0, n * sizeof(*bb_data));
}

/** \brief frees resources for branch and bound variable selection heuristics */
static void
bb_data_done()
{
	free(bb_data);
}

/** \brief computes the score of a variable as specified by A.Griggio in JSAT */
static double
bb_data_score(const Tsimplex_var v)
{
	double sl, sr;
	assert(v < bb_var_n);
	if (bb_data[v].low == 0) return 0;
	if (bb_data[v].upp == 0) return (bb_data[v].count_low / bb_data[v].low);
	sl = bb_data[v].count_low / bb_data[v].low;
	sr = bb_data[v].count_upp / bb_data[v].upp;
	return sl < sr ? sl : sr;
}

/** \brief updates the input data to compute a variable score */
static void
bb_data_update(const unsigned var_n)
{
	assert(bb_var < bb_var_n);
	if (bb_dir == LOW) {
		bb_data[bb_var].low += 1;
		bb_data[bb_var].count_low += var_n;
	} else {
		bb_data[bb_var].upp += 1;
		bb_data[bb_var].count_upp += var_n;
	}
}

void
LA_hw_conflict_z(void)
{
	/* globals:
     veriT_conflict
     LA_hw_intg_conflict
     conflict_lits
     conflict_eqs
  */
	unsigned i;
#ifdef DEBUG_LA_BB
	printf("LA_hw_conflict_z\n");
#endif
	assert(LA_hw_intg_conflict);
	LA_hw_intg_conflict = false;
	for (i = 0; i < stack_size(conflict_lits); ++i)
		if (stack_get(conflict_lits, i) != LIT_BRANCH_Z)
			stack_push(veriT_conflict, stack_get(conflict_lits, i));
	stack_reset(conflict_lits);
	for (i = 0; i < stack_size(conflict_eqs); i++) {
		Tsimplex_var var;
		var = stack_get(conflict_eqs, i);
		if (eq_test(var)) {
			TDAG DAG0, DAG1;
			eq_get_from_var(stack_get(conflict_eqs, i), &DAG0, &DAG1);
			stack_push(veriT_conflict_eqs, DAG0);
			stack_push(veriT_conflict_eqs, DAG1);
		}
	}
	stack_reset(conflict_eqs);
}

Tproof
LA_hw_conflict_proof_z(void)
{
	/* globals:
     veriT_conflict
     LA_hw_intg_conflict
     conflict_lits
     conflict_eqs
  */
	unsigned i;
	Tstack_DAG lits;
	Tproof proof;

	assert(LA_hw_intg_conflict);
	stack_INIT(lits);
	LA_hw_intg_conflict = false;
	for (i = 0; i < stack_size(conflict_lits); ++i)
		if (stack_get(conflict_lits, i) != LIT_BRANCH_Z) {
			Tlit lit = stack_get(conflict_lits, i);
			TDAG DAG = lit_to_DAG(lit);
			stack_push(lits, DAG_dup(lit_pol(lit) ? DAG_not(DAG) : DAG));
			stack_push(veriT_conflict, lit);
		}
	stack_reset(conflict_lits);
	for (i = 0; i < stack_size(conflict_eqs); i++) {
		Tsimplex_var var;
		var = stack_get(conflict_eqs, i);
		/* if var is not an equality then it has no reason */
		if (eq_test(var)) {
			TDAG DAG0, DAG1;
			eq_get_from_var(stack_get(conflict_eqs, i), &DAG0, &DAG1);
			stack_push(veriT_conflict_eqs, DAG0);
			stack_push(veriT_conflict_eqs, DAG1);
			stack_push(lits, DAG_dup(DAG_neq(DAG0, DAG1)));
		}
	}
	stack_reset(conflict_eqs);
	proof = proof_step_valid_stack(ps_type_lia_generic, lits);
	stack_apply(lits, DAG_free);
	stack_free(lits);
	return proof;
}

static Tstatus
LA_hw_solve_z_aux(
	unsigned d, unsigned m, Tstack_lit* Plits, Tstack_simplex_var* Peqs)
{
	Tstatus r;
	Tsimplex_var vi = 0;
	double scorei;
	unsigned it;
	unsigned conflict_var_n = 0;
	TLAdelta_hw* Pdelta = NULL;

	if (simplex_hw_solve() == UNSAT) {
		/* record unsatisfiable constraint set for conflict clause computation */
		simplex_hw_conflict(Plits, Peqs);
		return UNSAT;
	}
	RETURN_IF_OVERFLOW(UNDEF);
	if (d == m) return UNDEF;
	store_solve_aux_fit(d);

	/* checks if an integer variable is not assigned an integer value */
	for (it = simplex_integer_var_begin(); it != simplex_integer_var_end();
			 ++it) {
		vi = simplex_integer_var_get(it);
		Pdelta = simplex_hw_var_assign(vi);
		if (!LAdelta_hw_is_int(Pdelta)) break;
	}

	/* all integer variables are ok: the model is good */
	if (it == simplex_integer_var_end()) { return SAT; }
	/* vi is the least integer variable in conflict */
	assert(vi != 0);

	/* select a variable for branch-and-bound */
	if (!LA_disable_bbvsh) {
		/* prepare data for branch-and-bound variable selection heuristic */
		if (d == 1) bb_data_reset();
		/* compute in vi the conflict variable with the smallest score  */
		scorei = bb_data_score(vi);
		conflict_var_n = 1;
		++it;
		while (it != simplex_integer_var_end() && scorei > 0) {
			Tsimplex_var vj = simplex_integer_var_get(it);
			Pdelta = simplex_hw_var_assign(vj);
			if (!LAdelta_hw_is_int(Pdelta)) {
				double scorej = bb_data_score(vj);
				++conflict_var_n;
				if (scorej < scorei) {
					vi = vj;
					scorei = scorej;
				}
			}
			++it;
		}
	}
	Pdelta = simplex_hw_var_assign(vi);
	assert(Pdelta != NULL);
	LAdelta_hw_set(&store.solve_aux.xi[d], Pdelta);
	/* set up global variables to update heuristics data */
	if (!LA_disable_bbvsh) {
		/* update data for branch-and-bound variable selection */
		if (bb_var != 0) bb_data_update(conflict_var_n);
	}
	undo_level_new();
	LAdelta_hw_floor(&store.solve_aux.xi[d]);
	simplex_hw_assert_upper_bound(vi, store.solve_aux.xi[d], LIT_BRANCH_Z);
	if (!LA_disable_bbvsh) {
		bb_var = vi;
		bb_dir = UPP;
	}
	r = LA_hw_solve_z_aux(d + 1, m, Plits, Peqs);
	undo_level_del();

	if (r != UNSAT) goto finish;
	assert(r == UNSAT);
	LA_hw_repair();

	undo_level_new();
	LAdelta_hw_increment(&store.solve_aux.xi[d]);
	simplex_hw_assert_lower_bound(vi, store.solve_aux.xi[d], LIT_BRANCH_Z);
	/* set up global variables to update heuristics data */
	if (!LA_disable_bbvsh) {
		bb_var = vi; /* bb_var is a global variable: must be set */
		bb_dir = LOW;
	}
	r = LA_hw_solve_z_aux(d + 1, m, Plits, Peqs);
	undo_level_del();

	if (r != UNSAT) goto finish;
	assert(r == UNSAT);
	LA_hw_repair();

finish:
	return r;
}

Tstatus
LA_hw_solve_z(void)
{
	Tstatus r;
	RETURN_IF_OVERFLOW(UNDEF);
	assert(stack_size(conflict_lits) == 0);
	assert(stack_size(conflict_eqs) == 0);
	r = LA_hw_solve_z_aux(1, MAX_BRANCH_DEPTH, &conflict_lits, &conflict_eqs);
	if (r == UNSAT) {
		LA_hw_intg_conflict = true;
	} else {
		stack_reset(conflict_lits);
		stack_reset(conflict_eqs);
	}
	return r;
}

void
LA_hw_simp(void)
{
	simplex_hw_simp_const();
}

void
LA_hw_repair(void)
{
	simplex_hw_repair();
}

static void
LA_DAG_hook_resize(unsigned old_alloc, unsigned new_alloc)
{
#ifdef PEDANTIC
	printf("%d\n", old_alloc);
#endif
	if (!DAG_var_table) return;
	stack_resize(DAG_var_table, new_alloc);
	stack_resize(DAG_info, new_alloc);
}

void
LA_hw_init(void)
{
	LA_lemma_n = 0;
#if DEBUG_LA
	/* a hook may only be set once, let it be the first (HW) */
#ifdef HW_VERSION
	undo_set_hook(LA_ASSERT, (Tundo_hook)LA_hook_assert, 0);
#endif
#endif
	stack_INIT(DAG_var_table);
	stack_INIT(DAG_var_share);
	stack_INIT(DAG_info);
	DAG_set_hook_resize(LA_DAG_hook_resize);
	stack_INIT(monoms);
	stack_INIT(DAG_todo);
	stack_INIT(bounds);
	simplex_hw_init();
	stack_INIT(conflict_lits);
	stack_INIT(conflict_eqs);
	h_var_assoc = h_var_assoc_new(8);
	store_init();
	if (!LA_disable_bbvsh) bb_data_init();
	backtrack_init();
	LA_bound_ranking = 0;
	LA_bound_ranking_n = 0;

	if (var_max < 64)
		prior_var_alloc = 64;
	else
		prior_var_alloc = 2 * var_max;
	MY_MALLOC(prior_coefficient_var, prior_var_alloc * sizeof(TLArational_hw));
	memset(prior_coefficient_var, 0, prior_var_alloc * sizeof(TLArational_hw));
}

void
LA_hw_done(void)
{
	size_t i;
	stats_unsigned(
		"lemmas/bounds_hw", "Bound lemmas generated by arith (HW version)", "%9u",
		LA_lemma_n);
	if (!LA_disable_bbvsh) bb_data_done();
	if (LA_bound_ranking) free(LA_bound_ranking);
	backtrack_done();
	store_done();
	h_var_assoc_apply(h_var_assoc, var_assoc_free);
	h_var_assoc_free(&h_var_assoc);
	stack_free(conflict_eqs);
	stack_free(conflict_lits);
	simplex_hw_done();
	stack_free(DAG_var_table);
	stack_free(DAG_var_share);
	stack_free(DAG_info);
	stack_free(monoms);
	stack_free(DAG_todo);
	for (i = 0; i < stack_size(bounds); ++i) {
		LAdelta_hw_clear(&bounds->data[i].delta);
		LAdelta_hw_clear(&bounds->data[i].delta2);
	}
	stack_free(bounds);

	/* Ugly. The compiler hopfuly optimizes the empty loop away in the hw case */
	for (unsigned i = 0; i < prior_var_alloc; ++i) {
		if (prior_coefficient_var[i] != 0)
			LArational_hw_clear(prior_coefficient_var[i]);
	}
	free(prior_coefficient_var);
}

int
LA_hw_lit_satisfied(Tlit lit)
{
	Tvar atom = lit_var(lit);
	Tbound b = bounds->data[atom];
	if (b.var) {
		TLAdelta_hw* Pval = simplex_hw_var_assign(b.var);
		if (b.upper ^ SAT_lit_pol(lit))
			return LAdelta_hw_less(Pval, &b.delta);
		else
			return LAdelta_hw_less(&b.delta, Pval);
	} else
		return 0;
}

/*
  --------------------------------------------------------------
  Theory Propagation: Definitions
  --------------------------------------------------------------
*/

/** \brief sends a hint
    \param[in] lit the hinted literal
    \param[in] cause the literal that entails the hinted literal */

extern bool LA_enable_lemmas_totality;

static void
LA_hint(const Tlit lit, const Tlit cause)
{
#ifdef DEBUG_LA_PROPAGATE
	printf("\tLA_hint(%u=%c[", lit, lit_pol(lit) ? '+' : '-');
	bound_print(bounds->data[lit_var(lit)]);
	printf("])\n");
#endif
	if (LA_enable_lemmas_totality && totality_check(lit, cause)) {
#ifdef DEBUG_LA_PROPAGATE
		printf("totality hint: already registered.\n");
#endif
		return;
	}
	assert(!LA_is_hint(lit));
	hint_LA(lit, cause);
	bounds->data[lit_var(lit)].hint |= (1u << lit_pol(lit));
	backtrack_hint(lit);
}

/** \brief tests if lit has been hinted */
static bool
LA_is_hint(const Tlit lit)
{
	return bounds->data[lit_var(lit)].hint & (1u << lit_pol(lit));
}

/**
   \brief shortcut helper for LA_do_hint: increments iterators
   \param[out] J variable indexing in the bound ranking
   \param[out] A variable receiving the bound identifier
   \param[out] P variable receiving the address of the bound data
   \param[in] V new value of the index in the bound ranking  */

#define UPDATE_INDEX(J, A, P, V) \
	{ \
		(J) = (V); \
		(A) = LA_bound_ranking[J]; \
		(P) = bounds->data + (A); \
	}

/**
   \brief shortcut helper for LA_do_hint: tests if bound applies to variable
   \param[in] P address of bound data
   \param[in] V relevant simplex variable identifier
*/

#define VAR_INDEX(P, V) ((P)->var == (V))

/**
   \brief theory propagation from arithmetics to SAT, considering
   bound constraints.
   \param[in] lit a literal that has been asserted by the SAT solver
   \pre
   - lit is not a hint
   - LA_bound_ranking, LA_bound_ranking_n, and the rank field of
   elements of bounds have been computed */

static void
LA_do_hint(const Tlit lit)
{
	Tvar atom = SAT_lit_var(lit);
	Tbound bound = bounds->data[atom];
	Tsimplex_var svar = bound.var;
	unsigned rank = bound.rank;
	unsigned j;
	Tvar aj;
	Tbound* Pbj;
	Tlit lj;
#ifdef DEBUG_LA_PROPAGATE
	printf("LA_do_hint(%u=%c[", lit, lit_pol(lit) ? '+' : '-');
	bound_print(bounds->data[lit_var(lit)]);
	printf("])\n");
#endif
	/*
    The following invariant holds, for c, b of type Tbound:
    LA_bound_ranking[c] == LA_bound_ranking[b]+1 <=>
    \/  b.var != c.var
    /\  b.var == c.var
    \/  b.upper AND c.upper AND b.bound <= c.bound
    b.upper AND !c.upper
    !b.upper AND !c.upper AND b.bound >= c.bound
    =>  b.upper = c.upper
    /\  hinted[lit(b,1)] => hinted[lit(c, 1)]
    hinted[lit(c,0)] => hinted[lit(b, 0)]
  */
	if (bound.upper && SAT_lit_pol(lit)) { /* CASE 1 - lit : svar <= b
         hints: { svar <= u | b <= u } + {!(svar >= l) | b < l} */
		if (rank + 1 < LA_bound_ranking_n) {
			UPDATE_INDEX(j, aj, Pbj, rank + 1);
			/* hint: { +A | !LA_hinted(+A) and A == svar <= u and b <= u } */
			while (1) {
				if (!VAR_INDEX(Pbj, svar)) return;
				if (!Pbj->upper) break;
				lj = SAT_lit(aj, 1);
				if (LA_is_hint(lj)) break;
				LA_hint(lj, lit);
				if (j + 1 == LA_bound_ranking_n) return;
				UPDATE_INDEX(j, aj, Pbj, j + 1);
			}
			/* skip: { A == svar <= u | LA_hinted(+A) } */
			while (1) {
				if (!VAR_INDEX(Pbj, svar)) return;
				if (!Pbj->upper) break;
				assert(LA_is_hint(SAT_lit(aj, 1)));
				if (j + 1 == LA_bound_ranking_n) return;
				UPDATE_INDEX(j, aj, Pbj, j + 1);
			}
			/* skip: { A == svar >= l | b < l or LA_hinted(-A) } */
			while (1) {
				if (!VAR_INDEX(Pbj, svar)) return;
				if (!LAdelta_hw_less(&bound.delta, &Pbj->delta)) return;
				lj = SAT_lit(aj, 0);
				if (!LA_is_hint(lj)) break;
				if (j + 1 == LA_bound_ranking_n) return;
				UPDATE_INDEX(j, aj, Pbj, j + 1);
			}
			/* hint: { !(svar >= l) | b < l } */
			while (1) {
				if (!VAR_INDEX(Pbj, svar)) return;
				if (!LAdelta_hw_less(&bound.delta, &Pbj->delta)) return;
				lj = SAT_lit(aj, 0);
				assert(!LA_is_hint(lj));
				LA_hint(lj, lit);
				if (j + 1 == LA_bound_ranking_n) return;
				UPDATE_INDEX(j, aj, Pbj, j + 1);
			}
		}
	} else if (!bound.upper && SAT_lit_pol(lit)) { /* CASE 2 - lit: svar >= b
         hints: {svar >= l | b >= l} + {!(svar <= u) | b > u} */
		if (rank + 1 < LA_bound_ranking_n) {
			UPDATE_INDEX(j, aj, Pbj, rank + 1);
			/* hint {svar >= l | b >= l} */
			while (1) {
				if (!VAR_INDEX(Pbj, svar)) break;
				assert(!Pbj->upper);
				lj = SAT_lit(aj, 1);
				if (LA_is_hint(lj)) break;
				LA_hint(lj, lit);
				if (j + 1 == LA_bound_ranking_n) break;
				UPDATE_INDEX(j, aj, Pbj, j + 1);
			}
		}
		if (rank > 0) {
			UPDATE_INDEX(j, aj, Pbj, rank - 1);
			/* skip svar >= l with l > b */
			while (1) {
				if (!VAR_INDEX(Pbj, svar)) return;
				if (Pbj->upper) break;
				if (j == 0) return;
				UPDATE_INDEX(j, aj, Pbj, j - 1);
			}
			/* skip svar <= u with u >= b */
			while (1) {
				if (!VAR_INDEX(Pbj, svar)) return;
				if (LAdelta_hw_less(&Pbj->delta, &bound.delta)) break;
				if (j == 0) return;
				UPDATE_INDEX(j, aj, Pbj, j - 1);
			}
			/* hint {!(svar <= u) | u < b} */
			while (1) {
				if (!VAR_INDEX(Pbj, svar)) return;
				assert(LAdelta_hw_less(&Pbj->delta, &bound.delta));
				lj = SAT_lit(aj, 0);
				if (LA_is_hint(lj)) break;
				LA_hint(lj, lit);
				if (j == 0) return;
				UPDATE_INDEX(j, aj, Pbj, j - 1);
			}
		}
	} else if (
		bound.upper &&
		!SAT_lit_pol(lit)) { /* CASE 3 - lit : !(svar <= b) == svar > b
         hints: { !(svar <= U) | U <= b } + {svar >= L | L <= b} */
		if (rank > 0) {
			UPDATE_INDEX(j, aj, Pbj, rank - 1);
			/* hint: { -A | A == svar <= U and U <= b and !LA_hinted[-A] } */
			while (1) {
				if (!VAR_INDEX(Pbj, svar)) break;
				lj = SAT_lit(aj, 0);
				if (LA_is_hint(lj)) break;
				LA_hint(lj, lit);
				if (j == 0) break;
				UPDATE_INDEX(j, aj, Pbj, j - 1);
			}
		}
		if (rank + 1 < LA_bound_ranking_n) {
			UPDATE_INDEX(j, aj, Pbj, rank + 1);
			/* skip: { A | A == svar <= U and U >= b } */
			while (1) {
				if (!VAR_INDEX(Pbj, svar)) return;
				if (!Pbj->upper) break;
				if (j + 1 == LA_bound_ranking_n) return;
				UPDATE_INDEX(j, aj, Pbj, j + 1);
			}
			assert(!Pbj->upper);
			/* skip: { A | A == svar >= L and L > b and !LA_hinted[+A]} */
			while (1) {
				if (!VAR_INDEX(Pbj, svar)) return;
				lj = SAT_lit(aj, 1);
				if (LA_is_hint(lj)) return;
				if (!LAdelta_hw_less(&bound.delta, &Pbj->delta)) break;
				if (j + 1 == LA_bound_ranking_n) return;
				UPDATE_INDEX(j, aj, Pbj, j + 1);
			}
			/* hint: { +A | A == svar >= l and b >= l and !LA_hinted[+A]} */
			while (1) {
				LA_hint(lj, lit);
				if (j + 1 == LA_bound_ranking_n) return;
				UPDATE_INDEX(j, aj, Pbj, j + 1);
				if (!VAR_INDEX(Pbj, svar)) return;
				assert(!Pbj->upper);
				assert(!LAdelta_hw_less(&bound.delta, &Pbj->delta));
				lj = SAT_lit(aj, 1);
				if (LA_is_hint(lj)) return;
			}
		}
	} else { /* CASE 4 - lit : !(svar >= b)
          hints: { svar <= U | b <= U } + { !svar >= L | b < L} */
		assert(!bound.upper && !SAT_lit_pol(lit));
		if (rank > 0) {
			UPDATE_INDEX(j, aj, Pbj, rank - 1);
			/* hint: { -A | A == svar >= L && b <= L } */
			while (1) {
				if (!VAR_INDEX(Pbj, svar)) return;
				if (Pbj->upper) break;
				lj = SAT_lit(aj, 0);
				if (LA_is_hint(lj)) break;
				LA_hint(lj, lit);
				if (j == 0) return;
				UPDATE_INDEX(j, aj, Pbj, j - 1);
			}
			/* skip: { A | A == svar >= L && LA_hinted(-A) } */
			while (1) {
				assert(VAR_INDEX(Pbj, svar));
				if (Pbj->upper) break;
				assert(LA_is_hint(lj));
				if (j == 0) return;
				UPDATE_INDEX(j, aj, Pbj, j - 1);
				if (!VAR_INDEX(Pbj, svar)) return;
				lj = SAT_lit(aj, 0);
			}
			/* skip: { A | A == svar <= U && b <= U && LA_hinted(A) */
			while (1) {
				if (LAdelta_hw_less(&Pbj->delta, &bound.delta)) return;
				lj = SAT_lit(aj, 1);
				if (!LA_is_hint(lj)) break;
				assert(Pbj->upper);
				if (j == 0) return;
				UPDATE_INDEX(j, aj, Pbj, j - 1);
				if (!VAR_INDEX(Pbj, svar)) return;
			}
			/* hint: { A | A == svar <= u && b <= u */
			while (1) {
				assert(Pbj->upper);
				assert(!LAdelta_hw_less(&Pbj->delta, &bound.delta));
				assert(VAR_INDEX(Pbj, svar));
				assert(!LA_is_hint(lj));
				LA_hint(lj, lit);
				if (j == 0) return;
				UPDATE_INDEX(j, aj, Pbj, j - 1);
				if (!VAR_INDEX(Pbj, svar)) return;
				if (LAdelta_hw_less(&Pbj->delta, &bound.delta)) return;
				lj = SAT_lit(aj, 1);
			}
		}
	}
}

#undef UPDATE_INDEX
#undef VAR_INDEX

void
LA_hw_hint_explain(Tlit lit)
{
	stack_push(veriT_conflict, hint_LA_cause(lit));
	stack_push(veriT_conflict, lit_neg(lit));
}

static void
LA_bound_rank(void)
{
	unsigned i;

	if (LA_bound_ranking) {
		free(LA_bound_ranking);
		LA_bound_ranking = 0;
	}

	LA_bound_ranking = LA_hw_sort_bounds(&LA_bound_ranking_n);
	for (i = 0; i < LA_bound_ranking_n; ++i)
		bounds->data[LA_bound_ranking[i]].rank = i;
#ifdef DEBUG_LA_PROPAGATE
	my_DAG_message("bound ranking:\n");
	for (i = 0; i < LA_bound_ranking_n; ++i) {
		printf("atom %i: ", LA_bound_ranking[i]);
		bound_print(bounds->data[LA_bound_ranking[i]]);
		printf("\n");
	}
#endif
}

/*
  --------------------------------------------------------------
  Backtracking: Definitions
  --------------------------------------------------------------
*/

static void
backtrack_hint(Tlit lit)
{
	Tlit* Plit = (Tlit*)undo_push(LA_HINT);
	*Plit = lit;
}

static void
LA_hook_hint(Tlit* Plit)
{
	assert(bounds->data[lit_var(*Plit)].hint & (1u << lit_pol(*Plit)));
	bounds->data[lit_var(*Plit)].hint &= ~(1u << lit_pol(*Plit));
}

static void
backtrack_init(void)
{
	undo_set_hook(LA_HINT, (Tundo_hook)LA_hook_hint, sizeof(Tlit));
}

static void
backtrack_done(void)
{
	undo_unset_hook(LA_HINT);
}
