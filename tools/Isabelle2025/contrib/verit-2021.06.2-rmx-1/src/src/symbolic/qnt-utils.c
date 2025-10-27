#include "symbolic/qnt-utils.h"

#include "symbolic/DAG-prop.h"
#include "symbolic/DAG.h"
#include "symbolic/context-recursion.h"

/*
  --------------------------------------------------------------
  Triggers handling
  --------------------------------------------------------------
*/

Tstack_DAGstack
copy_triggers(Tstack_DAGstack triggers)
{
	unsigned i;
	Tstack_DAGstack result;
	stack_INIT(result);
	for (i = 0; i < stack_size(triggers); ++i) {
		stack_inc(result);
		stack_COPY(stack_top(result), stack_get(triggers, i));
	}
	for (i = 0; i < stack_size(result); ++i)
		stack_apply(stack_get(result, i), DAG_dup);
	return result;
}

void
DAG_append_triggers(TDAG orig, TDAG dest)
{
	Tstack_DAGstack *Ptriggers_orig, *Ptriggers_dest, triggers;
	Ptriggers_orig = DAG_prop_get(orig, DAG_PROP_TRIGGER);
	if (!Ptriggers_orig) return;
	triggers = copy_triggers(*Ptriggers_orig);
	Ptriggers_dest = DAG_prop_get(dest, DAG_PROP_TRIGGER);
	if (!Ptriggers_dest) {
		DAG_prop_set(dest, DAG_PROP_TRIGGER, &triggers);
		return;
	}
	stack_merge(*Ptriggers_dest, triggers);
	stack_free(triggers);
}

/*
  --------------------------------------------------------------
  Recursion aids
  --------------------------------------------------------------
*/

bool
DAG_quant_f(TDAG DAG)
{
	return DAG_quant(DAG);
}

bool
DAG_quant_or_under_binder(TDAG DAG)
{
	return DAG_quant(DAG) || context.binders;
}
