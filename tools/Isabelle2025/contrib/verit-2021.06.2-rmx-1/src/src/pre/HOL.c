/*
  --------------------------------------------------------------
  Module for handling higher order constants, beta reduction,...
  --------------------------------------------------------------
*/

/* TODO: IMPROVE rename bound variables, so that better sharing */

#include "pre/HOL.h"

#include "symbolic/DAG-print.h"
#include "symbolic/DAG-prop.h"
#include "symbolic/DAG-symb-DAG.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/DAG.h"
#include "symbolic/recursion.h"
#include "symbolic/veriT-DAG.h"
#include "utils/general.h"
#include "veriT-config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define DAG_sort_functional(A) (DAG_sort_arity(A) && !DAG_sort_instance(A))

/*
  --------------------------------------------------------------
  Temporary
  --------------------------------------------------------------

  #include "symbolic/DAG-prop.h"

  bool trigger_free_val = true;

  static bool
  trigger_free_aux(TDAG src)
  {
  if (!quantifier(DAG_symb(src)) || !DAG_prop_get(src, DAG_PROP_TRIGGER))
  return true;
  my_DAG_message("Trigger %D\n", src);
  return false;
  }

  static void
  check_trigger_free(TDAG src)
  {
  trigger_free_val = structural_recursion_pred(src, trigger_free_aux);
  }

  static void
  recheck_trigger_free(TDAG src)
  {
  unsigned tmp = structural_recursion_pred(src, trigger_free_aux);
  assert(trigger_free_val == tmp);
  }
*/

/*
  --------------------------------------------------------------
  General functions
  --------------------------------------------------------------
*/

static bool
is_FOL_node(TDAG src)
{
	return DAG_sort(src) && !DAG_sort_functional(DAG_sort(src));
}

bool
is_FOL(TDAG src)
{
	return structural_recursion_pred(src, is_FOL_node);
}

static bool
is_FOL_strict_node(TDAG src)
{
	unsigned i;
	if (
		DAG_symb(src) == LET || DAG_symb(src) == LAMBDA ||
		DAG_symb(src) == APPLY_LAMBDA) {
#ifdef DEBUG_HOL
		my_DAG_error("is_FOL_strict_node (%D)\n", src);
#endif
		return false;
	}
	if (quantifier(DAG_symb(src))) {
		assert(DAG_arity(src) > 0);
		for (i = 0; i < DAG_arity(src) - 1u; i++)
			if (DAG_sort(DAG_arg(src, i)) == SORT_BOOLEAN) {
#ifdef DEBUG_HOL
				my_DAG_error("is_FOL_strict_node (%D)\n", src);
#endif
				return false;
			}
		return true;
	}
	if (
		DAG_arity(src) &&
		!(boolean_connector(DAG_symb(src)) || DAG_symb(src) == FUNCTION_ITE))
		for (i = 0; i < DAG_arity(src); i++)
			if (
				DAG_sort(DAG_arg(src, i)) == SORT_BOOLEAN &&
				DAG_arg(src, i) != DAG_TRUE && DAG_arg(src, i) != DAG_FALSE) {
#ifdef DEBUG_HOL
				my_DAG_error("is_FOL_strict_node (%D)\n", src);
#endif
				return false;
			}
	if (
		DAG_symb(src) == FUNCTION_ITE &&
		DAG_sort(DAG_arg(src, 1)) == SORT_BOOLEAN) {
#ifdef DEBUG_HOL
		my_DAG_error("is_FOL_strict_node (%D)\n", src);
#endif
		return false;
	}
	return DAG_sort(src) && !DAG_sort_polymorphic(DAG_sort(src)) &&
		!DAG_sort_functional(DAG_sort(src));
	/* every node should be of non-functional sort */
}

bool
is_FOL_strict(TDAG src)
{
	return structural_recursion_pred(src, is_FOL_strict_node);
}

/*
  --------------------------------------------------------------
  General functions
  --------------------------------------------------------------
*/

static bool
is_binder_free_node(TDAG src)
{
	return !(binder(DAG_symb(src)) || DAG_symb(src) == LET);
}

