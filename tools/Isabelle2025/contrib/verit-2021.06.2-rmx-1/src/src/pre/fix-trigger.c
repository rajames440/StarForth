/*
  --------------------------------------------------------------
  In quantified formulas, moves triggers from matrix to top
  level.
  --------------------------------------------------------------
*/

#include "pre/fix-trigger.h"

#include "symbolic/DAG-prop.h"
#include "symbolic/context-recursion.h"
#include "symbolic/qnt-utils.h"
#include "symbolic/recursion.h"
#include "symbolic/veriT-DAG.h"
#include "utils/general.h"

#include <assert.h>
#include <stdlib.h>

static bool
DAG_quant_f_tmp(TDAG DAG)
{
	return DAG_quant(DAG);
}

static TDAG
fix_trigger_rec(TDAG src)
{
	Tstack_DAGstack *Pannot1, *Pannot2;
	if (!quantifier(DAG_symb(src)) || quantifier(DAG_symb(DAG_arg_last(src))))
		return src;
	Pannot2 = DAG_prop_get(DAG_arg_last(src), DAG_PROP_TRIGGER);
	if (!Pannot2) return src;
	Pannot1 = DAG_prop_get(src, DAG_PROP_TRIGGER);
	if (Pannot1) {
		unsigned i;
		for (i = 0; stack_size(*Pannot2) != 0; ++i)
			stack_push(*Pannot1, stack_pop(*Pannot2));
		stack_free(*Pannot2);
	} else
		DAG_prop_set(src, DAG_PROP_TRIGGER, Pannot2);
	DAG_prop_remove(DAG_arg_last(src), DAG_PROP_TRIGGER);
	return src;
}

void
fix_trigger_array(unsigned n, TDAG* Psrc)
{
	cond_structural_recursion_array(n, Psrc, fix_trigger_rec, DAG_quant_f_tmp);
}
