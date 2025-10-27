#ifndef __CONTEXT_RECURSION_PROOF_H
#define __CONTEXT_RECURSION_PROOF_H

#include "proof/proof.h"
#include "symbolic/DAG.h"
#include "symbolic/context-handling.h"
#include "symbolic/polarities.h"

/*
  --------------------------------------------------------------
  Interface
  --------------------------------------------------------------
*/

extern TDAG_proof context_tree_rec_proof(TDAG src);

extern TDAG context_structural_recursion_proof(
	TDAG src, Tproof* Pproof, void (*f_init)(void),
	void (*f_push)(TDAG, unsigned*), void (*f_pop)(TDAG, unsigned),
	Tproof (*f_replace)(TDAG, TDAG, Tstack_proof),
	TDAG_proof (*f_reduce)(TDAG, TDAG, Tproof), bool (*cont)(TDAG));

#endif
