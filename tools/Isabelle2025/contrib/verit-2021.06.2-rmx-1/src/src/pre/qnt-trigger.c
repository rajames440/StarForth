/* ------------------------------------------------------------
 *  qnt-trigger.c
 *
 *
 *  This file provides functions that compute triggers for quantified
 *  formulas.
 *
 *  Created by David Deharbe on 2/10/10.
 *
 ------------------------------------------------------------ */
#include <assert.h>

/* #define DEBUG_TRIGGER */

#include "symbolic/DAG-flag.h"
#include "symbolic/DAG-ptr.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/veriT-DAG.h"
#include "utils/general.h"
#ifdef DEBUG_TRIGGER
#include "symbolic/DAG-print.h"
#include "symbolic/dbg-trigger.h"

#include <stdio.h>
#endif

#include "pre/qnt-trigger.h"

#define symb_misc(s) DAG_symb_misc(s)
#define symb_set_misc(s, v) DAG_symb_set_misc(s, v)

/*
  Some traversed sub-DAGs have Pflag set. The misc flag of some symbols are
  used in the processing (but reset on function return).
*/
static void
trigger_prepare_aux(TDAG DAG)
{
	unsigned i, j;
	Tstack_symb symbols = NULL, symbols2;

	if (DAG_flag(DAG)) return;

	DAG_flag_set(DAG, 1);
	assert(!DAG_Pflag(DAG));

	if (DAG_symb_type(DAG_symb(DAG)) & SYMB_QUANTIFIER) return;

	if (DAG_symb_type(DAG_symb(DAG)) & SYMB_VARIABLE) {
		stack_INIT(symbols);
		stack_push(symbols, DAG_symb(DAG));
		DAG_Pflag_set(DAG, symbols);
		return;
	}

	for (i = 0; i < DAG_arity(DAG); ++i) trigger_prepare_aux(DAG_arg(DAG, i));

	if (DAG_symb_type(DAG_symb(DAG)) &
      (SYMB_BOOLEAN_CONSTANT | SYMB_BOOLEAN_CONNECTOR)/* ||
                                                         DAG_symb(DAG) == PREDICATE_EQ*/)
		return;

	stack_INIT(symbols);
	/* Build union of free variables of DAG subterms */
	for (i = 0; i < DAG_arity(DAG); ++i)
		if ((symbols2 = DAG_Pflag(DAG_arg(DAG, i))))
			for (j = 0; j < symbols2->size; j++)
				if (!DAG_symb_misc(symbols2->data[j])) {
					symb_set_misc(symbols2->data[j], 1);
					stack_push(symbols, symbols2->data[j]);
				}
	if (!symbols->size) {
		stack_free(symbols);
		return;
	}
	for (i = 0; i < symbols->size; i++) symb_set_misc(symbols->data[i], 0);
	DAG_Pflag_set(DAG, symbols);
}

/*
  Computes the set of free variables of each sub-term (not
  sub-formula!) of DAG that has no quantifier on the path to the
  outermost quantifier.
  Store this set as a stack of Tsymb in the Pflag attribute of
  the sub-term.
  Use trigger_cleanup to free the space used by these lists.
*/
static void
trigger_prepare(TDAG DAG)
{
	assert(DAG_symb_type(DAG_symb(DAG)) & SYMB_QUANTIFIER);
	trigger_prepare_aux(DAG_arg_last(DAG));
	DAG_reset_flag(DAG_arg_last(DAG));

	return;
}

static void
trigger_cleanup_aux(TDAG DAG)
{
	unsigned i;

	if (DAG_flag(DAG) == 1) return;
	DAG_flag_set(DAG, 1);
	if (DAG_symb_type(DAG_symb(DAG)) & SYMB_QUANTIFIER) return;

	for (i = 0; i < DAG_arity(DAG); ++i) trigger_cleanup_aux(DAG_arg(DAG, i));

	if (DAG_symb_type(DAG_symb(DAG)) &
      (SYMB_BOOLEAN_CONSTANT | SYMB_BOOLEAN_CONNECTOR)/* ||
                                                         DAG_symb(DAG) == PREDICATE_EQ*/)
		return;
	if (DAG_Pflag(DAG)) {
		Tstack_symb symbols = DAG_Pflag(DAG);
		stack_free(symbols);
		DAG_Pflag_set(DAG, NULL);
	}
}

