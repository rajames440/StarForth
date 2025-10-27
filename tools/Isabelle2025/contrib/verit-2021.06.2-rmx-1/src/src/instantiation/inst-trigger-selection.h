#ifndef __INST_TRIGGER_SELECTION_H
#define __INST_TRIGGER_SELECTION_H

#include "congruence/congruence.h"
#include "symbolic/DAG.h"
#include "symbolic/polarities.h"

extern void triggers_sel_init(void);
extern void triggers_sel_done(void);

/*
  --------------------------------------------------------------
  Triggers
  --------------------------------------------------------------
*/

/**
   \brief inspects the whole formula, adds trigger to every quantified formula
   without triggers, and ensures the triggers are on the formula itself, not on
   the body
   \param DAG the formula */
extern void set_triggers_old(TDAG DAG);

/**

   \brief computes triggers for a (universally) quantified formula and its
   quantified subformulas

   \param DAG a formula
   \param previous_vars variables from previous quantifiers
   \param triggers accumulates triggers found in nested quantifiers

   \remark assumes that DAG is in NNF and that no variable is bound by more
   than one quantifier (stronger than ca pture, as it does not require that the
   variable be under the scope of those quantifiers) */
extern void set_triggers(
	TDAG src, Tstack_DAG previous_vars, Tstack_DAGstack* triggers, Tpol pol);

/**

   \brief computes nested triggers for a (universally) quantified formula and its
   quantified subformulas

   \param DAG a formula
   \param previous_vars variables from previous quantifiers
   \param triggers accumulates triggers found in nested quantifiers

   \remark assumes that DAG is in NNF and that no variable is bound by more
   than one quantifier (stronger than ca pture, as it does not require that the
   variable be under the scope of those quantifiers) */
extern void set_nested_triggers(
	TDAG DAG, Tstack_DAG previous_vars, Tstack_DAGstack* triggers);

/** Should disappear */
extern void inst_pre(TDAG src);

#endif
