#include "congruence/congruence.h"
#include "instantiation/ccfv.h"
#include "instantiation/inst-index.h"
#include "instantiation/inst-man.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-ptr.h"
#include "symbolic/DAG.h"
#include "utils/options.h"
#include "utils/statistics.h"

#if defined(DEBUG_HEURISTICS)
#define DEBUG_ENUM 1
#endif

extern unsigned insts_stats_rounds;
extern float insts_time;
extern bool index_sorts;

static inline Tstack_DAG
get_enum_terms(Tsort sort)
{
	Tstack_DAG terms;
	terms = !index_sorts ? CC_get_sort_classes(sort) : get_sort_terms(sort);
	/* Create a dummy term for the sort */
	if (!terms || stack_is_empty(terms)) {
		stack_INIT(terms);
		stack_push(terms, DAG_dup(DAG_new_nullary(DAG_symb_const(DAG_sort(sort)))));
	}
	assert(terms && !stack_is_empty(terms));
	return terms;
}

static void
inst_enum_quant(TDAG qform, Tstack_DAG vars)
{
	unsigned i, j, var_it, term_it;
	bool completed_product;
	Tunifier unifier;
	Tstack_DAG terms;
	Tstack_DAGstack all_terms;
	Tstack_unsigned terms_cap, terms_ind;
	stack_INIT(all_terms);
	/* TODO: this is useless. Just compute for every sort */
	/* Partition variables per sort/class */
	for (i = 0; i < stack_size(vars); ++i) {
		/* If sort already in stack, add var ind to its list */
		for (j = 0; j < i; ++j)
			if (DAG_sort(stack_get(vars, i)) == DAG_sort(stack_get(vars, j))) {
				stack_INIT(terms);
				stack_merge(terms, stack_get(all_terms, j));
				stack_push(all_terms, terms);
				break;
			}
		if (j != i) continue;
		/* Add sort to stack and var ind to its list */
		stack_push(all_terms, get_enum_terms(DAG_sort(stack_get(vars, i))));
	}
	assert(stack_size(all_terms) == stack_size(vars));
	/* Initialize marker on terms */
	stack_INIT_s(terms_cap, stack_size(vars));
	stack_INIT_s(terms_ind, stack_size(vars));
	for (i = 0; i < stack_size(vars); ++i) {
		stack_push(terms_cap, 0);
		stack_push(terms_ind, 0);
	}
	var_it = 0;
	term_it = 0;
	unifier = unify_new(vars);
	completed_product = false;
	/* Iterate over all possibilities until a unifier is succesfull */
	do {
		if (completed_product) {
			/* TODO: account for case when could have saturated  */
			/* Stop if exhausted terms */
			for (i = 0; i < stack_size(vars); ++i)
				if (stack_get(terms_cap, i) < stack_size(stack_get(all_terms, i)) - 1)
					break;
			if (i == stack_size(vars)) break;
			/* Increment iterator and and terms_cap for next vector of terms that
             can be increased. The above check guarantees no infinite loop */
			do
				var_it = var_it == stack_size(vars) - 1 ? 0 : var_it + 1;
			while (stack_get(terms_cap, var_it) ==
						 stack_size(stack_get(all_terms, var_it)) - 1);
			stack_set(terms_cap, var_it, stack_get(terms_cap, var_it) + 1);
			/* Restart permutation machinery */
			completed_product = false;
			term_it = 0;
			for (i = 0; i < stack_size(vars); ++i) stack_set(terms_ind, i, 0);
			stack_set(terms_ind, var_it, stack_get(terms_cap, var_it));
		}
		/* Set substitution */
		for (i = 0; i < stack_size(vars); ++i)
			unify_set(
				unifier, i,
				stack_get(stack_get(all_terms, i), stack_get(terms_ind, i)));
		/* Permute */
		while (stack_get(terms_ind, term_it) == stack_get(terms_cap, term_it) &&
					 term_it < stack_size(vars)) {
			term_it++;
			if (term_it == var_it) term_it++;
		}
		if (term_it == stack_size(vars))
			completed_product = true;
		else
			stack_set(terms_ind, term_it, stack_get(terms_ind, term_it) + 1);
	} while (!inst_unifier(qform, NULL, unifier)); /* Try instantiating */
	/* Cleaning */
	unify_free(unifier);
	stack_free(terms_cap);
	stack_free(terms_ind);
	stack_apply(all_terms, stack_free);
	stack_free(all_terms);
}

bool
inst_enum(void)
{
	unsigned i, before;
	TDAG qform;
	/* Built term index for sorts based on current ground model */
	if (index_sorts) set_sorts_index(UNDEF_LVL);
	/* TODO: have a better way to do this */
	before = stack_size(lemmas);
	for (i = 0; i < stack_size(CC_quantified); ++i) {
		qform = stack_get(CC_quantified, i);
		assert(
			get_fvars(DAG_arg_last(qform)) &&
			!stack_is_empty(get_fvars(DAG_arg_last(qform))));
		inst_enum_quant(qform, get_fvars(DAG_arg_last(qform)));
	}
	if (index_sorts) unset_sorts_index();
#ifdef STATS_INST
#if DEBUG_INST
	if (before != stack_size(lemmas))
		my_message(
			"[(%2.2fs), Round %d] Sorts: got %d instances\n", insts_time,
			stats_counter_get(insts_stats_rounds), stack_size(lemmas) - before);
#endif
#if DEBUG_INST > 1
	print_Tstack_DAG(lemmas);
	my_message_return();
#endif
#endif
	return before != stack_size(lemmas);
}

