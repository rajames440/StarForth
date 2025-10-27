/**************************************************************/
/* ********************************************************** */
/* *                                                        * */
/* *                     SYMBOLS                            * */
/* *                                                        * */
/* *  $Module:   SYMBOL                                     * */ 
/* *                                                        * */
/* *  Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001      * */
/* *  MPI fuer Informatik                                   * */
/* *                                                        * */
/* *  This program is free software; you can redistribute   * */
/* *  it and/or modify it under the terms of the FreeBSD    * */
/* *  Licence.                                              * */
/* *                                                        * */
/* *  This program is distributed in the hope that it will  * */
/* *  be useful, but WITHOUT ANY WARRANTY; without even     * */
/* *  the implied warranty of MERCHANTABILITY or FITNESS    * */
/* *  FOR A PARTICULAR PURPOSE.  See the LICENCE file       * */
/* *  for more details.                                     * */
/* *                                                        * */
/* *                                                        * */
/* $Revision: 1.12 $                                         * */
/* $State: Exp $                                            * */
/* $Date: 2011-11-27 14:43:00 $                             * */
/* $Author: weidenb $                                       * */
/* *                                                        * */
/* *             Contact:                                   * */
/* *             Christoph Weidenbach                       * */
/* *             MPI fuer Informatik                        * */
/* *             Stuhlsatzenhausweg 85                      * */
/* *             66123 Saarbruecken                         * */
/* *             Email: spass@mpi-inf.mpg.de                * */
/* *             Germany                                    * */
/* *                                                        * */
/* ********************************************************** */
/**************************************************************/


/* $RCSfile: symbol.c,v $ */

#include "symbol.h"
#include "msorts.h"
/**************************************************************/
/* previously Inlined Functions                               */
/**************************************************************/
/**************************************************************/
/* Symbol Comparisons                                         */
/**************************************************************/

 BOOL symbol_Equal(SYMBOL A, SYMBOL B)
{
  return A==B;
}

 BOOL symbol_IsSignature(SYMBOL S)
{
  return S < 0;
}

 int symbol_Type(SYMBOL ActSymbol)
{
  symbol_CheckNoVariable(ActSymbol);
  return (-ActSymbol) & symbol_TYPEMASK;
}

 BOOL symbol_IsJunctor(SYMBOL S)
{
  return (symbol_IsSignature(S) && symbol_Type(S) == symbol_JUNCTOR);
}

 BOOL symbol_IsFunction(SYMBOL S)
{
  return (symbol_IsSignature(S) &&
	  (symbol_Type(S) == symbol_FUNCTION ||
	   symbol_Type(S) == symbol_CONSTANT));
}

 BOOL symbol_IsConstant(SYMBOL S)
{
  return (symbol_IsSignature(S) && symbol_Type(S) == symbol_CONSTANT);
}

 BOOL symbol_IsPredicate(SYMBOL S)
{
  return (symbol_IsSignature(S) && symbol_Type(S) == symbol_PREDICATE);
}

 BOOL symbol_IsVariable(SYMBOL S)
{
  return S > 0;
}

 BOOL symbol_IsStandardVariable(SYMBOL S)
{
  return symbol_IsVariable(S) && (S <= symbol__MAXSTANDARDVAR);
}

 BOOL symbol_IsIndexVariable(SYMBOL S)
{
  return (S > symbol__MAXSTANDARDVAR) && (S <= symbol__MAXINDEXVAR);
}

 BOOL symbol_IsComplex(SYMBOL S)
{
  return (!symbol_IsVariable(S) && !symbol_IsConstant(S));
}

 BOOL symbol_IsSuccessor(SYMBOL S, SYMBOL P)
{
  return S > P;
}


/**************************************************************/
/* Symbol Manipulation                                        */
/**************************************************************/

 int symbol_GetInitialStandardVarCounter(void)
{
  return 0;
}

 int symbol_GetInitialIndexVarCounter(void)
{
  return symbol__MAXSTANDARDVAR;
}

 int symbol_FirstIndexVariable(void)
{
  return symbol__MAXSTANDARDVAR + 1;
}

 int symbol_LastIndexVariable(void)
{
  return symbol_INDEXVARCOUNTER;
}

/* Special predefined symbols            */

#define symbol__NULL 0

 int symbol_MaxVars(void)
{
  return symbol__MAXVARIABLES;
}

 int symbol_MaxConsts(void)
{
  return symbol__MAXSIGNATURE;
}

 int symbol_MaxBaseSorts(void)
{
  return symbol__MAXSIGNATURE;
}

 int symbol_TypeBits(void)
{
  return symbol_TYPEBITS;
}

 int symbol_Null(void)
{
  return 0;
}

 int symbol_ActIndex(void)
{
  return symbol_ACTINDEX;
}

 void symbol_ResetSkolemIndex(void)
{
  symbol_ACTSKOLEMFINDEX = 0;
  symbol_ACTSKOLEMCINDEX = 0;
  symbol_ACTSKOLEMPINDEX = 0;
  symbol_ACTSKOLEMAINDEX = 0;
}


/**************************************************************/
/* ********************************************************** */
/* *                                                        * */
/* *  MEMORY MANAGEMENT                                     * */
/* *                                                        * */
/* ********************************************************** */
/**************************************************************/

 SIGNATURE symbol_GetSignature(void)
{
  return (SIGNATURE) memory_Malloc(sizeof(SIGNATURE_NODE));
} 


/**************************************************************/
/* Symbol Creation                                            */
/**************************************************************/

 SYMBOL symbol_CreateStandardVariable(void)
/***************************************************************
  INPUT:   None
  RETURNS: A new symbol for a new standard variable numbered according to
           symbol_STANDARDVARCOUNTER
  SUMMARY: Creates a new standard variable symbol.
  EFFECTS: None
****************************************************************/
{

  if (symbol_STANDARDVARCOUNTER >= symbol__MAXSTANDARDVAR) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In symbol_CreateStandardVariable: Number of standard variables exceeded.\n");
    misc_FinishErrorReport();
   }


  return (++symbol_STANDARDVARCOUNTER);
}


 SYMBOL symbol_CreateIndexVariable(void)
/***************************************************************
  INPUT:   None
  RETURNS: A new symbol for a new index variable numbered according to
           symbol_INDEXVARCOUNTER
  SUMMARY: Creates a new index variable symbol.
  EFFECTS: None
****************************************************************/
{

  if (symbol_INDEXVARCOUNTER >= symbol__MAXINDEXVAR) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In symbol_CreateIndexVariable: Number of index variables exceeded.\n");
    misc_FinishErrorReport();
  }

  return (++symbol_INDEXVARCOUNTER);
}


 SYMBOL symbol_NextIndexVariable(SYMBOL Variable)
{
#ifdef CHECK
  if ((Variable != symbol_GetInitialIndexVarCounter() &&
       !symbol_IsIndexVariable(Variable)) ||
      Variable == symbol__MAXINDEXVAR) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In symbol_NextVariable: Illegal input.\n");
    misc_FinishErrorReport();
  }
#endif

  return (Variable + 1);
}


 void symbol_SetStandardVarCounter(SYMBOL Variable)
{
#ifdef CHECK
  if (Variable != symbol_GetInitialStandardVarCounter() &&
      !symbol_IsStandardVariable(Variable)) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In symbol_SetStandardVarCounter: Illegal input.\n");
    misc_FinishErrorReport();
  } 
  else 
    if (Variable >= symbol__MAXSTANDARDVAR) {
      misc_StartErrorReport();
      misc_ErrorReport("\n In symbol_SetStandardVarCounter: Number of standard variables exceeded.\n");
      misc_FinishErrorReport();
    }
#endif

  symbol_STANDARDVARCOUNTER = Variable;
}

 SYMBOL symbol_FirstVariable(void)
{
  return 1;
}

 BOOL symbol_GreaterVariable(SYMBOL Var1, SYMBOL Var2)
{
  return Var1 > Var2;
}

 void symbol_ResetStandardVarCounter(void)
{
  symbol_STANDARDVARCOUNTER = symbol_GetInitialStandardVarCounter();
}

/**************************************************************/
/* Symbol Access                                              */
/**************************************************************/

 int symbol_VarIndex(SYMBOL ActSymbol)
{
  return ActSymbol;
}

 int symbol_NormVar(SYMBOL ActSymbol)
{
  /* Normalization of variables s.t. the index of the variable
     is normalized starting always with 1:
     Standard variables are already normalized.
     Index variables are decreased by the number of the
     underlying standard variables. */
  return (ActSymbol <= symbol__MAXSTANDARDVAR) ? ActSymbol : (ActSymbol - symbol__MAXSTANDARDVAR);
}

/* The name, index and arity macros are only defined for signature   */
/* elements not for variables. The type of the symbol is not checked */
/* by the macros.                                                    */

 int symbol_Index(SYMBOL ActSymbol)
{
  symbol_CheckNoVariable(ActSymbol);
  return (-ActSymbol) >> symbol_TYPESTATBITS;
}

 SYMBOL symbol_SignatureSymbol(int ActIndex, int Type, int Status)
{
  return -((ActIndex << symbol_TYPESTATBITS)
	   | (Status << symbol_TYPEBITS)
	   | Type);
}

 SIGNATURE symbol_Signature(int Index)
  /* Returns the signature of the symbol with <Index> or NULL */
  /* if the symbol was deleted */
{
  symbol_CheckIndexInRange(Index);
  return symbol_SIGNATURE[Index];
}

 SYMBOL symbol_GetSigSymbol(int Index)
{
  return symbol_Signature(Index)->info;
}

 int symbol_Stat(SYMBOL ActSymbol)
{
  symbol_CheckNoVariable(ActSymbol);
  return ((-ActSymbol) & symbol_STATMASK) >> symbol_TYPEBITS;
}

 SYMBOL symbol_ChangeType(SYMBOL S, int Type)
