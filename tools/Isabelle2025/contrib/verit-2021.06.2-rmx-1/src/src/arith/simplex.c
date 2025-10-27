#include "arith/simplex.h"

Tstatus simplex_status = UNDEF;

/*
  --------------------------------------------------------------
  Dedicated access to integer variables
  --------------------------------------------------------------
*/

Tstack_simplex_var integer_stack;

unsigned
simplex_integer_var_begin(void)
{
	return 0;
}

unsigned
simplex_integer_var_end(void)
{
	return stack_size(integer_stack);
}

Tsimplex_var
simplex_integer_var_get(unsigned it)
{
	return stack_get(integer_stack, it);
}
