#ifndef __PROOF_SAT_SOLVER_H
#define __PROOF_SAT_SOLVER_H

#include "src/bool/bool.h"

/*
  --------------------------------------------------------------
  Init/Done
  --------------------------------------------------------------
*/

extern void proof_SAT_init(void);
extern void proof_SAT_done(void);

/*
  --------------------------------------------------------------
  Interface with SAT solver
  --------------------------------------------------------------
*/

/* TODO: is this one really necessary? not used anywhere */
extern void proof_SAT_reset(void);

extern void proof_SAT_insert(Tclause clause);
extern void SAT_proof_set_id(SAT_Tclause clause_id);
extern void SAT_proof_notify(SAT_Tclause clause);

#endif
