/*
                                  ***   StarForth   ***

  cte_subst.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:02.467-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/e-3.1-1/src/TERMS/cte_subst.h
 */

#ifndef CTE_SUBST

#define CTE_SUBST

#include <clb_pstacks.h>
#include <clb_pqueue.h>
#include <cte_termbanks.h>

/*---------------------------------------------------------------------*/
/*                    Data type declarations                           */
/*---------------------------------------------------------------------*/

typedef PStackCell SubstCell;
typedef PStack_p   Subst_p;

/*---------------------------------------------------------------------*/
/*                Exported Functions and Variables                     */
/*---------------------------------------------------------------------*/

#define SubstAlloc()    PStackAlloc()
#define SubstFree(junk) PStackFree(junk)

#define SubstDelete(junk) SubstBacktrack(junk);SubstFree(junk)
#define SubstDeleteSkolem(junk) SubstBacktrackSkolem(junk);SubstFree(junk)
#define SubstIsEmpty(subst) PStackEmpty(subst)

static inline PStackPointer SubstAddBinding(Subst_p subst, Term_p var, Term_p bind);
bool          SubstBacktrackSingle(Subst_p subst);
int           SubstBacktrackToPos(Subst_p subst, PStackPointer pos);
int           SubstBacktrack(Subst_p subst);

PStackPointer SubstNormTerm(Term_p term, Subst_p subst, VarBank_p vars, Sig_p sig);

bool          SubstBindingPrint(FILE* out, Term_p var, Sig_p sig, DerefType deref);
long          SubstPrint(FILE* out, Subst_p subst, Sig_p sig, DerefType deref);
bool          SubstIsRenaming(Subst_p subt);
bool          SubstHasHOBinding(Subst_p subt);

PStackPointer SubstBindAppVar(Subst_p subst, Term_p var,
                              Term_p term, int up_to,
                              TB_p bank);

void          SubstBacktrackSkolem(Subst_p subst);
void          SubstSkolemizeTerm(Term_p term, Subst_p subst, Sig_p sig);
void          SubstCompleteInstance(Subst_p subst, Term_p term,
                                    Term_p default_binding);


/*-----------------------------------------------------------------------
//
// Function: SubstAddBinding()
//
//   Perform a new binding and store it in the subst. Return the old
//   stackpointer (i.e. the value that you'll have to backtrack to to
//   get rid of this binding).
//
//
// Global Variables: -
//
// Side Effects    : Changes bindings, adds to the substitution.
//
/----------------------------------------------------------------------*/

PStackPointer SubstAddBinding(Subst_p subst, Term_p var, Term_p bind)
{
   PStackPointer ret = PStackGetSP(subst);

   assert(subst);
   assert(var);
   assert(bind);
   assert(TermIsFreeVar(var));
   assert(!(var->binding));
   //assert(problemType == PROBLEM_HO || !TermCellQueryProp(bind, TPPredPos)
   //      || bind->f_code == SIG_TRUE_CODE || bind->f_code == SIG_FALSE_CODE); // Skolem symbols also
   assert(var->type);
   assert(bind->type);
   assert(var->type == bind->type);

   /* printf("# %ld <- %ld \n", var->f_code, bind->f_code); */
   var->binding = bind;
   PStackPushP(subst, var);

   return ret;
}

#endif

/*---------------------------------------------------------------------*/
/*                        End of File                                  */
/*---------------------------------------------------------------------*/
