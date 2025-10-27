#include "list.h"
#include "term.h"
#include "clause.h"
#include "array.h"
#include "hashmap.h"
#include "msorts.h"

HASHMAP argSort; //funsymbol -> array[sortsymbol]
HASHMAP rSort; //sortsymbol -> list[funsymbol] from range-sort to funsymbols
HASHMAP skolemArgList; //skolemsymbol -> list[variableterms] from skolemsym to argsortvariables
BOOL hasSkolem= FALSE;
void msorts_Init(void)
/**************************************************************
  INPUT:   None.
  RETURNS: void.
  SUMMARY: Initializes msorts internal data-structures
***************************************************************/

{
	msorts_InitLR();
        argSort= hm_Create(4,hm_PointerHash,hm_PointerEqual,FALSE);
        rSort= hm_Create(4,hm_PointerHash,hm_PointerEqual,FALSE);
        skolemArgList= hm_Create(4,hm_PointerHash,hm_PointerEqual,FALSE);
}

void msorts_Free(void)
/**************************************************************
  INPUT:   None.
  RETURNS: void.
  SUMMARY: Frees msorts internal data-structures
***************************************************************/
{
	msorts_FreeLR();
        hm_DeleteWithElement(argSort,(void (*)(POINTER))array_Delete);
        hm_DeleteWithElement(rSort,(void (*)(POINTER))list_Delete);
}


void msorts_SetFunSortsA(SYMBOL sym,ARRAY argSorts) {
        if (argSorts != NULL) {
                hm_Insert(argSort,(POINTER)sym,argSorts);
        }
        hm_InsertListInsertUnique(rSort,(POINTER)symbol_MFunctionSort(sym),(POINTER)sym);
}

BOOL msorts_CheckOrSetArgSortsFromArgList(TERM term,LIST argSorts)
/**
 * Assumes all symbols will be added directly after the first
 */
{
	SYMBOL sym= term_TopSymbol(term);
	hasSkolem= TRUE;
	BOOL Correct= TRUE;
	if (symbol_IsVariable(sym) || symbol_Arity(sym) <= 0 || fol_Equality() == sym) {
		return Correct;
	}
	ARRAY args= array_Create(symbol_Arity(sym));
	ARRAY argsstored= hm_Retrieve(argSort,(POINTER)sym);
	int a=0;
	for (LIST scan= argSorts; !list_Empty(scan); scan=list_Cdr(scan)) {
		TERM t= list_Car(scan);
//		if (!term_IsVariable(t)) {
//			misc_StartErrorReport();
//			misc_ErrorReport("\n In msorts_SetFunArgSortsFromVarList: Input is not a Variablelist.");
//			misc_FinishErrorReport();
//
//		}
		SYMBOL sort= term_GetSort(t);
		if (argsstored == NULL) {
//			symbol_Print(sym); printf(" add arg %d of sort: ",a); symbol_Print(sort); printf(" derived from "); term_PrettyPrint(t); printf("\n");
		} else {
//			symbol_Print(sym); printf(" arg %d is of sort: ",a); symbol_Print(sort);  printf(" derived from "); term_PrettyPrint(t); printf("\n");
		}
		++a;
		args = array_Add(args, (intptr_t) sort);
	}
	if (argsstored == NULL) {
		msorts_SetFunSortsA(sym,args);
		if (symbol_HasProperty(sym,SKOLEM)) {
			LIST argVars= term_CopyTermList(term_ArgumentList(term));
			hm_Insert(skolemArgList,(POINTER)sym,argVars);
		}
	} else {
		int arity= symbol_Arity(sym);
		for (int i=0; i<arity; ++i) {
			if (array_GetElement(args,i) != array_GetElement(argsstored,i)) {
				symbol_Print(sym); printf(" arg %d is of sort       : ",i); symbol_Print(array_GetElement(args,i)); printf("\n");
				symbol_Print(sym); printf(" arg %d should be of sort: ",i); symbol_Print(array_GetElement(argsstored,i)); printf("\n");
				Correct= FALSE;
//				printf("Before: "); term_PrettyPrint(t); printf("\n");
//				LIST argVars= hm_Retrieve(skolemArgList,(POINTER)sym);
//				term_RplacArgumentList(t,term_CopyTermList(argVars));
//				printf("After: "); term_PrettyPrint(t); printf("\n");
				break;
			}
		}
		array_Delete(args);
	}
	return Correct;
}

void msorts_processSortDeclarations(LIST Sorts)
/**************************************************************
  INPUT:   A List which contains the sort declaration in a weird way
           (TODO fix the way not the description).
  RETURNS: void.
  SUMMARY: Sets the Range-Sort of all Symbols which occur in the <Sorts>
***************************************************************/
{
        /* PROCESS SORT DECLARATIONS */
        for (LIST ScanSort=Sorts;!list_Empty(ScanSort);ScanSort=list_Cdr(ScanSort)) {
                POINTER pair=list_Car(ScanSort);
                TERM hassort= list_PairSecond(pair); //special term which encodes sort information
                TERM firstarg= term_FirstArgument(hassort);
                SYMBOL fsym  = term_TopSymbol(firstarg);
                if (symbol_IsFunction(fsym)) {

                        SYMBOL rsort   = term_TopSymbol(term_SecondArgument(hassort));
                        symbol_MSetFunctionSort(fsym,rsort);//*/
                }
                if (  symbol_IsFunction(fsym)
                        ||symbol_IsPredicate(fsym)) {

                        int arity=symbol_Arity(fsym);
                        ARRAY args= NULL;
                        if (arity > 0) {
                                args= array_Create(arity);
                                LIST arguments= term_ArgumentList(term_FirstArgument(hassort));
                                for (LIST scan= arguments; !list_Empty(scan); scan=list_Cdr(scan)) {
                                        array_Add(args, (intptr_t)term_TopSymbol(list_Car(scan)));
                                }
                        }
                        msorts_SetFunSortsA(fsym,args);
                }
        }
        /* SET REST TO TOP */

    LIST functions  = list_NReverse(symbol_GetAllFunctions());//TODO Why reverse?

    for (LIST scan=functions;!list_Empty(scan);scan=list_Cdr(scan)) {
        SYMBOL symbol= (SYMBOL)list_Car(scan);
                if (!symbol_MFunctionSort(symbol)) {
                        symbol_MSetFunctionSort(symbol,fol_Top());
                        int arity= symbol_Arity(symbol);
                        ARRAY args= NULL;
                        if (arity > 0) {
                                args= array_Create(arity);
                                for (int i=0; i<arity;++i) {
                                        array_Add(args, (intptr_t)fol_Top());
                                }
                        }
                        msorts_SetFunSortsA(symbol,args);
                }
    }
    list_Delete(functions);
}


