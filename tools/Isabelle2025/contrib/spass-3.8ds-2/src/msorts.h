/************************************************************/
/************************************************************/
/**                                                        **/
/**                       MSORTS                           **/
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


#ifndef _MSORTS_
#define _MSORTS_

/**************************************************************/
/* Includes                                                   */
/**************************************************************/

#include "clause.h"
#include "symbol.h"
#include "list.h"

/**************************************************************/
/* Structures                                                 */
/**************************************************************/

/**************************************************************/
/* Functions                                                  */
/**************************************************************/
void msorts_Init(void);
void msorts_Free(void);


#include "search.h"
BOOL msorts_SortCheckClause(CLAUSE );
BOOL msorts_SortCheckClauses(LIST );
LIST msorts_GetFunsForSort(SYMBOL );
ARRAY msorts_GetArgSortsForFun(SYMBOL );
BOOL msorts_CheckOrSetArgSortsFromArgList(TERM ,LIST );
void msorts_processSortDeclarations(LIST );



void msorts_InitLR(void);
void msorts_FreeLR(void);
void msorts_setForceLR(FLAGSTORE, CLAUSE);
void msorts_setForceLR(FLAGSTORE, CLAUSE);
void msorts_LR(TERM);
void msorts_LT(TERM);
LIST msorts_solveLR(PRECEDENCE , LIST );
void msorts_setWeight(const char* ,int );
BOOL msorts_isStolenForceLR(CLAUSE );

#endif

