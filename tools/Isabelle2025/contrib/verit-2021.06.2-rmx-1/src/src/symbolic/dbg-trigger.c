/* ------------------------------------------------------------
 *  dbg-trigger.c
 *
 *
 *  Debugging resources for triggers.
 *
 ------------------------------------------------------------ */

#include "symbolic/dbg-trigger.h"

#include "symbolic/DAG-print.h"
#include "symbolic/DAG-prop.h"
#include "symbolic/recursion.h"
#include "symbolic/veriT-DAG.h"
#include "utils/stack.h"

static void
trigger_print(Tstack_DAG trigger)
{
	unsigned i;
	for (i = 0; i < stack_size(trigger); ++i)
		my_DAG_message("\t%02u:\t%D\n", i + 1, stack_get(trigger, i));
}

static void
trigger_list_print(Tstack_DAGstack triggers)
{
	unsigned i;
	for (i = 0; i < stack_size(triggers); ++i) {
		trigger_print(stack_get(triggers, i));
		my_DAG_message("\n");
	}
}

void
dbg_trigger_print(TDAG DAG)
{
	Tstack_DAGstack* Ptriggers;
	if (!quantifier(DAG_symb(DAG))) return;
	Ptriggers = DAG_prop_get(DAG, DAG_PROP_TRIGGER);
	if (!Ptriggers) {
		my_DAG_message("NO triggers for:\n%D\n", DAG);
		return;
	}
	my_DAG_message("triggers for:\n%D\n", DAG);
	trigger_list_print(*Ptriggers);
}

#if 0

void look_for_triggers(TDAG DAG);

void
look_for_triggers(TDAG DAG)
{
  structural_recursion_void(DAG, dbg_trigger_print);
}

#endif
