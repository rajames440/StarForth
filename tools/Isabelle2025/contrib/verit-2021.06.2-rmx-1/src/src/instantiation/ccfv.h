/**
   \file ccfv.h
   \brief Module for searching conflicting instances

   Given a set of groundly consistent literals L and a quantified formula
   Ax1..xn.F a conflicting instantiation \sigma is a substitution s.t. L \wedge
   F\sigma is unsatisfiable. */
#ifndef __CCFV_H
#define __CCFV_H

#include "instantiation/inst-create.h"
#include "instantiation/ujobs.h"
#include "instantiation/unify.h"
#include "proof/proof.h"

/*
  --------------------------------------------------------------
  IO interface
  --------------------------------------------------------------
*/

/**
   \brief initializes module */
extern void CCFV_init(void);

/**
   \brief releases module */
extern void CCFV_done(void);

extern void CCFV_cycle_init(void);
extern void CCFV_cycle_done(void);

/**
   \brief checks all quantified formulas (inedependently) and builds
   conflicting instantiations
   \return all instances created along with the quantified formula/clause they
   instantiate */
extern Tstack_DAGinst CCFV(void);

/*
  --------------------------------------------------------------
  Workaround for triggers
  --------------------------------------------------------------
*/

extern Tstack_unifier CCFV_unify_args(TDAG D0, TDAG D1);

extern void match_DAGs(Tstack_unifier* result, TDAG NGDAG, Tstack_DAG terms);

extern void combine_unifiers(
	Tstack_unifier* result, Tstack_unifier base, Tstack_unifier new);

#endif
