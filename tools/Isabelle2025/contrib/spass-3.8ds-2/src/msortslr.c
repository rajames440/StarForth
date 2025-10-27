#include "msorts.h"
#include "foldfg.h"


typedef struct constraint {
	SYMBOL symbol;
	LIST isBiggerAs;
	LIST isSmallerAs;
} CONSTRAINT_NODE, *CONSTRAINT;

CONSTRAINT constraint_Create(SYMBOL s)
/**************************************************************
  INPUT:   A SYMBOL
  RETURNS: void.
  SUMMARY: Creates the constraint structure for this symbol
***************************************************************/
{
	CONSTRAINT c;
	c = memory_Malloc(sizeof(CONSTRAINT_NODE));
	c->symbol= s;
	//c->symbolname= symbol_Name(s);
	c->isBiggerAs= list_Nil();
	c->isSmallerAs= list_Nil();
	return c;
}

HASHMAP constraints= NULL;
LIST constraints_keys= NULL;
LIST forceLR= NULL;

void msorts_InitLR(void)
/**************************************************************
  INPUT:   None.
  RETURNS: void.
  SUMMARY: Initializes constraints HASHMAP
***************************************************************/
{
	constraints= hm_Create(4,hm_PointerHash,hm_PointerEqual,FALSE);
}

void msorts_FreeLR(void)
/**************************************************************
  INPUT:   None.
  RETURNS: void.
  SUMMARY: Frees constraints HASHMAP
***************************************************************/
{
	hm_Delete(constraints);
	list_Delete(forceLR);
}


void msorts_LR(TERM eq);
void msorts_LT(TERM eq) {
	forceLR= list_Cons(term_CopyIterative(eq),forceLR);
}


void msorts_setWeight(const char* id,int weight) {
	symbol_SetWeight(symbol_Lookup(id),weight);
}

BOOL msorts_isStolenForceLR(CLAUSE clause) {
	if (  clause_NumOfConsLits(clause) == 0
	   && clause_NumOfAnteLits(clause) == 0
	   && clause_NumOfSuccLits(clause) == 1
	   ) {
		LITERAL lit= clause_FirstSuccedentLit(clause);
		TERM atom=clause_LiteralAtom(lit);
		if (term_TopSymbol(atom) == fol_EQUALITY) {
			TERM copy= term_Copy(atom);//TODO delete
			term_EqualitySwap(copy);
			for (LIST scan= forceLR; !list_Empty(scan); scan = list_Cdr(scan)) {
				TERM forceLRT= (TERM)list_Car(scan);
				if (!unify_VariationVar(cont_LeftContext(),atom,forceLRT) && !term_IsVariable(term_SecondArgument(forceLRT))) {
					cont_Reset();
					if (unify_VariationVar(cont_LeftContext(),forceLRT,copy)) {
		//				printf("is Forced LR: %p,%p: ",copy,forceLR); term_PrettyPrint(copy); printf("|\n");
						clause_LiteralSetOrderStatus(lit,ord_GREATER_THAN);
	//					if (clause_LiteralIsOrientedEquality(lit)) {
//							printf("\n\nHACK\t: reorienting literal according to :lt flag");
//							printf("\n DEB\t\tatom  : "); term_Print(atom);
//							printf("\n DEB\t\tshould: "); term_Print(copy);
//							printf("\n DEB\t\tclause: "); clause_Print(clause);
	//					}
						return TRUE;
					}
				}
				cont_Reset();
			}
		}
	}
	return FALSE;
}

