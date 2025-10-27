#ifndef __FREE_VARS_H
#define __FREE_VARS_H

#include "symbolic/DAG.h"

/*
  --------------------------------------------------------------
  Associate DAGs to variables
  --------------------------------------------------------------
*/

/* TODO: Not very smart to use stacks for storing variables. They never change,
   so having size/alloc info does not help. Something better suited for fixed
   size would be better. */

/** \brief associates a DAG to all of its *free* variables.

    Non nested quantified formulas should have no free vars, being ground. */
extern Tstack_DAG* DAG_fvars;

/**
   \brief sets free variables occurring at each DAG
   \remark what is the complexity? */
extern void set_fvars(TDAG DAG);

/*
  --------------------------------------------------------------
  Hooks
  --------------------------------------------------------------
*/

extern void fvars_hook_resize(unsigned old_alloc, unsigned new_alloc);
extern void fvars_hook_free(TDAG DAG);

/*
  --------------------------------------------------------------
  Interface
  --------------------------------------------------------------
*/

#define ground(D) (!DAG_fvars[D])

#ifdef DEBUG
extern Tstack_DAG get_fvars(TDAG DAG);

#else
#define get_fvars(D) DAG_fvars[D]

#endif

inline bool
is_free_in(TDAG var, TDAG term) {
	if (ground(term)) return false;

	Tstack_DAG v = get_fvars(term);
	assert(v != NULL);
	return stack_DAG_contains(v, var);
}

#endif