/*
  Frees the Pflag attributes computed by trigger_prepare.
  Uses flag attribute of sub-DAGs for top-down traversal.
*/
static void
trigger_cleanup(TDAG DAG)
{
	assert(DAG_symb_type(DAG_symb(DAG)) & SYMB_QUANTIFIER);
	trigger_cleanup_aux(DAG_arg_last(DAG));
	DAG_reset_flag(DAG_arg_last(DAG));
}

/*   Uni-trigger                                                */

/*
  Consider the quantified formula:
  Q x1, ... xn . M (x1, ... xn)
  A uni-trigger t is
  1) a subterm of M,
  2) no quantifier occurs between the root of t and Q
  3) the free variables of t are x1, ... xn
  4) t is not a variable
  5) t has no sub-term that satisfy conditions 1-4
  6) there is no other sub-term that matches t and is greater than t.

  A sub-term that satisfy conditions 1-5 is said to be a candidate
  (see function unitrigger_candidates).

*/

#define VISITED 1
#define CANDIDATE 2

/*
  Returns VISITED if DAG has been visited and is not a candidate trigger
  (or a sub-term of DAG).
  Returns VISITED | CANDIDATE if DAG has been visited and is a
  candidate trigger.
  Adds DAG to the list pointed by Plist if DAG is a candidate.

  Side-effects: Top-down traversal sets DAG_flag(TDAG) up to leaves.  The
  misc flag of some symbols are used in the processing (but reset when
  function returns).
*/
static int
unitrigger_candidates_aux(TDAG f, TDAG DAG, Tstack_DAG* Pcandidates)
{
	unsigned i;
	int flag;
	Tstack_symb symbols;

	if (DAG_flag(DAG)) return DAG_flag(DAG);

	DAG_flag(DAG) |= VISITED;

	if (DAG_symb_type(DAG_symb(DAG)) & (SYMB_QUANTIFIER | SYMB_VARIABLE))
		return VISITED;

	for (i = 0; i < DAG_arity(DAG); ++i)
		DAG_flag(DAG) |= unitrigger_candidates_aux(f, DAG_arg(DAG, i), Pcandidates);

	if (DAG_symb_type(DAG_symb(DAG)) &
      (SYMB_BOOLEAN_CONSTANT | SYMB_BOOLEAN_CONNECTOR | SYMB_INTEGER)/* ||
                                                                        DAG_symb(DAG) == PREDICATE_EQ*/)
		return VISITED;

	if (DAG_flag(DAG) & CANDIDATE) return DAG_flag(DAG);

	/* No sub-term is candidate, now check if DAG is candidate. */
	/* For each son, Pflag stores the list of free variables.
     Compute the union of such lists. Use the misc field to
     avoid duplicates. */

	/* Visit free variables of DAG subterms, mark symb */
	if ((symbols = DAG_Pflag(DAG)))
		for (i = 0; i < symbols->size; i++) symb_set_misc(symbols->data[i], 1);

	/* If all the variables of f are in the union, then DAG is candidate. */
	flag = 1;
	for (i = 0; i < DAG_arity(f) - 1u; ++i) {
		Tsymb symb = DAG_symb(DAG_arg(f, i));
		if (!symb_misc(symb)) flag = 0;
		symb_set_misc(symb, 0);
	}
	if (flag) {
		DAG_flag(DAG) |= CANDIDATE;
		stack_push(*Pcandidates, DAG);
	}
	return DAG_flag(DAG);
}

