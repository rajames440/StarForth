/*
  --------------------------------------------------------------
  let operator removing in terms and formulas
  --------------------------------------------------------------
*/

/* TODO: eliminate x = t, where x is a constant with only this occurrence */

#include "pre/rare-symb.h"

#include "symbolic/DAG-subst.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/recursion.h"
#include "symbolic/veriT-DAG.h"
#include "utils/general.h"
#include "utils/stack.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* #define DEBUG_RARE_SYMB */

static Tstack_symb rare_symb_stack;

typedef struct Tsymb_ext
{
	unsigned count;
	union
	{
		TDAG DAG[2];
		Tstack_DAG stack_DAG;
	} DAGs;
} Tsymb_ext;
static Tsymb_ext* symb_ext;

#define symb_count(s) (symb_ext[s].count)
#define symb_eqs(s) (symb_ext[s].DAGs.DAG)
#define symb_DAG_stack(s) (symb_ext[s].DAGs.stack_DAG)

/*
  --------------------------------------------------------------
  Equalities with rare symbols
  --------------------------------------------------------------
*/

/* PF is a DAG a candidate for a rare constant? */
#define RARE_CONST_CANDIDATE(src) \
	(!DAG_arity(src) && \
	 !(DAG_symb_type(DAG_symb(src)) & (SYMB_INTERPRETED | SYMB_PREDEFINED)))

/* PF is a DAG a rare constant? */
#define RARE_CONST(src) \
	(RARE_CONST_CANDIDATE(src) && symb_count(DAG_symb(src)) == 2)

/* PF both members of equality are rare constants */
#define BOTH_RARE(eq) (RARE_CONST(DAG_arg0(eq)) && RARE_CONST(DAG_arg1(eq)))

/* PF get members of equality which is not a rare constant */
#define GET_NOT_RARE(eq) \
	(RARE_CONST(DAG_arg1(eq)) ? DAG_arg0(eq) : DAG_arg1(eq))

static void count_symb_occ2(TDAG DAG);

static inline void
count_symb_occ2_DAG(TDAG DAG)
{
	if (RARE_CONST_CANDIDATE(DAG))
		symb_count(DAG_symb(DAG))++;
	else
		count_symb_occ2(DAG);
}

/**
   \brief Sets the symb_count value of every uninterpreted constant a to
   1 if it occurs once in an equality a = t,
   2 if it occurs in two equalities a = t,
   >3 if it occurs elsewhere or in more equalities
   \param src the DAG */
static void
count_symb_occ2(TDAG src)
{
	unsigned i;
	if (RARE_CONST_CANDIDATE(src)) symb_count(DAG_symb(src)) += 3;
	if (DAG_tmp_bool[src]) return;
	DAG_tmp_bool[src] = 1;
	assert(!binder(DAG_symb(src)));
	if (DAG_symb(src) != PREDICATE_EQ)
		for (i = 0; i < DAG_arity(src); i++) count_symb_occ2(DAG_arg(src, i));
	else {
		count_symb_occ2_DAG(DAG_arg0(src));
		count_symb_occ2_DAG(DAG_arg1(src));
	}
}

static void collect_rare_symb2(TDAG);

static inline void
collect_rare_symb2_DAG(TDAG src, TDAG DAG)
{
	if (RARE_CONST(DAG)) {
		Tsymb symb = DAG_symb(DAG);
		assert(!symb_eqs(symb)[1]);
		if (symb_eqs(symb)[0])
			symb_eqs(symb)[1] = src;
		else {
			symb_eqs(symb)[0] = src;
			stack_push(rare_symb_stack, symb);
		}
	} else
		collect_rare_symb2(DAG);
}

static void
collect_rare_symb2(TDAG src)
{
	unsigned i;
	if (RARE_CONST_CANDIDATE(src)) symb_count(DAG_symb(src)) = 0;
	if (!DAG_tmp_bool[src]) return;
	DAG_tmp_bool[src] = 0;
	if (DAG_symb(src) != PREDICATE_EQ)
		for (i = 0; i < DAG_arity(src); i++) collect_rare_symb2(DAG_arg(src, i));
	else {
		collect_rare_symb2_DAG(src, DAG_arg0(src));
		collect_rare_symb2_DAG(src, DAG_arg1(src));
	}
}

