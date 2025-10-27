/**
   \file ujobs.h
   \brief Module for handling unification jobs

   A unification job is a triple <D0, pol, D1> in which D0, D1 are DAGs and pol
   is a boolean.  At least D0 is nonground. There are two kinds of unification
   jobs, determined by the polarity:
   - if pol = true, find unifiers u such that D0u = D1u
   - if pol = false, find unifiers u such that D0u != D1u

   All found unifiers, if any, are associated with <D0, pol, D1>, therefore
   avoiding the recomputation of these unifiers whenever the same job occurs
   again.

   Such jobs are indexed primarily by D0, then by pol and finally by D1. */
#ifndef __UJOBS_H
#define __UJOBS_H

#include "instantiation/unify.h"

/*
  --------------------------------------------------------------
  Initialization / Release
  --------------------------------------------------------------
*/

/**
   \brief initializes module */
extern void ujobs_init(void);

/**
   \brief releases module */
extern void ujobs_done(void);

/* PF2HB: What is an instantiation cycle ? */
/**
   \brief releases context dependent information at the end of each
   instantiation cycle */
extern void ujobs_done_cycle(void);

/*
  --------------------------------------------------------------
  Handling ujobs
  --------------------------------------------------------------
*/

/**
   \brief adds result of ujob to index
   \param NGDAG a nonground DAG part of ujob
   \param UDAG a DAG part of ujob
   \param pol polarity of ujob
   \param result resulting unifiers of ujob */
extern void set_ujob(TDAG NGDAG, TDAG UDAG, bool pol, Tstack_unifier result);

/**
   \brief retrieves result of ujob from index
   \param result set accumulating resulting unifiers
   \param NGDAG a non-ground DAG part of ujob
   \param UDAG a DAG part of ujob
   \param pol polarity of ujob
   \param new_vars variables in the context of ujob
   \return true if ujob has already been computed, false otherwise
   \remark the result of a ujob is stored with unifiers based on the variables
   of that context. The same ujob may appear in a different context, however, so
   although the same DAGs compose the job the resulting unifiers must be reset
   to the current variables.

   E.g. in the context of a clause with free variables {x1, x2} the ujob <fx1,
   T, ft> will be indexed with the resulting unifier {x1/t, x2/x2}. If this same
   ujob appear in a context with free variables {x1, x3} the retrieved unifier
   will be {x1/t, x3/x3} */
extern bool retrieve_ujob(
	Tstack_unifier* result, TDAG NGDAG, TDAG UDAG, bool pol, Tstack_DAG new_vars);

#endif
