/*
  --------------------------------------------------------------
  Module for hints, i.e., theory propagation

  Maintains information used to build conflict clauses 
  including hints.

  Some hints have no such information, and have to be
  notified directly with SAT_lit.

  Other hints are notified with the routines of this module
  (hint_CC and hint_LA).

  IMPROVE: The software architecture for handling hints is not
  satisfactory. It is an adaptation of a version where only
  CC produced hints to include hints from linear arithmetics.
  --------------------------------------------------------------
*/

#ifndef __HINT_H
#define __HINT_H
#include "bool/literal.h"
#include "symbolic/DAG.h"

/** \brief initializes module, must be called first */
void hint_init(void);
/** \brief frees used resources, must be called last */
void hint_done(void);

/** \brief propagates hint and stores DAG as explanation */
void hint_CC(Tlit lit, TDAG DAG, bool reversed);
/** \brief gets DAG associated with lit by CC */
TDAG hint_CC_cause(Tlit lit);
/** \brief helper bit for CC */
bool hint_CC_reversed(Tlit lit);

/** \brief propagates hint and stores literal as explanation */
void hint_LA(Tlit lit, Tlit cause);
/** \brief gets lit associated with lit by LA */
Tlit hint_LA_cause(Tlit lit);

/** \brief stores in veriT_conflict all literals implying lit
    \param lit the literal set as hint by the decision procedure
    \remark calls CC_hint_explain or LA_hint_explain */
void hint_explain_dispatch(Tlit lit);

#endif