static Tstack_DAG stack_eq = NULL;
/**
   \brief collects all equalities along a path of rare symbols
   \param eq an equality t = x1 where x1 is a rare symbol and t is not
   \return the conjunctions of axioms
   t != x1 or x1 != x2 or ... xn != t' where only one eq is not negated
   \remark all collected equalities are flagged */
static TDAG
follow(TDAG eq)
{
	unsigned i, j;
	TDAG result;
	TDAG first, next;
	TDAG new_eq;
	TDAG* clauses = NULL;
	assert(stack_eq && !stack_eq->size);
	first = GET_NOT_RARE(eq);
	next = (first == DAG_arg0(eq)) ? DAG_arg1(eq) : DAG_arg0(eq);
	stack_push(stack_eq, eq);
	assert(!DAG_tmp_bool[eq]);
	DAG_tmp_bool[eq] = 1;
	assert(RARE_CONST(next));
	do {
		TDAG eq1, eq2;
		assert(symb_count(DAG_symb(next)) == 2);
		assert(symb_eqs(DAG_symb(next))[1]);
		eq1 = symb_eqs(DAG_symb(next))[0];
		eq2 = symb_eqs(DAG_symb(next))[1];
		assert(DAG_tmp_bool[eq1] + DAG_tmp_bool[eq2] == 1);
		if (DAG_tmp_bool[eq1]) {
			stack_push(stack_eq, eq2);
			DAG_tmp_bool[eq2] = 1;
			assert(DAG_arg0(eq2) == next || DAG_arg1(eq2) == next);
			next = (DAG_arg0(eq2) == next) ? DAG_arg1(eq2) : DAG_arg0(eq2);
		} else {
			stack_push(stack_eq, eq1);
			DAG_tmp_bool[eq1] = 1;
			assert(DAG_arg0(eq1) == next || DAG_arg1(eq1) == next);
			next = (DAG_arg0(eq1) == next) ? DAG_arg1(eq1) : DAG_arg0(eq1);
		}
	} while (RARE_CONST(next));
	new_eq = DAG_dup(DAG_eq(first, next));
	stack_push(stack_eq, new_eq);

	MY_MALLOC(clauses, stack_eq->size * sizeof(TDAG));
	for (i = 0; i < stack_eq->size; ++i) {
		TDAG* PDAG;
		MY_MALLOC(PDAG, stack_eq->size * sizeof(TDAG));
		for (j = 0; j < stack_eq->size; ++j)
			PDAG[j] =
				(j == i) ? stack_get(stack_eq, j) : DAG_not(stack_get(stack_eq, j));
		clauses[i] = DAG_new(CONNECTOR_OR, stack_eq->size, PDAG);
	}
	result = DAG_new(CONNECTOR_AND, stack_eq->size, clauses);
	DAG_free(new_eq);
	stack_reset(stack_eq);
	return result;
}

/**
   assume x -- a zero-arity uninterpreted function (an uninterpreted
   constant) -- appears only in two atoms in phi
   phi(x=t1, x=t2)
   then any conflict involving x will contain both atoms.
   Introduces then axioms to ensure that
   either at least two of the eqs in x=t1, x=t2, t1=t2 are false,
   or all are true.
   Works also with chains of seldom (twice) used constants

   Assume x_1 ... x_n are rare symbols appearing in a formula
   alpha = F(x_1 = t, x_1 = x_2, ..., x_{n-1} = x_n, x_n = t')
   then alpha is equi-satisfiable to beta (as defined as follow) IF
   the domain contains more than 3 elements.
   beta = F(p_0, ... p_{n-1}, p_n)
   with p_0: x_1 = t
   p_1: x_1 = x_2
   ...
   p_{n-1}: x_{n-1} = x_n
   p_n: x_n = t'
   augmented with clauses that state that if n+1 elements in the set
   {p_0, ... p_n, t=t'} are true, then so are all.

   Proof:
   A model for alpha can be easily rewritten to a model to beta.
   Given a model for beta, it is possible to mimick the values of
   p_0, ... p_n only if there are at least three elements in the domain */