bool
is_binder_free(TDAG src)
{
	return structural_recursion_pred(src, is_binder_free_node);
}

static bool
is_quantifier_free_node(TDAG src)
{
	return !quantifier(DAG_symb(src));
}

bool
is_quantifier_free(TDAG src)
{
	return structural_recursion_pred(src, is_quantifier_free_node);
}

/*
  --------------------------------------------------------------
  Beta reduction
  --------------------------------------------------------------
*/

/*
  simp s v = vs                                               OK
  simp s (A \wedge B) = (simp s A) \wedge (simp s B)          OK
  simp s (f t) = let f = f \delta in
  let t' = simp s t in
  match f with
  f simple func -> f t'                       OK
  \lambda x.u   -> simp (s + {x -> t'}) u     OK
  ite(c,f1,f2)  -> ite simp s c               OK
  simp s (f1 t) !!NOT t'!
  simp s (f2 t)
  default: T -> ????                          Unsupported
  simp s (forall x t) = let x' fresh in
  forall x' simp (s + {x --> x'}) t   OK
  simp s (f = g) = let x fresh in
  forall x (simp s (f x)) = (simp s (g x)) OK
*/

/* PF This submodule applies lambda expressions exhaustively
   DAG_symb_(set or get)_bind are used to link variables and their values
   The code here is complicated also because of the fact that functions
   are not handled as normal terms within DAG module */

static TDAG
assert_FOL_node(TDAG src)
{
	if (!is_FOL_node(src)) my_DAG_error("Formula is higher order (%D)\n", src);
	return src;
}

/*
  --------------------------------------------------------------
  Equality lowering
  --------------------------------------------------------------
*/

static TDAG HOL_to_FOL_tree(TDAG src);

/**
   applies equality lowering to top most symbol if it can be.
   Rewrites equalities T1 = T2 where T1 and T2 have function
   (or predicate) sort into
   forall x_1 ... x_n . T1(x_1, ... , x_n) = T2(x_1, ... , x_n)
   New quantified variables symbols are of the form ?veriT__<n>, so
   such symbols should not be used elsewhere
   \param src the term (or formula) to rewrite
   \return the beta-reduced term
   \remarks destructive
   \remarks DAG-linear
   \remarks no particular requirements on formula (no variable capture,
   behaves honestly with quantifiers), if the binders are not used elsewhere.
   \remark ite-, quantifier-, lambda-, apply-tolerant */
static TDAG
equality_lower_one(TDAG src)
/* Safe for structural recursion if restriction on bound vars applies. */
{
	unsigned i;
	unsigned nb_bound;
	Tsymb* symb;
	TDAG *PDAG, *PDAG2, tmp;
	assert(
		DAG_symb(src) == PREDICATE_EQ &&
		DAG_sort_functional(DAG_sort(DAG_arg0(src))));
	assert(DAG_sort(DAG_arg0(src)) == DAG_sort(DAG_arg1(src)));
	if (DAG_sort_parametric(DAG_sort(DAG_arg0(src))))
		my_DAG_error("parametric sorts in equality (%D)\n", src);
	assert(DAG_sort_arity(DAG_sort(DAG_arg0(src))) != DAG_SORT_NARY);
	assert(DAG_sort_arity(DAG_sort(DAG_arg0(src))) > 0);

	nb_bound = DAG_sort_arity(DAG_sort(DAG_arg0(src))) - 1;
	MY_MALLOC(symb, nb_bound * sizeof(Tsymb));
	for (i = 0; i < nb_bound; i++)
		if (DAG_sort(DAG_arg0(src)) == SORT_BOOLEAN)
			my_DAG_error("equality lowering introduces Bool quantifier (%D)\n", src);
		else
			symb[i] = DAG_symb_variable(DAG_sort_sub(DAG_sort(DAG_arg0(src)), i));
	MY_MALLOC(PDAG2, 2 * sizeof(TDAG));
	for (i = 0; i < 2; i++) {
		MY_MALLOC(PDAG, (nb_bound + 1) * sizeof(TDAG));
		PDAG[0] = DAG_arg(src, i);
		for (i = 0; i < nb_bound; i++) PDAG[i + 1] = DAG_new_nullary(symb[i]);
		tmp = DAG_dup(DAG_new(APPLY_LAMBDA, nb_bound + 1, PDAG));
		PDAG2[i] = HOL_to_FOL_tree(tmp);
		DAG_free(tmp);
	}
	MY_MALLOC(PDAG, (nb_bound + 1) * sizeof(TDAG));
	for (i = 0; i < nb_bound; i++)
		assert_FOL_node(PDAG[i] = DAG_new_nullary(symb[i]));
	PDAG[i] = DAG_new(
		(DAG_sort(PDAG2[0]) == SORT_BOOLEAN) ? CONNECTOR_EQUIV : PREDICATE_EQ, 2,
		PDAG2);
	tmp = DAG_dup(DAG_new(QUANTIFIER_FORALL, nb_bound + 1, PDAG));
	DAG_free(DAG_arg0(DAG_arg_last(tmp)));
	DAG_free(DAG_arg1(DAG_arg_last(tmp)));
	free(symb);
	DAG_free(src);
	return tmp;
}

