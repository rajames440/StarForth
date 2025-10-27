#include "undo.h"

#include "utils/general.h"

unsigned undo_level = 0;
Tstack_unsigned undo = NULL;

unsigned undo_top_level = 0;
Tstack_unsigned undo_top = NULL;

Tundo_hook undo_hook[UNDO_NB] = { NULL };
unsigned undo_hook_arg_size[UNDO_NB] = { 0 };

#ifdef DEBUG
void
undo_debug(void)
{
	unsigned i = stack_size(undo) - 1;
	my_message("undo stack begin\n");
	while (i) {
		my_message("stack : %u %u\n", stack_get(undo, i), i);
		/* if (stack_get(undo, i) == 3) */
		/*   my_message("  SIG_ADD %d\n", stack_get(undo, i - 1)); */
		/* if (stack_get(undo, i) == 4) */
		/*   my_message("  SIG_DEL %d\n", stack_get(undo, i - 1)); */
		i -= undo_hook_arg_size[stack_get(undo, i)] + 1;
	}
	my_message("undo stack end\n");
	i = stack_size(undo_top) - 1;
	my_message("undo_top stack begin\n");
	while (i) {
		my_message("stack : %u %u\n", stack_get(undo_top, i), i);
		i -= undo_hook_arg_size[stack_get(undo_top, i)] + 1;
	}
	my_message("undo_top stack end\n");
}
#endif

void
undo_set_hook(Tundo_type type, Tundo_hook f, unsigned size)
{
	assert(type < UNDO_NB);
	assert(!undo_hook[type]);
	undo_hook[type] = f;
	undo_hook_arg_size[type] =
		(size + (((unsigned)sizeof(unsigned)) - 1u)) / ((unsigned)sizeof(unsigned));
}

void
undo_unset_hook(Tundo_type type)
{
	assert(type < UNDO_NB);
	assert(undo_hook[type]);
	undo_hook[type] = NULL;
}

void
undo_init(void)
{
	stack_INIT(undo);
	stack_INIT(undo_top);
}

void
undo_done(void)
{
	assert(!undo_level);
	assert(!stack_size(undo));
	stack_free(undo);
	assert(!undo_top_level);
	assert(!stack_size(undo_top));
	stack_free(undo_top);
}
