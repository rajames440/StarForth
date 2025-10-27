#include "instantiation/inst-symbs.h"

#include "instantiation/inst-index.h"
#include "symbolic/DAG-prop.h"

void
DAG_prop_symbols_free(Tstack_OSymb* symbs)
{
	stack_free(*symbs);
}

void
DAG_tmp_reset_symbs(TDAG DAG)
{
	unsigned i;
	if (!symbs_of[DAG]) return;
	stack_free(symbs_of[DAG]);
	for (i = 0; i < DAG_arity(DAG); i++) DAG_tmp_reset_symbs(DAG_arg(DAG, i));
}

void
set_symbs(TDAG DAG, bool is_pred, Tstack_DAG vars)
{
	unsigned i, j, k;
	TDAG tmp_DAG;
	OSymb osymb, tmp;
	Tstack_OSymb tmp_stack;
	assert(
		DAG_symb(DAG) != QUANTIFIER_FORALL && DAG_symb(DAG) != QUANTIFIER_EXISTS);
	assert(DAG_arity(DAG) && !ground(DAG));
	if (symbs_of[DAG]) return;
	if (!symbs_of[DAG]) stack_INIT(symbs_of[DAG]);
	if (!is_pred) {
		osymb.symbol = DAG_symb(DAG);
		osymb.nb_occurs = 1;
		stack_push(symbs_of[DAG], osymb);
	}
	for (i = 0; i < DAG_arity(DAG); ++i) {
		tmp_DAG = DAG_arg(DAG, i);
		/* TODO: this is problematic if groundness changes between clauses; some
         terms won't have their symbols accounted for */
		if (!DAG_arity(tmp_DAG) || ground(tmp_DAG)) continue;
		set_symbs(tmp_DAG, false, vars);
		if (stack_is_empty(symbs_of[tmp_DAG])) continue;
		/* TODO: see if this can't be done more efficiently */
		stack_INIT(tmp_stack);
		for (j = 0; j < stack_size(symbs_of[tmp_DAG]); ++j) {
			for (k = 0; k < stack_size(symbs_of[DAG]); ++k)
				if (
					stack_get(symbs_of[tmp_DAG], j).symbol ==
					stack_get(symbs_of[DAG], k).symbol) {
					tmp.symbol = stack_get(symbs_of[DAG], k).symbol;
					tmp.nb_occurs = stack_get(symbs_of[DAG], k).nb_occurs +
						stack_get(symbs_of[tmp_DAG], j).nb_occurs;
					stack_push(tmp_stack, tmp);
					break;
				} else
					stack_push(tmp_stack, stack_get(symbs_of[DAG], k));
			if (k == stack_size(symbs_of[DAG]))
				stack_push(tmp_stack, stack_get(symbs_of[tmp_DAG], j));
		}
		stack_reset(symbs_of[DAG]);
		stack_merge(symbs_of[DAG], tmp_stack);
		stack_free(tmp_stack);
	}
	stack_COPY(tmp_stack, symbs_of[DAG]);
	DAG_prop_set(DAG, DAG_PROP_SYMBS, &tmp_stack);
}

unsigned
score_lit(TDAG DAG, bool pol)
{
	unsigned i, score;
	Findex f_index;
	Pindex p_index;
	OSymb osymb;
	Tstack_OSymb symbs = *(Tstack_OSymb*)DAG_prop_get(DAG, DAG_PROP_SYMBS);
	score = 0;
	/* If solely vars or terms with fresh symbols only, super effective */
	if (stack_is_empty(symbs)) return score;
	score++;
	score = !pol * 1000;
	if (DAG_symb(DAG) != PREDICATE_EQ)
		if (get_Pindex(DAG_symb(DAG), &p_index) && p_index.signatures[pol])
			score += stack_size(p_index.signatures[pol]);
	for (i = 0; i < stack_size(symbs); ++i) {
		osymb = stack_get(symbs, i);
		if (
			!get_Findex(osymb.symbol, &f_index) || (pol && !f_index.signatures) ||
			!pol)
			continue;
		if (pol && f_index.signatures) {
			score += stack_size(f_index.signatures) * osymb.nb_occurs;
			continue;
		}
	}
	return score;
}