/**************************************************************
  INPUT:   A symbol and a new type for that symbols.
  RETURNS: A new symbol that is the old symbol with type changed to <Type>,
           therefore the return value is different from <S>.
  EFFECT:  Uses the signature memory of the input symbol.
  CAUTION: Usage only allowed by the parsing modules!!!
***************************************************************/
{
  SIGNATURE Sig;
  symbol_CheckNoVariable(S);
  Sig = symbol_Signature(symbol_Index(S));
  S   = symbol_SignatureSymbol(symbol_Index(S), Type, symbol_Stat(S));
  Sig->info = S;
  return S;
}

 int symbol_Arity(SYMBOL ActSymbol)
{
  return symbol_Signature(symbol_Index(ActSymbol))->arity;
}

 NAT symbol_PositiveArity(SYMBOL ActSymbol)
{
  int arity = symbol_Arity(ActSymbol);
  if (arity < 0)
    return NAT_MAX;
  else
    return arity;
}

 void symbol_SetArity(SYMBOL ActSymbol, int Arity)
{
  symbol_Signature(symbol_Index(ActSymbol))->arity = Arity;
}

 BOOL symbol_Fixed(SYMBOL ActSymbol)
{
  return symbol_Signature(symbol_Index(ActSymbol))->fixed;
}

 void symbol_SetFixed(SYMBOL ActSymbol, BOOL fixed)
{
  symbol_Signature(symbol_Index(ActSymbol))->fixed = fixed;
}

 int symbol_ArbitraryArity(void)
{
  return -1;
}

 const char* symbol_Name(SYMBOL ActSymbol)
{
  return symbol_Signature(symbol_Index(ActSymbol))->name;
}

 NAT symbol_NameLength(SYMBOL ActSymbol)
{
  return symbol_Signature(symbol_Index(ActSymbol))->length;
}

 int symbol_Info(SYMBOL ActSymbol)
{
  return symbol_Signature(symbol_Index(ActSymbol))->info;
}

 int symbol_Weight(SYMBOL ActSymbol)
{
  return symbol_Signature(symbol_Index(ActSymbol))->number.weight;
}

 void symbol_SetWeight(SYMBOL ActSymbol, int Weight)
{
  symbol_Signature(symbol_Index(ActSymbol))->number.weight = Weight;
}

SYMBOL symbol_MFunctionSort(SYMBOL ActSymbol)
{
  return symbol_Signature(symbol_Index(ActSymbol))->sort;
}

void symbol_MSetFunctionSort(SYMBOL ActSymbol, SYMBOL Sort)
{
  symbol_Signature(symbol_Index(ActSymbol))->sort = Sort;
}

void symbol_SetName(SYMBOL, char*);

 LIST symbol_GeneratedBy(SYMBOL S)
{
  return symbol_Signature(symbol_Index(S))->generatedBy;
}

 BOOL symbol_IsGeneratedBy(SYMBOL S1, SYMBOL S2)
{
  return list_PointerMember(symbol_GeneratedBy(S1), (POINTER)S2);
}

 void symbol_SetGeneratedBy(SYMBOL S, LIST SymbolList)
{
  symbol_Signature(symbol_Index(S))->generatedBy = SymbolList;
}

 void symbol_SetOrdering(PRECEDENCE P, SYMBOL ActSymbol,
					  int Ordering)
{
  int Index;

  Index = symbol_Index(ActSymbol);
  symbol_CheckIndexInRange(Index);
  P[Index] = Ordering;
}

 void symbol_SetIncreasedOrdering(PRECEDENCE P, SYMBOL S)
{
  symbol_SetOrdering(P, S, symbol_GetIncreasedOrderingCounter());
}


 BOOL symbol_PrecedenceGreater(PRECEDENCE P, SYMBOL S1, SYMBOL S2)
{
  return symbol_Ordering(P, S1) < symbol_Ordering(P, S2);
}

 BOOL symbol_HasProperty(SYMBOL ActSymbol, SPROPERTY Property)
{
  return (symbol_Signature(symbol_Index(ActSymbol))->props & Property);
}

 void symbol_AddProperty(SYMBOL ActSymbol, SPROPERTY Property)
{
  SIGNATURE S = symbol_Signature(symbol_Index(ActSymbol));
  S->props    = S->props | Property;
}

 void symbol_RemoveProperty(SYMBOL ActSymbol, SPROPERTY Property)
{
  SIGNATURE S = symbol_Signature(symbol_Index(ActSymbol));
  if (S->props & Property)
    S->props = S->props - Property;
}

 BOOL symbol_IsPredefined(SYMBOL Symbol)
{
  return (symbol_HasProperty(Symbol, PREDEFINED));
}

 BOOL symbol_IsPredefinedPred(SYMBOL Symbol)
{
  return symbol_IsPredicate(Symbol) && symbol_IsPredefined(Symbol);	
}

 BOOL symbol_IsBaseSort(SYMBOL Symbol)
{
  return (symbol_Arity(Symbol) == 1);
}

 void symbol_ClearPrecedence(PRECEDENCE P)
{
  int i;
  const int clear = -42; /* Some negative number */

  for (i = 0; i < symbol__MAXSIGNATURE; i++)
    P[i] = clear;        
}

 PRECEDENCE symbol_CreatePrecedence(void)
{
  PRECEDENCE P;

  P = memory_Malloc(sizeof(int[symbol__MAXSIGNATURE]));
  symbol_ClearPrecedence(P);
  return P;
}

 void symbol_DeletePrecedence(PRECEDENCE P)
{
  memory_Free(P, sizeof(int[symbol__MAXSIGNATURE]));
}

 void symbol_TransferPrecedence(PRECEDENCE Source,
						 PRECEDENCE Target)
  /* Copy settings from one precedence object to another */
{
  int i;

  for (i = 0; i < symbol__MAXSIGNATURE; i++)
    Target[i] = Source[i];
}

 LIST symbol_DeleteSymbolFromList(LIST Symbols, SYMBOL S)
  /* Deletes all occurrences of <S> from the list */
{
  return list_DeleteElement(Symbols, (POINTER) S, 
			    (BOOL (*)(POINTER, POINTER)) symbol_Equal);
}

 void symbol_DeleteSymbolList(LIST Symbols)
  /* The list AND the symbols within are deleted */
{
  list_DeleteWithElement(Symbols, (void (*)(POINTER))symbol_Delete);
}

/**************************************************************/
/* Symbol VARCONT                                             */
/**************************************************************/

 BOOL symbol_ContextIsClean(void)
{
  int i;
  for (i = 0; i < symbol__MAXSIGNATURE; i++)
    if (symbol_VARCONT[i] != (SYMBOL)0)
      return FALSE;
  return TRUE;
}

 void symbol_ContextClean(void)
{
  int i;
  for (i = 0; i < symbol__MAXSIGNATURE; i++)
    symbol_VARCONT[i] = (SYMBOL)0;
}

 BOOL symbol_ContextIsMapped(SYMBOL Symbol)
{
  int i;
  for (i = 0; i < symbol__MAXSIGNATURE; i++)
    if (symbol_Equal(symbol_VARCONT[i],Symbol))
      return TRUE;
  return FALSE;
}

 SYMBOL symbol_ContextGetValue(SYMBOL Symbol)
{
  int Index;

  Index = symbol_Index(Symbol);
  symbol_CheckIndexInRange(Index);
  return symbol_VARCONT[Index];
}

 void symbol_ContextSetValue(SYMBOL Symbol, SYMBOL Value)
{
  int Index;

  Index = symbol_Index(Symbol);
  symbol_CheckIndexInRange(Index);
  symbol_VARCONT[Index] = Value;
}

 void symbol_ContextClearValue(SYMBOL Symbol)
{
  symbol_ContextSetValue(Symbol, (SYMBOL)0);
}

 BOOL symbol_ContextIsBound(SYMBOL Symbol)
{
  return (symbol_ContextGetValue(Symbol) != (SYMBOL)0);
}

/**************************************************************/
/* Inline Functions ends here                                 */
/**************************************************************/
SIGNATURE *symbol_SIGNATURE;
HASHMAP    symbol_Speedup; /* Additional support to symbol_SIGNATURE to fasciliate fast lookup */

ARRAY  symbol_STANDMSORTS;     
ARRAY  symbol_STANDMVARS; 
SYMBOL symbol_STANDACTVAR;

SYMBOL symbol_STANDARDVARCOUNTER;
SYMBOL symbol_INDEXVARCOUNTER;

const int symbol_MASK         = 3;  /* 0000011 */
const int symbol_TYPEMASK     = 3;  /* 0000011 */
const int symbol_STATMASK     = 4;  /* 0000100 */
const int symbol_TYPESTATMASK = 7;  /* 0000111 */


const int symbol_TYPEBITS      = 2;
const int symbol_STATBITS      = 1;
const int symbol_TYPESTATBITS  = 3;

const int symbol_SIGTYPES = 4;

const char symbol_SKFNAME[]="skf";
const char symbol_SKCNAME[]="skc";
const char symbol_SKPNAME[]="SkP";
const char symbol_SKANAME[]="SkC";
const int  symbol_SKLENGTH = 3;

static BOOL symbol_HASSIGNATURE;
static LIST symbol_FREEDSYMBOLS;

int  symbol_ACTINDEX;
int  symbol_ACTSKOLEMFINDEX;
int  symbol_ACTSKOLEMCINDEX;
int  symbol_ACTSKOLEMAINDEX;
int  symbol_ACTSKOLEMPINDEX;
int  symbol_ORDERING;

SYMBOL symbol_VARCONT[symbol__MAXSIGNATURE];

static unsigned long symbol_COUNT[symbol__MAXSIGNATURE];

/* Take care of name conflicts between variables and signature symbols */
char* symbol_STDVARIABLENAMES[symbol__MAXSTANDARDVAR];
int   symbol_VARNAMEACTINDEX;
int   symbol_VARNAMEMAXINDEX;

/**************************************************************/
/* ********************************************************** */
/* *                                                        * */
/* *  SYMBOL CREATION                                       * */
/* *                                                        * */
/* ********************************************************** */
/**************************************************************/

int symbol_GetIncreasedOrderingCounter(void)
{
  return symbol_ORDERING++;
}