void msorts_setForceLR(FLAGSTORE Flags, CLAUSE clause) {
	if (flag_GetFlagIntValue(Flags, flag_LT)) {

		if (  clause_NumOfConsLits(clause) == 0
				&& clause_NumOfAnteLits(clause) == 0
				&& clause_NumOfSuccLits(clause) == 1
		) {
			LITERAL lit= clause_FirstSuccedentLit(clause);
			TERM atom=clause_LiteralAtom(lit);
			for (LIST scan= forceLR; !list_Empty(scan); scan = list_Cdr(scan)) {
				TERM forceLRT= (TERM)list_Car(scan);
				if (unify_VariationVar(cont_LeftContext(),atom,forceLRT)) {
					clause_LiteralSetOrderStatus(lit,ord_GREATER_THAN);
//									if (clause_LiteralIsOrientedEquality(lit)) {
//										printf("\tworked\n");
//									}
				}
				cont_Reset();

			}
		}
	}
}
//void msorts_processForceLR(FLAGSTORE Flags, LIST clauselist) {
//	while (!(list_Empty(clauselist))) {
//		CLAUSE clause= list_Car(clauselist);
//		msorts_setForceLR(Flags, clause);
//		clauselist = list_Cdr(clauselist);
//	}
//}
void msorts_LR(TERM eq)
/**************************************************************
  INPUT:   An Equation-Term
  RETURNS: void.
  SUMMARY: Adds an constraint that the left-hand side of the
           equation has to be bigger than the right-hand side
***************************************************************/
{
	if (term_TopSymbol(eq) == fol_EQUALITY) {
		TERM lhs= term_FirstArgument(eq);
		SYMBOL lsym = term_TopSymbol(lhs);
		TERM rhs= term_SecondArgument(eq);
		SYMBOL rsym = term_TopSymbol(rhs);
		if (symbol_Equal(lsym,rsym)) {
			printf(" Warning ignoring \"uneasy\" KBO eq:lr: "); term_PrettyPrint(eq); printf("\n");
			return;
		} else {
//			printf(" DEBUG eq:lr: "); term_PrettyPrint(eq); printf("\n");
		}
		BOOL Found= FALSE;
		if (term_IsVariable(lhs) || term_IsVariable(rhs))
			return;

		CONSTRAINT lc= (CONSTRAINT) hm_RetrieveFound(constraints,(POINTER)lsym,&Found);
		if (Found == FALSE) {
			lc= constraint_Create(lsym);
			hm_Insert(constraints,(POINTER)lsym,lc);
			constraints_keys= list_Cons((POINTER)lsym,constraints_keys);
		}
		if (!list_PointerMember(lc->isBiggerAs,(POINTER)rsym)) {
			lc->isBiggerAs= list_Cons((POINTER)rsym,lc->isBiggerAs);
		}

		Found= FALSE;
		CONSTRAINT rc= (CONSTRAINT) hm_RetrieveFound(constraints,(POINTER)rsym,&Found);
		if (Found == FALSE) {
			rc= constraint_Create(rsym);
			hm_Insert(constraints,(POINTER)rsym,rc);
			constraints_keys= list_Cons((POINTER)rsym,constraints_keys);
		}
		if (!list_PointerMember(rc->isSmallerAs,(POINTER)lsym)) {
			rc->isSmallerAs= list_Cons((POINTER)lsym,rc->isSmallerAs);
		}
	}
}

LIST order=NULL; //in the end one possible order
LIST rounds=NULL; //list of list of symbols (inner list symbols that have no constraint in precedence between them)
void msorts_removeFromLR(LIST symbols)
/**************************************************************
  INPUT:   A LIST of SYMBOLs
  RETURNS: void.
  SUMMARY: removes symbols from constraints_key,
           and from all entries of the new constrains_key
  MEMORY : ERROR: has to free entries but does not do so yet
***************************************************************/
{
	rounds=list_Cons((POINTER)symbols,rounds);
	for (LIST ScanS=symbols;!list_Empty(ScanS);ScanS=list_Cdr(ScanS)) {
		SYMBOL sym= (SYMBOL) list_Car(ScanS);
		constraints_keys= list_PointerDeleteElement(constraints_keys,(POINTER)sym);
	}
	if (list_Empty(constraints_keys)) {
		return;
	}
	for (LIST Scan=constraints_keys;!list_Empty(Scan);Scan=list_Cdr(Scan)) {
		SYMBOL key= (SYMBOL) list_Car(Scan);
		CONSTRAINT c= (CONSTRAINT) hm_Retrieve(constraints,(POINTER)key);
		for (LIST ScanS=symbols;!list_Empty(ScanS);ScanS=list_Cdr(ScanS)) {
			SYMBOL sym= (SYMBOL) list_Car(ScanS);
			c->isBiggerAs= list_PointerDeleteElement(c->isBiggerAs,(POINTER)sym);
			c->isSmallerAs= list_PointerDeleteElement(c->isSmallerAs,(POINTER)sym);
		}
	}
}