static TDAG
rare_eq(TDAG src)
{
	TDAG dest;
	Tstack_DAG transitivity_axioms = NULL;
	Tstack_DAG definition_stack = NULL;
	unsigned i, j;
	Tsymb symb;

	DAG_tmp_reserve();
	stack_INIT(rare_symb_stack);
	count_symb_occ2(src);
	collect_rare_symb2(src);
	DAG_tmp_release();

	if (!stack_size(rare_symb_stack)) {
		stack_free(rare_symb_stack);
		return DAG_dup(src);
	}

	DAG_tmp_reserve();
	stack_INIT(transitivity_axioms);
	stack_INIT(stack_eq);
	stack_INIT(definition_stack);
	/* PF for all rare symbols, check their rare eqs and add transitivity axioms
     Mark all visited eqs and collect them */
	for (i = 0; i < rare_symb_stack->size; ++i)
		for (j = 0; j < 2; j++) {
			TDAG eq;
			symb = rare_symb_stack->data[i];
			eq = symb_eqs(symb)[j];
			assert(symb_count(symb) == 2 && eq);
			stack_push(definition_stack, eq);
			if (!DAG_tmp_bool[eq] && !BOTH_RARE(eq))
				stack_push(transitivity_axioms, follow(eq));
		}
	for (i = 0; i < rare_symb_stack->size; ++i) {
		symb = rare_symb_stack->data[i];
		symb_count(symb) = 0;
		symb_eqs(symb)[0] = DAG_NULL;
		symb_eqs(symb)[1] = DAG_NULL;
	}

	stack_push(transitivity_axioms, src);
	src = DAG_dup(DAG_new_stack(CONNECTOR_AND, transitivity_axioms));
	stack_free(transitivity_axioms);
	stack_free(rare_symb_stack);

	/* PF eliminate equations not occurring in any above axioms
     Basically those in a cycle of rare symbols */
	for (i = 0, j = 0; i < definition_stack->size; i++)
		if (DAG_tmp_bool[definition_stack->data[i]]) {
			definition_stack->data[j++] = definition_stack->data[i];
			DAG_tmp_bool[definition_stack->data[i]] = 0;
		}
	definition_stack->size = j;
	DAG_tmp_release();

	DAG_tmp_reserve();
	for (i = 0; i < definition_stack->size; i++) {
		TDAG DAG = definition_stack->data[i];
		/* PF do not introduce axioms for cycles of rare symbols */
		if (!DAG_tmp_DAG[DAG]) {
			TDAG tmp = DAG_new_nullary(DAG_symb_const(SORT_BOOLEAN));
			assert(DAG_sort(DAG) == SORT_BOOLEAN);
			DAG_tmp_DAG[DAG] = tmp;
		}
	}
	DAG_tmp_subst(src);
	dest = DAG_dup(DAG_tmp_DAG[src]);
	DAG_tmp_reset_DAG(src);
	DAG_free(src);
	for (i = 0; i < definition_stack->size; i++)
		DAG_tmp_reset_DAG(definition_stack->data[i]);
	DAG_tmp_release();
	stack_free(definition_stack);
	stack_free(stack_eq);
	return dest;
}

/*
  --------------------------------------------------------------
  Ackermann
  --------------------------------------------------------------
*/

/* TODO: INCREMENTAL??? */

static void
count_symb_occ(TDAG src)
{
	unsigned i;
	if (DAG_tmp_bool[src]) return;
	DAG_tmp_bool[src] = 1;
	assert(!binder(DAG_symb(src)));
	if (
		DAG_arity(src) &&
		!(DAG_symb_type(DAG_symb(src)) & (SYMB_PREDICATE | SYMB_INTERPRETED)))
		symb_count(DAG_symb(src))++;
	for (i = 0; i < DAG_arity(src); i++) count_symb_occ(DAG_arg(src, i));
}

