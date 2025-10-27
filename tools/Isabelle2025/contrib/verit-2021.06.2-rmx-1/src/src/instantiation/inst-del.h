#ifndef __INST_DEL_H
#define __INST_DEL_H

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
  Global variables
  --------------------------------------------------------------
*/

/** \brief associates indexed DAGs to lits in which they appeared */
extern Tstack_var* index_lits;

/** \brief workaround to warn trigger inst I'm trying to avoid loops */
extern bool inst_check_loop;

/** \brief workaround to warn veriT I'm marking the lemmas */
extern bool inst_marking;

/** \brief workaround for storing the round when instantiation finished */
extern unsigned inst_done_round;

/** \brief workaround for storing which strategy yielded instances */
extern Tstrategy inst_successful;

/** \brief associates lits to qnt formulas from which they were derived */
extern Tstack_DAG* lit_qforms;

/*
  --------------------------------------------------------------
  IO interface
  --------------------------------------------------------------
*/

extern void inst_del_init(void);
extern void inst_del_done(void);

extern void inst_promote_vars(void);
extern void inst_promote_clauses(void);
extern void inst_mark_instances(void);
extern void inst_mark_clause(Tclause clause, unsigned clause_id);

#endif
