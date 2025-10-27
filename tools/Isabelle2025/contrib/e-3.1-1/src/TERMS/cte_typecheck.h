/*
                                  ***   StarForth   ***

  cte_typecheck.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:02.550-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/e-3.1-1/src/TERMS/cte_typecheck.h
 */

#ifndef CTE_TYPECHECK

#define CTE_TYPECHECK

#include <cte_signature.h>
#include <cte_termtypes.h>

/*---------------------------------------------------------------------*/
/*                    Data type declarations                           */
/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/*                Exported Functions and Variables                     */
/*---------------------------------------------------------------------*/


Type_p TypeCheckEq(Sig_p sig, Term_p t);
Type_p TypeCheckDistinct(Sig_p sig, Term_p t);
Type_p TypeCheckArithBinop(Sig_p sig, Term_p t);
Type_p TypeCheckArithConv(Sig_p sig, Term_p t);


bool     TypeCheckConsistent(Sig_p sig, Term_p term);
void     TypeInferSort(Sig_p sig, Term_p term, Scanner_p in);
void     TypeDeclareIsPredicate(Sig_p sig, Term_p term);
void     TypeDeclareIsNotPredicate(Sig_p sig, Term_p term, Scanner_p in);

#endif

/*---------------------------------------------------------------------*/
/*                        End of File                                  */
/*---------------------------------------------------------------------*/