/*
  --------------------------------------------------------------
  HOL_to_FOL
  --------------------------------------------------------------
*/

static TDAG
HOL_to_FOL_tree(TDAG src)
{
	unsigned shift = 0;
	TDAG f, dest;
	Tstack_DAGstack triggers = NULL;
	if (!DAG_arity(src)) {
		if (DAG_symb_DAG[DAG_symb(src)])
			return assert_FOL_node(DAG_dup(DAG_symb_DAG[DAG_symb(src)]));
		return assert_FOL_node(DAG_dup(src));
	}
	if (quantifier(DAG_symb(src))) {
		unsigned i;
		TDAG *PDAG, *PDAG2;
		MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
		MY_MALLOC(PDAG2, DAG_arity(src) * sizeof(TDAG));
		for (i = 0; i < DAG_arity(src) - 1u; i++) {
			PDAG[i] = DAG_symb_DAG[DAG_symb(DAG_arg(src, i))];
			PDAG2[i] = DAG_new_nullary(DAG_symb_variable(DAG_sort(DAG_arg(src, i))));
			DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = DAG_dup(PDAG2[i]);
		}
		PDAG[i] = PDAG2[i] = HOL_to_FOL_tree(DAG_arg(src, i));

		{
			Tstack_DAGstack* Ptriggers = DAG_prop_get(src, DAG_PROP_TRIGGER);
			if (Ptriggers) {
				unsigned i, j;
				stack_INIT_s(triggers, stack_size(*Ptriggers));
				for (i = 0; i < stack_size(*Ptriggers); ++i) {
					Tstack_DAG old_trigger = stack_get(*Ptriggers, i);
					Tstack_DAG trigger;
					stack_INIT_s(trigger, stack_size(old_trigger));
					for (j = 0; j < stack_size(old_trigger); ++j)
						stack_push(trigger, HOL_to_FOL_tree(stack_get(old_trigger, j)));
					stack_push(triggers, trigger);
				}
			}
		}

		for (i = 0; i < DAG_arity(src) - 1u; i++) {
			DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = PDAG[i];
			PDAG[i] = PDAG2[i];
		}
		dest = DAG_dup(DAG_new(DAG_symb(src), DAG_arity(src), PDAG2));
		if (triggers) DAG_prop_set(dest, DAG_PROP_TRIGGER, &triggers);
		for (i = 0; i < DAG_arity(src); i++) DAG_free(PDAG[i]);
		free(PDAG);
#ifdef DEBUG_HOL
		my_DAG_message("Before %D After %D\n", src, dest);
#endif
		return dest;
	}
	if (DAG_symb(src) == LET) {
		unsigned i;
		TDAG *PDAG, *PDAG2;
		MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
		MY_MALLOC(PDAG2, DAG_arity(src) * sizeof(TDAG));
		assert(DAG_arity(src) >= 3);
		/* Get the variable values */
		for (i = 1; i < DAG_arity(src); i += 2)
			PDAG[i] = HOL_to_FOL_tree(DAG_arg(src, i));
		/* Attach values to variables */
		for (i = 0; i < DAG_arity(src) - 1u; i += 2) {
			PDAG[i] = DAG_symb_DAG[DAG_symb(DAG_arg(src, i))];
			DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = PDAG[i + 1];
		}
		/* Compute term wrapped in let */
		dest = HOL_to_FOL_tree(DAG_arg_last(src));
		/* Detach variable values, and free them */
		for (i = 0; i < DAG_arity(src) - 1u; i += 2) {
			DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = PDAG[i];
			DAG_free(PDAG[i + 1]);
		}
		free(PDAG);
		free(PDAG2);
#ifdef DEBUG_HOL
		my_DAG_message("Before %D After %D\n", src, dest);
#endif
		return dest;
	}
	if (
		DAG_symb(src) == PREDICATE_EQ &&
		DAG_sort_functional(DAG_sort(DAG_arg0(src))))
		return equality_lower_one(DAG_dup(src));
	if (DAG_symb(src) == APPLY_LAMBDA) {
		f = DAG_arg(src, 0);
		shift = 1;
		while (!DAG_arity(f) && DAG_symb_DAG[DAG_symb(f)])
			f = DAG_symb_DAG[DAG_symb(f)];
	} else if (DAG_symb_DAG[DAG_symb(src)]) {
		f = DAG_symb_DAG[DAG_symb(src)];
		while (!DAG_arity(f) && DAG_symb_DAG[DAG_symb(f)])
			f = DAG_symb_DAG[DAG_symb(f)];
	} else
		f = src;
	if (!DAG_arity(f) || f == src) /* f is a simple function */
	{
		unsigned i;
		TDAG *PDAG, *PDAG2;
		MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
		MY_MALLOC(PDAG2, DAG_arity(src) * sizeof(TDAG));
		for (i = 0; i < DAG_arity(src) - shift; i++)
			PDAG[i] = PDAG2[i] = HOL_to_FOL_tree(DAG_arg(src, i + shift));
		dest = DAG_dup(DAG_new(DAG_symb(f), DAG_arity(src) - shift, PDAG2));
		for (i = 0; i < DAG_arity(src) - shift; i++) DAG_free(PDAG[i]);
		free(PDAG);
#ifdef DEBUG_HOL
		my_DAG_message("Before %D After %D\n", src, dest);
#endif
		return assert_FOL_node(dest);
	}
	if (DAG_symb(f) == LAMBDA) {
		unsigned i;
		TDAG *PDAG, *PDAG2;
		assert(DAG_arity(f) == DAG_arity(src) + 1 - shift);
		MY_MALLOC(PDAG, (DAG_arity(f) - 1u) * sizeof(TDAG));
		MY_MALLOC(PDAG2, (DAG_arity(f) - 1u) * sizeof(TDAG));
		/* Get the argument values and save previous attached vals */
		for (i = 0; i < DAG_arity(f) - 1u; i++) {
			PDAG[i] = DAG_symb_DAG[DAG_symb(DAG_arg(f, i))];
			PDAG2[i] = HOL_to_FOL_tree(DAG_arg(src, i + shift));
		}
		/* Attach arguments to variables */
		for (i = 0; i < DAG_arity(f) - 1u; i++)
			DAG_symb_DAG[DAG_symb(DAG_arg(f, i))] = PDAG2[i];
		/* Compute body */
		dest = HOL_to_FOL_tree(DAG_arg(f, i));
		/* Free and restore previous values */
		for (i = 0; i < DAG_arity(f) - 1u; i++) {
			DAG_free(PDAG2[i]);
			DAG_symb_DAG[DAG_symb(DAG_arg(f, i))] = PDAG[i];
		}
		free(PDAG);
		free(PDAG2);
#ifdef DEBUG_HOL
		my_DAG_message("Before %D After %D\n", src, dest);
#endif
		return dest;
	}
	if (DAG_symb(f) == FUNCTION_ITE) {
		unsigned i;
		TDAG *PDAG, *PDAG2;
		TDAG cond = HOL_to_FOL_tree(DAG_arg(f, 0));
		TDAG then_case, else_case, tmp;
		MY_MALLOC(PDAG, (DAG_arity(src) - shift + 1u) * sizeof(TDAG));
		MY_MALLOC(PDAG2, (DAG_arity(src) - shift + 1u) * sizeof(TDAG));
		for (i = 0; i < DAG_arity(src) - shift; i++)
			PDAG[i + 1] = PDAG2[i + 1] = DAG_arg(src, i + shift);
		PDAG[0] = PDAG2[0] = DAG_arg(f, 1);
		tmp = DAG_dup(DAG_new(APPLY_LAMBDA, DAG_arity(src) - shift + 1u, PDAG));
		then_case = HOL_to_FOL_tree(tmp);
		DAG_free(tmp);
		MY_MALLOC(PDAG, (DAG_arity(src) - shift + 1u) * sizeof(TDAG));
		for (i = 0; i < DAG_arity(src) - shift; i++) PDAG[i + 1] = PDAG2[i + 1];
		PDAG[0] = PDAG2[0] = DAG_arg(f, 2);
		tmp = DAG_dup(DAG_new(APPLY_LAMBDA, DAG_arity(src) - shift + 1u, PDAG));
		else_case = HOL_to_FOL_tree(tmp);
		DAG_free(tmp);
		for (i = 1; i < DAG_arity(src) - shift + 1u; i++) DAG_free(PDAG2[i]);
		free(PDAG2);
		if (DAG_sort(then_case) == SORT_BOOLEAN)
			dest = DAG_dup(
				DAG_new_args(CONNECTOR_ITE, cond, then_case, else_case, DAG_NULL));
		else
			dest = DAG_dup(
				DAG_new_args(FUNCTION_ITE, cond, then_case, else_case, DAG_NULL));
		DAG_free(cond);
		DAG_free(then_case);
		DAG_free(else_case);
#ifdef DEBUG_HOL
		my_DAG_message("Before %D After %D\n", src, dest);
#endif
		return dest;
	}
	/* left hand side might be an application too */
	my_error("HOL_to_FOL: not implemented\n");
	return DAG_NULL;
}

