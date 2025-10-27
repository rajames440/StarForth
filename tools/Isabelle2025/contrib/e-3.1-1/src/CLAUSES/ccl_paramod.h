/*
                                  ***   StarForth   ***

  ccl_paramod.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:02.116-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/e-3.1-1/src/CLAUSES/ccl_paramod.h
 */

#ifndef CCL_PARAMOD

#define CCL_PARAMOD

#include <ccl_clausesets.h>
#include "ccl_clausecpos.h"
#include <cte_replace.h>

/*---------------------------------------------------------------------*/
/*                    Data type declarations                           */
/*---------------------------------------------------------------------*/

typedef enum
{
   ParamodPlain = 0,            /* Use standard paramodulation */
   ParamodSim,                  /* Always use simultaneous paramod */
   ParamodOrientedSim,          /* Use simultaneous if rw-literal is
                                   oriented */
   ParamodSuperSim,             /* Always use super-simultaneous paramod */
   ParamodOrientedSuperSim,     /* Use super-simultaneous if rw-literal is
                                   oriented */
   ParamodDecreasingSim,        /* Use sim if rw-literal instance is
                                   orientable */
   ParamodSizeDecreasingSim     /* Use sim if instantiated RHS is
                                   smaller */
}ParamodulationType;

typedef struct
{
   TB_p        bank;
   OCB_p       ocb;
   VarBank_p   freshvars;
   Clause_p    new_orig;
   Clause_p    from;
   CompactPos  from_cpos;
   ClausePos_p from_pos;
   Clause_p    into;
   CompactPos  into_cpos;
   ClausePos_p into_pos;
   bool        subst_is_ho;
}ParamodInfoCell, *ParamodInfo_p;


/*---------------------------------------------------------------------*/
/*                Exported Functions and Variables                     */
/*---------------------------------------------------------------------*/

char*              ParamodStr(ParamodulationType pm_type);
ParamodulationType ParamodType(char *pm_str);


void     ParamodInfoPrint(FILE* out, ParamodInfo_p info);

Clause_p ClausePlainParamodConstruct(ParamodInfo_p ol_desc);
Clause_p ClauseSimParamodConstruct(ParamodInfo_p ol_desc);
Clause_p ClauseSuperSimParamodConstruct(ParamodInfo_p ol_desc);
Clause_p ClauseParamodConstruct(ParamodInfo_p ol_desc,
                                ParamodulationType pm_type);


Term_p ComputeOverlap(TB_p bank, OCB_p ocb, ClausePos_p from, Term_p
            into, TermPos_p pos,  Subst_p subst, VarBank_p
            freshvars);

Eqn_p  EqnOrderedParamod(TB_p bank, OCB_p ocb, ClausePos_p from,
          ClausePos_p into, Subst_p subst, VarBank_p
          freshvars);

Clause_p ClauseOrderedParamod(TB_p bank, OCB_p ocb, ClausePos_p
               from,ClausePos_p into, VarBank_p
               freshvars);

Clause_p ClauseOrderedSimParamod(TB_p bank, OCB_p ocb, ClausePos_p
                                 from,ClausePos_p into, VarBank_p
                                 freshvars);

Clause_p ClauseOrderedSuperSimParamod(TB_p bank, OCB_p ocb, ClausePos_p
                                      from,ClausePos_p into, VarBank_p
                                      freshvars);


Term_p   ClausePosFirstParamodInto(Clause_p clause, ClausePos_p pos,
                                   ClausePos_p from_pos, bool no_top,
                                   ParamodulationType pm_type);
Term_p   ClausePosNextParamodInto(ClausePos_p pos, ClausePos_p
                                  from_pos, bool no_top);

Term_p   ClausePosFirstParamodFromSide(Clause_p from, ClausePos_p
                                       from_pos);
Term_p   ClausePosNextParamodFromSide(ClausePos_p from_pos);

Term_p   ClausePosFirstParamodPair(Clause_p from, ClausePos_p
                                   from_pos, Clause_p into,
                                   ClausePos_p into_pos, bool no_top,
                                   ParamodulationType pm_type);
Term_p   ClausePosNextParamodPair(ClausePos_p from_pos, ClausePos_p
                                  into_pos, bool no_top, ParamodulationType pm_type);

#ifdef ENABLE_LFHO
bool    CheckHOUnificationConstraints(UnificationResult res, UnifTermSide exp_side, Term_p from, Term_p to);
#else
#define CheckHOUnificationConstraints(a,b,c,d) (true)
#endif

#endif

/*---------------------------------------------------------------------*/
/*                        End of File                                  */
/*---------------------------------------------------------------------*/