static void
collect_rare_symb(TDAG src)
{
	unsigned i;
	if (!DAG_tmp_bool[src]) return;
	DAG_tmp_bool[src] = 0;
	if (
		DAG_arity(src) &&
		!(DAG_symb_type(DAG_symb(src)) & (SYMB_PREDICATE | SYMB_INTERPRETED))) {
		if (
			symb_count(DAG_symb(src)) > 1 &&
			(symb_count(DAG_symb(src)) == 2 ||
			 symb_count(DAG_symb(src)) <= DAG_arity(src))) {
			if (!symb_DAG_stack(DAG_symb(src))) {
				stack_INIT(symb_DAG_stack(DAG_symb(src)));
				stack_push(rare_symb_stack, DAG_symb(src));
			}
			stack_push(symb_DAG_stack(DAG_symb(src)), src);
		} else
			symb_count(DAG_symb(src)) = 0;
	}
	for (i = 0; i < DAG_arity(src); i++) collect_rare_symb(DAG_arg(src, i));
}

static Tstack_DAG
ackermann_axiom(Tstack_DAG ackermann, TDAG DAG1, TDAG DAG2)
{
	unsigned i;
	Tstack_DAG clause = NULL;
	assert(DAG1 != DAG2);
	assert(DAG_arity(DAG1) == DAG_arity(DAG2));
	stack_INIT(clause);
	for (i = 0; i < DAG_arity(DAG1); i++)
		if (DAG_arg(DAG1, i) != DAG_arg(DAG2, i)) {
			TDAG DAG;
			if (DAG_sort(DAG_arg(DAG1, i)) == SORT_BOOLEAN)
				DAG = DAG_equiv(DAG_arg(DAG1, i), DAG_arg(DAG2, i));
			else
				DAG = DAG_eq(DAG_arg(DAG1, i), DAG_arg(DAG2, i));
			stack_push(clause, DAG_not(DAG));
		}
	assert(clause->size);
	if (DAG_sort(DAG1) == SORT_BOOLEAN) {
		stack_push(clause, DAG1);
		stack_push(clause, DAG_not(DAG2));
		stack_push(ackermann, DAG_dup(DAG_new_stack(CONNECTOR_OR, clause)));
		stack_dec(clause);
		stack_dec(clause);
		stack_push(clause, DAG_not(DAG1));
		stack_push(clause, DAG2);
	} else
		stack_push(clause, DAG_eq(DAG1, DAG2));
	stack_push(ackermann, DAG_dup(DAG_new_stack(CONNECTOR_OR, clause)));
	stack_free(clause);
	return ackermann;
}

static inline Tstack_DAG
ackermannize_occurences(Tstack_DAG ackermann, Tstack_DAG occurrences)
{
	unsigned i, j;
	for (i = 0; i < occurrences->size; i++)
		for (j = i + 1; j < occurrences->size; j++)
			ackermann =
				ackermann_axiom(ackermann, occurrences->data[i], occurrences->data[j]);
	return ackermann;
}

/**
   ackermannize f if
   1. f is uninterpreted
   2. the number of distinct occurrences (i.e. with different args) of f
   is smaller than its arity */
