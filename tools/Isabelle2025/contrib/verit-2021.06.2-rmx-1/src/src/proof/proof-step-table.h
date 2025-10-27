#ifndef __PROOF_STEP_TABLE_H
#define __PROOF_STEP_TABLE_H

#include "proof/proof-step.h"

/* typedefs Tstack_proof_step_stack */
TSstack(_proof_step_stack, Tstack_proof_step);

extern Tstack_proof_step_stack steps_stack;

#define top_steps stack_top(steps_stack)

typedef struct TDAG_assoc
{
	TDAG src, dest;
} TDAG_assoc;

TSstack(_DAG_assoc, TDAG_assoc); /* typedefs Tstack_DAG_assoc */

/* stores map from variables to choice functions for the current quantified
   formula being skolemized (which may include skolemizable subformulas) */
extern Tstack_DAG_assoc sko_var_to_choice;

extern Tstack_DAG_assoc choices;

extern Tstack_DAG_assoc ite_csts;

/*
  --------------------------------------------------------------
  Interface
  --------------------------------------------------------------
*/

extern void steps_init(void);

/**
   \brief adds clause to steps, after simplification */
extern Tproof steps_push(Tproof_step proof_step);

extern void steps_prune(void);

/**
   \brief Merges common proof steps by utilizing a hash table */
extern void steps_merge(void);

extern void steps_done(void);

#endif