/*
  Computes the list of terms of DAG that are potential unitriggers
  of f, and stores it in the address pointed to by Plist.

  The traversal recurses down f, halting on quantified sub-formulas.

  See unitrigger_candidates_aux for additional details.

  Assumes that the Pflag attribute of the sub-terms of DAG stores
  the list of its free variables (see trigger_prepare).

*/
static void
unitrigger_candidates(TDAG f, TDAG DAG, Tstack_DAG* Pcandidates)
{
	unitrigger_candidates_aux(f, DAG, Pcandidates);
	DAG_reset_flag(DAG);
}

static bool
unitrigger_match_aux(TDAG DAG1, TDAG DAG2)
{
	if (DAG_flag(DAG1)) {
		if (DAG_symb_type(DAG_symb(DAG1)) & SYMB_VARIABLE)
			return DAG_of_ptr(DAG_Pflag(DAG1)) == DAG2;
		return true;
	}
	DAG_flag_set(DAG1, 1);
	assert(!DAG_Pflag(DAG1));
	if (DAG_symb_type(DAG_symb(DAG1)) & SYMB_VARIABLE) {
		DAG_Pflag_set(DAG1, DAG_ptr_of(DAG2));
		return true;
	} else if (DAG_symb(DAG1) == DAG_symb(DAG2)) {
		unsigned i = 0;
		while (i < DAG_arity(DAG1) &&
					 unitrigger_match_aux(DAG_arg(DAG1, i), DAG_arg(DAG2, i)))
			++i;
		return i == DAG_arity(DAG1);
	} else
		return false;
}

/*
  Checks if DAG1 matches (is more general than) DAG2.
  Top-down traversal uses flag attribute of DAG1's sub-DAGs and
  Pflag attribute of DAGs for quantified variables.
  These side-effects are cleared before the function returns.
*/
static bool
unitrigger_match(TDAG* vars, unsigned n, TDAG DAG1, TDAG DAG2)
{
	unsigned i;
	bool res;
	res = unitrigger_match_aux(DAG1, DAG2);
	for (i = 0; i < n; ++i) DAG_Pflag_set(vars[i], NULL);
	DAG_reset_flag(DAG1);
	return res;
}

/*   Multi-triggers                                             */

/*
  Consider the quantified formula:
  Q x1, ... xn . M (x1, ... xn)
  A multi-trigger t is
  1) is a set {t1... tk} of subterms of M,
  2) no quantifier occurs between the root of each ti and Q
  3) the free variables of t1...tk are {x1...xn} and the set of free variables
  of any subterm of any ti is strictly included in {x1...xn}
  4) no ti is a variable
  5) no ti has a sub-term that satisfy conditions 1-4
  6) there is no other sub-term that matches ti and is greater than ti.

  A sub-term that satisfy conditions 1-5 is said to be a candidate
  (see function unitrigger_candidates).
*/

/*
  t is a multitrigger element of DAG if
  1) t is a subterm of DAG,
  2) no quantifier occurs between the root of DAG and t
  3) t is not a variable, not a quantified formula, not an equality, not
  a boolean operation
  4) no sub-term of t has the same free variables as t.
*/

/*
  Gathers in Plist the multitrigger elements of DAG
  Assumes that if DAG is not an equality or a boolean operation,
  the free variables of DAG are stored in Pflag.
  Assumes that the Pflag attribute of sub-terms contains the list of
  free variables (post-condition of unitrigger_candidates).
  Uses and modifies flag and misc.
*/
static void
multitrigger_elements_aux(TDAG DAG, Tstack_DAG* Pelements)
{
	unsigned i;
	unsigned max;
	Tstack_symb symbols;

	if (DAG_flag(DAG)) return;

	DAG_flag_set(DAG, 1);

	if (DAG_symb_type(DAG_symb(DAG)) & (SYMB_QUANTIFIER | SYMB_VARIABLE)) return;

	for (i = 0; i < DAG_arity(DAG); ++i)
		multitrigger_elements_aux(DAG_arg(DAG, i), Pelements);

	if (/*DAG_symb(DAG) == PREDICATE_EQ ||
       */
			DAG_symb_type(DAG_symb(DAG)) &
			(SYMB_INTEGER | SYMB_BOOLEAN_CONNECTOR | SYMB_BOOLEAN_CONSTANT))
		return;

	for (max = i = 0; i < DAG_arity(DAG); ++i) {
		if ((unsigned)DAG_misc(DAG_arg(DAG, i)) > max)
			max = (unsigned)DAG_misc(DAG_arg(DAG, i));
	}
	symbols = (Tstack_symb)DAG_Pflag(DAG);
	DAG_misc_set(DAG, (int)(symbols ? symbols->size : 0));
	if ((unsigned)DAG_misc(DAG) > max) stack_push(*Pelements, DAG);
}