static TDAG
ackermannize(TDAG src)
{
	TDAG dest;
	Tstack_DAG result = NULL;
	Tstack_DAG ackermann_list = NULL;
	Tstack_DAG definition_list = NULL;
	unsigned i, j;
	/* PF get candidates for ackermannization */
	assert(!rare_symb_stack);
	stack_INIT(rare_symb_stack);
	DAG_tmp_reserve();
	count_symb_occ(src);
	collect_rare_symb(src);
	DAG_tmp_release();
	if (!rare_symb_stack->size) {
		stack_free(rare_symb_stack);
		return DAG_dup(src);
	}
	stack_INIT(definition_list);
	stack_INIT(ackermann_list);
	/* PF create all ackermannization axioms and collect all terms to replace */
	for (i = 0; i < rare_symb_stack->size; ++i) {
		Tsymb symb = rare_symb_stack->data[i];
		Tstack_DAG DAG_stack = symb_DAG_stack(symb);
		ackermann_list = ackermannize_occurences(ackermann_list, DAG_stack);
		for (j = 0; j < DAG_stack->size; j++)
			stack_push(definition_list, DAG_stack->data[j]);
		stack_free(symb_DAG_stack(symb));
		symb_count(symb) = 0;
	}
	stack_free(rare_symb_stack);

	DAG_tmp_reserve();
	/* PF get all terms that should be replaced
     generate a list of definitions cst = Term */
	for (i = 0; i < definition_list->size; i++) {
		TDAG tmp;
		tmp = DAG_new_nullary(DAG_symb_const(DAG_sort(definition_list->data[i])));
		assert(!DAG_tmp_DAG[definition_list->data[i]]);
		DAG_tmp_DAG[definition_list->data[i]] = tmp;
		/*
        if (DAG_sort(DAG) == SORT_BOOLEAN)
        result = list_add(result, DAG_equiv(DAG, DAG_tmp));
        else
        result = list_add(result, DAG_eq(DAG, DAG_tmp));
      */
	}

	stack_INIT(result);
	/* PF replace by definitions in ackermannization */
	for (i = 0; i < ackermann_list->size; i++) {
		TDAG DAG = ackermann_list->data[i];
		DAG_tmp_subst(DAG);
		stack_push(result, DAG_tmp_DAG[DAG]);
	}
	/* PF replace by definitions in formula */
	DAG_tmp_subst(src);
	stack_push(result, DAG_tmp_DAG[src]);
	dest = DAG_dup(DAG_new_stack(CONNECTOR_AND, result));
	stack_free(result);

	/* PF tidy everything */
	DAG_tmp_reset_DAG(src);
	for (i = 0; i < ackermann_list->size; i++) {
		TDAG DAG = ackermann_list->data[i];
		DAG_tmp_reset_DAG(DAG);
		DAG_free(DAG);
	}
	stack_free(ackermann_list);

	for (i = 0; i < definition_list->size; i++)
		DAG_tmp_reset_DAG(definition_list->data[i]);
	stack_free(definition_list);
	DAG_tmp_release();
	return dest;
}

/*
  --------------------------------------------------------------
  A = A elimination
  --------------------------------------------------------------
*/

static TDAG
trivial_eq_elim_aux(TDAG src)
{
	if (DAG_symb(src) != PREDICATE_EQ || DAG_arg0(src) != DAG_arg1(src))
		return src;
	DAG_free(src);
	return DAG_dup(DAG_TRUE);
}

static TDAG
trivial_eq_elim(TDAG src)
{
	return structural_recursion(src, trivial_eq_elim_aux);
}

/*
  --------------------------------------------------------------
  Interface functions
  --------------------------------------------------------------
*/

TDAG
rare_symb(TDAG src)
{
	TDAG dest;
	unsigned symb_stack_size = DAG_symb_stack->size;
	assert(DAG_sort(src) == SORT_BOOLEAN);
	DAG_dup(src);
	if (DAG_quant(src)) return src;
	/* First make sure no equality A = A exists */
	MY_MALLOC(symb_ext, symb_stack_size * sizeof(Tsymb_ext));
	memset(symb_ext, 0, symb_stack_size * sizeof(Tsymb_ext));
	dest = trivial_eq_elim(src);
	DAG_free(src);
	src = dest;
#ifdef DEBUG_RARE_SYMB
	my_DAG_message("Before rare_symb: %D\n", src);
#endif
	dest = ackermannize(src);
	DAG_free(src);
	src = dest;

#ifdef DEBUG
	{
		unsigned i;
		for (i = 0; i < symb_stack_size; i++) {
			assert(!symb_ext[i].count);
			assert(!symb_ext[i].DAGs.DAG[0]);
			assert(!symb_ext[i].DAGs.DAG[1]);
		}
	}
#endif
	free(symb_ext);
	symb_stack_size = DAG_symb_stack->size;
	MY_MALLOC(symb_ext, symb_stack_size * sizeof(Tsymb_ext));
	memset(symb_ext, 0, symb_stack_size * sizeof(Tsymb_ext));

#ifdef DEBUG_RARE_SYMB
	my_DAG_message("After ackermann: %D\n", src);
#endif
	dest = rare_eq(src);
	DAG_free(src);
	src = dest;
#ifdef DEBUG_RARE_SYMB
	my_DAG_message("After rare_symb: %D\n", src);
#endif
#ifdef DEBUG
	{
		unsigned i;
		for (i = 0; i < symb_stack_size; i++) {
			assert(!symb_ext[i].count);
			assert(!symb_ext[i].DAGs.DAG[0]);
			assert(!symb_ext[i].DAGs.DAG[1]);
		}
	}
#endif
	free(symb_ext);
	return src;
}
