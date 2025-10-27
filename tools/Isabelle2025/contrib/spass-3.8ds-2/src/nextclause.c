/************************************************************/
/************************************************************/
/**                                                        **/
/**                       NEXTCLAUSE                       **/
/**                                                        **/
/**                                                        **/
/**                                                        **/
/**                Author: Daniel Wand                     **/
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
/**                                                        **/
/************************************************************/
/************************************************************/

#include "symbol.h"
#include "msorts.h"
#include "nextclause.h"

extern CLAUSE top_SelectClauseDepth(LIST , FLAGSTORE );

#define COUNTERMAX 100
#define COUNTER2MAX 500


LIST allowedsymlist= NULL;
BOOL init= TRUE;

LIST nextclause_getAllowedSymbolCopy() {
	return list_Copy(allowedsymlist);
}
LIST nextclause_addSymbolsTerm(TERM t, LIST symbols) {
	SYMBOL topsym= term_TopSymbol(t);

	//***************************
	//*****Sort Check Update*****
	//***************************

//	if (init) {
//		LIST  args= term_ArgumentList(t);
//		if (!msorts_CheckOrSetArgSortsFromArgList(t,args)) {
//#ifdef CHECK
//			printf(" missorted: "); term_PrettyPrint(t); printf("\n");
//			misc_StartErrorReport();
//			misc_ErrorReport("\n In nextclause_addSymbolsTerm: ill-sorted Term detected.");
//			misc_FinishErrorReport();
//#endif
//		}
//	}

	//***************************
	//***************************
	if ((symbol_IsFunction(topsym) || symbol_IsPredicate(topsym)) && topsym != fol_Equality() && !list_PointerMember(symbols,(POINTER)topsym)) {
		symbols= list_Cons((POINTER)topsym,symbols);
	}
	for (LIST args= term_ArgumentList(t);!list_Empty(args);args=list_Cdr(args)) {
		symbols= nextclause_addSymbolsTerm(list_Car(args),symbols);
	}
	return symbols;
}

LIST nextclause_addSymbolsClause(CLAUSE clause, LIST symbols) {
	int c = clause_NumOfConsLits(clause);
	int a = clause_NumOfAnteLits(clause);
	int s = clause_NumOfSuccLits(clause);
	for (int i=0; i < c+a+s; ++i) {
		TERM Atom= clause_LiteralAtom(clause_GetLiteral(clause,i));
		if (term_TopSymbol(Atom) == fol_Not()) {
			Atom= term_FirstArgument(Atom);
		}
		symbols= nextclause_addSymbolsTerm(Atom,symbols);
	}
	symbols= list_PointerSort(symbols);
	return symbols;
}

BOOL nextclause_containsOnlyAllowedSymbolsTerm(TERM term) {
	LIST termsyms= nextclause_addSymbolsTerm(term,list_Nil());
	//TODO: would be much more efficient (complexity) to use deleteonce in pointerdifference (as lists are kept sorted ...)
	//So a spezialized version would be even better (when the first symbol of clause is left over stop)
	termsyms= list_NPointerDifference(termsyms,allowedsymlist);
	if (list_Empty(termsyms)) {
		return TRUE;
	} else {
		list_Delete(termsyms);
		return FALSE;
	}
}
BOOL nextclause_containsOnlyAllowedSymbols(CLAUSE clause) {

	if (   clause_NumOfAnteLits(clause) == 0
	    && clause_NumOfConsLits(clause) == 0
	    && clause_NumOfSuccLits(clause) == 1) {
//		printf("pos unit clause: "); clause_Print(clause); printf("\n");
	}



	LIST clausesyms= nextclause_addSymbolsClause(clause,list_Nil());
	//TODO: would be much more efficient (complexity) to use deleteonce in pointerdifference (as lists are kept sorted ...)
	//So a spezialized version would be even better (when the first symbol of clause is left over stop)
	clausesyms= list_NPointerDifference(clausesyms,allowedsymlist);
	if (list_Empty(clausesyms)) {
		return TRUE;
	} else {
		list_Delete(clausesyms);
		return FALSE;
	}
}