static void
multitrigger_elements_cleanup(TDAG DAG)
{
	unsigned i;
	if (DAG_flag(DAG) == 0) return;
	DAG_flag(DAG) = 0;
	DAG_misc_set(DAG, 0);

	if (DAG_symb_type(DAG_symb(DAG)) & (SYMB_QUANTIFIER | SYMB_VARIABLE)) return;

	for (i = 0; i < DAG_arity(DAG); ++i)
		multitrigger_elements_cleanup(DAG_arg(DAG, i));
}

static void
multitrigger_elements(TDAG DAG, Tstack_DAG* Pelements)
{
	assert(DAG_symb_type(DAG_symb(DAG)) & SYMB_QUANTIFIER);
	multitrigger_elements_aux(DAG_arg_last(DAG), Pelements);
	multitrigger_elements_cleanup(DAG_arg_last(DAG));
}

static void
mark_unmarked_variables(TDAG DAG, Tstack_symb* Psys)
{
	unsigned i;
	Tstack_symb symbols = (Tstack_symb)DAG_Pflag(DAG);
	if (!symbols) return;
	for (i = 0; i < symbols->size; i++)
		if (!DAG_symb_misc(symbols->data[i])) {
			symb_set_misc(symbols->data[i], 1);
			stack_push(*Psys, symbols->data[i]);
		}
}

static unsigned
count_unmarked_variables(TDAG DAG)
{
	unsigned i;
	unsigned result = 0;
	Tstack_symb symbols = (Tstack_symb)DAG_Pflag(DAG);
	if (symbols)
		for (i = 0; i < symbols->size; i++)
			result += (symb_misc(symbols->data[i]) == 0);
	return result;
}

static void
build_multitriggers_aux(
	unsigned count, unsigned total, Tstack_DAG elements, Tstack_DAG* Pcandidate,
	Tstack_DAGstack* Presult)
{
	unsigned i, j;
	Tstack_DAG stack;
	unsigned max;

	assert(count < total);
	assert(elements != NULL);

	/* list: triggers with the max number of non-marked free variables */
	max = 0;
	for (i = 0; i < stack_size(elements); ++i) {
		TDAG DAG = stack_get(elements, i);
		DAG_misc_set(DAG, (int)count_unmarked_variables(DAG));
		if ((unsigned)DAG_misc(DAG) > max) max = (unsigned)DAG_misc(DAG);
	}
	stack_INIT(stack);
	for (i = 0; i < stack_size(elements); ++i) {
		TDAG DAG = stack_get(elements, i);
		if ((unsigned)DAG_misc(DAG) == max) stack_push(stack, DAG);
		DAG_misc_set(DAG, 0);
	}

	/* for each such element... */
	assert(stack_size(stack) != 0);
	assert(count + max <= total);
	for (i = 0; i < stack_size(stack); ++i) {
		TDAG DAG = stack_get(stack, i);
		Tstack_symb symbols;
		stack_INIT(symbols);
		/* add element to candidate and mark its free variables */
		mark_unmarked_variables(DAG, &symbols);
		stack_push(*Pcandidate, DAG);
		/* if all variables have been marked, then the candidate is ready */
		if (max + count == total) {
			Tstack_DAG trigger;
			stack_INIT_s(trigger, stack_size(*Pcandidate));
			for (j = 0; j < stack_size(*Pcandidate); ++j)
				stack_push(trigger, DAG_dup(stack_get(*Pcandidate, j)));
			stack_push(*Presult, trigger);
		}
		/* if some variables have not been marked, process recursively to
         add new elements to candidate */
		else if (count > 0) {
			assert(count + max < total);
			build_multitriggers_aux(
				count + max, total, elements, Pcandidate, Presult);
		}
		/* restore state by removing element from candidate and unmarking
         the corresponding variables */
		stack_dec(*Pcandidate);
		for (j = 0; j < symbols->size; j++) symb_set_misc(symbols->data[j], 0);
		stack_free(symbols);
	}
	stack_free(stack);
}