NAT symbol_MaxStringLength(void)
/**************************************************************
  INPUT:   None.
  RETURNS: The maximal number of characters needed for a string in the signature.
***************************************************************/
{
  int       Index;
  NAT       Result,Length;
  SIGNATURE Entry;

  Result = 0;
  
  for (Index=1; Index < symbol_ACTINDEX; Index++) {
    Entry = symbol_Signature(Index);
    if (Entry != NULL) {
      Length = Entry->length;
      if (Length > Result)
	Result = Length;
    }
  }
  return Result;
}

static void symbol_SetSignature(intptr_t ActIndex, SIGNATURE Sig)
/**************************************************************
  Store the given SIGNATURE to a given index.
  Sig can also be null.

  Non null signature is also remembered in
  speedup table for fast future lookup
  (which results in updating the hash field of the Sig).

  The deletion from speedup is, however,
  not provided by this function.
***************************************************************/
{
  symbol_CheckIndexInRange(ActIndex);
  symbol_SIGNATURE[ActIndex] = Sig;

  if (Sig != NULL) { /* adding a new SIGNATURE entry to the speedup */
    hm_Insert(symbol_Speedup,Sig->name,(POINTER)ActIndex);
  }
}

void symbol_SetName(SYMBOL ActSymbol, char* Name)
/**************************************************************
  INPUT:   A symbol, and its new name.
  RETURNS: void.
  SUMMARY: Sets a name of a given Symbol to the new given value.

  The respective signature item is removed from and reinserted
  to the speedup table to be still accasible by lookup.

  CAUTION: Doesn't free the memory of the old name.
***************************************************************/
{
  intptr_t idx;
  SIGNATURE sig;
    
  idx       = symbol_Index(ActSymbol);
  sig       = symbol_Signature(idx);  

  /*take care of speedup*/
  hm_Remove(symbol_Speedup,sig->name);    
  
  sig->name = Name;

  /*take care of speedup*/
  hm_Insert(symbol_Speedup,sig->name,(POINTER)idx);    
}

static SYMBOL symbol_SignatureCreate(char* String, int Type, int Arity,
				     int Status, PRECEDENCE Precedence)
/**************************************************************
  INPUT:   A pointer to a string, a type, the arity and the status
  RETURNS: The symbol containing the passed parameters.
  SUMMARY: Establishes a new symbol in the symbol table and returns the
	   internal representation (pointer and type).
  CAUTION: The string is not copied!
***************************************************************/
{
  SIGNATURE Entry;
  
#ifdef CHECK
  if (!symbol_SignatureExists()) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In symbol_SignatureCreate:");
    misc_ErrorReport(" Module was initialized with no signature.\n");
    misc_FinishErrorReport();
  } 
  if (Type < 0 || Type >= symbol_SIGTYPES) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In symbol_SignatureCreate: Illegal input.\n");
    misc_FinishErrorReport();
  }
#endif

  if ((int)symbol_ACTINDEX >= symbol__MAXSIGNATURE &&
      list_Empty(symbol_FREEDSYMBOLS)) {
    misc_StartUserErrorReport();
    misc_UserErrorReport("\n In symbol_SignatureCreate: No more symbols available.\n");
    misc_FinishUserErrorReport();
  }
        
  Entry                 = symbol_GetSignature();
  Entry->number.weight  = 1;
  Entry->props          = 0;
  Entry->name           = String;
  Entry->length         = strlen(String);
  Entry->arity          = Arity;
  Entry->fixed          = FALSE;
  Entry->generatedBy    = list_Nil();
  Entry->sort           = (SYMBOL)NULL;
  
  if (list_Empty(symbol_FREEDSYMBOLS)) {
    Entry->info = symbol_SignatureSymbol(symbol_ACTINDEX, Type, Status);
    symbol_SetSignature(symbol_ACTINDEX++, Entry);
  }
  else {
    intptr_t Index;
    
    Index               = (intptr_t)list_Car(symbol_FREEDSYMBOLS);
    symbol_FREEDSYMBOLS = list_PointerDeleteElement(symbol_FREEDSYMBOLS,
						    (POINTER)Index);
    Entry->info = symbol_SignatureSymbol(Index, Type, Status);
    symbol_SetSignature(Index, Entry);
  }

  /* Define precedence of symbol */
  symbol_SetIncreasedOrdering(Precedence, Entry->info);

  return Entry->info;
}


SYMBOL symbol_CreateFunction(const char* String, int Arity, int Status,
			     PRECEDENCE Precedence)
/**************************************************************
  INPUT:   A string defining a symbol name, an arity, the status the 
           created function symbol is supposed to have and a precedence
	   object.
  RETURNS: A new symbol for a new function.
  SUMMARY: Creates a new function symbol.
  EFFECTS: Inserts the new function in the symbol table.
           The precedence of the new symbol is set in <Precedence>.
***************************************************************/
{
  if (Arity == 0)
    return symbol_SignatureCreate(string_StringCopy(String),
				  symbol_CONSTANT, Arity, Status, Precedence);
  else
    return symbol_SignatureCreate(string_StringCopy(String),					 
				  symbol_FUNCTION, Arity, Status, Precedence);
}

SYMBOL symbol_CreateSkolemFunction(SYMBOL Var, int Arity, PRECEDENCE Precedence)
/**************************************************************
  INPUT:   A variable symbol, the arity of a function to be created, and a precedence.
  RETURNS: A new skolem symbol having the required arity and range sort.
  SUMMARY: Creates a new function symbol. The function is declared to have
           the range sort of <Var>.
  EFFECTS: Inserts the new function in the symbol table.
           The precedence of the new symbol is set in <Precedence>.
***************************************************************/
{
  char   newname[10];
  SYMBOL Result;

  Result = (SYMBOL)NULL;

  while (Result == (SYMBOL)NULL) {
    if (Arity == 0)
      sprintf(newname,"%s%d",symbol_SKCNAME,symbol_ACTSKOLEMCINDEX++);
    else
      sprintf(newname,"%s%d",symbol_SKFNAME,symbol_ACTSKOLEMFINDEX++);
    if (symbol_Lookup(newname) == 0)
      Result = symbol_CreateFunction(newname, Arity, symbol_STATLEX, Precedence);
  }

  symbol_AddProperty(Result,SKOLEM);
  symbol_MSetFunctionSort(Result, symbol_MSortVariableSort(Var));
  /* printf("\n Created function "); symbol_Print(Result); 
     printf(" with range sort "); symbol_Print(symbol_MSortVariableSort(Var)); printf("\n");  */

  return Result;
}


SYMBOL symbol_CreatePredicate(const char* String, int Arity, int Status,
			      PRECEDENCE Precedence)
/**************************************************************
  INPUT:   A string, a symbol arity, the status the created predicate
           is supposed to have, and a precedence object.
  RETURNS: A new symbol for a new predicate.
  SUMMARY: Creates a new predicate symbol.
  EFFECTS: Inserts the new predicate symbol into the symbol table.
           The precedence of the new symbol is set in <Precedence>.
***************************************************************/
{ 
  return symbol_SignatureCreate(string_StringCopy(String),				       
				symbol_PREDICATE, Arity, Status, Precedence);
}

SYMBOL symbol_CreateSkolemPredicate(int Arity, PRECEDENCE Precedence)
/**************************************************************
  INPUT:   The arity of a new Skolem predicate symbol and a precedence object.
  RETURNS: A new symbol for a new predicate.
  SUMMARY: Creates a new predicate symbol.
  EFFECTS: Inserts the new predicate symbol into the symbol table.
           The precedence of the new symbol is set in <Precedence>.
***************************************************************/
{ 
  char   newname[10];
  SYMBOL Result;

  Result = (SYMBOL)NULL;

  while (Result == (SYMBOL)NULL) {
    if (Arity==0)
      sprintf(newname,"%s%d",symbol_SKANAME,symbol_ACTSKOLEMAINDEX++);
    else
      sprintf(newname,"%s%d",symbol_SKPNAME,symbol_ACTSKOLEMPINDEX++);
    if (symbol_Lookup(newname) == 0)
      Result = symbol_CreatePredicate(newname, Arity, symbol_STATLEX, Precedence);
  }
  return Result;
}


SYMBOL symbol_CreateJunctor(const char* String, int Arity, int Status,
			    PRECEDENCE Precedence)
/**************************************************************
  INPUT:   A string, a symbol arity, the status the created junctor
           is supposed to have, and a precedence object.
  RETURNS: A new symbol for a new junctor.
  SUMMARY: Creates a new junctor symbol.
  EFFECTS: Inserts the new junctor symbol into the symbol table.
           The precedence of the new symbol is set in <Precedence>.
***************************************************************/
{ 
  return symbol_SignatureCreate(string_StringCopy(String),				       
				symbol_JUNCTOR, Arity, Status, Precedence);
}


BOOL symbol_IsSymbol(SYMBOL Symbol)
/**************************************************************
  INPUT:    A symbol.
  RETURNS: TRUE if the symbols is  a variable or contained
           in the symbol table.
***************************************************************/
{
  return 
    (!symbol_SignatureExists() ||
     ((!symbol_Equal(Symbol, symbol__NULL)) &&
      ((symbol_IsVariable(Symbol) && Symbol<symbol_MaxVars()) || 
       (symbol_IsSignature(Symbol) && symbol_Index(Symbol)<symbol_ACTINDEX))));
}


BOOL symbol_IsFreed(SYMBOL Symbol)
/**************************************************************
  INPUT:    A symbol.
  RETURNS: TRUE if the symbol is already freed.
***************************************************************/
{
  return list_PointerMember(symbol_FREEDSYMBOLS, (POINTER)Symbol);
}

 void symbol_FreeSignature(SIGNATURE Sig)
/***************************************************************
  INPUT:   A signature datum.
  RETURNS: void
  EFFECTS: The datum is deleted and its memory freed,
           the Speedup is taken care of separately elsewhere.
****************************************************************/
{   
  string_StringFree(Sig->name);
  list_Delete(Sig->generatedBy);
  memory_Free(Sig, sizeof(SIGNATURE_NODE));
}