static TDAG
HOL_to_FOL_DAG(TDAG src)
{
	unsigned i;
	TDAG *PDAG, tmp;
	if (DAG_tmp_DAG[src]) return DAG_tmp_DAG[src];
	if (
		DAG_symb(src) == APPLY_LAMBDA || quantifier(DAG_symb(src)) ||
		DAG_symb(src) == LET ||
		(DAG_symb(src) == PREDICATE_EQ &&
		 DAG_sort_functional(DAG_sort(DAG_arg0(src))))) {
		tmp = HOL_to_FOL_tree(src);
		DAG_tmp_DAG[src] = tmp;
		return tmp;
	}
	/* (f t1 ... tn) */
	MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
	for (i = 0; i < DAG_arity(src); i++)
		PDAG[i] = HOL_to_FOL_DAG(DAG_arg(src, i));
	tmp = DAG_dup(DAG_new(DAG_symb(src), DAG_arity(src), PDAG));
	DAG_tmp_DAG[src] = tmp;
	return tmp;
}

TDAG
HOL_to_FOL(TDAG src)
{
	TDAG dest;
	DAG_tmp_reserve();
	dest = DAG_dup(HOL_to_FOL_DAG(src));
	DAG_tmp_free_DAG(src);
	DAG_tmp_release();
#ifdef DEBUG_HOL
	my_DAG_message("HOL_to_FOL\nbefore:%D\nafter:%D\n", src, dest);
#endif
	return dest;
}

void
HOL_to_FOL_array(unsigned n, TDAG* Psrc)
{
	unsigned i;
	TDAG* PDAG;
	DAG_tmp_reserve();
	MY_MALLOC(PDAG, n * sizeof(TDAG));
	for (i = 0; i < n; i++) PDAG[i] = DAG_dup(HOL_to_FOL_DAG(Psrc[i]));
	for (i = 0; i < n; i++) {
		DAG_tmp_free_DAG(Psrc[i]);
		DAG_free(Psrc[i]);
		Psrc[i] = PDAG[i];
	}
	free(PDAG);
	DAG_tmp_release();
}