void printSymbolList(LIST lst) {
	for (LIST scan= lst;!list_Empty(scan);scan=list_Cdr(scan)) {
		 symbol_Print((SYMBOL)list_Car(scan));
		 if (!list_Empty(list_Cdr(scan))) {
			 printf(", ");
		 }
	}
}

void nextclause_printAllowedSymbols() {
//	printf("  symbols: "); printSymbolList(allowedsymlist);printf("\n");
}

void nextclause_findConjectures(LIST Search) {
	//prfs_UsableClauses(
	for (LIST Scan = Search; !list_Empty(Scan); Scan = list_Cdr(Scan)) {
		CLAUSE c= list_Car(Scan);
		if (clause_GetFlag(c, CONCLAUSE)) {
//			printf("conjecture clause: "); clause_Print(c); printf("\n");
			allowedsymlist=nextclause_addSymbolsClause(c,allowedsymlist);
			nextclause_printAllowedSymbols();
		}
	}
	int allowed=0;
	int total=0;
	for (LIST Scan = Search; !list_Empty(Scan); Scan = list_Cdr(Scan)) {
		CLAUSE c= list_Car(Scan);
		if (clause_GetFlag(c, CONCLAUSE)) {
			if (!nextclause_containsOnlyAllowedSymbols(c)) {
				printf("Conjecture Allowed itself is not allowed? \n");
				exit(11);
			}
		} else {
			++total;
			if (nextclause_containsOnlyAllowedSymbols(c)) {
//				printf("allowed Axiom: "); clause_Print(c); printf("\n");
				++allowed;
			}
		}
	}
	printf("\n\nFrom %d Axiom clauses, %d were allowed.\n\n\n",total,allowed);
	init= FALSE;
}



void nextclause_HACKFORCELT(PROOFSEARCH Search,CLAUSE clause) {

	if (flag_GetFlagIntValue(prfs_Store(Search), flag_LT)) {
		if (msorts_isStolenForceLR(clause)) {
			term_EqualitySwap(clause_LiteralAtom(clause_FirstSuccedentLit(clause)));
//			printf("\t\t\tclause now: "); clause_Print(clause); printf("\n");

			if (msorts_isStolenForceLR(clause)) {
//				printf("bad boy 1 swap:\n");
				clause_Print(clause);
//				printf("\nbad boy 2 swap:\n");
				term_EqualitySwap(clause_LiteralAtom(clause_FirstSuccedentLit(clause)));
				clause_Print(clause);
//				printf("\n");
	//			exit(5);
			} else {
	//			printf("bad boy is now good boy\n");
			}
		}
		msorts_setForceLR(prfs_Store(Search),clause);
	}
}

CLAUSE nextclause_RankAidedGet(PROOFSEARCH Search)
/**************************************************************
  INPUT:   A list of clauses and a flag store.
  RETURNS: A clause selected from the list.
  EFFECT:  This function selects a clause with minimal weight.
           If more than one clause has minimal weight and the flag
	   'PrefVar' is TRUE, a clause with maximal number of variable
	   occurrences is selected. If 'PrefVar' is FALSE, a clause with
	   minimal number of variable occurrences is selected.
           If 'PrefVar" is 2 (don't care) then the first clause is selected.
	   If two clauses are equal with respect to the two criteria
	   the clause with the smaller list position is selected.
  CAUTION: THE LIST HAS TO BY SORTED BY WEIGHT IN ASCENDING ORDER!
 ***************************************************************/
{
//#ifdef CHECK
//	/* Check invariant: List has to be sorted by weight (ascending) */
//	LIST Scan;
//	NAT Weight = clause_WeightRanked(list_Car(usableList));
//	for (Scan = list_Cdr(usableList); !list_Empty(Scan); Scan = list_Cdr(Scan)) {
//		NAT NewWeight;
//		NewWeight = clause_WeightRanked(list_Car(Scan));
//		if (NewWeight < Weight) {
//			misc_StartErrorReport();
//			misc_ErrorReport("\n In top_SelectMinimalConWeightClause: clause list ");
//			misc_ErrorReport("isn't sorted by weight");
//			misc_FinishErrorReport();
//		}
//		Weight = NewWeight;
//	}
//#endif

	static BOOL RESORT= TRUE;
	if (RESORT) {
		prfs_SortUsable(Search,FALSE);
		RESORT= FALSE;
	}

	LIST usableList= prfs_UsableClauses(Search);
	static BOOL Input= TRUE;
	/* Put Input into Workedoff */
	if (Input) {
		/* Input Conjecture Clauses (most relevant Clauses for Proof of Theorems) into worked of*/
		for(LIST scan= usableList;!list_Empty(scan);scan=list_Cdr(scan)) {
			CLAUSE clause= (CLAUSE)list_Car(scan);
			if (clause_GetFlag(clause, CONCLAUSE) ) {
				//Search Conjecture related clauses to more depth then the rest
				if (clause_Depth(clause) <= 1) {
					return clause;
				}
			}
		}

		for(LIST scan= usableList;!list_Empty(scan);scan=list_Cdr(scan)) {
			CLAUSE clause= (CLAUSE)list_Car(scan);
			if (clause_Depth(clause) == 0) {
			    return clause;
			}
		}
	}
	Input= FALSE;

	return list_Car(usableList);
}