void symbol_Delete(SYMBOL Symbol)
/**************************************************************
  INPUT:   A symbol.
  RETURNS: Nothing.
  SUMMARY: Deletes the symbol from the symbol table and frees its memory.
***************************************************************/
{
#ifdef CHECK
  if (!symbol_SignatureExists()) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In symbol_Delete: Module was initialized without signature.\n");
    misc_FinishErrorReport();
  }
  if (!symbol_IsSymbol(Symbol)) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In symbol_Delete: Illegal input.\n");
    misc_FinishErrorReport();
  }
#endif

  if (!symbol_IsVariable(Symbol)) {
    intptr_t      Index;
    SIGNATURE Entry;

    Index               = symbol_Index(Symbol);
    symbol_FREEDSYMBOLS = list_Cons((POINTER)Index,symbol_FREEDSYMBOLS);
    Entry               = symbol_Signature(Index);
    hm_Remove(symbol_Speedup,Entry->name);
    symbol_SetSignature(Index, NULL);
    symbol_FreeSignature(Entry);
  }
}

LIST symbol_GetAllSymbols(void)
/**************************************************************
  INPUT:   None.
  RETURNS: A list of all generated symbols.
***************************************************************/
{
  LIST Result;

  Result = list_Nil();

  if (symbol_SignatureExists()) {
    int       Index;
    SIGNATURE S;
    
    for (Index = 1; Index < symbol_ACTINDEX; Index++) {
      S = symbol_Signature(Index);
      if (S != NULL) {
	Result = list_Cons((POINTER)symbol_GetSigSymbol(Index), Result);
      }
    }
  }
  return Result;
}


LIST symbol_GetAllPredicates(void)
/**************************************************************
  INPUT:   None.
  RETURNS: A list of all predicate symbols.
***************************************************************/
{
  LIST Result;

  Result = list_Nil();

  if (symbol_SignatureExists()) {
    int       Index;
    SIGNATURE S;
    
    for (Index = 1; Index < symbol_ACTINDEX; Index++) {
      S = symbol_Signature(Index);
      if (S != NULL && symbol_IsPredicate(symbol_GetSigSymbol(Index)))
	Result = list_Cons((POINTER)symbol_GetSigSymbol(Index), Result);
    }
  }
  return Result;
}


LIST symbol_GetAllFunctions(void)
/**************************************************************
  INPUT:   None.
  RETURNS: A list of all function symbols.
***************************************************************/
{
  LIST Result;

  Result = list_Nil();

  if (symbol_SignatureExists()) {
    int       Index;
    SIGNATURE S;
    
    for (Index = 1; Index < symbol_ACTINDEX; Index++) {
      S = symbol_Signature(Index);
      if (S != NULL && symbol_IsFunction(symbol_GetSigSymbol(Index)))
	Result = list_Cons((POINTER)symbol_GetSigSymbol(Index), Result);
    }
  }
  return Result;
}

LIST symbol_GetAllSymbolsWithProperty(SPROPERTY Property)
/**************************************************************
  INPUT:   None.
  RETURNS: A list of all selected symbols.
***************************************************************/
{
  LIST Result;

  Result = list_Nil();

  if (symbol_SignatureExists()) {
    int       Index;
    
    for (Index = 1; Index < symbol_ACTINDEX; Index++) {
      SIGNATURE S;
      SYMBOL    Symbol;
      
      S      = symbol_Signature(Index);

      if (S != NULL) {
	Symbol = symbol_GetSigSymbol(Index);
      
	if (symbol_HasProperty(Symbol, Property))
	  Result = list_Cons((POINTER)Symbol, Result);
      }
    }
  }
  return Result;
}

void symbol_SeparateVariableSymbolNames(void)
/**************************************************************
  INPUT:   None.
  EFFECT:  It is checked whether a standard variable name conflicts with
           a signature name. If this is the case the variable is renamed.
  RETURNS: void.
***************************************************************/
{
  if (symbol_SignatureExists()) {
    int i;
    i = 0;
    while (i<symbol__MAXSTANDARDVAR && symbol_VARNAMEACTINDEX < symbol_VARNAMEMAXINDEX) {
      if (symbol_Lookup(symbol_STDVARIABLENAMES[i]) != 0)
	sprintf(symbol_STDVARIABLENAMES[i],"%c%d", 'U', symbol_VARNAMEACTINDEX++);
      else
	i++;
    }
    if (!(symbol_VARNAMEACTINDEX < symbol_VARNAMEMAXINDEX)) {
      misc_StartErrorReport();
      misc_ErrorReport("\n In symbol_SeparateVariabeSymbolNames:");
      misc_ErrorReport(" Ran out of variable name space.\n");
      misc_FinishErrorReport();
    }
  }
}


void symbol_FreeAllSymbols(void)
/**************************************************************
  INPUT:   None.
  RETURNS: Nothing.
  EFFECTS: Frees all generated symbols
***************************************************************/
{
  int i;

  if (symbol_SignatureExists()) {
    int       Index;
    SIGNATURE S;
    
    for (Index = 1; Index < symbol_ACTINDEX; Index++) {
      S = symbol_Signature(Index);
      if (S != NULL)
	symbol_FreeSignature(S);
    }
    memory_Free(symbol_SIGNATURE, sizeof(SIGNATURE[symbol__MAXSIGNATURE]));

    for(Index = 1; Index <= symbol_STANDACTVAR; Index++)
      memory_Free((POINTER)array_GetElement(symbol_STANDMVARS, Index), sizeof(MANYVARS_NODE));
    array_Delete(symbol_STANDMVARS);
    for(Index = 0; Index < array_GetSize(symbol_STANDMSORTS); Index++)
      memory_Free((POINTER)array_GetElement(symbol_STANDMSORTS, Index), sizeof(MANYSORTS_NODE));
    array_Delete(symbol_STANDMSORTS);

    hm_Delete(symbol_Speedup);    
  }

  for (i=0;i<symbol__MAXSTANDARDVAR;i++) 
    memory_Free(symbol_STDVARIABLENAMES[i],symbol__SYMBOLVARMAXLEN);
    
  list_Delete(symbol_FREEDSYMBOLS); 
}


void symbol_Init(BOOL Signature)
/**************************************************************
  INPUT:   A flag indicating whether a signature is used or not.
  RETURNS: None.
  SUMMARY: Initializes the Symbol Module.
  EFFECTS: Initializes global variables.
  CAUTION: MUST BE CALLED BEFORE ANY OTHER Symbol-FUNCTION.
***************************************************************/
{
  int i;

  symbol_ResetSkolemIndex();
  symbol_ContextClean();

  if (Signature) {
    symbol_SIGNATURE   = (SIGNATURE*)memory_Malloc(sizeof(SIGNATURE[symbol__MAXSIGNATURE]));
    symbol_STANDMSORTS = array_Create(100);    
    symbol_STANDMVARS  = array_Create(symbol__MAXSTANDARDVAR);
    symbol_STANDACTVAR = symbol_GetInitialStandardVarCounter();
    symbol_Speedup     = hm_Create(4, (HM_GET_HASH_FUNCTION)hm_StringHash, (HM_KEY_EQUAL_FUNCTION)string_Equal, FALSE);
  }

  symbol_STANDARDVARCOUNTER = symbol_GetInitialStandardVarCounter();
  symbol_INDEXVARCOUNTER    = symbol_GetInitialIndexVarCounter();
  
  symbol_ACTINDEX           = 1;
  symbol_ORDERING           = 1;  
  
  symbol_HASSIGNATURE       = Signature;
  symbol_FREEDSYMBOLS       = list_Nil();

  for (i=0;i<6;i++) {       /* for the six standard variable names u,v,w,x,y,z */
    symbol_STDVARIABLENAMES[i] = (char *)memory_Malloc(symbol__SYMBOLVARMAXLEN);
    sprintf(symbol_STDVARIABLENAMES[i],"%c", 'U' + i);
  }
  for (i=6;i<symbol__MAXSTANDARDVAR;i++) {
    symbol_STDVARIABLENAMES[i] = (char *)memory_Malloc(symbol__SYMBOLVARMAXLEN);
    sprintf(symbol_STDVARIABLENAMES[i],"%c%d", 'X', i-5);
  }
  symbol_VARNAMEACTINDEX    = 1;
  symbol_VARNAMEMAXINDEX    = 99999;                           /* 10^(symbol__SYMBOLVARMAXLEN - 1) - 1 */   
}


BOOL symbol_SignatureExists(void)
/**************************************************************
  INPUT:   None.
  RETURNS: TRUE, if a symbol signature was created, FALSE otherwise.
***************************************************************/
{
  return symbol_HASSIGNATURE;
}

void symbol_ReinitGenericNameCounters(void)
/**************************************************************
  INPUT:   None.
  RETURNS: Nothing.
  EFFECTS: Scans the symbol signature for generic names and if
           there exists some, the corresponding counters are increased.
***************************************************************/
{
  if (symbol_SignatureExists()) {
    int          Index,Num;
    SYMBOL       Info;
    SIGNATURE    Entry;
    char         *Name,*Subname;
  
    for (Index=1; Index < symbol_ACTINDEX; Index++) {
      Entry = symbol_Signature(Index);
      if (Entry != NULL) {
	Info   = Entry->info;
	Name   = Entry->name;
	if (strlen(Name)>symbol_SKLENGTH) {
	  Subname = Name + symbol_SKLENGTH;
	
	  switch (symbol_Type(Info)) {
	  
	  case symbol_CONSTANT:
	    if (strncmp(Name,symbol_SKCNAME,symbol_SKLENGTH) == 0 && 
		string_StringIsNumber(Subname)) {
	      Num = atoi(Subname);
	      if (Num >= symbol_ACTSKOLEMCINDEX)
		symbol_ACTSKOLEMCINDEX = Num + 1;
	    }
	    break;
	  case symbol_FUNCTION:
	    if (strncmp(Name,symbol_SKFNAME,symbol_SKLENGTH) == 0  &&
		string_StringIsNumber(Subname)) {
	      Num = atoi(Subname);
	      if (Num >= symbol_ACTSKOLEMFINDEX)
		symbol_ACTSKOLEMFINDEX = Num + 1;
	    }
	    break;
	  case symbol_PREDICATE:
	    if (Entry->arity == 0) {
	      if (strncmp(Name,symbol_SKANAME,symbol_SKLENGTH) == 0  &&
		  string_StringIsNumber(Subname)) {
		Num = atoi(Subname);
		if (Num >= symbol_ACTSKOLEMAINDEX)
		  symbol_ACTSKOLEMAINDEX = Num + 1;
	      }
	    }
	    else {
	      if (strncmp(Name,symbol_SKPNAME,symbol_SKLENGTH) == 0  &&
		  string_StringIsNumber(Subname)) {
		Num = atoi(Subname);
		if (Num >= symbol_ACTSKOLEMPINDEX)
		  symbol_ACTSKOLEMPINDEX = Num + 1;
	      }
	    }    
	  }
	}
      }
    }
  }
}

  
/**************************************************************/
/* ********************************************************** */
/* *                                                        * */
/* *  SYMBOL SEARCHING                                      * */
/* *                                                        * */
/* ********************************************************** */
/**************************************************************/

