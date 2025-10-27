/************************************************************/
/************************************************************/
/**                                                        **/
/**                     NEXTCLAUSE                         **/
/**                                                        **/
/**        Infrastructure for Many-Sorted SPASS            **/
/**                                                        **/
/**                Author: Daniel Wand                     **/
/**                                                        **/
/**                                                        **/
/**  This program is free software; you can redistribute   **/
/**  it and/or modify it under the terms of the FreeBSD    **/
/**  Licence.                                              **/
/**                                                        **/
/**  This program is distributed in the hope that it will  **/
/**  be useful, but WITHOUT ANY WARRANTY; without even     **/
/**  the implied warranty of MERCHANTABILITY or FITNESS    **/
/**  FOR A PARTICULAR PURPOSE.  See the LICENCE file       **/
/**  for more details.                                     **/
/************************************************************/
/************************************************************/


#ifndef _NEXTCLAUSE_
#define _NEXTCLAUSE_

#include "search.h"
#include "clause.h"
#include "list.h"

CLAUSE nextclause_RankAidedGet(PROOFSEARCH);
CLAUSE nextclause_GoalOrientedGet(PROOFSEARCH );
void nextclause_findConjectures(LIST);
LIST nextclause_getAllowedSymbolCopy();
void nextclause_setFlags(FLAGSTORE ,FLAGSTORE );



void nextclauseweights_Init(void);
void nextclauseweights_Free(void);
void nextclause_HACKFORCELT(PROOFSEARCH ,CLAUSE);
void nextclauseweights_addInputClauses(LIST, HASHMAP);
void nextclauseweights_addLabelToClauseFromClause(intptr_t , intptr_t );
intptr_t nextclauseweights_getRank(CLAUSE);
void nextclauseweights_addClause(CLAUSE);
void nextclauseweights_addLabel(const char* ,intptr_t );

#endif
