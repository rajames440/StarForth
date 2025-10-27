#include "instantiation/free-vars.h"

#include "symbolic/DAG-symb.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/DAG.h"
#include "symbolic/veriT-DAG.h"

Tstack_DAG* DAG_fvars;

void
fvars_hook_resize(unsigned old_alloc, unsigned new_alloc)
{
	unsigned i;
	MY_REALLOC(DAG_fvars, new_alloc * sizeof(Tstack_DAG));
	for (i = old_alloc; i < new_alloc; ++i) DAG_fvars[i] = NULL;
}

void
fvars_hook_free(TDAG DAG)
{
	if (!ground(DAG)) stack_free(DAG_fvars[DAG]);
}

/* When this is used with new generated terms, such as instances, there will be
   recomputations for everything that is ground... should combine with the
   ground and quant bit???

   Only goes down if !DAG_ground || DAG_quant? */
void
set_fvars_rec(TDAG DAG)
{
	unsigned i, j;
	Tstack_DAG bvars;
	/* Variables already computed */
	if (DAG_tmp_bool[DAG] || DAG_fvars[DAG]) return;
	DAG_tmp_bool[DAG] = 1;
	/* variables */
	if (!DAG_arity(DAG) && DAG_symb_type(DAG_symb(DAG)) & SYMB_VARIABLE) {
		/* HB remember the memory alignment */
		stack_INIT(DAG_fvars[DAG]);
		stack_push(DAG_fvars[DAG], DAG);
	} else if (quantifier(DAG_symb(DAG))) {
		set_fvars_rec(DAG_arg_last(DAG));
		/* Whether there are free vars in the quantified formula */
		if (stack_size(DAG_fvars[DAG_arg_last(DAG)]) > DAG_arity(DAG) - 1) {
			stack_INIT(DAG_fvars[DAG]);
			stack_INIT(bvars);
			for (i = 0; i < DAG_arity(DAG) - 1; ++i)
				stack_push(bvars, DAG_arg(DAG, i));
			stack_sort(bvars, DAG_cmp_q);
			i = j = 0;
			/* Gets the difference of free vars - bound vars */
			while (i < stack_size(DAG_fvars[DAG_arg_last(DAG)]) &&
						 j < stack_size(bvars))
				if (stack_get(DAG_fvars[DAG_arg_last(DAG)], i) < stack_get(bvars, j)) {
					stack_push(
						DAG_fvars[DAG], stack_get(DAG_fvars[DAG_arg_last(DAG)], i));
					++i;
				} else if (
					stack_get(DAG_fvars[DAG_arg_last(DAG)], i) > stack_get(bvars, j))
					j++;
				else {
					i++;
					j++;
				}
			while (i < stack_size(DAG_fvars[DAG_arg_last(DAG)]))
				stack_push(
					DAG_fvars[DAG], stack_get(DAG_fvars[DAG_arg_last(DAG)], i++));
			stack_free(bvars);
		}
	} else {
		for (i = 0; i < DAG_arity(DAG); ++i) {
			set_fvars_rec(DAG_arg(DAG, i));
			if (DAG_fvars[DAG_arg(DAG, i)]) {
				if (!DAG_fvars[DAG]) stack_INIT(DAG_fvars[DAG]);
				stack_merge(DAG_fvars[DAG], DAG_fvars[DAG_arg(DAG, i)]);
			}
		}
		if (DAG_fvars[DAG]) {
			stack_sort(DAG_fvars[DAG], DAG_cmp_q);
			stack_uniq(DAG_fvars[DAG]);
		}
	}
	if (!DAG_fvars[DAG]) return;
}

void
set_fvars(TDAG DAG)
{
	DAG_tmp_reserve();
	set_fvars_rec(DAG);
	DAG_tmp_reset_bool(DAG);
	DAG_tmp_release();
}

#ifdef DEBUG
Tstack_DAG
get_fvars(TDAG DAG)
{
	assert(!DAG_fvars[DAG] || !stack_is_empty(DAG_fvars[DAG]));
	return DAG_fvars[DAG];
}
#endif

bool is_free_in(TDAG var, TDAG term);
