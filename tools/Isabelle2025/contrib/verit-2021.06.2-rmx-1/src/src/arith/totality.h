/**
   \brief This module handles the totality propery on bound constraints:
   \f$t_1 \le t_2\f$ or \f$t_2 \le t_1\f$. Pre-processing rewrites all 
   equalities between arithmetic terms as a conjunction of two inequalities
   that are related by the totality property.

   This module provides a function to register all such equalities
   (totality_register), a function to generate the instances of the
   totality property corresponding to such equalities (totality_lemmas)
   and a function to test if two literals correspond to the atoms of a
   previously generated lemma (totality_check).
*/
#ifndef __TOTALITY_H
#define __TOTALITY_H
#include "bool/literal.h"
#include "symbolic/DAG.h"
#include "utils/stack.h"

extern void totality_init(void);
extern void totality_done(void);
extern void totality_register(const TDAG);
extern void totality_process(const TDAG);
extern bool totality_check(Tlit lit1, Tlit lit2);

#endif
