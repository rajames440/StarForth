/*
                                  ***   StarForth   ***

  cco_eqnresolving.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:02.149-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/e-3.1-1/src/CONTROL/cco_eqnresolving.h
 */

#ifndef CCO_EQNRESOLVING

#define CCO_EQNRESOLVING

#include <ccl_eqnresolution.h>
#include <che_proofcontrol.h>

/*---------------------------------------------------------------------*/
/*                    Data type declarations                           */
/*---------------------------------------------------------------------*/




/*---------------------------------------------------------------------*/
/*                Exported Functions and Variables                     */
/*---------------------------------------------------------------------*/

long ComputeAllEqnResolvents(TB_p bank, Clause_p clause, ClauseSet_p
              store, VarBank_p freshvars);

long ClauseERNormalizeVar(TB_p bank, Clause_p clause, ClauseSet_p
           store, VarBank_p freshvars, bool strong);

#endif

/*---------------------------------------------------------------------*/
/*                        End of File                                  */
/*---------------------------------------------------------------------*/





