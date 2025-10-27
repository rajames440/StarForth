#include "hint.h"

#include "arith/LA.h"
#include "bool/literal.h"
#include "congruence/congruence.h"
#include "utils/stack.h"
#include "utils/statistics.h"

/*
  --------------------------------------------------------------
  Data types
  --------------------------------------------------------------
*/

#define hint_is_CC 1
#define hint_is_LA 2

/** \brief structure to record information on the origin of a hint */
typedef struct TShint_data
{
	unsigned char origin; /*< the procedure that produced the hint */
	bool reversed; /*< helper bit for CC */
	union
	{
		TDAG DAG; /*< the DAG that produced the hint (in CC) */
		Tlit lit; /*< the lit that produced the hint (in LA) */
	} data; /*< the reason the hint was produced */
} Thint_data;

TSstack(_hint_data, Thint_data);

/*
  --------------------------------------------------------------
  Local data
  --------------------------------------------------------------
*/

/** This table has Tlit as indices and TDAG or Tlit as values */
static Tstack_hint_data hint_storage = NULL;

/*
  --------------------------------------------------------------
  Module statistics
  --------------------------------------------------------------
*/

static unsigned stats_hint_LA;
static unsigned stats_hint_CC;

/*
  --------------------------------------------------------------
  Local functions
  --------------------------------------------------------------
*/

/** \brief resizes internal storage to accomodate all literals */
static inline void
check_storage(void)
{
	if (2 * var_max + 1 >= stack_size(hint_storage))
		stack_resize(hint_storage, (2 * var_max) + 2);
}

/*
  --------------------------------------------------------------
  Public functions
  --------------------------------------------------------------
*/

void
hint_CC(Tlit lit, TDAG DAG, bool reversed)
{
	check_storage();
	assert(lit < stack_size(hint_storage));
	hint_storage->data[lit].origin = hint_is_CC;
	hint_storage->data[lit].reversed = reversed;
	hint_storage->data[lit].data.DAG = DAG;
	SAT_hint(lit);
	stats_counter_inc(stats_hint_CC);
}

void
hint_LA(Tlit lit, Tlit cause)
{
	check_storage();
	assert(lit < stack_size(hint_storage));
	hint_storage->data[lit].origin = hint_is_LA;
	hint_storage->data[lit].data.lit = cause;
	SAT_hint(lit);
	stats_counter_inc(stats_hint_LA);
}

TDAG
hint_CC_cause(Tlit lit)
{
	assert(lit < stack_size(hint_storage));
	return hint_storage->data[lit].data.DAG;
}

bool
hint_CC_reversed(Tlit lit)
{
	assert(lit < stack_size(hint_storage));
	return hint_storage->data[lit].reversed;
}

Tlit
hint_LA_cause(Tlit lit)
{
	assert(lit < stack_size(hint_storage));
	return hint_storage->data[lit].data.lit;
}

void
hint_explain_dispatch(Tlit lit)
{
	assert(lit < stack_size(hint_storage));
	switch (hint_storage->data[lit].origin) {
		case hint_is_CC: CC_hint_explain(lit); break;
		case hint_is_LA: LA_hint_explain(lit); break;
		default: my_error("strange literal in hint_explain_dispatch.");
	}
}

void
hint_init(void)
{
	stack_INIT(hint_storage);
	stats_hint_CC =
		stats_counter_new("hint/CC", "Hints produced by congruence closure", "%9d");
	stats_hint_LA =
		stats_counter_new("hint/LA", "Hints produced by linear arithmetics", "%9d");
}

void
hint_done(void)
{
	stack_free(hint_storage);
}