SYMBOL symbol_Lookup(const char* String)
/**********************************************************
  INPUT:   A pointer to a string.
  RETURNS: If a symbol with name String exists in the signature, the symbol;
           0 otherwise
********************************************************/
{
  if (symbol_SignatureExists()) {
    SIGNATURE S;
    POINTER   Index; 
    BOOL      Found;
    
    Index = hm_RetrieveFound(symbol_Speedup, (POINTER)String, &Found);
    
    if (Found) {
      S = symbol_Signature((intptr_t)Index);
      return S->info;
    }
  }
  
  return 0;
}

/**************************************************************/
/* ********************************************************** */
/* *                                                        * */
/* *  SPECIALS                                              * */
/* *                                                        * */
/* ********************************************************** */
/**************************************************************/

void symbol_LowerSignature(void)
/**************************************************************
  INPUT:   None.
  RETURNS: Nothing.
  EFFECT:  Any predicate or function symbol in the signature that
           starts with a capital letter is prefixed with "ss"
***************************************************************/
{
  int       Index;
  SIGNATURE Entry;
  SYMBOL    Info;
  char*     String;

  for (Index = 1; Index < symbol_ACTINDEX; Index++) {
    Entry = symbol_Signature(Index);
    if (Entry != NULL) {
      Info = Entry->info;
	
      if (symbol_IsPredicate(Info) || symbol_IsFunction(Info)) {
	String = Entry->name;
	if ('A' <= String[0] && String[0] <= 'Z') {
	  char* New;
	  New = (char *)memory_Malloc(strlen(String)+3);
	  strcpy(&(New[2]),String);
	  New[0] = New[1] = 's';             /* prefix "ss" */

      symbol_SetName(Info,New);	  
      string_StringFree(String);	  
	}
      }
    }
  }
}

void symbol_Dump(PRECEDENCE Precedence)
/**************************************************************
  INPUT:   A precedence
  RETURNS: Nothing.
  EFFECT:  Dumps all existing symbols to stdout.
***************************************************************/
{
  if (symbol_SignatureExists()) {
    int       Index;
    SIGNATURE S;
    fputs("Dump:", stdout);
    for (Index = 1; Index < symbol_ACTINDEX; Index++) {
      S = symbol_Signature(Index);
      if (S != NULL)
	printf("\n\t %4d:%zd:%4d:%d:%zd:%s:%zd", Index, S->info, S->number.weight,
	       Precedence[Index], S->props, S->name, S->length);
    }
  }
}


LIST symbol_SortByPrecedence(LIST Symbols, PRECEDENCE Precedence)
/**************************************************************
  INPUT:   A list of symbols, and a precedence.
  RETURNS: The same list where the elements are sorted with
           respect to decreasing precedence.
  CAUTION: Destructive.
***************************************************************/
{
  LIST Scan1,Scan2,Min;
  POINTER Exchange;

  /* We can't use list_Sort, since list_Sort expects an ordering
     function that receives two arguments. Since symbol_PrecedenceGreater
     requires three arguments, we have to define our own sorting function.
  */

  for (Scan1=Symbols; !list_Empty(Scan1); Scan1=list_Cdr(Scan1)) {
    Min = Scan1;
    for (Scan2 = list_Cdr(Scan1); !list_Empty(Scan2); Scan2 = list_Cdr(Scan2))
      if (symbol_PrecedenceGreater(Precedence, (SYMBOL) list_Car(Scan2),
				   (SYMBOL) list_Car(Min))) {
	Exchange = list_Car(Min);
	list_Rplaca(Min, list_Car(Scan2));
	list_Rplaca(Scan2, Exchange);
      }
  }

  return Symbols;
}

LIST symbol_MergeByPrecedence(LIST List1, LIST List2, PRECEDENCE Precedence)
/**************************************************************
  INPUT:   Two sorted lists List1 and List2, and a precedence.
  RETURNS: The merged list ordered with respect to the precedence.
  EFFECT:  The function needs time O(n), where <n> is the length of the list.
  CAUTION: The elements of <List1> and <List2> are assumed to be
		   Symbols occuring in <Precedence>
***************************************************************/
{

  LIST Scan1, Scan2, Result, ResultStart;


  if (list_Empty(List1))
    return List2;

  if (list_Empty(List2))
    return List1;

  /* This version is derived from list_NNumberMerge, but it doesn't need */
  /* to allocate and deallocate memory, so it should be more efficient.  */

  /* Use the list with the least element as result list. */
  if (symbol_PrecedenceGreater(Precedence, (SYMBOL) list_Car(List1), (SYMBOL)list_Car(List2))) {
    ResultStart = List1;
    Scan1       = list_Cdr(List1);
    Scan2       = List2;
  }
  else {
    ResultStart = List2;
    Scan1       = List1;
    Scan2       = list_Cdr(List2);
  }

  /* Result is the last element of the merged list. */

  Result = ResultStart;

  while (!list_Empty(Scan1) && !list_Empty(Scan2)) {
    /* This function doesn't implement stable merging. */
    /* Add another test if you need it.                */

    if (symbol_PrecedenceGreater(Precedence, (SYMBOL) list_Car(Scan1), (SYMBOL) list_Car(Scan2))) {
      list_Rplacd(Result,Scan1);
      Scan1  = list_Cdr(Scan1);
    }
    else {
      list_Rplacd(Result,Scan2);
      Scan2  = list_Cdr(Scan2);
    }
    Result = list_Cdr(Result);
  }

  if (list_Empty(Scan1))
    list_Rplacd(Result, Scan2);
  else
    list_Rplacd(Result, Scan1);

  return ResultStart;
}


LIST symbol_MergeSortByPrecedence(LIST Symbols, PRECEDENCE Precedence)
/**************************************************************
  INPUT:   A list of symbols, and a precedence.
  RETURNS: The same list where the elements are sorted with
           respect to decreasing precedence.
  CAUTION: Destructive.
***************************************************************/
{
  LIST Result;
#ifdef CHECK
  NAT  originallength;

  originallength = list_Length(Symbols);
#endif

  /* Only sort if list has more than one element */
  if (!list_Empty(Symbols) && !list_Empty(list_Cdr(Symbols))) {
    LIST  lowerhalf;
    LIST  greaterhalf;

    LIST *lowerhalfptr;
    LIST *greaterhalfptr;

    lowerhalf      = list_Nil();
    greaterhalf    = list_Nil();
    lowerhalfptr   = &lowerhalf;
    greaterhalfptr = &greaterhalf;

    list_Split(Symbols, lowerhalfptr, greaterhalfptr);

#ifdef CHECK
    if((list_Length(lowerhalf) + list_Length(greaterhalf))
       != originallength) {
      /* output an error message and exit */
      misc_StartErrorReport();
      misc_ErrorReport("\n In list_MergeSort: Split lists' total sizes");
      misc_ErrorReport("\n don't match original list's size.");
      misc_FinishErrorReport();
    }
#endif

    lowerhalf   = symbol_MergeSortByPrecedence(lowerhalf, Precedence);

    greaterhalf = symbol_MergeSortByPrecedence(greaterhalf, Precedence);

#ifdef CHECK
    if((list_Length(lowerhalf) + list_Length(greaterhalf))
       != originallength) {
      /* output an error message and exit */
      misc_StartErrorReport();
      misc_ErrorReport("\n In list_MergeSort: Mergesorted lists' total sizes");
      misc_ErrorReport("\n don't match original list's size.");
      misc_FinishErrorReport();
    }
#endif

    Result = symbol_MergeByPrecedence(lowerhalf, greaterhalf, Precedence);

#ifdef CHECK
    if(list_Length(Result) != originallength) {
      /* output an error message and exit */
      misc_StartErrorReport();
      misc_ErrorReport("\n In list_MergeSort: Merged list's size doesn't match ");
      misc_ErrorReport("\n original list's size.");
      misc_FinishErrorReport();
    }
#endif

  }
  else {
    Result = Symbols;
  }

  return Result;
}



void symbol_RearrangePrecedence(PRECEDENCE Precedence, LIST UserPrecedence)
/**************************************************************
  INPUT:   A precedence and a list of symbols in the user 
           specified precedence, sorted in decreasing order.
  RETURNS: Nothing.
  EFFECT:  Modifies the given precedence to comply with the
           symbol precedence selected by the user.
***************************************************************/
{
  LIST Scan1, Scan2, Precedences;

  Precedences = list_Nil();

  for (Scan1 = UserPrecedence; !list_Empty(Scan1); Scan1 = list_Cdr(Scan1)) {
    Precedences = list_Cons((POINTER) symbol_Ordering(Precedence, (SYMBOL) list_Car(Scan1)), 
			    Precedences);
  }

  Precedences = list_PointerSort(Precedences);

  Scan1 = UserPrecedence;
  Scan2 = Precedences;

  while (Scan1 != list_Nil() && Scan2 != list_Nil()) {
    symbol_SetOrdering(Precedence, (SYMBOL) list_Car(Scan1), (intptr_t) list_Car(Scan2));
    Scan1 = list_Cdr(Scan1);
    Scan2 = list_Cdr(Scan2);
  }

  list_Delete(Precedences);
}
  