CLAUSE nextclause_GoalOrientedGet(PROOFSEARCH Search)
/**************************************************************
  INPUT:   A list of clauses and a flag store.
  RETURNS: A clause selected from the list.
  EFFECT:  This function selects a clause with minimal weight.
           If more than one clause has minimal weight and the flag
	   'PrefVar' is TRUE, a clause with maximal number of variable
	   occurrences is selected. If 'PrefVar' is FALSE, a clause with
	   minimal number of variable occurrences is selected.
           If 'PrefVar" is 2 (don't care) then the first clause is selected.
	   If two clauses are equal with respect to the two criteria
	   the clause with the smaller list position is selected.
  CAUTION: THE LIST HAS TO BY SORTED BY WEIGHT IN ASCENDING ORDER!
 ***************************************************************/
{
	LIST usableList= prfs_UsableClauses(Search);
	const BOOL SPEAK= FALSE;
	static int MaxDepth=1;
#ifdef CHECK
	/* Check invariant: List has to be sorted by weight (ascending) */
	LIST Scan;
	NAT Weight = clause_Weight(list_Car(usableList));
	for (Scan = list_Cdr(usableList); !list_Empty(Scan); Scan = list_Cdr(Scan)) {
		NAT NewWeight;
		NewWeight = clause_Weight(list_Car(Scan));
		if (NewWeight < Weight) {
			misc_StartErrorReport();
			misc_ErrorReport("\n In top_SelectMinimalConWeightClause: clause list ");
			misc_ErrorReport("isn't sorted by weight");
			misc_FinishErrorReport();
		}
		Weight = NewWeight;
	}
#endif
	static int counter= 0;//TODO get rid of counter
	static int counter2= 0;//TODO get rid of counter
	if (SPEAK) nextclause_printAllowedSymbols();


	/* Input Conjecture Clauses (most relevant Clauses for Proof of Theorems) into worked of*/
	for(LIST scan= usableList;!list_Empty(scan);scan=list_Cdr(scan)) {
		CLAUSE clause= (CLAUSE)list_Car(scan);
		if (clause_GetFlag(clause, CONCLAUSE) ) {
			//Search Conjecture related clauses to more depth then the rest
			if (clause_Depth(clause) <= 1) {
				if (SPEAK) printf("\nfound conjecture clause\n ");
				return clause;
			} else {
				if (SPEAK) printf("\nfound but skipped conjecture clause due depth of %d with limit of %d.\n",(int)clause_Depth(clause),MaxDepth);
			}
		}
	}

	/* Put Input into Workedoff */
	static BOOL Input= TRUE;
	if (Input) {
		for(LIST scan= usableList;!list_Empty(scan);scan=list_Cdr(scan)) {
			CLAUSE clause= (CLAUSE)list_Car(scan);
			if (clause_Depth(clause) == 0) {
				if (SPEAK) printf("\nfound input clause\n ");
			    return clause;
			}
		}
	}
	Input= FALSE;

	const int CONJDEPTHBONUS= flag_GetFlagIntValue(prfs_Store(Search),flag_DepthConjMax);
	const int MAXDEPTHLIMIT = flag_GetFlagIntValue(prfs_Store(Search),flag_DepthMax);

	CLAUSE minclause= NULL; //used for finding new symbols if all else fails
	while (minclause == NULL) {
		/* Conjecture Clauses most relevant Clauses for Proof of Theorems*/
		for(LIST scan= usableList;!list_Empty(scan);scan=list_Cdr(scan)) {
			CLAUSE clause= (CLAUSE)list_Car(scan);
			if (clause_GetFlag(clause, CONCLAUSE) ) {
				//Search Conjecture related clauses to more depth then the rest
				if (clause_Depth(clause) < MaxDepth + CONJDEPTHBONUS) {
					if (SPEAK) printf("\nfound conjecture clause\n ");
					return clause;
				} else {
					if (SPEAK) printf("\nfound but skipped conjecture clause due depth of %d with limit of %d.\n",(int)clause_Depth(clause),MaxDepth);
				}
			}
		}


		/* Add Ground instances from input and derivable from conjecture */
		for(LIST scan= usableList;!list_Empty(scan);scan=list_Cdr(scan)) {
			CLAUSE clause= (CLAUSE)list_Car(scan);
			if (clause_IsGround(clause)) {
				if (clause_Depth(clause) < MaxDepth) {
					if (SPEAK) printf("\nfound ground clause\n ");
					return clause;
				} else {
					if (SPEAK) printf("\nfound but skipped ground clause due depth of %d with limit of %d.\n",(int)clause_Depth(clause),MaxDepth);
				}
			}
		}

		if (SPEAK) printf("\ncounter %d\ncounter2 %d\ndepht %d\n",counter,counter2,MaxDepth);
		if (counter2 > COUNTER2MAX) {//TODO get rid of constant
			++counter2;
			if (counter2 % flag_GetFlagIntValue(prfs_Store(Search), flag_WDRATIO) == 0)
				return top_SelectClauseDepth(prfs_UsableClauses(Search), prfs_Store(Search));
			else
				return (CLAUSE)list_Car(usableList);
		} else {

			/* find clauses with the same symbols as the conjecture */

			++counter;
			int pos=0;
			for(LIST scan= usableList;!list_Empty(scan);scan=list_Cdr(scan)) {
				++pos;
				CLAUSE clause= (CLAUSE)list_Car(scan);

				if (nextclause_containsOnlyAllowedSymbols(clause)) {
					int length= list_Length(usableList);
					//TODO something nicer?
					//compare to the previous position it was in? if it stays the same bad sign?
					//compare to the previous weight it had? if it stays the same bad sign?
					if (SPEAK) printf("our clause is %d out of %d clauses (pos base on weight).\n",pos,length);
					int minweight= clause_Weight(list_Car(usableList));
					int ourweight= clause_Weight(clause);
					if (SPEAK) printf("our clause weighs %d, min clause weighs %d.\n",ourweight,minweight);
					if (length == pos) {
						break;
					}

					if (SPEAK) printf("given clause depth: %d\n",(int)clause_Depth(clause));
					if (clause_Depth(clause) < MaxDepth) {
						MaxDepth= clause_Depth(clause)+1;
						return clause;
					} else {
						if (SPEAK) printf("\nfound but skipped symbol-allowed clause due depth of %d with limit of %d.\n",(int)clause_Depth(clause),MaxDepth);
					}

				}
			}


			/* Out of Symbols */

			/* Search for Rewriting/Superposition clauses that might help*/
			//TODO also check that non-maximal literals only contain allowed symbols, otherwise not directly useful
			//or just do it anyways?
			for(LIST scan= usableList;!list_Empty(scan);scan=list_Cdr(scan)) {
				CLAUSE clause= (CLAUSE)list_Car(scan);
				for (int s=clause_FirstSuccedentLitIndex(clause); s <= clause_LastSuccedentLitIndex(clause); ++s) {
					LITERAL lit= clause_GetLiteral(clause,s);
					if (clause_LiteralGetFlag(lit,STRICTMAXIMAL)) {
						TERM atom= clause_GetLiteralAtom(clause,s);
						if (fol_IsEquality(atom)) {
							TERM lhs= term_FirstArgument(atom);
							TERM rhs= term_SecondArgument(atom);
							if (   nextclause_containsOnlyAllowedSymbolsTerm(lhs)
								|| nextclause_containsOnlyAllowedSymbolsTerm(rhs) ) {
							//TODO check if smaller (otherwise "useless" to keep inside of allowed symbols)
								if (clause_Depth(clause) < MaxDepth) {
									if (SPEAK) {printf("found strictly maximal pos equality-literal with one side only-allowed symbols");}
									return clause;
								} else {
									if (SPEAK) {printf("found but skipped strictly maximal pos equality-literal with one side only-allowed symbols due depth of %d with limit of %d.\n",(int)clause_Depth(clause),MaxDepth);}
								}
							}
						}
					}
				}
			}

			/* Search for new Symbols */
			//TODO implement hierachical symbol ordering so that clauses that have only conjecture
			//symbols are still prefered after extending the allowed symbols
			/* Search for instances of */
			counter= 0;
			SHARED_INDEX  ShIndex = prfs_WorkedOffSharingIndex(Search);
			int minval= INT_MAX;
			for(LIST scan= usableList;!list_Empty(scan);scan=list_Cdr(scan)) {
				CLAUSE clause= list_Car(scan);
				if (clause_GetFlag(clause, CONCLAUSE)) {
					NAT clauseweight= clause_Weight(clause);
					BOOL clausehaslit= FALSE;
					int c = clause_NumOfConsLits(clause);
					int a = clause_NumOfAnteLits(clause);
					int s = clause_NumOfSuccLits(clause);
					int literals=c+a+s;
					for (int i=0; i < literals; ++i) {
						TERM atom= clause_LiteralAtom(clause_GetLiteral(clause,i));
						if (nextclause_containsOnlyAllowedSymbolsTerm(atom)) {
							LIST instances= st_GetInstance(cont_LeftContext(),sharing_Index(ShIndex),atom);
							for(LIST scanI= instances;!list_Empty(scanI);scanI=list_Cdr(scanI)) {
								TERM t=list_Car(scanI);
								if (clauseweight < minval) {
									if (clause_Depth(clause) < MaxDepth) {
										minval= clauseweight;
										minclause= clause;
										if (nextclause_containsOnlyAllowedSymbolsTerm(t)) {
											clausehaslit= TRUE;
										}
									}
								}
							}
							list_Delete(instances);
						}

					}
				}
			}
			if (minclause == NULL ) {
				if (MaxDepth < MAXDEPTHLIMIT) {
					MaxDepth= MaxDepth +1;
				} else {
					break;
				}
			}
		}
	}
	if (minclause != NULL) {
		if (SPEAK) printf("\nwere allowed: ");
		for (LIST Scan = allowedsymlist; !list_Empty(Scan); Scan = list_Cdr(Scan)) {
			SYMBOL S= (SYMBOL)list_Car(Scan);
			symbol_Print(S); printf(",");
		}
		if (SPEAK) printf("\nnow  allowed: ");
		allowedsymlist= nextclause_addSymbolsClause(minclause,allowedsymlist);
		for (LIST Scan = allowedsymlist; !list_Empty(Scan); Scan = list_Cdr(Scan)) {
			SYMBOL S= (SYMBOL)list_Car(Scan);
			symbol_Print(S); printf(",");
		}
		if (SPEAK) printf("\n");
		return minclause;
	} else {
		++counter2;
		if (SPEAK) printf("increasing counter2: %d\n",counter2);
		CLAUSE clause=(CLAUSE)list_Car(usableList);
		if (clause_Depth(clause) >= MaxDepth) {
			for (LIST scan=usableList; !list_Empty(scan); scan=list_Cdr(scan)) {
				if (clause_Depth((CLAUSE)list_Car(scan)) < MaxDepth) {
					printf("\n WARNING hard fallback to min weight with depth limit selection: depth %d with limit of %d.\n",(int)clause_Depth(list_Car(scan)),MaxDepth);
					printf("returning: "); clause_Print(list_Car(scan)); printf("\n");
					return list_Car(scan);
				}
			}
		}
		if (SPEAK){
			printf("\n WARNING hard fallback to min weight without depth limit selection: depth %d with limit of %d.\n",(int)clause_Depth(clause),MaxDepth);
			printf("returning: "); clause_Print(clause); printf("\n");
		}
		return clause;
	}
}


