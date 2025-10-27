#ifndef __INST_SYMBS_H
#define __INST_SYMBS_H

/*
  --------------------------------------------------------------
  Handles deletion or dismissal of instances
  --------------------------------------------------------------
*/

#include "bool/bool.h"
#include "congruence/congruence.h"
#include "instantiation/ccfv-bckt.h"
#include "instantiation/ccfv.h"
#include "instantiation/free-vars.h"
#include "instantiation/inst-strategy.h"
#include "instantiation/inst-trigger.h"
#include "instantiation/unify.h"

/*
  --------------------------------------------------------------
  Scoring, Symbols
  --------------------------------------------------------------
*/

/**
   \brief marks the symbol occurrences throughout a given DAG
   \remark Only occurrences in non-ground DAGs count */
typedef struct OSymb
{
	Tsymb symbol;
	unsigned nb_occurs;
} OSymb;

TSstack(_OSymb, OSymb); /* typedefs Tstack_OSymb */

extern void DAG_prop_symbols_free(Tstack_OSymb* symbs);

/**
   \brief recursevely resets DAG_tmp casted as Tstack_OSymb for DAG
   \param DAG a DAG */
extern void DAG_tmp_reset_symbs(TDAG DAG);

/**
   \brief collects all predicate and function symbols occurring in non-ground
   sub-DAGs of given DAG
   \param DAG the DAG being evaluated
   \param is_pred whether DAG is an atom or a term
   \param vars the variables to define groundness */
extern void set_symbs(TDAG DAG, bool is_pred, Tstack_DAG vars);

/**
   \brief computes unification potential of an atom along with its polarity
   \param DAG an atom
   \param pol polarity
   \return the scoring value, with the smaller meaning the easier
   \remark Currently the features considered are:
   - whether DAG has only variables or fresh terms
   - polarity (positives first)
   - index size of predicate/function symbols occurring in the DAG */
extern unsigned score_lit(TDAG DAG, bool pol);

#define symbs_of ((Tstack_OSymb*)DAG_tmp)

#endif