/* unused */
void symbol_PrintPrecedence(PRECEDENCE Precedence)
/**************************************************************
  INPUT:   A precedence.
  RETURNS: Nothing.
  EFFECT:  Prints the precedence to stdout.
***************************************************************/
{
  if (symbol_SignatureExists()) {
    LIST      Symbols, Scan;
    int       Index;
    SIGNATURE S;
    Symbols = list_Nil();
    for (Index = 1; Index < symbol_ACTINDEX; Index++) {
      S = symbol_Signature(Index);
      if (S != NULL &&
	  (symbol_IsPredicate(S->info) || symbol_IsFunction(S->info))) 
	Symbols = list_Cons((POINTER)S->info, Symbols);
    }
    Symbols = symbol_SortByPrecedence(Symbols, Precedence);

    for (Scan = Symbols; !list_Empty(Scan); Scan = list_Cdr(Scan)) {
      S = symbol_Signature(symbol_Index((SYMBOL)list_Car(Scan)));
      fputs(S->name, stdout);
      if (!list_Empty(list_Cdr(Scan)))
	fputs(" > ", stdout);
    }
    list_Delete(Symbols);
  }
}


void symbol_FPrintPrecedence(FILE *File, PRECEDENCE Precedence)
/**************************************************************
  INPUT:   A file to print to, and a precedence.
  RETURNS: Nothing.
  EFFECT:  Prints the current precedence as a setting 
           command in DFG syntax to <File>.
	   fol symbols are excluded.
***************************************************************/
{
  if (symbol_SignatureExists()) {
    LIST      Symbols, Scan;
    SYMBOL    Symbol;
    int       Index;
    SIGNATURE S;

    Symbols = list_Nil();
    for (Index = 1; Index < symbol_ACTINDEX; Index++) {
      S = symbol_Signature(Index);
      if (S != NULL) {
	Symbol = S->info;
	if ((symbol_IsPredicate(Symbol) || symbol_IsFunction(Symbol)) &&
	    !symbol_IsPredefined(Symbol))
	  Symbols = list_Cons((POINTER)Symbol, Symbols);
      }
    }
    Symbols = symbol_MergeSortByPrecedence(Symbols, Precedence);
    Index   = 0;
    fputs("set_precedence(", File);
    for (Scan = Symbols; !list_Empty(Scan); Scan = list_Cdr(Scan)) {
      S = symbol_Signature(symbol_Index((SYMBOL)list_Car(Scan)));
      putc('(', File);
      fputs(S->name, File);
      putc(',', File);
      fprintf(File, "%d", S->number.weight);
      putc(',', File);
      putc((symbol_HasProperty((SYMBOL)list_Car(Scan),ORDRIGHT) ? 'r' :
	    (symbol_HasProperty((SYMBOL)list_Car(Scan),ORDMUL) ? 'm' : 'l')),
	   File);
      putc(')', File);
      if (!list_Empty(list_Cdr(Scan)))
	putc(',', File);

      if (Index > 15) {
	Index = 0;
	fputs("\n\t", File);
      }
      else
	Index++;
    }
    fputs(").", File);
    list_Delete(Symbols);
  }
}


void symbol_SetCount(SYMBOL Symbol, unsigned long Count)
/**************************************************************
  INPUT:   A symbol, and a symbol count.
  RETURNS: Nothing.
  SUMMARY: Sets the symbol count for the symbol to Count. 
***************************************************************/
{
  symbol_COUNT[symbol_Index(Symbol)] = Count;
}

unsigned long symbol_GetCount(SYMBOL Symbol)
/**************************************************************
  INPUT:   A symbol.
  RETURNS: The number of occurences of the symbol in the clause
           set.
  SUMMARY: Gets the symbol count for the symbol. 
***************************************************************/
{
  return symbol_COUNT[symbol_Index(Symbol)];
}

/**************************************************************
  INPUT:   A precedence and a symbol.
  RETURNS: The ordering of the symbol in the precedence.
***************************************************************/
intptr_t symbol_Ordering(PRECEDENCE P, SYMBOL ActSymbol)
{
  intptr_t Index;
  
  Index = symbol_Index(ActSymbol);
#ifdef CHECK
  symbol_CheckIndexInRange(Index);
  if (P[Index] < 0) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In symbol_Ordering: Ordering of symbol %s is invalid\n",
		     symbol_Name(ActSymbol));
    misc_FinishErrorReport();
  }
#endif
  return P[Index];
}

/**************************************************************
  INPUT:   An index.
  RETURNS: Nothing.
  SUMMARY: Checks whether index is in range. 
***************************************************************/
void symbol_CheckIndexInRange(int Index)
{
#ifdef CHECK
  if (Index < 0 || Index >= symbol__MAXSIGNATURE) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In symbol_CheckIndexInRange: Symbol index is out of range.\n");
    misc_FinishErrorReport();
  }
#endif
}

/**************************************************************
  INPUT:   An symbol.
  RETURNS: Nothing.
  SUMMARY: Checks whether symbol is not a variable. 
***************************************************************/
void symbol_CheckNoVariable(SYMBOL S)
{
#ifdef CHECK
  if (!symbol_IsSignature(S)) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In symbol_CheckNoVariable: illegal input\n");
    misc_FinishErrorReport();
  }
#endif
}

/**************************************************************/
/* ********************************************************** */
/* *                                                        * */
/* *  SYMBOL OUTPUT	                                    * */
/* *                                                        * */
/* ********************************************************** */
/**************************************************************/

void symbol_Print(SYMBOL Symbol)
/**************************************************************
  INPUT:   A symbol.
  RETURNS: Nothing.
  SUMMARY: Prints a symbol to stdout. 
***************************************************************/
{
  symbol_FPrint(stdout, Symbol);
}


void symbol_FPrint(FILE* File, SYMBOL Symbol)
/**************************************************************
  INPUT:   A file and a symbol.
  RETURNS: None.
  SUMMARY: Prints a symbol to the file. 
***************************************************************/
{ 
#ifdef CHECK
  if (!symbol_IsSymbol(Symbol)) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In symbol_FPrint: Illegal input.\n");
    misc_FinishErrorReport();
  }
#endif

  if (symbol_Equal(symbol_Null(),Symbol))
    fputs("NULL", File);
  else if (symbol_IsVariable(Symbol)) {
    SYMBOL NormSymbol;

    NormSymbol = symbol_NormVar(Symbol);

    if (symbol_IsStandardVariable(Symbol)) {
      fprintf(File,"%s", symbol_STDVARIABLENAMES[NormSymbol-1]);
      if (symbol_SignatureExists() && array_GetElement(symbol_STANDMVARS, Symbol) != (intptr_t)NULL) {
	fputs(":", File);
	fputs(symbol_Name(symbol_MVarSort((MANYVARS)array_GetElement(symbol_STANDMVARS, Symbol))), File);
      }
    }
    else if (symbol_IsIndexVariable(Symbol))
      /* I1, I2, I3, ... */
      fprintf(File,"I%zd", NormSymbol);
#ifdef CHECK
    else {
      misc_StartErrorReport();
      misc_ErrorReport("\n In symbol_FPrint: Cannot print symbol.\n");
      misc_FinishErrorReport();
    }
#endif    
  }
  else if (symbol_SignatureExists())
    fputs(symbol_Name(Symbol), File);
  else
    fprintf(File, "%zd", Symbol);
}


void symbol_FPrintOtter(FILE* File, SYMBOL Symbol)
/**************************************************************
  INPUT:   A file and a symbol.
  RETURNS: None.
  SUMMARY: Prints a symbol in Otter format to stdout. 
***************************************************************/
{ 
#ifdef CHECK
  if (!symbol_IsSymbol(Symbol)) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In symbol_FPrintOtter: Illegal input.\n");
    misc_FinishErrorReport();
  }
#endif

  if (symbol_IsVariable(Symbol)) {
    SYMBOL NormSymbol;

    NormSymbol = symbol_NormVar(Symbol);

    if (symbol_IsStandardVariable(Symbol)) {
      if (Symbol <= 6)
	/* u, v, w, x, y, z */
	fprintf(File,"%zd", 116 + NormSymbol);
      else
	/* x1, x2, x3, ... */
	fprintf(File,"x%zd", NormSymbol - 6);
    }
    else if (symbol_IsIndexVariable(Symbol))
      /* I1, I2, I3, ... */
      fprintf(File,"I%zd", NormSymbol);
#ifdef CHECK
    else {
      misc_StartErrorReport();
      misc_ErrorReport("\n In symbol_FPrintOtter: Cannot print symbol.\n");
      misc_FinishErrorReport();
    }
#endif    
  }
  else 
    if (symbol_SignatureExists()) {
      if (symbol_IsConstant(Symbol))
	fprintf(File, "c%s", symbol_Name(Symbol));
      else
	if (symbol_IsFunction(Symbol))
	  fprintf(File, "f%s", symbol_Name(Symbol));
	else
	  if (symbol_IsPredicate(Symbol))
	    fprintf(File, "P%s", symbol_Name(Symbol));
	  else
	    fputs(symbol_Name(Symbol), File);
    }
    else
      fprintf(File, "%zd", Symbol);
}


void symbol_PrintLn(SYMBOL Symbol)
/**************************************************************
  INPUT:   A symbol.
  RETURNS: None.
  SUMMARY: Prints a symbol and a newline to stdout. 
***************************************************************/
{ 
  symbol_Print(Symbol);
  putchar('\n');
  
}


