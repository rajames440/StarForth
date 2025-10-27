#ifndef __QNT_UTILS_H
#define __QNT_UTILS_H

#include "symbolic/DAG.h"
#include "symbolic/context-recursion.h"

/**
   \brief copy given triggers and return result
   \param triggers a set of triggers
   \return a copy of the given triggers */
extern Tstack_DAGstack copy_triggers(Tstack_DAGstack triggers);

/**
   \brief add triggers from a DAG into those of the another
   \param orig a formula
   \param dest a formula
   \remark non-destructive */
extern void DAG_append_triggers(TDAG orig, TDAG dest);

/**
   \brief check whether DAG has quantifiers
   \param DAG a formula or term
   \return true if DAG contains quantifiers, false otherwise */
extern bool DAG_quant_f(TDAG DAG);

/**
   \brief checks whether DAG has quantifiers or is under a binder
   \param DAG a formula or term
   \return true if DAG contains quantifiers or is in the matrix of a
   binder, false otherwise */
extern bool DAG_quant_or_under_binder(TDAG DAG);

#endif