void nextclause_setFlags(FLAGSTORE InputFlags,FLAGSTORE Flags) {

	if (flag_GetFlagIntValue(Flags, flag_LR)) {
		flag_SetFlagIntValue(Flags,flag_ORD,flag_ORDKBO);
	}
	if (flag_GetFlagIntValue(Flags, flag_HEURISTIC) == flag_HEURISTICRANK) {
		flag_SetFlagIntValue(Flags,flag_LIGHTDOCPROOF,flag_LIGHTDOCPROOFON);
	}
	/* Expand Isabelle flags */
	if (flag_GetFlagIntValue(Flags, flag_ISABELLE)) {
		//BUG FIXES
		flag_SetFlagIntValue(Flags,flag_RAED,flag_RAEDOFF); //TODO remove when AED bug fixed
		flag_SetFlagIntValue(Flags,flag_RUNC,flag_RUNCON);
		flag_SetFlagIntValue(Flags,flag_IChain,flag_CHAININGOFF); //TODO remove when Chaining bug fixed
		flag_SetFlagIntValue(Flags,flag_CNFSTRSKOLEM,flag_CNFSTRSKOLEMOFF); //TODO remove when Strong Skolem bug fixed
		//EXPERIMENTAL FEATURES
		flag_SetFlagIntValue(Flags,flag_LT,flag_LTON);
		flag_SetFlagIntValue(Flags,flag_LR,flag_LRON);
		if (flag_GetFlagIntValue(Flags, flag_LR)) {
			flag_SetFlagIntValue(Flags,flag_ORD,flag_ORDKBO);
		}
		//DEFAULT SETTINGS
		flag_SetFlagIntValue(Flags,flag_DepthMax,2);
		flag_SetFlagIntValue(Flags,flag_DepthConjMax,3);
		flag_SetFlagIntValue(Flags,flag_CNFREDTIMELIMIT,2);
		flag_SetFlagIntValue(Flags,flag_HEURISTIC,flag_HEURISTICGOAL);
		flag_SetFlagIntValue(Flags,flag_RTAUT,flag_RTAUTSYNTACTIC); //flag_RTAUTSEMANTIC
		flag_SetFlagIntValue(Flags,flag_RFREW,flag_RFREWON); //flag_RFREWCRW
		flag_SetFlagIntValue(Flags,flag_RBREW,flag_RBREWON); //flag_RBREWCRW
		flag_SetFlagIntValue(Flags,flag_PGIVEN,flag_PGIVENOFF);
		flag_SetFlagIntValue(Flags,flag_PPROBLEM,flag_PPROBLEMOFF);
		flag_SetFlagIntValue(Flags,flag_DOCPROOF, flag_DOCPROOFON);

		flag_SetFlagIntValue(Flags,flag_SORTS , flag_SORTSMONADICWITHVARIABLE);
		flag_SetFlagIntValue(Flags,flag_VARWEIGHT , 20);

	}

	if (flag_GetFlagIntValue(Flags, flag_ISABELLE) == flag_ISABELLEUNSOUND) {
		for (int i=flag_IEMS; i<flag_IDEF; ++i) {
			flag_SetFlagIntValue(Flags,i,flag_OFF);
		}
		flag_SetFlagIntValue(Flags,flag_ISPR,flag_ON);
		flag_SetFlagIntValue(Flags,flag_ISPL,flag_ON);
		flag_SetFlagIntValue(Flags,flag_IORE,flag_ON);
	}
	/* Input flags have higher precedence */
	flag_TransferSetFlags(InputFlags, Flags);
}