void symbol_PrintAll(void)
/**************************************************************
  INPUT:   None.
  RETURNS: None.
  SUMMARY: Prints all symbols to stdout.
***************************************************************/
{
  const int symbol_TABLEBLANKS = 2;
  const int symbol_TABLETYPE   = 6;  /* breadth of the columns */
  const int symbol_TABLEARITY  = 7;
  const int symbol_TABLESTAT   = 6;
  const int symbol_TABLEPROP   = 8;  
  const int symbol_TABLESTRING = 36;

  if (symbol_SignatureExists()) {
    int          Index;
    SYMBOL       Info;
    SIGNATURE    Entry;
    unsigned int TypePos, ArityPos, StatPos, PropPos, StringPos, EndPos, ActPos;

    TypePos   = symbol_TABLEBLANKS;
    ArityPos  = TypePos   + symbol_TABLETYPE   + 1;
    StatPos   = ArityPos  + symbol_TABLEARITY  + 1;
    PropPos   = StatPos   + symbol_TABLESTAT   + 1;
    StringPos = PropPos   + symbol_TABLEPROP   + 1;
    EndPos    = StringPos + symbol_TABLESTRING + 1;

    fputs("\n\n", stdout);
    misc_PrintChar(symbol_TABLEBLANKS, ' '); putchar('+');
    misc_PrintChar(symbol_TABLETYPE,   '-'); putchar('+');
    misc_PrintChar(symbol_TABLEARITY,  '-'); putchar('+');
    misc_PrintChar(symbol_TABLESTAT,   '-'); putchar('+');
    misc_PrintChar(symbol_TABLEPROP,   '-'); putchar('+');
    misc_PrintChar(symbol_TABLESTRING, '-'); putchar('+'); putchar('\n');
    misc_PrintChar(symbol_TABLEBLANKS, ' '); putchar('|');
    ActPos = TypePos   + 1 + fputs(" Type", stdout);
    misc_PrintChar((ArityPos  - ActPos), ' '); putchar('|');
    ActPos = ArityPos  + 1 + fputs(" Arity", stdout);
    misc_PrintChar((StatPos   - ActPos), ' '); putchar('|');
    ActPos = StatPos   + 1 + fputs(" Stat", stdout);
    misc_PrintChar((PropPos   - ActPos), ' '); putchar('|');
    ActPos = PropPos   + 1 + fputs(" Prop", stdout);
    misc_PrintChar((StringPos - ActPos), ' '); putchar('|');
    ActPos = StringPos + 1 + fputs(" String", stdout);
    misc_PrintChar((EndPos    - ActPos), ' '); putchar('|'); putchar('\n');
    misc_PrintChar(symbol_TABLEBLANKS,   ' '); putchar('+');
    misc_PrintChar(symbol_TABLETYPE,     '-'); putchar('+');
    misc_PrintChar(symbol_TABLEARITY,    '-'); putchar('+');
    misc_PrintChar(symbol_TABLESTAT,     '-'); putchar('+');
    misc_PrintChar(symbol_TABLEPROP,     '-'); putchar('+');
    misc_PrintChar(symbol_TABLESTRING,   '-'); putchar('+'); putchar('\n');
  
    for (Index = 1; Index < symbol_ACTINDEX; Index++) {
      Entry  = symbol_Signature(Index);
      if (Entry != NULL) {
	Info = Entry->info;
	
	misc_PrintChar(symbol_TABLEBLANKS,   ' '); putchar('|');
	ActPos = TypePos + 1;

	switch (symbol_Type(Info)) {
	case symbol_CONSTANT:
	  ActPos += fputs(" Con", stdout); break;
	case symbol_FUNCTION:
	  ActPos += fputs(" Fun", stdout); break;
	case symbol_PREDICATE:
	  ActPos += fputs(" Pre", stdout); break;
	case symbol_JUNCTOR:
	  ActPos += fputs(" Jun", stdout); break;
	}
	misc_PrintChar((ArityPos  - ActPos), ' '); putchar('|');
	ActPos = ArityPos + 1 + printf(" %3d", Entry->arity);
	misc_PrintChar((StatPos   - ActPos), ' '); putchar('|');
	ActPos = StatPos + 1;
	if (symbol_Type(Info) == symbol_FUNCTION) {
	  switch (symbol_Stat(Info)) {
	  case symbol_STATLEX:
	    ActPos += fputs(" Lex", stdout); break;
	  case symbol_STATMUL:
	    ActPos += fputs(" Mul", stdout); break;
	  }
	}
	misc_PrintChar((PropPos   - ActPos), ' '); putchar('|');
	ActPos = PropPos + 1 + printf(" %zd", Entry->props);
	misc_PrintChar((StringPos - ActPos), ' '); putchar('|');
	ActPos = StringPos + 1 +  printf(" %s", Entry->name);
	misc_PrintChar((EndPos    - ActPos), ' '); putchar('|'); putchar('\n');
      }
    }
    misc_PrintChar(symbol_TABLEBLANKS, ' '); putchar('+');
    misc_PrintChar(symbol_TABLETYPE,   '-'); putchar('+');
    misc_PrintChar(symbol_TABLEARITY,  '-'); putchar('+');
    misc_PrintChar(symbol_TABLESTAT,   '-'); putchar('+');
    misc_PrintChar(symbol_TABLEPROP,   '-'); putchar('|');
    misc_PrintChar(symbol_TABLESTRING, '-'); putchar('+'); putchar('\n');
  }
}


void symbol_FPrintDFGSignature(FILE* File)
/**************************************************************
  INPUT:   A file stream.
  RETURNS: Nothing.
  SUMMARY: Prints all signature symbols in DFG format to the
           file stream.
***************************************************************/
{
  NAT    i;
  SYMBOL symbol;
  LIST   functions, predicates;

  functions  = list_NReverse(symbol_GetAllFunctions());
  predicates = list_NReverse(list_DeleteElementIf(symbol_GetAllPredicates(), (BOOL (*)(POINTER))symbol_IsPredefined));

  /* First print the function symbols */
  if (!list_Empty(functions)) {
    fputs("  functions[", File);
    i = 0;
    do {
      symbol = (SYMBOL) list_Top(functions);
      fprintf(File, "(%s, %d)", symbol_Name(symbol), symbol_Arity(symbol));
      functions = list_Pop(functions);
      if (!list_Empty(functions))
	fputs(", ", File);
      if (i < 15)
	i++;
      else {
	i = 0;
	fputs("\n\t", File);
      }
      
    } while (!list_Empty(functions));
    fputs("].\n", File);
  }

  /* Now print the predicate symbols */
  if (!list_Empty(predicates)) {
    i = 0;
    fputs("  predicates[", File);
    do {
      symbol = (SYMBOL) list_Top(predicates);
      fprintf(File, "(%s, %d)", symbol_Name(symbol), symbol_Arity(symbol));
      predicates = list_Pop(predicates);
      if (!list_Empty(predicates))
	fputs(", ", File);
      if (i < 15)
	i++;
      else {
	i = 0;
	fputs("\n\t", File);
      }
    } while (!list_Empty(predicates));
    fputs("].\n", File);
  }
  list_Delete(predicates);
  list_Delete(functions);
}

/**************************************************************/
/* Functions for Many Sorting                                 */
/**************************************************************/


int symbol_MSortSymbolIndex(SYMBOL ActSymbol)
{
  return symbol_Signature(symbol_Index(ActSymbol))->number.index;
}

 void symbol_MSortSetSymbolIndex(SYMBOL ActSymbol, int Index)
{
  symbol_Signature(symbol_Index(ActSymbol))->number.index = Index;
}

SYMBOL symbol_MSortSymbol(MANYSORTS MSort)
{
  return MSort->manysort;
}

void symbol_MSortSetSymbol(MANYSORTS MSort, SYMBOL Symbol)
{
  MSort->manysort = Symbol;
}

int symbol_MSortFirstVarIndex(MANYSORTS MSort)
{
  return MSort->firstvarindex;
}

void symbol_MSortSetFirstVarIndex(MANYSORTS MSort, int Index)
{
  MSort->firstvarindex = Index;
}

int symbol_MSortFinalVarIndex(MANYSORTS MSort)
{
  return MSort->finalvarindex;
}

void symbol_MSortSetFinalVarIndex(MANYSORTS MSort, int Index)
{
  MSort->finalvarindex = Index;
}

int symbol_MSortActVarIndex(MANYSORTS MSort)
{
  return MSort->actvarindex;
}

void symbol_MSortSetActVarIndex(MANYSORTS MSort, int Index)
{
  MSort->actvarindex = Index;
}

SYMBOL symbol_MVarSort(MANYVARS MVar)
{
  return MVar->manysort;
}

void symbol_MVarSetSort(MANYVARS MVar, SYMBOL Sort)
{
  MVar->manysort = Sort;
}

SYMBOL symbol_MVarNextVar(MANYVARS MVar)
{
  return MVar->nextvar;
}

void symbol_MVarSetNextVar(MANYVARS MVar, SYMBOL Var)
{
  MVar->nextvar = Var;
}

SYMBOL symbol_MSortVariableSort(SYMBOL Var)
{
#ifdef CHECK
  if ((MANYVARS)array_GetElement(symbol_STANDMVARS, Var) == (MANYVARS)NULL) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In symbol_MSortVariableSort: No sort declared for variable.\n");
    misc_FinishErrorReport();
  }
#endif

  return symbol_MVarSort((MANYVARS)array_GetElement(symbol_STANDMVARS, Var));
}

BOOL symbol_IsSort(SYMBOL Symbol)
{
  return symbol_IsPredicate(Symbol) && symbol_Arity(Symbol) == 1 && symbol_HasProperty(Symbol, MANYSORT);
}

void symbol_MSortCreate(SYMBOL Symbol)
/**************************************************************
  INPUT:   A monadic predicate symbol.
  RETURNS: Nothing.
  EFFECT:  Creates a sort out of the symbol.
***************************************************************/
{

  MANYSORTS NewSort;
  int       Index;

#ifdef CHECK
  if (!symbol_IsPredicate(Symbol) || symbol_Arity(Symbol) != 1 || symbol_HasProperty(Symbol, MANYSORT)) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In symbol_MSortCreate: Illegal input.\n");
    misc_FinishErrorReport();
  }