void msorts_solveLRINTERN(PRECEDENCE InputPrecedence, LIST preferablySmallSymbols)
/**************************************************************
  INPUT:   A PRECEDENCE which is supposed to be updated, but it is not, yet
  RETURNS: void.
  SUMMARY: calculates recursively an ordering which satisfies the constraints added
           to the constraints HASHMAP
***************************************************************/
{
	LIST precedence= list_Nil(); //all symbols which are not smaller than anything else (in this round)

	//Two loops that do the same, but first loops tries first the preferred symbols
	for (LIST Scan= preferablySmallSymbols;!list_Empty(Scan);Scan=list_Cdr(Scan)) {
		SYMBOL key= (SYMBOL) list_Car(Scan);
		if (!list_PointerMember(order,(POINTER)key)) {
			CONSTRAINT c= (CONSTRAINT) hm_Retrieve(constraints,(POINTER)key);
			if (c != NULL) {
				if (list_Empty(c->isSmallerAs)){
					precedence= list_Cons((POINTER)key,precedence);
					order     = list_Cons((POINTER)key,order);
				}
			}
		}
	}
	if (!list_Empty(precedence)) { //if we have found some prefered symbols
		msorts_removeFromLR(precedence); //remove/finish them
		msorts_solveLRINTERN(InputPrecedence,preferablySmallSymbols); //start next round
	}
	//else remove some non prefered symbols
	for (LIST Scan=constraints_keys;!list_Empty(Scan);Scan=list_Cdr(Scan)) {
		SYMBOL key= (SYMBOL) list_Car(Scan);
		CONSTRAINT c= (CONSTRAINT) hm_Retrieve(constraints,(POINTER)key);
		if (list_Empty(c->isSmallerAs)){
			precedence= list_Cons((POINTER)key,precedence);
			order     = list_Cons((POINTER)key,order);
		}
	}
	msorts_removeFromLR(precedence); //remove this rounds symbols
	if (!list_Empty(constraints_keys)) { //if there are still symbols
		if (list_Empty(precedence)) {//if there has been no progress last round
			printf("Warning: equal:lr information is contradicting at least for KBO constraint generation.");
			for (LIST scan=constraints_keys; !list_Empty(scan); scan=list_Cdr(scan)) {
				SYMBOL key= (SYMBOL) list_Car(scan);
				CONSTRAINT c= (CONSTRAINT) hm_Retrieve(constraints,(POINTER)key);

				printf(" symbol: "); symbol_Print(c->symbol); printf("\n");
				printf("\tsmaller terms: ");
				for (LIST smaller=c->isSmallerAs; !list_Empty(smaller); smaller=list_Cdr(smaller)) {
					printf("\t\t"); symbol_Print((SYMBOL)list_Car(smaller)); printf("\n ");
				}
				printf("\n"); printf("\tbigger terms: ");
				for (LIST bigger=c->isBiggerAs; !list_Empty(bigger); bigger=list_Cdr(bigger)) {
					printf("\t\t"); symbol_Print((SYMBOL)list_Car(bigger)); printf("\n ");
				}
				printf("\n");

			}
			//exit(22); TODO
			return;
		}
		msorts_solveLRINTERN(InputPrecedence,preferablySmallSymbols); //start next round
	}
}
LIST msorts_solveLR(PRECEDENCE InputPrecedence, LIST preferablySmallSymbols)
/**************************************************************
	  INPUT:   A PRECEDENCE which is supposed to be updated, but it is not, yet
	  RETURNS: void.
	  SUMMARY:
***************************************************************/
{
	order= list_Nil();
	rounds= list_Nil();
	msorts_solveLRINTERN(InputPrecedence,preferablySmallSymbols);
	printf("A precedence of symbols which satisfies all compatible equal:lr annotations (the actual ordering is in general less restricted):\n\t[");
    for (LIST Scan=order;!list_Empty(Scan);Scan=list_Cdr(Scan)) {
		symbol_Print((SYMBOL) list_Car(Scan)); printf(" < ");
	}
	printf("]\n");
	return list_NReverse(order);
}
