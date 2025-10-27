#ifndef __CONTEXT_HANDLING_H
#define __CONTEXT_HANDLING_H

#include "proof/proof-id.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/DAG.h"
#include "symbolic/polarities.h"

/*
  --------------------------------------------------------------
  Data structures
  --------------------------------------------------------------
*/

/* TODO: for pols and bounds try not to use stacks... */
/* TODO: use a single stack for vars and subs */

/* the number of inversions tells me whether its POS or NEG. Then just need a
   way of knowing how it's going with the literals/ITE etc */

/**
   \brief a set of properties relevant for the recursive transformations */
typedef struct Tcontext
{
	Tstack_pol pols; /*< polarities */
	unsigned binders; /*< number of binders */
	Tstack_DAG DAGs; /*< variables or substitutions in scope */
} Tcontext;

extern Tcontext context;

#define ctx_pol (stack_top(context.pols))

#define ctx_free(ctx) \
	do { \
		if ((ctx).pols) stack_free((ctx).pols); \
		(ctx).binders = 0; \
		if ((ctx).DAGs) stack_free((ctx).DAGs); \
	} while (0)

/* TODO: defined because old "binder" macro does not include lets and I was
   afraid of adding it */
/**
   \brief tests if symbol c is a binder
   \param c shall be an expression of type Tsymb */
#define binder_all(c) \
	((c == QUANTIFIER_EXISTS) || (c == QUANTIFIER_FORALL) || (c == LAMBDA) || \
	 (c == LET))
#endif