#endif
  
  symbol_AddProperty(Symbol, MANYSORT);
  NewSort = (MANYSORTS)memory_Malloc(sizeof(MANYSORTS_NODE));
  symbol_MSortSetSymbol(NewSort,Symbol);
  symbol_MSortSetFirstVarIndex(NewSort, 0);
  symbol_MSortSetFinalVarIndex(NewSort, 0);
  symbol_MSortSetActVarIndex(NewSort, 0);
  symbol_STANDMSORTS = array_AddGetIndex(symbol_STANDMSORTS, (intptr_t)NewSort, &Index);
  symbol_MSortSetSymbolIndex(Symbol, Index);

}

SYMBOL symbol_MSortFirstVariable(SYMBOL Symbol)
/**************************************************************
  INPUT:   A sort symbol.
  RETURNS: The smallest variable having the respective sort.
  EFFECT:  Potentially creates a new variable if none defined so far
***************************************************************/
{
  SYMBOL    NewVar;
  MANYSORTS MSort;
  MANYVARS  MVar;

#ifdef CHECK
  if (!symbol_HasProperty(Symbol, MANYSORT)) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In symbol_MSortFirstVariable: Input is not an MSort.\n");
    misc_FinishErrorReport();
  }
#endif

  MSort  = (MANYSORTS)array_GetElement(symbol_STANDMSORTS, symbol_MSortSymbolIndex(Symbol));
  NewVar = symbol_MSortFirstVarIndex(MSort);

  if (NewVar != 0)
    return NewVar;

  MVar = (MANYVARS)memory_Malloc(sizeof(MANYVARS_NODE));
  
  symbol_MVarSetNextVar(MVar, 0);
  symbol_MVarSetSort(MVar, Symbol);
  symbol_STANDMVARS = array_AddAtIndex(symbol_STANDMVARS, ++symbol_STANDACTVAR, (intptr_t)MVar);
  symbol_MSortSetFirstVarIndex(MSort, symbol_STANDACTVAR);
  symbol_MSortSetFinalVarIndex(MSort, symbol_STANDACTVAR);
  NewVar = symbol_STANDACTVAR;
  return NewVar;
}

SYMBOL symbol_MSortLastVariable(SYMBOL Symbol)
/**************************************************************
  INPUT:   A sort symbol.
  RETURNS: The largest variable having the respective sort.
  EFFECT:  Potentially creates a new variable if none defined so far
***************************************************************/
{
  SYMBOL    NewVar;
  MANYSORTS MSort;
  MANYVARS  MVar;

#ifdef CHECK
  if (!symbol_HasProperty(Symbol, MANYSORT)) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In symbol_MSortFirstVariable: Input is not an MSort.\n");
    misc_FinishErrorReport();
  }
#endif

  MSort  = (MANYSORTS)array_GetElement(symbol_STANDMSORTS, symbol_MSortSymbolIndex(Symbol));
  NewVar = symbol_MSortFinalVarIndex(MSort);

  if (NewVar != 0) 
    return NewVar;

  MVar = (MANYVARS)memory_Malloc(sizeof(MANYVARS_NODE));
  
  symbol_MVarSetNextVar(MVar, 0);
  symbol_MVarSetSort(MVar, Symbol);
  symbol_STANDMVARS = array_AddAtIndex(symbol_STANDMVARS, ++symbol_STANDACTVAR, (intptr_t)MVar);
  symbol_MSortSetFirstVarIndex(MSort, symbol_STANDACTVAR);
  symbol_MSortSetFinalVarIndex(MSort, symbol_STANDACTVAR);
  NewVar = symbol_STANDACTVAR;
  return NewVar;
}

SYMBOL symbol_MSortNextVariable(SYMBOL Var)
/**************************************************************
  INPUT:   A variable symbol.
  RETURNS: The next variable having the same sort.
  EFFECT:  Potentially creates a new variable if none defined so far
***************************************************************/
{
  SYMBOL    NewVar;
  MANYSORTS MSort;
  MANYVARS  MVar, NewMVar;

#ifdef CHECK
  if (array_GetElement(symbol_STANDMVARS, Var) == (intptr_t)NULL) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In symbol_MSortNextVariable: Input is not a sorted variable.\n");
    misc_FinishErrorReport();
  }
#endif

  MVar   = (MANYVARS)array_GetElement(symbol_STANDMVARS, Var);
  NewVar = symbol_MVarNextVar(MVar);
  
  if (NewVar != 0)
    return NewVar;

  NewMVar = (MANYVARS)memory_Malloc(sizeof(MANYVARS_NODE));

#ifdef CHECK
  if (symbol_STANDACTVAR >= symbol__NOOFSTANDARDVAR) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In symbol_MSortNextVariable: Ran out of standard variables.\n");
    misc_FinishErrorReport();
  }
#endif
  
  symbol_MVarSetNextVar(MVar, ++symbol_STANDACTVAR);
  symbol_MVarSetNextVar(NewMVar, 0);
  symbol_MVarSetSort(NewMVar, symbol_MVarSort(MVar));
  symbol_STANDMVARS = array_AddAtIndex(symbol_STANDMVARS, symbol_STANDACTVAR, (intptr_t)NewMVar);
  MSort = (MANYSORTS)array_GetElement(symbol_STANDMSORTS, symbol_MSortSymbolIndex(symbol_MVarSort(MVar)));
  symbol_MSortSetFinalVarIndex(MSort, symbol_STANDACTVAR);
  NewVar = symbol_STANDACTVAR;
  return NewVar;
}

void symbol_MSortResetActVarCounters()
/**************************************************************
  INPUT:   None.
  RETURNS: None.
  EFFECT:  Resets all act sort var counters to the first variable.
***************************************************************/
{
  if (symbol_SignatureExists()) {
    int       Index;
    MANYSORTS Sort;
    for(Index = 0; Index < array_GetSize(symbol_STANDMSORTS); Index++) {
      Sort = (MANYSORTS)array_GetElement(symbol_STANDMSORTS, Index);
      symbol_MSortSetActVarIndex(Sort, symbol_MSortFirstVarIndex(Sort));
    }
  }
}

SYMBOL symbol_MSortGetNextActVar(SYMBOL Sort)
/**************************************************************
  INPUT:   A sort symbol.
  RETURNS: The current variable in the sequence of variables of <Sort>
           with respect to the act var counter.
  EFFECT:  Updated the act var counter by next variable, if needed creates an
           additional variable of sort <Sort>
***************************************************************/
{
  SYMBOL    NewVar, OldVar;
  MANYSORTS MSort;

#ifdef CHECK
  if (!symbol_HasProperty(Sort, MANYSORT)) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In symbol_MSortGetNextActVar: Input is not an MSort.\n");
    misc_FinishErrorReport();
  }
#endif

  MSort  = (MANYSORTS)array_GetElement(symbol_STANDMSORTS, symbol_MSortSymbolIndex(Sort));
  OldVar = symbol_MSortActVarIndex(MSort);
  NewVar = (SYMBOL)NULL;

  if (OldVar == 0) {                               /* No variable for the sort available */
    NewVar = symbol_MSortFirstVariable(Sort);
    symbol_MSortSetActVarIndex(MSort, NewVar);
    return NewVar;				
  }

  NewVar = symbol_MSortNextVariable(OldVar);
  symbol_MSortSetActVarIndex(MSort, NewVar);

  return OldVar;
}

void symbol_MSortSetActVarCounters(SYMBOL Var)
/**************************************************************
  INPUT:   A variable symbol.
  RETURNS: None.
  EFFECT:  Sets all act sort var counters to a variable larger
           than <Var>; if necessary fresh variables are created
***************************************************************/
{
  if (symbol_SignatureExists()) {
    int       Index;
    MANYSORTS MSort;
    SYMBOL    ActVar;
    for(Index = 0; Index < array_GetSize(symbol_STANDMSORTS); Index++) {
      MSort  = (MANYSORTS)array_GetElement(symbol_STANDMSORTS, Index);
      ActVar = symbol_MSortFirstVariable(symbol_MSortSymbol(MSort));
      while (ActVar <= Var) 
	ActVar = symbol_MSortNextVariable(ActVar);
      symbol_MSortSetActVarIndex(MSort, ActVar);
    }
  }
}

void symbol_MSortPrintVars(void)
/**************************************************************
  INPUT:   None.
  RETURNS: Nothing.
  EFFECTS: Prints the Many sorted variable structure
***************************************************************/
{

  if (symbol_SignatureExists()) {
    int       Index;
    MANYVARS  MVar;

    printf("\n MVar ARRAY \n");
    for(Index = 1; Index <= symbol_STANDACTVAR; Index++) {
      MVar = (MANYVARS)array_GetElement(symbol_STANDMVARS, Index);
      printf(" %5d ", Index); 
      if (MVar && symbol_MVarSort(MVar)) {
	printf("%5d ", (int)symbol_MVarNextVar(MVar));
	symbol_Print(symbol_MVarSort(MVar));
      }
      else
	printf(" None ");
      printf("\n");
    }
  }
}

void symbol_MSortPrintFunctions(void)
/**************************************************************
  INPUT:   A file stream.
  RETURNS: Nothing.
  SUMMARY: Prints all signature symbols in DFG format to the
           file stream.
***************************************************************/
{
  if (symbol_SignatureExists()) {
    NAT    i;
    SYMBOL symbol;
    LIST   functions;

    functions  = list_NReverse(symbol_GetAllFunctions());

    /* First print the function symbols */
    if (!list_Empty(functions)) {
      printf("\n Functions with Range Sort \n");
      i = 0;
      do {
	symbol = (SYMBOL) list_Top(functions);
	printf("%s  ", symbol_Name(symbol));
	if (symbol_MFunctionSort(symbol))
	  printf("%s  \n", symbol_Name(symbol_MFunctionSort(symbol)));
	else
	  printf(" None \n");
	functions = list_Pop(functions);
      
      } while (!list_Empty(functions));
    }

    list_Delete(functions);
  }
}
