#ifndef __CCFV_BCKT_H
#define __CCFV_BCKT_H

#include "instantiation/ccfv-constr.h"
#include "instantiation/inst-man.h"
#include "instantiation/unify.h"

/*
  \brief Handles instances creation, management and deletion */

/*
  --------------------------------------------------------------
  Interface
  --------------------------------------------------------------
*/

extern bool CCFV_entail_constraint(
	Tunifier solution, Tstack_constr constraints);

extern void CCFV_bckt_cycle_init(Tstack_DAG lits, bool (*f)(Tunifier));
extern void CCFV_bckt_cycle_done(Tstack_DAG lits);
extern void CCFV_bckt_init(void);
extern void CCFV_bckt_done(void);

#endif
