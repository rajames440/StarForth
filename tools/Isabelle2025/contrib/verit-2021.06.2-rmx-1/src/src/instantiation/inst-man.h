#ifndef __INST_MAN_H
#define __INST_MAN_H

/*
  --------------------------------------------------------------
  Instances management
  --------------------------------------------------------------
*/

#include "bool/bool.h"
#include "congruence/congruence.h"
#include "instantiation/ccfv-bckt.h"
#include "instantiation/ccfv.h"
#include "instantiation/free-vars.h"
#include "instantiation/inst-trigger.h"
#include "instantiation/unify.h"

/*
  --------------------------------------------------------------
  IO interface
  --------------------------------------------------------------
*/

extern void inst(Tstack_DAG* Plemmas);

extern void inst_init(void);
extern void inst_done(void);

/** \brief Quantified formulas asserted in CC */
extern Tstack_DAG CC_quantified;

/* Macro for checking whether terms are congruent. CC_abstract of a fresh term
   is DAG_NULL, therefore the further tests to avoid that two fresh terms which
   are not the same are wrongly seem as congruent */
#define congruent(t1, t2) \
	((CC_abstract(t1) && CC_abstract(t1) == CC_abstract(t2)) || t1 == t2)

#endif
