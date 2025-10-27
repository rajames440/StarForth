/*
  --------------------------------------------------------------
  Module to link one DAG to symbols
  Used for macros, defined functions, and variable handling
  --------------------------------------------------------------
*/

#include "symbolic/DAG.h"

#include "symbolic/DAG-symb-DAG.h"
#include "symbolic/DAG-symb.h"

#include <string.h>

TDAG* DAG_symb_DAG = NULL;

static void
DAG_symb_DAG_hook_resize(unsigned old_alloc, unsigned new_alloc)
{
#ifdef DMEM
	MY_REALLOC_DMEM(
		DAG_symb_DAG, new_alloc * sizeof(TDAG), old_alloc * sizeof(TDAG));
#else
	MY_REALLOC(DAG_symb_DAG, new_alloc * sizeof(TDAG));
#endif
	if (new_alloc > old_alloc)
		memset(DAG_symb_DAG + old_alloc, 0, (new_alloc - old_alloc) * sizeof(TDAG));
	/*
    else
    {
    unsigned i;
    for (i = new_alloc; i < old_alloc; i++)
    if ((DAG_symb_type(symb) | SYMB_INTERPRETED) && DAG_symb_DAG[symb])
    DAG_free(DAG_symb_DAG[symb]);
    }
  */
}

static Tstack_symb macro_symb = NULL;

void
DAG_symb_macro(Tsymb symb, TDAG DAG)
{
	DAG_symb_type(symb) |= SYMB_INTERPRETED;
	DAG_symb_DAG[symb] = DAG;
	stack_push(macro_symb, symb);
}

/*
  --------------------------------------------------------------
  Initialization-release functions
  --------------------------------------------------------------
*/

void
DAG_symb_DAG_init(void)
{
	DAG_symb_set_hook_resize(DAG_symb_DAG_hook_resize);
	stack_INIT(macro_symb);
}

void
DAG_symb_DAG_done(void)
{
	unsigned i;
	for (i = 0; i < stack_size(macro_symb); i++)
		DAG_free(DAG_symb_DAG[stack_get(macro_symb, i)]);
	stack_free(macro_symb);
}