/**
   \brief Build multi-triggers from a list of candidate terms
*/
static void
build_multitriggers(unsigned n, Tstack_DAG elements, Tstack_DAGstack* Pannot)
{
	Tstack_DAG candidates;
	stack_INIT(candidates);
	build_multitriggers_aux(0, n, elements, &candidates, Pannot);
	stack_free(candidates);
}

/*   Triggers                                                   */

Tstack_DAGstack
qnt_triggers(TDAG DAG)
{
	Tstack_DAGstack result;
	Tstack_DAG candidates;
	stack_INIT(candidates);
	assert(
		DAG_symb(DAG) == QUANTIFIER_FORALL || DAG_symb(DAG) == QUANTIFIER_EXISTS);
	trigger_prepare(DAG);
	unitrigger_candidates(DAG, DAG_arg_last(DAG), &candidates);
	/* From the candidates, retain only those that satisfy condition 6. */
	if (stack_size(candidates) != 0) {
		unsigned i;
		stack_INIT(result);
		trigger_cleanup(DAG);
		for (i = 0; i < stack_size(candidates); ++i) {
			TDAG DAG1 = stack_get(candidates, i);
			unsigned j;
			for (j = i + 1; j < stack_size(candidates); ++j) {
				TDAG DAG2 = stack_get(candidates, j);
				if (unitrigger_match(DAG_args(DAG), DAG_arity(DAG) - 1u, DAG1, DAG2))
					break;
			}
			if (j == stack_size(candidates)) {
				Tstack_DAG trigger;
				stack_INIT_s(trigger, 1);
				stack_push(trigger, DAG_dup(DAG1));
				stack_push(result, trigger);
			}
		}
#ifdef DEBUG_TRIGGER
		{
			unsigned i, j;
			my_DAG_message("DAG: {%d}%D\n", DAG, DAG);
			for (i = 0; i < stack_size(result); ++i) {
				Tstack_DAG trigger = stack_get(result, i);
				my_DAG_message("uni-trigger:\n");
				for (j = 0; j < stack_size(trigger); ++j)
					my_DAG_message("\t%D\n", stack_get(trigger, j));
			}
		}
#endif
	} else {
		stack_INIT(result);
		/* Find multitrigger elements */
		multitrigger_elements(DAG, &candidates);
		if (stack_size(candidates) > 0) /* Compose elements into multitriggers */
			build_multitriggers(DAG_arity(DAG) - 1u, candidates, &result);
		trigger_cleanup(DAG);
#ifdef DEBUG_TRIGGER
		if (stack_size(result)) {
			unsigned i, j;
			my_DAG_message("multi-trigger: DAG: {%d}%D\n", DAG, DAG);
			for (i = 0; i < stack_size(result); ++i) {
				Tstack_DAG trigger = stack_get(result, i);
				my_DAG_message("trigger:\n");
				for (j = 0; j < stack_size(trigger); ++j)
					my_DAG_message("\t%D\n", stack_get(trigger, j));
			}
		}
#endif
	}
	stack_free(candidates);
	return result;
}
