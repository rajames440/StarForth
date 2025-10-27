#include "veriT-state.h"

#include <limits.h>

Tstack_unsigned xqueue = NULL;

Tstack_DAG veriT_lemmas;

Tstack_DAG veriT_conflict_eqs;

void
veriT_state_init(void)
{
	assert(sizeof(Tlit) == 4);
#if 0
#ifndef PEDANTIC
  assert(is_xlit(-1) && is_xlit(INT_MIN));
#endif
#endif
	stack_INIT(xqueue);
	stack_INIT(veriT_conflict_eqs);
	stack_INIT(veriT_lemmas);
}

void
veriT_state_done(void)
{
	stack_free(xqueue);
	stack_free(veriT_conflict_eqs);
	stack_apply(veriT_lemmas, DAG_free);
	stack_free(veriT_lemmas);
}