SYMBOL msorts_getArgSort(SYMBOL sym,int arg)
/**************************************************************
  INPUT:   A Symbol and an argument position/index
  RETURNS: The Sort of the argument at position <arg>
  SUMMARY:
***************************************************************/
{
#ifdef CHECK
  if (symbol_Arity(sym) <= arg) {
	  misc_StartErrorReport();
	  misc_ErrorReport("\n In msorts_getArgSort: asked for argument number which is larger than the arity.");
	  misc_FinishErrorReport();
  }
#endif
	ARRAY args= hm_Retrieve(argSort,(POINTER)sym);
#ifdef CHECK
  if (args == NULL) {
	  printf("symbol not found: ");symbol_Print(sym); printf("\n");
	  misc_StartErrorReport();
	  misc_ErrorReport("\n In msorts_getArgSort: symbol not found.");
	  misc_FinishErrorReport();
  }
#endif

	return (SYMBOL) array_GetElement(args,arg);
}


BOOL msorts_SortCheckTerm(TERM term)
/**************************************************************
  INPUT:   A Term
  RETURNS: <True>  if all subterms are of matching Sort
           <False> else [an arguments sorts does not match its enclosing symbols sort]
  SUMMARY:
***************************************************************/
{
	SYMBOL sym=term_TopSymbol(term);
	if (  symbol_IsConstant(sym)
		||term_IsVariable(term)) {
		return TRUE;
	}
	LIST arguments= term_ArgumentList(term);
	int i=0;
	for (LIST scan=arguments; !list_Empty(scan); scan=list_Cdr(scan)) {
		TERM arg= (TERM) list_Car(scan);

		if  ( (( hasSkolem || !symbol_HasProperty(sym,SKOLEM)) && !term_CheckSort(arg,msorts_getArgSort(sym,i)))
			|| !msorts_SortCheckTerm(arg)) {
			return FALSE;
		}
		++i;
	}
	return TRUE;
}

BOOL msorts_SortCheckLiteral(LITERAL Literal)
/**************************************************************
  INPUT:   A Literal
  RETURNS: <True>  if the Literal and all its Terms are of their matching Sort
           <False> else [an arguments sorts does not match its enclosing symbols Sort,
                         or the two sides of the equality have different Sort]
  SUMMARY:
***************************************************************/
{
	TERM Atom= clause_LiteralAtom(Literal);
	if (term_TopSymbol(Atom) == fol_Not()) {
			Atom= term_FirstArgument(Atom);
	}

	if (term_TopSymbol(Atom) == fol_Equality()) {
		TERM lhs= term_FirstArgument(Atom);
		TERM rhs= term_SecondArgument(Atom);
		SYMBOL lsort= term_GetSort(lhs);
		BOOL b= term_CheckSort(rhs,lsort) && msorts_SortCheckTerm(lhs) && msorts_SortCheckTerm(rhs);
		if (b == FALSE) {
			printf("missorted: ");clause_LiteralPrint(Literal);printf("\n");

		}
		return b;
	} else if (symbol_IsPredicate(term_TopSymbol(Atom))) {
		BOOL b= msorts_SortCheckTerm(Atom);
		if (b == FALSE) {
			printf("missorted: ");clause_LiteralPrint(Literal);printf("\n");
		}
		return b;
	} else {
		printf("ERROR\n");
		//TODO error
		return FALSE;
	}
}

BOOL msorts_SortCheckClause(CLAUSE Clause)
/**************************************************************
  INPUT:   A Clause
  RETURNS: <True>  if all its Literal and their Terms are well-sorted
           <False> else
  SUMMARY:
***************************************************************/
{
	int c = clause_NumOfConsLits(Clause);
	int a = clause_NumOfAnteLits(Clause);
	int s = clause_NumOfSuccLits(Clause);
	for (int i=0; i < c+a+s; ++i) {
		if (!msorts_SortCheckLiteral(clause_GetLiteral(Clause,i)))
			return FALSE;
	}
	return TRUE;
}

BOOL msorts_SortCheckClauses(LIST ClauseList)
/**************************************************************
  INPUT:   A List of Clauses
  RETURNS: <True>  if all Clauses and their Literal and their Terms are well-sorted
           <False> else
  SUMMARY:
***************************************************************/
{
	while (!(list_Empty(ClauseList))) {
		CLAUSE clause= list_Car(ClauseList);
		if (!msorts_SortCheckClause(clause)) {
			return FALSE;
		}
		ClauseList = list_Cdr(ClauseList);
	}
	return TRUE;
}

