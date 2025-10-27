/**************************************************************/
/* ********************************************************** */
/* *                                                        * */
/* *                  PARSER FOR DFG SYNTAX                 * */
/* *                                                        * */
/* *  $Module:   DFG                                        * */ 
/* *                                                        * */
/* *  Copyright (C) 1997, 1998, 1999, 2000, 2001            * */
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
/* $Revision: 1.14 $                                        * */
/* $State: Exp $                                            * */
/* $Date: 2011-05-22 12:53:37 $                             * */
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





#include "dfgparser.h"
#include "msorts.h"
#include "nextclause.h"
	
/* Used for delayed parsing of plain clauses */
typedef struct {
  LIST constraint;
  LIST antecedent;
  LIST succedent;
  TERM selected;
} DFG_PLAINCLAUSEHELP, *DFG_PLAINCLAUSE;

static DFGDESCRIPTION      dfg_DESC;
static LIST                dfg_AXIOMLIST;
static LIST                dfg_CONJECLIST;
static LIST                dfg_SORTDECLLIST;
static LIST                dfg_INCLUDELIST;
/* symbol precedence explicitly defined by user */
static LIST                dfg_USERPRECEDENCE;
static LIST                dfg_USERSELECTION;
static LIST                dfg_CLAXRELATION;
static LIST                dfg_CLAXAXIOMS;
static LIST                dfg_AXCLAUSES;
static LIST                dfg_CONCLAUSES;
static LIST                dfg_PROOFLIST;     /* list_of_proofs */
static LIST                dfg_TERMLIST;      /* list_of_terms  */
static LIST                dfg_PLAINAXCLAUSES; /* list of DFG_PLAINCLAUSE */
static LIST                dfg_PLAINCONCLAUSES; /* list of DFG_PLAINCLAUSE */
static LIST                dfg_TEMPPLAINCLAUSES; /* temporal list of DFG_PlAINCLAUSE, until they are concatenated to dfg_PLAINAXCLAUSES or dfg_PLAINCONCLAUSES */
static TERM                dfg_SELECTED_LITERAL; /* last selected literal */
static BOOL                dfg_IGNORE;      /* tokens are ignored while TRUE */
static BOOL		   dfg_IGNORESETTINGS; /* issue a warning for included file with SPASS settings */
static FLAGSTORE           dfg_FLAGS;
static PRECEDENCE          dfg_PRECEDENCE;

static LIST                dfg_DECLARATIONS;  /* list_of_declarations */

/* used also in the scanner */
NAT                        dfg_LINENUMBER;
BOOL			   dfg_IGNORETEXT;
dfgtokentype               dfg_LastChecked = DFG_begin_problem;

void yyerror(const char*);
int  yylex(void);		/* Defined in dfgscanner.l */

static void   dfg_SymbolDecl(int, char*, int);
static SYMBOL dfg_Symbol(char*, intptr_t);
/*static void   dfg_SubSort(char*, char*);*/
static void   dfg_SymbolGenerated(SYMBOL, BOOL, LIST);
static void   dfg_TranslPairDecl(char*, char*);
static TERM   dfg_CreateQuantifier(SYMBOL, LIST, TERM);

TERM dfg_TermCreate(char* Name, LIST Arguments)
/***************************************************************
  INPUT:   A string <Name> and a list of terms <Arguments>.
  RETURNS: Does a symbol lookup for <Name> and creates out of
           the found or otherwise created new symbol and <Arguments>
           a new term.
  CAUTION: Deletes the string <Name>.
***************************************************************/
{
  SYMBOL s;
  NAT    arity;
  arity = list_Length(Arguments);
  s = dfg_Symbol(Name, arity); /* Frees the string */
  if (!symbol_IsVariable(s) && !symbol_IsFunction(s)) {
    misc_StartUserErrorReport();
    misc_UserErrorReport("\n Line %d: is not a function.\n", dfg_LINENUMBER);
    misc_FinishUserErrorReport();
  }
  return term_Create(s, Arguments);
}

TERM dfg_AtomCreate(char* Name, LIST Arguments)
/* Look up the symbol, check its arity and create the atom term */
{
  SYMBOL s;
  s = dfg_Symbol(Name, list_Length(Arguments)); /* Frees the string */
  if (symbol_IsVariable(s) || !symbol_IsPredicate(s)) {
    misc_StartUserErrorReport();
    misc_UserErrorReport("\n Line %d: Symbol is not a predicate.\n", dfg_LINENUMBER);
    misc_FinishUserErrorReport();
  }
  return term_Create(s, Arguments);
}

void dfg_DeleteStringList(LIST List)
{
  list_DeleteWithElement(List, (void (*)(POINTER)) string_StringFree);
}

/**************************************************************/
/* Functions that handle symbols with unspecified arity       */
/**************************************************************/

/* The symbol list holds all symbols whose arity wasn't       */
/* specified in the symbol declaration section.               */
/* If the arity of a symbol was not specified in this section */
/* it is first set to 0. If the symbol occurs with always the */
/* same arity 'A' the arity of this symbol is set to 'A'.     */
static LIST   dfg_SYMBOLLIST;

static void dfg_SymAdd(SYMBOL);
static void dfg_SymCheck(SYMBOL, NAT);
static void dfg_SymCleanUp(void);

/**************************************************************/
/* Functions that handle variable names                       */
/**************************************************************/

/* List of quantified variables in the current input formula. */
/* This list is used to find symbols that by mistake weren't  */
/* declared in the symbol declaration section                 */
/* --> free variables                                         */
/* This is a list of lists, since each time a quantifier is   */
/* reached, a new list is added to the global list.           */
static LIST dfg_VARLIST;
static LIST dfg_MSORTLIST;
static BOOL dfg_VARDECL;

static void   dfg_VarStart(void);
static void   dfg_VarStop(void);
static void   dfg_VarBacktrack(void);
static void   dfg_VarCheck(void);
static SYMBOL dfg_VarLookup(char*,SYMBOL);
static SYMBOL dfg_VarSortCreate(char*,SYMBOL);
static DFG_PLAINCLAUSE dfg_PlainClauseCreate(void);
static void dfg_PlainClauseFree(DFG_PLAINCLAUSE Clause);

static char * dfg_StripQuotes(char * str); /* strip quotes from include filename */




static void dfg_Init(FILE* Input, FLAGSTORE Flags, PRECEDENCE Precedence, DFGDESCRIPTION Description)
/**************************************************************
  INPUT:   The input file stream for the parser, a flag store,
           a precedence and a problem description store.
  RETURNS: Nothing.
  EFFECT:  The parser and scanner are initialized.
           The parser will use the flag store and the precedence
	   to memorize the settings from the input file.
***************************************************************/
{
  dfg_AXIOMLIST        = list_Nil();
  dfg_CONJECLIST       = list_Nil();
  dfg_INCLUDELIST      = list_Nil();
  dfg_SORTDECLLIST     = list_Nil();
  dfg_USERPRECEDENCE   = list_Nil();
  dfg_USERSELECTION    = list_Nil();
  dfg_CLAXRELATION     = list_Nil();
  dfg_CLAXAXIOMS       = list_Nil();
  dfg_AXCLAUSES        = list_Nil();
  dfg_CONCLAUSES       = list_Nil();
  dfg_PROOFLIST        = list_Nil();
  dfg_TERMLIST         = list_Nil();
  dfg_SYMBOLLIST       = list_Nil();
  dfg_VARLIST          = list_Nil();
  dfg_MSORTLIST        = list_Nil();
  dfg_PLAINAXCLAUSES   = list_Nil();
  dfg_PLAINCONCLAUSES  = list_Nil();
  dfg_TEMPPLAINCLAUSES = list_Nil();
  dfg_SELECTED_LITERAL = (TERM) NULL;
  dfg_VARDECL          = FALSE;
  dfg_IGNORE           = FALSE;
  dfg_FLAGS            = Flags;
  dfg_PRECEDENCE       = Precedence;
  dfg_DESC             = Description;
}

void dfg_DeleteClAxRelation(LIST ClAxRelation)
{
  LIST Scan1, Scan2;

  for (Scan1=ClAxRelation;!list_Empty(Scan1);Scan1=list_Cdr(Scan1)) {
    for (Scan2=list_Cdr(list_Car(Scan1));!list_Empty(Scan2);Scan2=list_Cdr(Scan2))
      string_StringFree((char *)list_Car(Scan2));
    list_Delete(list_Car(Scan1));
  }
  list_Delete(ClAxRelation);
}

FILE* dfg_OpenFile(const char * FileName, char* IncludePath, char ** const DiscoveredName)
/**************************************************************
  INPUT: A string filename, a string IncludePath,
           and a pointer to char pointer
           to hold the fullpath name of the file 
           that was opened in the end (can be null).
  RETURNS: A opened file.
  EFFECT:
  Opens a file for reading.
  The file is searched for using the extended search mechanism.
  If the parameter IncludePath has non-trivial value, its value is used,
  otherwise we use the SPASSINPUT environment variable.  
***************************************************************/
{  
  if (IncludePath != NULL && strlen(IncludePath)>0) 
    return misc_OpenFileExt(FileName,"r",IncludePath,DiscoveredName);
  else
    return misc_OpenFileEnv(FileName,"r","SPASSINPUT",DiscoveredName);
}

static LIST dfg_TestList; /* list of strings used in nonmember tests */
	
static BOOL dfg_LabelFormulaPairIsNonmember(POINTER pair) {
  if (list_PairFirst(pair) == NULL)
    return TRUE;
	
  return !list_Member(dfg_TestList,list_PairFirst(pair),(BOOL (*)(POINTER, POINTER))string_Equal);
}

static void dfg_DeleteLabelTermPair(POINTER pair)	{
  term_Delete(list_PairSecond(pair));
  if (list_PairFirst(pair) != NULL)
    string_StringFree(list_PairFirst(pair));  /* Free the label */
  list_PairFree(pair);
}

void dfg_FilterClausesBySelection(LIST* Clauses, LIST* ClAxRelation, LIST selection)
/**************************************************************
  INPUT: A pointer to a clause list, a pointer to a ClAxRelation list
         and a char* list.
  RETURNS: Nothing.
  EFFECT: The clause list is filtered in such a way that clauses
          with names not in the selection list are deleted.
          The name for the clauses is found in the ClAxRelation structure.
          And the corresponding entry in ClAxRelation is also 
          deleted if the clause is.
          The clause list and the ClAxRelation list are assumed to be coherent.
  NOTE: The coherence is assured when the lists come from
        an included file, because in includes settings sections are forbidden
        and so the user cannot specify ClAxRelation herself.
***************************************************************/
{
  /* result lists */
  LIST RClauses;
  LIST RClAxRelation;

  /* pointers to last element in the result */
  LIST RLClauses = list_Nil();
  LIST RLClAxRelation = list_Nil();

  RClauses = list_Nil();
  RClAxRelation = list_Nil();

  /*
    Performs parallel tranversal of Clauses and ClAxRelation.
    Either deleting from both or keeping both for the result.
  */

  while (!list_Empty(*Clauses)) {  
    CLAUSE Clause;
    LIST ClAx;
    LIST Labels;
    char* Label;

    /* current list elements */
    LIST CClause;
    LIST CClAxRelation;

    CClause = *Clauses;
    CClAxRelation = *ClAxRelation;

    Clause = (CLAUSE) list_Car(CClause);

#ifdef CHECK
    if (list_Empty(CClAxRelation)) {
      misc_StartUserErrorReport();
      misc_ErrorReport("\n In dfg_FilterClausesBySelection: ClAxRelation too short!\n");
      misc_FinishUserErrorReport();
    }
#endif

    ClAx = (LIST) list_Car(CClAxRelation);
#ifdef CHECK
    if ((intptr_t)list_Car(ClAx) != clause_Number(Clause)) {
      misc_StartUserErrorReport();
      misc_ErrorReport("\n In dfg_FilterClausesBySelection: Incompatible ClAxRelation!\n");
      misc_FinishUserErrorReport();
    }
#endif

    Labels = list_Cdr(ClAx);
    if (list_Empty(Labels)) 
      Label = NULL;
    else
      Label = (char*)list_Car(Labels);

    /*and we can already move on*/
    *Clauses = list_Cdr(*Clauses);
    *ClAxRelation = list_Cdr(*ClAxRelation);

    if (!Label || !list_Member(selection,Label,(BOOL (*)(POINTER, POINTER))string_Equal)) {
      /* deleting */
      if (RClauses) { /*if RClauses exists so does RLClauses and RLClAxRelation*/
        list_Rplacd(RLClauses,*Clauses);
        list_Rplacd(RLClAxRelation,*ClAxRelation);
      }

      clause_Delete(Clause);
      list_DeleteWithElement(Labels,(void (*)(POINTER))string_StringFree);
      list_Free(ClAx);
      list_Free(CClause);
      list_Free(CClAxRelation);

    } else {
      /* keeping */
      if (!RClauses) {
        /* the result will contain at least one element */
        RClauses = CClause;
        RClAxRelation = CClAxRelation;
      }

      /* if we delete the next, whis will need to be updated */
      RLClauses = CClause;
      RLClAxRelation = CClAxRelation;
    }
  }

  *Clauses = RClauses;
  *ClAxRelation = RClAxRelation;
}

LIST dfg_DFGParser(FILE* File, char * IncludePath, FLAGSTORE Flags, PRECEDENCE Precedence, DFGDESCRIPTION Description,
		   LIST* Axioms, LIST* Conjectures, LIST* Declarations,
		   LIST* UserDefinedPrecedence, LIST* UserDefinedSelection,
		   LIST* ClAxRelation, BOOL* HasPlainClauses)
/**************************************************************
  A stub around the real parser that takes care of following
  include directives.
  
  NOTE: This function is almost the same as tptp_TPTPParser form tptpparser.y
    any change in its implementation should propably be also propagated there.
***************************************************************/
{
  LIST Includes, Clauses;
  LIST FilesRead;
  DFGDESCRIPTION description_dummy;

  FilesRead = list_Nil();
  Includes = list_Nil();	

  dfg_IGNORESETTINGS= FALSE; 
  Clauses = dfg_DFGParserIncludesExplicit(File,Flags,Precedence,Description,FALSE,Axioms,Conjectures,Declarations,UserDefinedPrecedence,UserDefinedSelection,ClAxRelation,&Includes,HasPlainClauses);
	
  while (list_Exist(Includes)) {
    LIST pair;
    char* filename;
    LIST selection;
  
    pair      = list_Top(Includes);
    filename  = (char*)list_PairFirst(pair);
    selection = (LIST)list_PairSecond(pair);
    list_PairFree(pair);
    Includes = list_Pop(Includes);
		
    if (list_Member(FilesRead,filename,(BOOL (*)(POINTER, POINTER))string_Equal)) {
      misc_UserWarning("File %s already included, skipped!\n",filename);
      string_StringFree(filename);
    } else {
      LIST Axs, Conjs, Cls, UDS, CAR;
      BOOL HPC;
      FILE* FileToInclude;      
				
      FileToInclude = dfg_OpenFile(filename, IncludePath, NULL);

      Axs = Conjs = UDS = CAR = list_Nil();

      dfg_IGNORESETTINGS = TRUE; 
      description_dummy = desc_Create();
      Cls = dfg_DFGParserIncludesExplicit(FileToInclude, Flags, Precedence, description_dummy, TRUE, &Axs, &Conjs, Declarations, UserDefinedPrecedence, &UDS, &CAR, &Includes, &HPC);
      desc_Delete(description_dummy);

      if (list_Exist(selection)) { /*use the selection to filter Axs and Conjs */
        dfg_FilterClausesBySelection(&Cls,&CAR,selection);
      
        dfg_TestList = selection;
        Axs = list_DeleteElementIfFree(Axs,dfg_LabelFormulaPairIsNonmember,dfg_DeleteLabelTermPair);
	Conjs = list_DeleteElementIfFree(Conjs,dfg_LabelFormulaPairIsNonmember,dfg_DeleteLabelTermPair);
      }

      Clauses = list_Nconc(Clauses, Cls); 

      *Axioms = list_Nconc(*Axioms,Axs);
      *Conjectures = list_Nconc(*Conjectures,Conjs);

      /* "Summing up" UserDefinedSelection and ClAxRelation from all the files included. */
      *UserDefinedSelection = list_Nconc(*UserDefinedSelection, UDS);
      /*
	No explicit (user specified) ClAxRelation coming in dfg from includes!      
	*ClAxRelation = list_Nconc(*ClAxRelation, CAR);
	*/
      dfg_DeleteClAxRelation(CAR);

      /*The whole thing has plain clauses only if all the files have!*/			
      if (!HPC)
	*HasPlainClauses = FALSE;

      misc_CloseFile(FileToInclude,filename);
      FilesRead = list_Cons(filename,FilesRead);
    }

    list_DeleteWithElement(selection,(void (*)(POINTER))string_StringFree);		
  }
		
  list_DeleteWithElement(FilesRead,(void (*)(POINTER))string_StringFree);		
  return Clauses;
}

void dfg_InitExplicitParser() 
/**************************************************************
  This function should be called 
  prior to calling dfg_DFGParserIncludesExplicit
  from outside the dfg module.
***************************************************************/
{
  dfg_IGNORESETTINGS = FALSE; 
}

void dfg_CreateClausesFromTerms(LIST* Axioms, LIST* Conjectures,
				LIST* ClAxRelation, BOOL BuildClAx,
				FLAGSTORE Flags, PRECEDENCE Precedence)
/**************************************************************
  INPUT:   Pointers to two lists of term-label pairs,
            pointer to ClAx list,
            boolean flag and
            a flag store and a precedence.
  EFFECT:  Applies dfg_CreateClauseFromTerm to the 
           pairs in the lists treating the firsts as axioms 
           and second as conjectures, releasing the pair struct
           and the labels.
           If BuildClAx is set the labels are not deleted,
           but instead used to record the relation 
           between the clauses and their original names
           in the ClAxRelation structure
           (otherwise ClAxRelation remains intact).
***************************************************************/
{
  LIST    scan, tupel;
  TERM    clauseTerm;
  CLAUSE  clause;
  LIST    ClAxContribution, labels;

  ClAxContribution = list_Nil();
 
  /* Remove clause labels and create clauses from the terms */
  for (scan = *Axioms; !list_Empty(scan); scan = list_Cdr(scan)) {
    tupel = list_Car(scan);
    clauseTerm = list_PairSecond(tupel);
    clause = dfg_CreateClauseFromTerm(clauseTerm,TRUE, Flags, Precedence);
    list_Rplaca(scan, clause);

    if (BuildClAx) {
      if (list_PairFirst(tupel) != NULL)
        labels = list_List(list_PairFirst(tupel));
      else
        labels = list_Nil();
      
      ClAxContribution = list_Cons(list_Cons((POINTER)clause_Number(clause),labels),ClAxContribution);
    } else {
      if (list_PairFirst(tupel) != NULL)        /* Label is defined */
        string_StringFree(list_PairFirst(tupel));  /* Delete the label */
    }
  
    list_PairFree(tupel);
  }
  /* Since dfg_CreateClauseFromTerm() returns NULL for trivial tautologies */
  /* we now delete those NULL pointers from the clause list.               */
  *Axioms = list_PointerDeleteElement(*Axioms, NULL);
  for (scan = *Conjectures; !list_Empty(scan); scan = list_Cdr(scan)) {
    tupel = list_Car(scan);
    clauseTerm = list_PairSecond(tupel);
    clause = dfg_CreateClauseFromTerm(clauseTerm,FALSE, Flags, Precedence);
    list_Rplaca(scan, clause);

    if (BuildClAx) {
      if (list_PairFirst(tupel) != NULL)
        labels = list_List(list_PairFirst(tupel));
      else
        labels = list_Nil();
      
      ClAxContribution = list_Cons(list_Cons((POINTER)clause_Number(clause),labels),ClAxContribution);
    } else {
      if (list_PairFirst(tupel) != NULL)        /* Label is defined */
        string_StringFree(list_PairFirst(tupel));  /* Delete the label */
    }         
  
    list_PairFree(tupel);
  }
  /* Since dfg_CreateClauseFromTerm() returns NULL for trivial tautologies */
  /* we now delete those NULL pointers from the clause list.               */
  *Conjectures = list_PointerDeleteElement(*Conjectures, NULL);

  if (BuildClAx) {
    /* appending the contribution to the end! */
    *ClAxRelation = list_Nconc(*ClAxRelation,list_NReverse(ClAxContribution));
  }
}

LIST dfg_DFGParserIncludesExplicit(FILE* File, FLAGSTORE Flags, PRECEDENCE Precedence,
				   DFGDESCRIPTION Description, BOOL AppendImplicitClAx,
				   LIST* Axioms, LIST* Conjectures, LIST* Declarations,
				   LIST* UserDefinedPrecedence, LIST* UserDefinedSelection,
				   LIST* ClAxRelation, LIST* Includes, BOOL* HasPlainClauses)
/**************************************************************
  INPUT:   The input file containing clauses or formulae in DFG syntax,
           a flag store, a precedence used to memorize settings
	       from the file, and Description to store problem description.
           Boolean flag determinig whether the implicit ClAxRelation
           from Clauses should be appended to the explicit one 
           coming from settings.
           Axioms, Conjectures, SortDecl, UserDefinedPrecedence
           UserDefinedSelection, and ClAxRelation, and Includes are
	   pointers to lists used as return values.
           HasPlainClauses is a return value to indicate if
           the problem had clauses in the plain format.
  RETURNS: The list of clauses from File.
  EFFECT:  Reads formulae and clauses from the input file.
           The axioms, conjectures, sort declarations, user-defined
	   precedences, and includes are appended to the respective lists,
       the lists are not deleted!
	   The Includes list contains pairs (filename, selection),
	   where selection is a list of names of formulas chosen to include.
	   All lists except the returned clause list contain pairs
	   (label, term), where <label> may be NULL, if no
	   label was specified for that term.
	   <UserDefinedPrecedence> contains symbols sorted by decreasing
	   precedence. This list will only be changed, if the precedence
	   is explicitly defined in the input file. This can be done
	   by the 'set_precedence' flag in the SPASS settings list in
	   the DFG input file.
           <UserDefinedSelection> is set to a list of predicates to be
           selected in clause inferences by the 'set_selection' flag
           <ClAxRelation> is set to a clause, axiom relation where the
           clauses are given by their number, the axioms by name (string)
  CAUTION: The weight of the clauses is not correct and the literals
           are not oriented!
***************************************************************/
{
  LIST  scan,to_delete;
  NAT   bottom;

  dfg_Init(File, Flags, Precedence, Description);  /* Initialize the parser and scanner */
  bottom = stack_Bottom();
  
  dfg_parse(File);
    
  /* dfg_parse();         
     #ifdef CHECK 
     if (!stack_Empty(bottom)) {
     misc_StartErrorReport();
     misc_ErrorReport("\n In dfg_DFGParser: Stack not empty!\n");
     misc_FinishErrorReport();
     }
     #endif*/
  dfg_SymCleanUp();

  dfg_CreateClausesFromTerms(&dfg_AXCLAUSES,&dfg_CONCLAUSES,&dfg_CLAXRELATION,AppendImplicitClAx,Flags,Precedence);

  /* Delete the proof list */
  dfg_DeleteProofList(dfg_PROOFLIST);

  /* Delete the list_of_terms, since it'll be ignored */
  term_DeleteTermList(dfg_TERMLIST);

  if (list_Empty(dfg_PLAINAXCLAUSES) && list_Empty(dfg_PLAINCONCLAUSES))
    *HasPlainClauses = FALSE;
  else
    *HasPlainClauses = TRUE;
  scan = dfg_PLAINAXCLAUSES;
  while(!list_Empty(scan)) {
    DFG_PLAINCLAUSE clause = list_Car(scan);
    CLAUSE newclause = clause_CreateFromLiteralLists(clause->constraint, clause->antecedent, 
                                                     clause->succedent, FALSE, clause->selected);
    dfg_AXCLAUSES = list_Cons(newclause,dfg_AXCLAUSES);
  
    dfg_PlainClauseFree(clause);
    to_delete = scan;
    scan = list_Cdr(scan);    
    list_Free(to_delete);
  }  
  
  scan = dfg_PLAINCONCLAUSES;
  while(!list_Empty(scan)) {   
    DFG_PLAINCLAUSE clause = list_Car(scan);
    CLAUSE newclause = clause_CreateFromLiteralLists(clause->constraint, clause->antecedent, 
                                                     clause->succedent, TRUE, clause->selected);
    dfg_CONCLAUSES = list_Cons(newclause,dfg_CONCLAUSES);
     
    dfg_PlainClauseFree(clause);
    to_delete = scan;
    scan = list_Cdr(scan);    
    list_Free(to_delete);
  }  
  
  /*Temporary addition to atleast print the declarations*/

  scan = dfg_DECLARATIONS;



  scan = list_Nconc(list_NReverse(dfg_AXCLAUSES), list_NReverse(dfg_CONCLAUSES));

  *Axioms      = list_Nconc(*Axioms, dfg_AXIOMLIST);
  *Conjectures = list_Nconc(*Conjectures, dfg_CONJECLIST);
  //  *SortDecl    = list_Nconc(*SortDecl, dfg_SORTDECLLIST);
  *Includes    = list_Nconc(*Includes, dfg_INCLUDELIST);
  dfg_USERPRECEDENCE = list_NReverse(dfg_USERPRECEDENCE);
  *UserDefinedPrecedence = list_Nconc(*UserDefinedPrecedence, dfg_USERPRECEDENCE);
  *UserDefinedSelection  = dfg_USERSELECTION;
  *ClAxRelation          = dfg_CLAXRELATION;
  *Declarations          = list_Nconc(*Declarations, dfg_DECLARATIONS);

  return scan;
}

LIST dfg_ProofParser(FILE* File, FLAGSTORE Flags, PRECEDENCE Precedence, DFGDESCRIPTION Description)
/**************************************************************
  INPUT:   The input file containing clauses in DFG syntax,
           a flag store and a precedence used to memorize settings
           from the file.
  RETURNS: A list of tuples (label,clause,justificationlist,splitlevel,origin)
           representing a proof.
  EFFECT:  Reads inputs clauses with labels and the proof lists
           from the input file.
           The elements of the list are lists with five items.
	   1. the label (a string) of a clause,
	   2. the clause in TERM format,
           3. the list of justification labels (strings, too),
           4. the split level of the clause,
           5. the origin of the clause (RULE struct from clause.h).
	   Note that the justification list is empty for input
	   clauses.
***************************************************************/
{
  LIST  scan, tupel;
  TERM  term;
  NAT   bottom; 
  
  dfg_Init(File, Flags, Precedence, Description);  /* Initialize the parser and scanner */
  bottom = stack_Bottom();
  
  dfg_parse(File);          /* Invoke the parser */
  /*#ifdef CHECK 
    if (!stack_Empty(bottom)) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In dfg_ProofParser: Stack not empty!\n");
    misc_FinishErrorReport();
    }
    #endif*/

  dfg_SymCleanUp();

  /* Build the union of axiom and conjecture clauses */
  dfg_AXCLAUSES  = list_Nconc(dfg_AXCLAUSES, dfg_CONCLAUSES);
  dfg_CONCLAUSES = list_Nil();
  for (scan = dfg_AXCLAUSES; !list_Empty(scan); scan = list_Cdr(scan)) {
    tupel = list_Car(scan);
    term  = list_PairSecond(tupel);
    if (list_PairFirst(tupel) == NULL) {
      /* Ignore input clauses without label */
      term_Delete(term);
      list_PairFree(tupel);
      list_Rplaca(scan, NULL);
    } else
      /* Expand the pair to a tuple                            */
      /* (label,clause,justificationlist, split level, origin) */
      /* For input clauses the justificationlist is empty.     */
      /* Input clauses have split level 0.                     */
      list_Rplacd(tupel, list_Cons(term,list_Cons(list_Nil(),list_Cons(0, list_List((POINTER)INPUTCLAUSE)))));
  }
  /* Now delete the list items without labels */
  dfg_AXCLAUSES = list_PointerDeleteElement(dfg_AXCLAUSES, NULL);

  /* Delete the formula lists */
  dfg_DeleteFormulaPairList(dfg_AXIOMLIST);
  dfg_DeleteFormulaPairList(dfg_CONJECLIST);
  /* Delete includes list*/
  dfg_DeleteIncludePairList(dfg_INCLUDELIST);	  
  /* Delete the list of sort declarations */
  dfg_DeleteFormulaPairList(dfg_SORTDECLLIST);
  /* Delete the list_of_terms, since it'll be ignored */
  term_DeleteTermList(dfg_TERMLIST);

  /* Finally append the proof list to the list of input clauses with labels */
  dfg_PROOFLIST = list_NReverse(dfg_PROOFLIST);
  dfg_AXCLAUSES = list_Nconc(dfg_AXCLAUSES, dfg_PROOFLIST);

  return dfg_AXCLAUSES;
}


LIST dfg_TermParser(FILE* File, FLAGSTORE Flags, PRECEDENCE Precedence, DFGDESCRIPTION Description)
/**************************************************************
  INPUT:   The input file containing a list of terms in DFG syntax,
           a flag store and a precedence used to memorize settings
	   from the file.
  RETURNS: The list of terms from <File>.
  EFFECT:  Reads terms from the list_of_terms from the input file.
***************************************************************/
{
  NAT bottom;
  
  dfg_Init(File, Flags, Precedence, Description);   /* Initialize the parser and scanner */
  bottom = stack_Bottom();
  dfg_parse(File);          /* Invoke the parser */
  /*#ifdef CHECK 
    if (!stack_Empty(bottom)) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In dfg_TermParser: Stack not empty!\n");
    misc_FinishErrorReport();
    }
    #endif*/

  dfg_SymCleanUp();

  /* Delete the clause lists */
  dfg_DeleteFormulaPairList(dfg_AXCLAUSES);
  dfg_DeleteFormulaPairList(dfg_CONCLAUSES);
  /* Delete the formula lists */
  dfg_DeleteFormulaPairList(dfg_AXIOMLIST);
  dfg_DeleteFormulaPairList(dfg_CONJECLIST);
  /* Delete includes list*/
  dfg_DeleteIncludePairList(dfg_INCLUDELIST);	  
  /* Delete the proof list */
  dfg_DeleteProofList(dfg_PROOFLIST);
  /* Delete the list of sort declarations */
  dfg_DeleteFormulaPairList(dfg_SORTDECLLIST);

  return dfg_TERMLIST;
}

void dfg_DeleteFormulaPairList(LIST FormulaPairs)
/**************************************************************
  INPUT:   A list of pairs (label, formula).
  RETURNS: Nothing.
  EFFECT:  The list and the pairs with their strings and terms
           are completely deleted.
***************************************************************/
{
  LIST pair;

  for ( ; !list_Empty(FormulaPairs); FormulaPairs = list_Pop(FormulaPairs)) {
    pair = list_Car(FormulaPairs);  /* (label, term) */
    term_Delete(list_PairSecond(pair));
    if (list_PairFirst(pair) != NULL)
      string_StringFree(list_PairFirst(pair));  /* Free the label */
    list_PairFree(pair);
  }
}

void dfg_DeleteIncludePairList(LIST IncludePairs) 
/**************************************************************
  INPUT:   A list of pairs (filename, selectionlist).
  RETURNS: Nothing.
  EFFECT:  The list and the pairs with their strings
           are completely deleted.
***************************************************************/
{
  LIST pair;

  for ( ; !list_Empty(IncludePairs); IncludePairs = list_Pop(IncludePairs)) {
    pair = list_Car(IncludePairs);  /* (filename, selectionlist) */

    string_StringFree((char *)list_PairFirst(pair));
    list_DeleteWithElement(list_PairSecond(pair),(void (*)(POINTER))string_StringFree);
    list_PairFree(pair);
  }	
}

void dfg_StripLabelsFromList(LIST FormulaPairs)
/**************************************************************
  INPUT:   A list of pairs (label, formula).
  RETURNS: Nothing.
  EFFECT:  The pairs are replaced by the respective formula
           and the pairs with their label strings are deleted.
***************************************************************/
{
  LIST pair, scan;

  for (scan = FormulaPairs; !list_Empty(scan); scan = list_Cdr(scan)) {
    pair = list_Car(scan);  /* (label, term) */
    list_Rplaca(scan, list_PairSecond(pair));
    if (list_PairFirst(pair) != NULL)
      string_StringFree(list_PairFirst(pair));  /* Free the label */
    list_PairFree(pair);
  }
}

void dfg_DeleteProofList(LIST Proof)
/**************************************************************
  INPUT:   A list of tuples (label, term, justificationlist, split level).
  RETURNS: Nothing.
  EFFECT:  All memory used by the proof list is freed.
           The labels must NOT be NULL entries!
***************************************************************/
{
  /* Delete the proof list */
  for ( ; !list_Empty(Proof); Proof = list_Pop(Proof)) {
    LIST tupel = list_Car(Proof);
    string_StringFree(list_First(tupel));
    term_Delete(list_Second(tupel));
    dfg_DeleteStringList(list_Third(tupel));
    list_Delete(tupel);
  }
}

/**************************************************************/
/* Static Functions                                           */
/**************************************************************/

static void dfg_SymbolDecl(int SymbolType, char* Name, int Arity)
/**************************************************************
  INPUT:   The type of a symbol, the name, and the arity.
  RETURNS: Nothing.
  EFFECT:  This function handles the declaration of symbols.
           If <Arity> is -2, it means that the arity of the symbol
	   was not specified, if it is -1 the symbol is declared
	   with arbitrary arity. User defined symbols with arbitrary
           arity are not allowed.
	   The <Name> is deleted.
***************************************************************/
{
  NAT    arity;
  SYMBOL symbol;

  switch (Arity) {
  case -2: /* not specified */
    arity = 0;
    break;
  case -1:
    misc_StartUserErrorReport();
    misc_UserErrorReport("\n Line %u: symbols with arbitrary arity are not allowed.\n",
			 dfg_LINENUMBER);
    misc_FinishUserErrorReport();
  default:
    arity = Arity;
  }

  /* Check if this symbol was declared earlier */
  symbol = symbol_Lookup(Name);
  if (symbol != 0) {
    /* Symbol was declared before */
    /* Check if the old and new symbol type are equal */
    if ((SymbolType == DFG_FUNC && !symbol_IsFunction(symbol)) ||
	(SymbolType == DFG_PRDICAT && !symbol_IsPredicate(symbol)) ||
	(SymbolType == DFG_SRT && !symbol_IsSort(symbol))) {
      misc_StartUserErrorReport();
      misc_UserErrorReport("\n Line %u: symbol %s was already declared as ",
			   dfg_LINENUMBER, Name);
      switch (symbol_Type(symbol)) {
      case symbol_CONSTANT:
      case symbol_FUNCTION:
	misc_UserErrorReport("function.\n"); break;
      case symbol_PREDICATE:
	misc_UserErrorReport("predicate.\n"); break;
      case symbol_JUNCTOR:
	misc_UserErrorReport("predefined junctor.\n"); break;
      default:
	misc_UserErrorReport("unknown type.\n");
      }
      misc_FinishUserErrorReport();
    }
    /* Now check the old and new arity if specified */
    if (Arity != -2 && Arity != symbol_Arity(symbol)) {
      misc_StartUserErrorReport();
      misc_UserErrorReport("\n Line %u: symbol %s was already declared with arity %d\n",
			   dfg_LINENUMBER, Name, symbol_Arity(symbol));
      misc_FinishUserErrorReport();
    }
  } else {
    /* Symbol was not declared before */
    switch (SymbolType) {
    case DFG_FUNC:
      symbol = symbol_CreateFunction(Name, arity, symbol_STATLEX,dfg_PRECEDENCE);
      break;
    case DFG_PRDICAT:
      symbol = symbol_CreatePredicate(Name, arity,symbol_STATLEX,dfg_PRECEDENCE);
      break;
    case DFG_SRT:
      symbol = symbol_CreatePredicate(Name, arity,symbol_STATLEX,dfg_PRECEDENCE);
      symbol_MSortCreate(symbol);
      break;
    default:
      symbol = symbol_CreateJunctor(Name, arity, symbol_STATLEX, dfg_PRECEDENCE);
    }
    if (Arity == -2)
      /* Arity wasn't specified so check the arity for each occurrence */
      dfg_SymAdd(symbol);
  }

  string_StringFree(Name);  /* Name was copied */
}


static SYMBOL dfg_Symbol(char* Name, intptr_t Arity)
/**************************************************************
  INPUT:   The name of a symbol and the actual arity or 
           possibly sort in case of the symbol being a Variable.
  RETURNS: The corresponding SYMBOL.
  EFFECT:  This function checks if the <Name> was declared as
           symbol or variable. If not, an error message is printed
	   to stderr.
	   The <Name> is deleted.
***************************************************************/
{
  SYMBOL symbol;

  symbol = symbol_Lookup(Name);
  if (symbol != 0) {
    if (Arity < 0) {
      misc_StartUserErrorReport();
      misc_UserErrorReport("\n Line %d: Symbol %s cannot be sorted.\n",dfg_LINENUMBER,Name);
      misc_UserErrorReport("It has already been defined but not as a Variable.\n");
      misc_FinishUserErrorReport();
    }
    string_StringFree(Name);
    dfg_SymCheck(symbol, Arity); /* Check the arity */
  } else {
    /* Variable */
    if (Arity > 0) {
      misc_StartUserErrorReport();
      misc_UserErrorReport("\n Line %d: Undefined symbol %s.\n",dfg_LINENUMBER,Name);
      misc_FinishUserErrorReport();
    }
    symbol = dfg_VarLookup(Name,Arity);
  }
  return symbol;
}


static TERM dfg_CreateQuantifier(SYMBOL Symbol, LIST VarTermList, TERM Term)
/**************************************************************
  INPUT:   A quantifier symbol, a list possibly containing sorts,
           and a term.
  RETURNS: The created quantifier term..
***************************************************************/
{
  LIST varlist, sortlist, scan;
  TERM helpterm;

  /* First collect the variable symbols in varlist and the sorts in sortlist */
  varlist = sortlist = list_Nil();
  for ( ; !list_Empty(VarTermList); VarTermList = list_Pop(VarTermList)) {
    helpterm = list_Car(VarTermList);
    if (term_IsVariable(helpterm)) {
      varlist = list_Nconc(varlist, list_List((POINTER)term_TopSymbol(helpterm)));
      term_Delete(helpterm);
    } else {
      SYMBOL var = term_TopSymbol(term_FirstArgument(helpterm));
      varlist  = list_Nconc(varlist, list_List((POINTER)var));
      sortlist = list_Nconc(sortlist, list_List(helpterm));
    }
  }

  varlist = list_PointerDeleteDuplicates(varlist);
  /* Now create terms from the variables */
  for (scan = varlist; !list_Empty(scan); scan = list_Cdr(scan))
    list_Rplaca(scan, term_Create((SYMBOL)list_Car(scan), list_Nil()));

  if (!list_Empty(sortlist)) {
    if (symbol_Equal(fol_All(), Symbol)) {
      /* The conjunction of all sortterms implies the Term */
      if (symbol_Equal(fol_Or(), term_TopSymbol(Term))) {
	/* Special treatment if <Term> is a term with "or" like */
	/* in clauses: add all sort terms negated to the args    */
	/* of the "or" */
	for (scan = sortlist; !list_Empty(scan); scan = list_Cdr(scan))
	  /* Negate the sort terms */
	  list_Rplaca(scan, term_Create(fol_Not(), list_List(list_Car(scan))));
	sortlist = list_Nconc(sortlist, term_ArgumentList(Term));
	term_RplacArgumentList(Term, sortlist);
      } else {
	/* No "or" term, so build the implication term */
	if (list_Empty(list_Cdr(sortlist))) {
	  /* Only one sort term */
	  list_Rplacd(sortlist, list_List(Term));
	  Term = term_Create(fol_Implies(), sortlist);
	} else {
	  /* More than one sort term */
	  helpterm = term_Create(fol_And(), sortlist);
	  Term = term_Create(fol_Implies(), list_Cons(helpterm, list_List(Term)));
	}
      }
    } else if (symbol_Equal(fol_Exist(), Symbol)) {
      /* Quantify the conjunction of all sort terms and <Term> */
      if (symbol_Equal(fol_And(), term_TopSymbol(Term))) {
	/* Special treatment if <Term> has an "and" as top symbol: */
	/* just add the sort terms to the args of the "and".       */
	sortlist = list_Nconc(sortlist, term_ArgumentList(Term));
	term_RplacArgumentList(Term, sortlist);
      } else {
	sortlist = list_Nconc(sortlist, list_List(Term));
	Term = term_Create(fol_And(), sortlist);
      }
    }
  }
  helpterm = fol_CreateQuantifier(Symbol, varlist, list_List(Term));
  return helpterm;
}


CLAUSE dfg_CreateClauseFromTerm(TERM Clause, BOOL IsAxiom, FLAGSTORE Flags,
				PRECEDENCE Precedence)
/**************************************************************
  INPUT:   A clause term, a boolean value, a flag store and a precedence.
  RETURNS: The clause term converted to a CLAUSE.
  EFFECT:  This function converts a clause stored as term into an
           EARL clause structure.
	   If 'IsAxiom' is TRUE the clause is treated as axiom
	   clause else as conjecture clause.
           The function deletes the literals "false" and "not(true)"
           if they occur in <Clause>.
	   The contents of the flag store and the precedence are changed
	   because the parser read flag and precedence settings from
  MEMORY:  The clause term is deleted.
***************************************************************/
{
  LIST   literals, scan;
  TERM   literal;
  CLAUSE result;
  
  if (term_TopSymbol(Clause) == fol_All()) {
    /* Remove and free the quantifier and the OR term */
    literals = term_ArgumentList(term_SecondArgument(Clause));
    term_RplacArgumentList(term_SecondArgument(Clause), list_Nil());
  } else {
    /* Remove and free the OR term */
    literals = term_ArgumentList(Clause);
    term_RplacArgumentList(Clause, list_Nil());
  }
  term_Delete(Clause);

  for (scan = literals; !list_Empty(scan); scan = list_Cdr(scan)) {
    literal = (TERM) list_Car(scan);
    if (symbol_IsPredicate(term_TopSymbol(literal))) {  /* Positive literal */
      if (fol_IsFalse(literal)) {
	/* Ignore this literal */
	term_Delete(literal);
	list_Rplaca(scan, NULL); /* Mark the actual list element */
      }
    } else {
      /* Found a negative literal */
      TERM atom = term_FirstArgument(literal);
      if (fol_IsTrue(atom)) {
	/* Ignore this literal */
	term_Delete(literal);
	list_Rplaca(literals, NULL); /* Mark the actual list element */
      }
    }
  }
  

  literals = list_PointerDeleteElement(literals, NULL);
  /* Remove the special literals treated above from the list */
  result = clause_CreateFromLiterals(literals, FALSE, !IsAxiom, FALSE, Flags, Precedence);
  /* Don't create sorts! */
  list_Delete(literals);

  return result;
}

/*
  static void dfg_SubSort(char* Name1, char* Name2)*/
/**************************************************************
  INPUT:   Two sort symbol names.
  RETURNS: Nothing.
  EFFECT:  This functions adds the formula
           forall([U], implies(Name1(U), Name2(U)))
	   to the list of axiom formulas. Both <Name1> and <Name2>
	   are deleted.
***************************************************************/
/*{
  SYMBOL s1, s2;
  TERM   varterm, t1, t2, term;

  s1 = dfg_Symbol(Name1, 1);    Should be unary predicates 
  s2 = dfg_Symbol(Name2, 1);
  if (!symbol_IsSort(s1)) {
    misc_StartUserErrorReport();
    misc_UserErrorReport("\n Line %d: Symbol is not a sort.\n", dfg_LINENUMBER);
    misc_FinishUserErrorReport();
  }
  if (!symbol_IsSort(s2)) {
    misc_StartUserErrorReport();
    misc_UserErrorReport("\n Line %d: Symbol is not a sort.\n", dfg_LINENUMBER);
    misc_FinishUserErrorReport();
  }

  varterm = term_Create(symbol_CreateStandardVariable(), list_Nil());
  symbol_ResetStandardVarCounter();
  
  t1   = term_Create(s1, list_List(varterm));
  t2   = term_Create(s2, list_List(term_Copy(varterm)));
  term = term_Create(fol_Implies(), list_Cons(t1, list_List(t2)));
  term = fol_CreateQuantifier(fol_All(), list_List(term_Copy(varterm)),
			      list_List(term));
  dfg_SORTDECLLIST = list_Nconc(dfg_SORTDECLLIST, list_List(list_PairCreate(NULL,term)));
}*/


//static void dfg_SymbolGenerated(SYMBOL SortPredicate, BOOL FreelyGenerated,
//				LIST GeneratedBy)
///**************************************************************
//  INPUT:   A sort predicate, a boolean flag, and a list of function
//           symbol names.
//  RETURNS: Nothing.
//  EFFECT:  This function stores the information that the <SortPredicate>
//           is generated by the function symbols from the <GeneratedBy>
//           list. The list contains only symbol names!
//	   The <SortPredicate> AND the symbols from the list get
//           the property GENERATED. Additionally the symbols get
//	   the property FREELY, if the flag <FreelyGenerated> is TRUE.
//***************************************************************/
//{
//  SYMBOL symbol;
//  LIST   scan;
//
//  if (!symbol_IsSort(SortPredicate)) {
//    misc_StartUserErrorReport();
//    misc_UserErrorReport("\n Line %d: Symbol is not a sort predicate.\n", dfg_LINENUMBER);
//    misc_FinishUserErrorReport();
//  }
//  /* First reset the old information */
//  symbol_RemoveProperty(SortPredicate, GENERATED);
//  symbol_RemoveProperty(SortPredicate, FREELY);
//  list_Delete(symbol_GeneratedBy(SortPredicate));
//  /* Now set the new information */
//  symbol_AddProperty(SortPredicate, GENERATED);
//  if (FreelyGenerated)
//    symbol_AddProperty(SortPredicate, FREELY);
//  for (scan = GeneratedBy; !list_Empty(scan); scan = list_Cdr(scan)) {
//    symbol = symbol_Lookup(list_Car(scan));
//    if (symbol == 0) {
//      misc_StartUserErrorReport();
//      misc_UserErrorReport("\n Line %d: undefined symbol %s.\n", dfg_LINENUMBER,
//			   (char*)list_Car(scan));
//      misc_FinishUserErrorReport();
//    } else if (!symbol_IsFunction(symbol)) { /* must be function or constant */
//      misc_StartUserErrorReport();
//      misc_UserErrorReport("\n Line %d: Symbol is not a function.\n", dfg_LINENUMBER);
//      misc_FinishUserErrorReport();
//    }
//    string_StringFree(list_Car(scan));
//    list_Rplaca(scan, (POINTER)symbol);  /* change the name list to a symbol list */
//    /* Set GENERATED properties for generating symbols */
//    symbol_AddProperty(symbol, GENERATED);
//    if (FreelyGenerated)
//      symbol_AddProperty(symbol, FREELY);
//  }
//  symbol_SetGeneratedBy(SortPredicate, GeneratedBy);
//}

static void dfg_TranslPairDecl(char* PropName, char* FoName)
/**************************************************************
  INPUT:   A propositional name/first order name pair
  RETURNS: Nothing.
  EFFECT:  Updates the appropriate list.
***************************************************************/
{
  SYMBOL PropSymbol, FoSymbol;
  
  PropSymbol = symbol_Lookup(PropName);
  FoSymbol = symbol_Lookup(FoName);
  
  if (PropSymbol == 0) {
    fprintf(stderr, "Line %zu: Undefined symbol %s\n", 
	    dfg_LINENUMBER, PropName); 
    misc_Error(); 
  }
  else if (FoName == 0) {
    fprintf(stderr, "Line %zu: Undefined symbol %s\n", 
	    dfg_LINENUMBER, FoName); 
    misc_Error(); 
  }
  else 
    eml_SetPropFoSymbolAssocList(PropSymbol,list_List((POINTER)FoSymbol));
  string_StringFree(PropName);  /* Names were copied */
  string_StringFree(FoName);  
}

/**************************************************************/
/* Functions for the Symbol Table                             */
/**************************************************************/

typedef struct {
  SYMBOL symbol;
  BOOL   valid;
  int    arity;
} DFG_SYMENTRY, *DFG_SYM;

DFG_SYM dfg_SymCreate(void)
{
  return (DFG_SYM) memory_Malloc(sizeof(DFG_SYMENTRY));
}

void dfg_SymFree(DFG_SYM Entry)
{
  memory_Free(Entry, sizeof(DFG_SYMENTRY));
}


static void dfg_SymAdd(SYMBOL Symbol)
/**************************************************************
  INPUT:   A symbol.
  RETURNS: Nothing.
  EFFECT:  This function adds 'Symbol' to the symbol list.
           The arity of these symbols will be checked every time
	   the symbol occurs.
***************************************************************/
{
  DFG_SYM newEntry = dfg_SymCreate();
  newEntry->symbol = Symbol;
  newEntry->valid  = FALSE;
  newEntry->arity  = 0;
  dfg_SYMBOLLIST = list_Cons(newEntry, dfg_SYMBOLLIST);
}


static void dfg_SymCheck(SYMBOL Symbol, NAT Arity)
/**************************************************************
  INPUT:   A symbol and the current arity of this symbol.
  RETURNS: Nothing.
  EFFECT:  This function compares the previous arity of 'Symbol'
           with the actual 'Arity'. If these values differ
	   the symbol's arity is set to arbitrary.
	   The arity of symbols whose arity was specified in
	   the symbol declaration section is checked and a warning
	   is printed to stderr in case of differences.
***************************************************************/
{
  LIST scan = dfg_SYMBOLLIST;
  while (!list_Empty(scan)) {
    DFG_SYM actEntry = (DFG_SYM) list_Car(scan);
    if (actEntry->symbol == Symbol) {
      if (actEntry->valid) {
	if (actEntry->arity != Arity) {
	  misc_StartUserErrorReport();
	  misc_UserErrorReport("\n Line %u:", dfg_LINENUMBER);
	  misc_UserErrorReport(" The actual arity %u", Arity);
	  misc_UserErrorReport(" of symbol %s differs", symbol_Name(Symbol));
	  misc_UserErrorReport(" from the previous arity %u.\n", actEntry->arity);
	  misc_FinishUserErrorReport();
	}
      } else {
	/* Not valid => first time */
	actEntry->arity = Arity;
	actEntry->valid = TRUE;
      }
      return;
    }
    scan = list_Cdr(scan);
  }

  /* Symbol isn't in SymbolList, so its arity was specified.        */
  /* Check if the specified arity corresponds with the actual arity */
  if (symbol_Arity(Symbol) != Arity) {
    misc_StartUserErrorReport();
    misc_UserErrorReport("\n Line %u: Symbol %s was declared with arity %u.\n",
			 dfg_LINENUMBER, symbol_Name(Symbol), symbol_Arity(Symbol));
    misc_FinishUserErrorReport();
  }
}


static void dfg_SymCleanUp(void)
/**************************************************************
  INPUT:   None.
  RETURNS: Nothing.
  EFFECT:  This function corrects all symbols whose arity wasn't
           specified in the symbol declaration section but seem
	   to occur with always the same arity.
	   The memory for the symbol list is freed.
***************************************************************/
{
  while (!list_Empty(dfg_SYMBOLLIST)) {
    DFG_SYM actEntry  = (DFG_SYM) list_Car(dfg_SYMBOLLIST);
    SYMBOL  actSymbol = actEntry->symbol;

    if (actEntry->arity != symbol_Arity(actSymbol))
      symbol_SetArity(actSymbol, actEntry->arity);

    dfg_SymFree(actEntry);
    dfg_SYMBOLLIST = list_Pop(dfg_SYMBOLLIST);
  }
}


/**************************************************************/
/* Functions for the Variable Table                           */
/**************************************************************/
  
typedef struct {
  char*  name;
  SYMBOL symbol;
} DFG_VARENTRY, *DFG_VAR;

typedef struct {
  SYMBOL symbol;
  SYMBOL nextvar;
} DFG_SORTENTRY, *DFG_SORT;

char* dfg_VarName(DFG_VAR Entry)
{
  return Entry->name;
}

SYMBOL dfg_VarSymbol(DFG_VAR Entry)
{
  return Entry->symbol;
}

DFG_VAR dfg_VarCreate(void)
{
  return (DFG_VAR) memory_Malloc(sizeof(DFG_VARENTRY));
}

static void dfg_VarFree(DFG_VAR Entry)
{
  string_StringFree(Entry->name);
  memory_Free(Entry, sizeof(DFG_VARENTRY));
}

SYMBOL dfg_SortSymbol(DFG_SORT Entry)
{
  return Entry->symbol;
}

SYMBOL dfg_SortNextVar(DFG_SORT Entry)
{
  Entry->nextvar = symbol_MSortNextVariable(Entry->nextvar);
  return Entry->nextvar;
}

DFG_SORT dfg_SortCreate(void)
{
  return (DFG_SORT) memory_Malloc(sizeof(DFG_SORTENTRY));
}

static void dfg_SortFree(DFG_SORT Entry)
{
  memory_Free(Entry, sizeof(DFG_SORTENTRY));
}

static void dfg_VarStart(void)
{
  dfg_VARLIST   = list_Push(list_Nil(), dfg_VARLIST);
  dfg_MSORTLIST = list_Push(list_Nil(), dfg_MSORTLIST);
  dfg_VARDECL   = TRUE;
}

static void dfg_VarStop(void)
{
  dfg_VARDECL = FALSE;
}

static void dfg_VarBacktrack(void)
{
  list_DeleteWithElement(list_Top(dfg_VARLIST), (void (*)(POINTER)) dfg_VarFree);
  dfg_VARLIST = list_Pop(dfg_VARLIST);

  list_DeleteWithElement(list_Top(dfg_MSORTLIST), (void (*)(POINTER)) dfg_SortFree);
  dfg_MSORTLIST = list_Pop(dfg_MSORTLIST);
}

static void dfg_VarCheck(void)
/* Should be called after a complete clause or formula was parsed */
{
  if (!list_Empty(dfg_VARLIST)) {
    misc_StartErrorReport();
    misc_ErrorReport("\n In dfg_VarCheck: List of variables should be empty!\n");
    misc_FinishErrorReport();
  }
  //symbol_ResetStandardVarCounter();
}

static SYMBOL dfg_VarLookup(char* Name,SYMBOL sort)
/**************************************************************
  INPUT:   A variable name.
  RETURNS: The corresponding variable symbol.
  EFFECT:  If the variable name was quantified before, the
           corresponding symbol is returned and the <Name> is freed.
	   If the variable name was not quantified, and <dfg_VARDECL>
	   is TRUE, a new variable is created, else an error
	   message is printed and the program exits.
***************************************************************/
{
  LIST   scan, scan2;
  SYMBOL symbol = symbol_Null();

  scan  = dfg_VARLIST;
  scan2 = list_Nil();
  if(sort != 0){
    while (!list_Empty(scan) && list_Empty(scan2)) {
      scan2 = list_Car(scan);
      while (!list_Empty(scan2) &&
	     (!string_Equal(dfg_VarName(list_Car(scan2)), Name) ||
	      symbol_MSortVariableSort(dfg_VarSymbol(list_Car(scan2)))!=sort)) {
	scan2 = list_Cdr(scan2);
      }
      scan = list_Cdr(scan);
    }
  }else if(dfg_VARDECL){
    sort = fol_Top();
    scan2 = list_Car(scan);
    while (!list_Empty(scan2) &&
	   (!string_Equal(dfg_VarName(list_Car(scan2)), Name))) {
      scan2 = list_Cdr(scan2);
    }
  } else {
    while (!list_Empty(scan) && list_Empty(scan2)) {
      scan2 = list_Car(scan);
      while (!list_Empty(scan2) &&
             (!string_Equal(dfg_VarName(list_Car(scan2)), Name))) {
        scan2 = list_Cdr(scan2);
      }
      scan = list_Cdr(scan);
    }
  }

  if (!list_Empty(scan2)) {
    /* Found variable */
    string_StringFree(Name);
    symbol = dfg_VarSymbol(list_Car(scan2));
  } else {
    /* Variable not found */
    if (dfg_VARDECL && sort!=0) {
      /*     DFG_VAR newEntry = dfg_VarCreate();
      newEntry->name   = Name;
      newEntry->symbol = symbol_CreateStandardVariable(); */
      /* Add <newentry> to the first list in dfg_VARLIST */
      /*list_Rplaca(dfg_VARLIST, list_Cons(newEntry,list_Car(dfg_VARLIST)));
	symbol = dfg_VarSymbol(newEntry);*/
      symbol = dfg_VarSortCreate(Name,sort);
    } else {
      misc_StartUserErrorReport();
      misc_UserErrorReport("\n Line %u: Free Variable %s.\n", dfg_LINENUMBER, Name);
      misc_FinishUserErrorReport();
    }
  }
  return symbol;
}

static SYMBOL dfg_VarSortCreate(char* Name,SYMBOL sort)
{
  LIST   scan, scan2;
  SYMBOL variable = symbol_Null();
  SYMBOL symbol   = symbol_Null();

  scan  = list_Top(dfg_MSORTLIST);
  while (!list_Empty(scan) &&
	 (!(dfg_SortSymbol(list_Car(scan)) == sort)))
    scan = list_Cdr(scan);

  if(list_Empty(scan)){
    scan = list_Cdr(dfg_MSORTLIST);
    scan2 = list_Nil();
    while (!list_Empty(scan) && list_Empty(scan2)) {
      scan2 = list_Car(scan);
      while (!list_Empty(scan2) &&
	     ((dfg_SortSymbol(list_Car(scan2)) != sort)))
	scan2 = list_Cdr(scan2);
      scan = list_Cdr(scan);
    }
    DFG_SORT newsort = dfg_SortCreate();
    if(!list_Empty(scan2)){
      newsort->symbol = sort;
      newsort->nextvar = dfg_SortNextVar((DFG_SORT)(list_Car(scan2)));
      list_Rplaca(dfg_MSORTLIST, list_Cons(newsort,list_Car(dfg_MSORTLIST)));
    }else{
      newsort->symbol = sort;
      newsort->nextvar = symbol_MSortFirstVariable(sort);
      list_Rplaca(dfg_MSORTLIST, list_Cons(newsort,list_Car(dfg_MSORTLIST)));
    }
    variable = newsort->nextvar;
    scan2 = list_Top(dfg_MSORTLIST);
  }else{
    scan2 = scan;
    variable = dfg_SortNextVar(list_Car(scan2));
  }

  scan  = list_Top(dfg_VARLIST);
  while (!list_Empty(scan) &&
         (!string_Equal(dfg_VarName(list_Car(scan)), Name)))
    scan = list_Cdr(scan);

  if (!list_Empty(scan)) {
    /* Found variable */
    misc_StartUserErrorReport();
    misc_UserErrorReport("\n Line %u: Variable was defined twice in the same Quantifier %s.\n", dfg_LINENUMBER, Name);
    misc_FinishUserErrorReport();
  } else {
    /* Variable not found */

    DFG_VAR newEntry = dfg_VarCreate();
    newEntry->name   = Name;
    newEntry->symbol = variable;

    /* Add <newentry> to the first list in dfg_VARLIST */
    list_Rplaca(dfg_VARLIST, list_Cons(newEntry,list_Car(dfg_VARLIST)));
    symbol = dfg_VarSymbol(newEntry);

  }
  return symbol;
}


/**************************************************************/
/* Functions for the plain clauses                            */
/**************************************************************/
static DFG_PLAINCLAUSE dfg_PlainClauseCreate(void)
{
  return (DFG_PLAINCLAUSE) memory_Malloc(sizeof(DFG_PLAINCLAUSEHELP));
}

static void dfg_PlainClauseFree(DFG_PLAINCLAUSE Clause)
{
  list_Delete(Clause->constraint);
  list_Delete(Clause->antecedent);
  list_Delete(Clause->succedent);
  memory_Free(Clause, sizeof(DFG_PLAINCLAUSEHELP));
}

/**************************************************************/
/* Miscelaneous                                               */
/**************************************************************/

/*Get rid of leading and trailing qoutes of a string.
  Returns a newly allocated thing and frees the one destroyed.*/
//static char * dfg_StripQuotes(char * str) {
//  char * newstr;
//  int len = strlen(str);
//
//  str[len-1] = '\0'; /*one characer shorter from end*/
//
//  newstr = string_StringCopy(str+1);    /*start duplicating from the second char*/
//
//  memory_Free(str,len+1);
//
//  return newstr;
//}


POINTER array_Peek(ARRAY ar)
/**************************************************************
  INPUT:   A pointer to the array
  RETURNS: A pointer to the element at the end of the array
***************************************************************/
{
  return (POINTER)ar->data[ar->size-1];
}

POINTER array_Pop(ARRAY ar)
/**************************************************************
  INPUT:   A pointer to the array
  RETURNS: A pointer to the element at the end of the array
           which then will be removed
***************************************************************/
{
  POINTER ret =  (POINTER)ar->data[--ar->size];
  return ret;
}



DFG_TOKEN getNextTok(DFG_LEXER lex, DFG_TOKEN* pcurrentToken, DFG_TOKEN* plastToken)
/**********************************************************
  INPUT:   The current lexer and last scanned token.
  RETURNS: Next token from lexer and ignoring whitespace.
  SUMMARY: Deletes the current token and frees the storage.
           Global linenumber is updated
  CAUTION: The arguments of pcurrentToken are also deleted.
********************************************************/
{
  freeToken(*plastToken);
  *plastToken =*pcurrentToken;
  *pcurrentToken = nextToken(lex);
  dfg_LINENUMBER = (unsigned)((*pcurrentToken)->line);
   
  return *pcurrentToken;
}

DFG_TOKEN getNextTokWS(DFG_LEXER lex, DFG_TOKEN* pcurrentToken, DFG_TOKEN* plastToken)
/**********************************************************
  INPUT:   The current lexer and last scanned token.
  RETURNS: Next token from lexer whitespace is a regarded
           as possible token
  SUMMARY: Deletes the current token and frees the storage.
           Global linenumber is updated
  CAUTION: The arguments of pcurrentToken are also deleted.
********************************************************/
{
  freeToken(*plastToken);
  *plastToken = *pcurrentToken;
  *pcurrentToken = nextTokenorWS(lex);
  dfg_LINENUMBER = (unsigned)((*pcurrentToken)->line);
  
  return *pcurrentToken;
}

static BOOL dfg_Check(DFG_TOKEN tok, dfgtokentype toCheck)
{
  dfg_LastChecked = toCheck;
  return tok->type == toCheck; 
}

static BOOL dfg_IsPredefinedSort(SYMBOL S){
  return symbol_Equal(S,fol_Natural()) || symbol_Equal(S,fol_Integer())
    || symbol_Equal(S,fol_Rational()) || symbol_Equal(S,fol_Real())
    || symbol_Equal(S,fol_Top());
}

static void dfg_ErrorOnCheck(DFG_TOKEN cur,DFG_TOKEN last)
{
  if(dfg_LastChecked == DFG_IDENTIFIER){
    misc_StartUserErrorReport();
    misc_UserErrorReport("\n After ");
    dfg_ErrorPrintToken(last);
    misc_UserErrorReport(" an Identifier was expected.");
    if(cur->type == DFG_NUMBER){
      misc_UserErrorReport("\n But a Number: ");
      dfg_ErrorPrintToken(cur);
      misc_UserErrorReport(" was found instead");
    }else{
      misc_UserErrorReport("\n But predefined Keyword: ");
      dfg_ErrorPrintToken(cur);
      misc_UserErrorReport(" was found instead");
    }
    misc_FinishUserErrorReport();
  }else{
    misc_StartUserErrorReport();
    misc_UserErrorReport("\n After ");
    dfg_ErrorPrintToken(last);
    misc_UserErrorReport(" a token of type '");
    dfg_ErrorPrintType(dfg_LastChecked);
    misc_UserErrorReport("' was expected. ");
    misc_UserErrorReport("\n But : ");
    dfg_ErrorPrintToken(cur);
    misc_UserErrorReport(" was found instead");
    misc_FinishUserErrorReport();
  }
}

/**************************************************************/
/* Core Function                                              */
/**************************************************************/


static void dfg_parse(FILE* file)
/**********************************************************
  INPUT:   The file to parse
  SUMMARY: Works like a pushdown automaton for the LL(1)
           grammar describing the SPASS-Syntax.
  CAUTION: Uses global Variables! Shouldn't be invoked
           directly only over other predefined methods.
********************************************************/
{
  
  /*a stack used for saving components the parser needs to
    reuse in another state of the automaton.
    should be only accessed with array_Add(), array_Pop()
    and array_Peek().
   */ 
  ARRAY depot = array_Create(8);

  /*a stack used for saving the states the automaton still
    has to visit.
    should be only accessed with array_Add(), array_Pop()
    and array_Peek().
    Warning: State to visit last has to be added first
    because this is a stack!
   */
  ARRAY states = array_Create(8);

  /*Initialization of Lexer and then taking the first
    token. Putting starting state on the stack and 
    Defining all variables later used. All later
    changes to currentToken should be done with the
    Macros NEXTTOK or NEXTTOKWS.
   */
  array_Add(states, (intptr_t)PAR_problem);
  DFG_LEXER lex = createLexer(file);
  DFG_TOKEN currentToken = nextToken(lex);
  DFG_TOKEN lastToken = createToken(DFG_FileBegin,0,0,string_StringCopy("the start of File"));  

  char* label = NULL;
  char* id = NULL;
  char* id1 = NULL;
  char* id2 = NULL;
  char* numberid = NULL;
  LIST list1 = NULL;
//  LIST list2 = NULL;
  LIST pair = NULL;
  TERM clause = NULL;
  TERM lit1 = NULL;
  intptr_t* arity = NULL;
  intptr_t* origin = NULL;
  DFG_PLAINCLAUSE clause1 = NULL;
//  TERM atom = NULL;
  LIST termlist = NULL;
  TERM term2 = NULL;
  TERM term1 = NULL;
//  TERM term  = NULL;
  TERM sort1 = NULL;
  intptr_t* number  = NULL;
//  intptr_t* decimal = NULL;
  intptr_t isneg = 1;
  SYMBOL s = 0;
  SYMBOL s1 = 0;
  SYMBOL s2 = 0;

  /*While there is still a state the automaton has to
    visit pop a state from stack states and switch to
    perform the rule devised for this state. Afterwards
    it is checked if the last EOF is reached.
   */
  while(states->size >0){
    switch(((parserstate)array_Pop(states))){
      /*How to build (and understand a rule).
        Imagine our grammar had a rule:
        RULE     := a b A c B d | e C;
        with terminals a,b,c,d,e (or in our case words accepted
        by the scanner) and nonterminals A,B,C (parser states).
        First we have to change RULE because we can't check 
        terminals in the same rule after a nonterminal:
        RULE     := a b A rule_1 | e C;
        RULE_1   := c B rule_end
        RULE_end := d
        Our cases than look like this:
        case PAR_RULE:
          //case distinction because we have
          //a choice in our rule
          if(currentToken->type == DFG_a){
            //informations from currentToken could be
            //pushed on the depot. After calling NEXTTOK
            //they will never be accessible.
            array_Add((POINTER)string_StringCopy(currentToken->text),depot);
            if(NEXTTOK->type == DFG_b){
              //Add non terminals in reverse order.
              array_Add((POINTER)PAR_rule_1,states);
              array_Add((POINTER)PAR_A,states);
              NEXTTOK;
            }else{
              //throw Error because there is no alternate
              //how to follow this rule.
            }
          }else if(currentToken->type == DFG_e){
            array_Add((POINTER)PAR_C,states); //Add non Terminal
            NEXTTOK; //if the next rule has to
                     //starts with the next Token
                     //you have to change in this
                     // state to the next one.
          }else{
            //Throw Error
          }
          break;
       case PAR_RULE_1:
         //should be obvious
         ...
       case PAR_RULE_end:
         if(currentToken->type == DFG_d){
           //If RULE shall derive information use
           //depot to recycle saved information from
           //other states and reassamble it.
           char* textofa = (char*)array_Pop(depot);
           printf("Hello %s",textofa);
           NEXTTOK;
         }else{
           //Throw Error
         }
         break;

       The general functionality won't be described in
       every rule, but the associated LL(1) rules will
       be stated. 
       */

      /* Start of actual rules*/
    case PAR_problem:
      /*RULE:
        problem:='begin_problem' '(' id ')' '.'
                 description
                 logicalpart
                 includelistopt
                 settinglistsopt
                 problem_end
         EFFECT: id will be pushed on depot for reuse
                 in problem_end
       */
      if(dfg_Check( currentToken , DFG_begin_problem ) && dfg_Check( NEXTTOK , DFG_OPENBR
	 ) && dfg_Check( NEXTTOK , DFG_IDENTIFIER)){
	
	array_Add(depot,(intptr_t)string_StringCopy(currentToken->text));
        if(dfg_Check( NEXTTOK , DFG_CLOSEBR ) && dfg_Check( NEXTTOK , DFG_POINT)){
	  array_Add(states,(intptr_t)(PAR_problem_end));
          array_Add(states,(intptr_t)(PAR_settinglistsopt));
          //TO_DO  array_Add((intptr_t)(PAR_includelistsopt),states);
          array_Add(states,(intptr_t)(PAR_logicalpart));
          array_Add(states,(intptr_t)(PAR_description));
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// p0 ");
	}
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// p1 ");
      }
      break;
      
    case PAR_problem_end:
      /*RULE:
        problem_end:= 'end_problem' '.'
        EFFECT:Additionally it will be check on EOF.
       */
      if(dfg_Check( currentToken , DFG_end_problem ) && dfg_Check( NEXTTOK , DFG_POINT ) && dfg_Check( NEXTTOK , DFG_FileEnd)){
	id = (char*)array_Pop(depot);
	string_StringFree(id);
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// pend ");
      }
      break;

      /****************/
      /* LOGICAL PART */
      /****************/

    case PAR_logicalpart:
      /*RULE:
        logicalpart:= symbollistopt
              declarationlistopt
              formulalistsopt
              specialformulalistsopt
              clauselistsopt
              prooflistsopt
       */
      /*array_Add((intptr_t)(PAR_prooflistsopt),states);*/
      array_Add(states,(intptr_t)(PAR_clauselistsopt));
      /*array_Add((intptr_t)(PAR_specialformulalistsopt),states);*/
	array_Add(states,(intptr_t)(PAR_formulalistsopt));
	array_Add(states,(intptr_t)(PAR_declarationlistopt));
      array_Add(states,(intptr_t)(PAR_symbollistopt));
      break;
      
      /****************/
      /* DESCRIPTION  */
      /****************/
    case PAR_description:
      /*RULE:
        description:= 'list_of_descriptions' '.'
              name
              author
              versionopt
              logicopt
              status
              desctext
              dateopt
              description_end
       */
      if(dfg_Check( currentToken , DFG_list_of_descriptions ) && dfg_Check( NEXTTOK , DFG_POINT)){
        array_Add(states,(intptr_t)(PAR_POINT));
        array_Add(states,(intptr_t)(PAR_end_of_list));
        array_Add(states,(intptr_t)(PAR_dateopt));
        array_Add(states,(intptr_t)(PAR_desctext));
        array_Add(states,(intptr_t)(PAR_status));
        array_Add(states,(intptr_t)(PAR_logicopt));
        array_Add(states,(intptr_t)(PAR_versionopt));
        array_Add(states,(intptr_t)(PAR_author));
	array_Add(states,(intptr_t)(PAR_name));
        NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// description ");
      }
      break;
      
    case PAR_name:
      /*RULE:
        name:='name' '(' DFG_TEXT ')' '.'
        EFFECT:set name in dfg_DESC
       */
      if(dfg_Check( currentToken , DFG_name ) && dfg_Check( NEXTTOK , DFG_OPENBR ) && dfg_Check( NEXTTOK , DFG_TEXT)){
	dfg_DESC->name = string_StringCopy(currentToken->text);
	if(dfg_Check( NEXTTOK , DFG_CLOSEBR ) && dfg_Check( NEXTTOK , DFG_POINT)){
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;
      
    case PAR_author:
      /*RULE:
        author:= 'author' '(' DFG_TEXT ')' '.'
        EFFECT:set author in dfg_DESC
       */
      if(dfg_Check( currentToken , DFG_author ) && dfg_Check( NEXTTOK , DFG_OPENBR ) && dfg_Check( NEXTTOK , DFG_TEXT)){
	dfg_DESC->author = string_StringCopy(currentToken->text);
	if(dfg_Check( NEXTTOK , DFG_CLOSEBR ) && dfg_Check( NEXTTOK , DFG_POINT)){
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;
      
    case PAR_status:
      /*RULE:
        status:= 'status' '(' 'satisfiable' | 'unsatisfiable' | 'unknown' ')' '.'
        EFFECT:set status in dfg_DESC
       */
      if(dfg_Check( currentToken , DFG_status ) && dfg_Check( NEXTTOK , DFG_OPENBR)){
	switch(NEXTTOK->type){
	case DFG_satisfiable:
	  dfg_DESC->status = DFG_SATISFIABLE;
	  break;  
	case DFG_unsatisfiable:
	  dfg_DESC->status = DFG_UNSATISFIABLE;
	  break;
	case DFG_unknown:
	  dfg_DESC->status = DFG_UNKNOWNSTATE;
	  break;
	default:
	  ;dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}
	
	if(dfg_Check( NEXTTOK , DFG_CLOSEBR ) && dfg_Check( NEXTTOK , DFG_POINT)){
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;
      
    case PAR_desctext:
      /*RULE:
        desctext:= 'description' '(' DFG_TEXT ')' '.'
        EFFECT:set description in dfg_DESC
       */
      if(dfg_Check( currentToken , DFG_description ) &&
         dfg_Check( NEXTTOK , DFG_OPENBR ) && dfg_Check( NEXTTOK , DFG_TEXT)){
	
        dfg_DESC->description = string_StringCopy(currentToken->text);
      
	if(dfg_Check( NEXTTOK , DFG_CLOSEBR ) && dfg_Check( NEXTTOK , DFG_POINT)){
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;
      
    case PAR_versionopt:
      /*RULE:
	versionopt:= * empty * 
          | 'version' '(' DFG_TEXT ')' '.'
	EFFECT:optionaly set version in dfg_DESC
       */
      if(dfg_Check( currentToken , DFG_version)){
        if(dfg_Check( NEXTTOK , DFG_OPENBR ) && dfg_Check( NEXTTOK , DFG_TEXT)){
	  dfg_DESC->version = string_StringCopy(currentToken->text);
	  if(dfg_Check( NEXTTOK , DFG_CLOSEBR ) && dfg_Check( NEXTTOK , DFG_POINT)){
	    NEXTTOK;
	  }else{
	    dfg_ErrorOnCheck(currentToken,lastToken);// ");
	  }
        }else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
        }
      }
      break;
      
    case PAR_logicopt:
      /*RULE:
	logicopt:= * empty *
        | 'logic' '(' DFG_TEXT ')' '.'
	EFFECT:optionaly set logic in dfg_DESC
       */
      if(dfg_Check( currentToken , DFG_logic)){
        if(dfg_Check( NEXTTOK , DFG_OPENBR ) && dfg_Check( NEXTTOK , DFG_TEXT)){
	  dfg_DESC->logic = string_StringCopy(currentToken->text);
	  if(dfg_Check( NEXTTOK , DFG_CLOSEBR ) && dfg_Check( NEXTTOK , DFG_POINT)){
	    NEXTTOK;
	  }else{
	    dfg_ErrorOnCheck(currentToken,lastToken);// ");
	  }
        }else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
        }
      }
      break;
      
    case PAR_dateopt:
      /*RULE:
	dateopt:= * empty *
       | DFG_DATE '(' DFG_TEXT ')' '.'
	EFFECT:optionaly set date in dfg_DESC
       */
      if(dfg_Check( currentToken , DFG_date)){
        if(dfg_Check( NEXTTOK , DFG_OPENBR ) && dfg_Check( NEXTTOK , DFG_TEXT)){
	  dfg_DESC->date = string_StringCopy(currentToken->text);
	  if(dfg_Check( NEXTTOK , DFG_CLOSEBR ) && dfg_Check( NEXTTOK , DFG_POINT)){
	    NEXTTOK;
	  }else{
	    dfg_ErrorOnCheck(currentToken,lastToken);// ");
	  }
        }else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
        }
      }
      break;
      
      /****************/
      /* SYMBOLS      */
      /****************/
    case PAR_symbollistopt:
      /*RULE:
	symbollistopt:=* empty *
              | 'list_of_symbols' '.'
                functionsopt
		predicatesopt
		sortsopt
		operatorsopt
		quantifiersopt
		translpairsopt
		end_of_list POINT
         EFFECT: optionaly parse symbollist
       */
      if(dfg_Check( currentToken , DFG_list_of_symbols)){
	if(dfg_Check( NEXTTOK , DFG_POINT)){
	  array_Add(states,(intptr_t)(PAR_POINT));
	  array_Add(states,(intptr_t)(PAR_end_of_list));
	  array_Add(states,(intptr_t)(PAR_translpairsopt));
	  array_Add(states,(intptr_t)(PAR_sortsopt));
	  array_Add(states,(intptr_t)(PAR_weightsopt));
	  array_Add(states,(intptr_t)(PAR_predicatesopt));
	  array_Add(states,(intptr_t)(PAR_weightsopt));
	  array_Add(states,(intptr_t)(PAR_functionsopt));
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}

      }
      break;

    case PAR_functionsopt:
      /*RULE:
        functionsopt:= * empty *
	      | functions '[' func functionlist_1 ']' '.'
       */
      if(dfg_Check( currentToken , DFG_functions)){
	if(dfg_Check( NEXTTOK , DFG_OPENEBR)){
	  array_Add(states,(intptr_t)(PAR_POINT));
	  array_Add(states,(intptr_t)(PAR_CLOSEEBR));
	  array_Add(states,(intptr_t)(PAR_functionlist_1));
	  array_Add(states,(intptr_t)(PAR_func));
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}

      }
      break;
      

    case PAR_functionlist_1:
      /*RULE:
        functionlist_1:= * empty *
              | ',' func functionlist_1
       */
      if(currentToken->type == DFG_COMMA){
        array_Add(states,(intptr_t)(PAR_functionlist_1));
	array_Add(states,(intptr_t)(PAR_func));
	NEXTTOK;
      }
      break;


    case PAR_func:
      /*RULE:
        func:= DFG_ID 
	     | '(' DFG_ID ',' DFG_NUMBER ')'
        EFFECT: Create new function with first element of
                the tupel as the name and second as arity, or 
                in first case just with default arity
       */
      if(dfg_Check( currentToken , DFG_IDENTIFIER)){
	id = string_StringCopy(currentToken->text);
	dfg_SymbolDecl(DFG_FUNC, id, -2);	
      }else if(dfg_Check( currentToken , DFG_OPENBR ) && dfg_Check( NEXTTOK , DFG_IDENTIFIER)){
	id = string_StringCopy(currentToken->text);
	if(dfg_Check( NEXTTOK , DFG_COMMA ) && dfg_Check( NEXTTOK , DFG_NUMBER)){
	  arity = (intptr_t*)memory_Malloc(sizeof(intptr_t));
	  if(string_StringToInt(currentToken->text,FALSE,(intptr_t*)arity) && dfg_Check( NEXTTOK , DFG_CLOSEBR)){
	    
            dfg_SymbolDecl(DFG_FUNC, id, *arity);
	    memory_Free(arity,sizeof(intptr_t));
	  }else{
	    dfg_ErrorOnCheck(currentToken,lastToken);// ");
	  }
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	} 
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      NEXTTOK;
      break;
      


    case PAR_weightsopt:
      /*RULE:
        functionsopt:= * empty *
	      | weight '[' func weightlist_1 ']' '.'
       */
      if(dfg_Check( currentToken , DFG_weights)){
	if(dfg_Check( NEXTTOK , DFG_OPENEBR)){
	  array_Add(states,(intptr_t)(PAR_POINT));
	  array_Add(states,(intptr_t)(PAR_CLOSEEBR));
	  array_Add(states,(intptr_t)(PAR_weightlist_1));
	  array_Add(states,(intptr_t)(PAR_weight));
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}

      }
      break;



    case PAR_weightlist_1:
      /*RULE:
        PAR_weightlist_1:= * empty *
              | ',' weight PAR_weightlist_1
       */
      if(currentToken->type == DFG_COMMA){
        array_Add(states,(intptr_t)(PAR_weightlist_1));
	array_Add(states,(intptr_t)(PAR_weight));
	NEXTTOK;
      }
      break;


    case PAR_weight:
    	/*RULE:
        weight:= '(' DFG_ID ',' DFG_NUMBER ')'
        EFFECT: Create new function with first element of
                the tupel as the name and second as arity, or
                in first case just with default arity
    	 */
    	if(dfg_Check( currentToken , DFG_OPENBR ) && dfg_Check( NEXTTOK , DFG_IDENTIFIER)){
    		id = string_StringCopy(currentToken->text);
    		if(dfg_Check( NEXTTOK , DFG_COMMA ) && dfg_Check( NEXTTOK , DFG_NUMBER)){
    			arity = (intptr_t*)memory_Malloc(sizeof(intptr_t));
    			if(string_StringToInt(currentToken->text,FALSE,(intptr_t*)arity) && dfg_Check( NEXTTOK , DFG_CLOSEBR)){

//    				dfg_SymbolDecl(DFG_FUNC, id, *arity);
    				msorts_setWeight(id,*arity);
    				memory_Free(arity,sizeof(intptr_t));
    			}else{
    				dfg_ErrorOnCheck(currentToken,lastToken);// ");
    			}
    		}else{
    			dfg_ErrorOnCheck(currentToken,lastToken);// ");
    		}
    	}else{
    		dfg_ErrorOnCheck(currentToken,lastToken);// ");
    	}
    	NEXTTOK;
    break;
    case PAR_predicatesopt:
      /*RULE:
        predicatesopt:= * empty *
                 | predicates '[' predicatelist ']' '.'
       */
      if(dfg_Check( currentToken , DFG_predicates)){
	if(dfg_Check( NEXTTOK , DFG_OPENEBR)){
	  array_Add(states,(intptr_t)(PAR_POINT));
	  array_Add(states,(intptr_t)(PAR_CLOSEEBR));
	  array_Add(states,(intptr_t)(PAR_predicatelist_1));
	  array_Add(states,(intptr_t)(PAR_pred));
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}

      }
      break;

    case PAR_predicatelist_1:
      /*RULE:
        predicatelist_1:= * empty *
                        | ',' pred predicatelist_1
      */
      if(dfg_Check( currentToken , DFG_COMMA)){
        array_Add(states,(intptr_t)(PAR_predicatelist_1));
	array_Add(states,(intptr_t)(PAR_pred));
	NEXTTOK;
      }
      break;
      
    case PAR_pred:
      /*RULE:
        pred:= DFG_ID 
             | '(' DFG_ID ',' DFG_NUMBER ')'
        EFFECT: Create new predicate with first element of
                the tupel as the name and second as arity, or 
                in first case just with default arity
       */
      if(dfg_Check( currentToken , DFG_IDENTIFIER )){
	id = string_StringCopy(currentToken->text);
	dfg_SymbolDecl(DFG_PRDICAT, id, -2);	
      }else if(dfg_Check( currentToken , DFG_OPENBR ) && dfg_Check( NEXTTOK , DFG_IDENTIFIER )){
	id = string_StringCopy(currentToken->text);
	if(dfg_Check( NEXTTOK , DFG_COMMA ) && dfg_Check( NEXTTOK , DFG_NUMBER)){
	  arity = (intptr_t*)memory_Malloc(sizeof(intptr_t));
	  if(string_StringToInt(currentToken->text,FALSE,(intptr_t*)arity)  && dfg_Check( NEXTTOK , DFG_CLOSEBR)){
	    
	    dfg_SymbolDecl(DFG_PRDICAT, id, *arity);
	    memory_Free(arity,sizeof(intptr_t));
	  }else{
	    dfg_ErrorOnCheck(currentToken,lastToken);// ");
	  }
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	} 
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      NEXTTOK;
      break;

    case PAR_sortsopt:
      /*RULE:
	sortsopt:= * empty *
	             | 'sorts' '[' sort sortlist_1 ']' '.'
      */
      if(dfg_Check( currentToken , DFG_sorts)){
        if(dfg_Check( NEXTTOK , DFG_OPENEBR)){
          array_Add(states,(intptr_t)(PAR_POINT));
          array_Add(states,(intptr_t)(PAR_CLOSEEBR));
          array_Add(states,(intptr_t)(PAR_sortlist_1));
          array_Add(states,(intptr_t)(PAR_sort));
          NEXTTOK;
        }else{
          dfg_ErrorOnCheck(currentToken,lastToken);// sortsopt ");
        }

      }
      break;

    case PAR_sortlist_1:
      /*RULE:
	sortlist_1:= *empty*
	               | ',' sort sortlist_1
      */
      if(currentToken->type == DFG_COMMA){
        array_Add(states,(intptr_t)(PAR_sortlist_1));
        array_Add(states,(intptr_t)(PAR_sort));
        NEXTTOK;
      }
      break;

    case PAR_sort:
      /*RULE:
        sort:= DFG_ID 
        EFFECT:Create new sort with name DFG_IDENTIFIER or DFG_NUMBER
       */
      s=0;
      if(dfg_Check( currentToken , DFG_IDENTIFIER )){
        id = string_StringCopy(currentToken->text);
	if(dfg_Check( NEXTTOK , DFG_OPENPBR)){
	  s++;
	  id = string_Nconc(id,string_StringCopy(currentToken->text));
	  NEXTTOK;
	  while(s>0){
	    if(dfg_Check( currentToken , DFG_IDENTIFIER)){
	      id = string_Nconc(id,string_StringCopy(currentToken->text));
	    }else{
	      misc_StartUserErrorReport();
              misc_UserErrorReport("\n %s is not allowed for the composition of sorts", currentToken->text);
	      misc_UserErrorReport("\n At line %d start of sort was %s",currentToken->line,currentToken->text);
		misc_FinishUserErrorReport();

	    }
	    switch(NEXTTOK->type){
	    case DFG_OPENPBR:
	      s++;
	      id = string_Nconc(id,string_StringCopy(currentToken->text));
	      NEXTTOK;
	      break;
	    case DFG_CLOSEPBR:
	      s--;
	      id = string_Nconc(id,string_StringCopy(currentToken->text));
	      while(dfg_Check( NEXTTOK , DFG_CLOSEPBR ) && s>0){
		s--;
		id = string_Nconc(id,string_StringCopy(currentToken->text));
	      }
	      break;
	    case DFG_IDENTIFIER:
	      continue;
	      break;
	    default:
	      misc_StartUserErrorReport();
	      misc_UserErrorReport("\n %s is not allowed for the composition of sorts", currentToken->text);
	      misc_UserErrorReport("\n At line %d start of sort was %s",currentToken->line,currentToken->text);
	      misc_FinishUserErrorReport();
	    }
	  }
	}
        dfg_SymbolDecl(DFG_SRT, id, 1);
      }else{
        dfg_ErrorOnCheck(currentToken,lastToken);// sort ");
      }
      break;
      
    case PAR_translpairsopt:
      /*RULE:
	translpairsopt:= * empty *
	      | 'translpairs' '[' translpairlist ']' '.'
       */
      if(dfg_Check( currentToken , DFG_translpairs)){
	if(dfg_Check( NEXTTOK , DFG_OPENEBR)){
	  array_Add(states,(intptr_t)(PAR_POINT));
	  array_Add(states,(intptr_t)(PAR_CLOSEEBR));
	  array_Add(states,(intptr_t)(PAR_translpairlist_1));
	  array_Add(states,(intptr_t)(PAR_translpair));
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}

      }
      break;

    case PAR_translpairlist_1:
      /*RULE:
	translpairlist:=translpair
	      | translpair translpairlist'
       */
      if(dfg_Check( currentToken , DFG_COMMA)){
        array_Add(states,(intptr_t)(PAR_translpairlist_1));
	array_Add(states,(intptr_t)(PAR_translpair));
	NEXTTOK;
      }
      break;
      
    case PAR_translpair:
      /*RULE:
	translpair: '(' DFG_ID ',' DFG_ID ')'
        EFFECT: Create new translpair linking two predicates.
       */
      if(dfg_Check( currentToken , DFG_OPENBR
         ) && dfg_Check( NEXTTOK , DFG_IDENTIFIER )){
	id= string_StringCopy(currentToken->text);
	if(dfg_Check( NEXTTOK , DFG_COMMA 
           ) && dfg_Check( NEXTTOK , DFG_IDENTIFIER)){
	  id2= string_StringCopy(currentToken->text);
	  if(dfg_Check( NEXTTOK , DFG_CLOSEBR)){
	    dfg_TranslPairDecl(id, id2);
	  }else{
	    dfg_ErrorOnCheck(currentToken,lastToken);// ");
	    
	  }
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}   
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      NEXTTOK;
      break;
      
      /****************/
      /* DECLARATIONS */
      /****************/

    case PAR_declarationlistopt:
      /*RULE:
	declarationlistopt:= * empty *
	    | 'list_of_declarations' '.' decllistopt end_of_list POINT
       */
      if(dfg_Check( currentToken , DFG_list_of_declarations)){
        if(dfg_Check( NEXTTOK , DFG_POINT)){
          array_Add(states,(intptr_t)PAR_POINT);
	  array_Add(states,(intptr_t)PAR_end_of_list);
	  array_Add(states,(intptr_t)PAR_decllistopt);
          NEXTTOK;
	}else{
          dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}
      }
      break;

    case PAR_decllistopt:
      /*RULE:
	decllistopt:= * empty *
          |   [ subsortdec | predicatedec | datatypedec | distinctdecl | functiondecl ] decllistopt 
      */
      switch(currentToken->type){
      case DFG_subsort:
	array_Add(states,(intptr_t)PAR_decllistopt);
        array_Add(states,(intptr_t)PAR_subsortdec);
	NEXTTOK;
	break;
      case DFG_predicate:
	array_Add(states,(intptr_t)PAR_decllistopt);
        array_Add(states,(intptr_t)PAR_predicatedec);
	NEXTTOK;
	break;
      case DFG_datatype:
	array_Add(states,(intptr_t)PAR_decllistopt);
        array_Add(states,(intptr_t)PAR_datatypedec);
	NEXTTOK;
	break;
      case DFG_distinct_symbols:
	array_Add(states,(intptr_t)PAR_decllistopt);
        array_Add(states,(intptr_t)PAR_distinctdec);
	NEXTTOK;	
	break;
      case DFG_function:
	array_Add(states,(intptr_t)PAR_decllistopt);
        array_Add(states,(intptr_t)PAR_functiondec);
	NEXTTOK;
	break;
      default:
       	;
      }
      break;

    case PAR_subsortdec:
      /*RULE:
	subsortdec:= '(' DFG_ID  ',' DFG_ID ')' '.'
        EFFECT: TO DO
       */
      if(dfg_Check( currentToken , DFG_OPENBR )){
	array_Add(states,(intptr_t)PAR_subsortdec_end);
	array_Add(states,(intptr_t)PAR_sortdec);
	array_Add(states,(intptr_t)PAR_COMMA);
	array_Add(states,(intptr_t)PAR_sortdec);
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;
	 
    case PAR_subsortdec_end:
      if(dfg_Check( currentToken , DFG_CLOSEBR ) && dfg_Check( NEXTTOK , DFG_POINT)){
	s2 = (intptr_t)array_Pop(depot);
	s  = (intptr_t)array_Pop(depot);

	if(dfg_IsPredefinedSort(s) || dfg_IsPredefinedSort(s2)){ 
	  misc_StartUserErrorReport();
          misc_UserErrorReport("\n LINE %d: No predefined sorts allowed in subsort declaration.", dfg_LINENUMBER);
          misc_FinishUserErrorReport();
	}


	term1 = term_Create(fol_Subsort(),list_Cons(term_Create(s,list_Nil()),list_List(term_Create(s2,list_Nil()))));
	pair = list_PairCreate(NULL, term1);

	dfg_DECLARATIONS = list_Nconc(dfg_DECLARATIONS, list_List(pair));

	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;

    case PAR_predicatedec:
      /*RULE:
	predicatedec:= '(' DFG_ID ',' sortdeclist predicatedec_end
	EFFECT: Saves the name of the Predicate to the depot the combination of this with
	        its sorts happens in predicatedec_end
       */
      if(dfg_Check( currentToken , DFG_OPENBR ) && dfg_Check( NEXTTOK , DFG_IDENTIFIER )){
	id1 = string_StringCopy(currentToken->text);
	s = symbol_Lookup(id1);
        if (s == 0) {
          misc_StartUserErrorReport();
          misc_UserErrorReport("\n Undefined symbol %s", id1);
          misc_UserErrorReport(" in declarations at line: %d pos: %d . Should be a predicate.\n"
                               ,currentToken->line,currentToken->pos);
          misc_FinishUserErrorReport();
        }
        if (!symbol_IsPredicate(s)) {
          misc_StartUserErrorReport();
          misc_UserErrorReport("\n Symbol %s isn't a predicate", id1);
          misc_UserErrorReport(" in declarations at line: %d pos: %d .\n"
                               ,currentToken->line,currentToken->pos);
          misc_FinishUserErrorReport();
        }
        array_Add(depot,(intptr_t)id1);
        if(dfg_Check( NEXTTOK , DFG_COMMA)){
	  array_Add(states,(intptr_t)PAR_predicatedec_end);
	  array_Add(states,(intptr_t)PAR_sortdeclist);
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}
      }
      break;

    case PAR_predicatedec_end:
      /*RULE:
	predicatedec_end:= ')' '.'
	EFFECT: Take opredicate name and sortlist from depot and combine them.
       */
      if(dfg_Check( currentToken , DFG_CLOSEBR ) && dfg_Check( NEXTTOK , DFG_POINT)){
	list1 = (LIST) array_Pop(depot);
	id1 = (char*) array_Pop(depot);

	s = dfg_Symbol(id1,list_Length(list1));

        term1 = term_Create(s,list1);
        term1 = term_Create(fol_Hassort(),list_List(term1));
	pair = list_PairCreate(NULL, term1);

	dfg_DECLARATIONS = list_Nconc(dfg_DECLARATIONS, list_List(pair));
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;

    case PAR_datatypedec:
      /*RULE:
	datatypedec:= '(' DFG_ID
	                     ',''[' fundeclist datatypedec_end
        EFFECT: Puts the name of the sort on the depot.
		They list of functions generating this sort will later be combined
                with the sort in datatypedec_end.
       */
      if(dfg_Check( currentToken , DFG_OPENBR )){
	 array_Add(states,(intptr_t)PAR_datatypedec_end);
	 array_Add(states,(intptr_t)PAR_fundeclist);
	 array_Add(states,(intptr_t)PAR_OPENEBR);
	 array_Add(states,(intptr_t)PAR_COMMA);
	 array_Add(states,(intptr_t)PAR_sortdec);
	 NEXTTOK; 
	
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;

    case PAR_datatypedec_end:
      /*RULE:
        datatypedecdec_end:= ')' '.'
        EFFECT: Take sort name and the functions generating it from depot and
	        combine them.
      */
      if(dfg_Check( currentToken , DFG_CLOSEEBR ) && dfg_Check( NEXTTOK , DFG_CLOSEBR 
	 ) && dfg_Check( NEXTTOK , DFG_POINT)){
        list1 = (LIST) array_Pop(depot);
        s = (intptr_t) array_Pop(depot);
	term1 = term_Create(fol_Datatype(), list_Cons(term_Create(s,list_Nil()),list1));
	pair = list_PairCreate(NULL, term1);

	dfg_DECLARATIONS = list_Nconc(dfg_DECLARATIONS,list_List(pair));

        NEXTTOK;
      }else{
        dfg_ErrorOnCheck(currentToken,lastToken);// "); 
      }
      break;

    case PAR_distinctdec:
      /*RULE:
	distinctdec:= '(' fundeclist distinctdec_end
       */
      if(dfg_Check( currentToken , DFG_OPENBR)){
	array_Add(states,(intptr_t)PAR_distinctdec_end);
	array_Add(states,(intptr_t)PAR_fundeclist);
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;

    case PAR_distinctdec_end:
      /*RULE:
        distinctdec_end:= ')' '.'
        EFFECT: Take function name and function list from depot and make them 
                distinct.
      */
      if(dfg_Check( currentToken , DFG_CLOSEBR ) && dfg_Check( NEXTTOK , DFG_POINT)){
        list1 = (LIST) array_Pop(depot);
        term1 = term_Create(fol_Dist(), list1);
	pair = list_PairCreate(NULL, term1);

	dfg_DECLARATIONS = list_Nconc(dfg_DECLARATIONS,list_List(pair));
        NEXTTOK;
      }else{
        dfg_ErrorOnCheck(currentToken,lastToken);// "); 
      }
      break;

    case PAR_functiondec:
      /*RULE:
        functiondec:= '(' DFG_ID ',' sortdec functiondec_end
                    | '(' DFG_ID ',' '(' sortlist CLOSEBR sortdec functiondec_end

        EFFECT: Puts the name of the function on the stack.
                If the Function has paramteres the sort list with their types
                will be later combined with the return sort and function name
                in functiondec_end. 
       */
      if(dfg_Check( currentToken , DFG_OPENBR ) && dfg_Check( NEXTTOK , DFG_IDENTIFIER)){
        id1 = string_StringCopy(currentToken->text);
	s = symbol_Lookup(id1);
        if (s == 0) {
          misc_StartUserErrorReport();
          misc_UserErrorReport("\n Undefined symbol %s", id1);
          misc_UserErrorReport(" in declarations at line: %d pos: %d . Should be a function or constant.\n"
                               ,currentToken->line,currentToken->pos);
          misc_FinishUserErrorReport();
        }
        if (!symbol_IsFunction(s)) {
          misc_StartUserErrorReport();
          misc_UserErrorReport("\n Symbol %s isn't a function or constant", id1);
          misc_UserErrorReport(" in declarations at line: %d pos: %d .\n"
                               ,currentToken->line,currentToken->pos);
          misc_FinishUserErrorReport();
        }
	array_Add(depot,(intptr_t)id1);
        if(dfg_Check( NEXTTOK , DFG_COMMA)){
          array_Add(states,(intptr_t)PAR_functiondec_end);
	  array_Add(states,(intptr_t)PAR_sortdec);
          if(dfg_Check( NEXTTOK , DFG_OPENBR)){
	    array_Add(states,(intptr_t)PAR_CLOSEBR);
	    array_Add(states,(intptr_t)PAR_sortdeclist);
	    NEXTTOK;
	  }else{
	    array_Add(depot,(intptr_t)list_Nil());
	  }
	}else{
          dfg_ErrorOnCheck(currentToken,lastToken);// ");
        }
      }else{
        dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;

    case PAR_functiondec_end:
      /*RULE:
        functiondec_end:= ')' '.'
        EFFECT: Take function name and sortlist and sort from depot and combine them.
      */
      if(dfg_Check( currentToken , DFG_CLOSEBR ) && dfg_Check( NEXTTOK , DFG_POINT)){
        s1 = (intptr_t) array_Pop(depot);
        list1 = (LIST) array_Pop(depot);
        id1 = (char*) array_Pop(depot);
	s = dfg_Symbol(id1,list_Length(list1));

	term1 = term_Create(s,list1);
	term2 = term_Create(s1,list_Nil());
        term1 = term_Create(fol_Hassort(),list_Cons(term1,list_List(term2)));
	pair = list_PairCreate(NULL, term1);        

	dfg_DECLARATIONS = list_Nconc(dfg_DECLARATIONS,list_List(pair));
        NEXTTOK;
      }else{
        dfg_ErrorOnCheck(currentToken,lastToken);// "); 
      }
      break;

    case PAR_fundeclist:
      /*RULE:
	fundeclist:= DFG_ID fundeclist_1;
	EFFECT: TO DO
       */
      if(dfg_Check( currentToken , DFG_IDENTIFIER )){
	id1 = string_StringCopy(currentToken->text);
	s = symbol_Lookup(id1);
	if (s == 0) {
          misc_StartUserErrorReport();
          misc_UserErrorReport("\n Undefined symbol %s", id1);
          misc_UserErrorReport(" in declarations at line: %d pos: %d . Should be a function or constant.\n"
                               ,currentToken->line,currentToken->pos);
          misc_FinishUserErrorReport();
        }
        if (!symbol_IsFunction(s)) {
          misc_StartUserErrorReport();
          misc_UserErrorReport("\n Symbol %s isn't a function or constant", id1);
          misc_UserErrorReport(" in declarations at line: %d pos: %d .\n"
			       ,currentToken->line,currentToken->pos);
          misc_FinishUserErrorReport();
        }
        term1 = term_Create(s,list_Nil());
        string_StringFree(id1);
	array_Add(depot,(intptr_t)list_List(term1));
	array_Add(states,(intptr_t)PAR_fundeclist_1);
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;

    case PAR_fundeclist_1:
      /*RULE:
	fundeclist:= *empty* 
	             | ',' DFG_ID fundeclist_1;
	EFFECT: TO DO
       */
      if(dfg_Check( currentToken , DFG_COMMA)){
	if(dfg_Check( NEXTTOK , DFG_IDENTIFIER )){
	  id1 = string_StringCopy(currentToken->text);
	  s = symbol_Lookup(id1);
	  if (s == 0) {
	    misc_StartUserErrorReport();
	    misc_UserErrorReport("\n Undefined symbol %s", id1);
	    misc_UserErrorReport(" in declarations at line: %d pos: %d . Should be a function or constant.\n"
				 ,currentToken->line,currentToken->pos);
	    misc_FinishUserErrorReport();
	  }
	  if (!symbol_IsFunction(s)) {
	    misc_StartUserErrorReport();
	    misc_UserErrorReport("\n Symbol %s isn't a function or constant", id1);
	    misc_UserErrorReport(" in declarations at line: %d pos: %d .\n"
				 ,currentToken->line,currentToken->pos);
	    misc_FinishUserErrorReport();
	  }
	  term1 = term_Create(s,list_Nil());
	  list1 = (LIST)array_Pop(depot);
          string_StringFree(id1);
	  array_Add(depot,(intptr_t)list_Nconc(list1,list_List(term1)));
	  array_Add(states,(intptr_t)PAR_fundeclist_1);
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}
      }
      break;

    case PAR_sortdec:
      /*RULE:
	sortdec:= sortterm
	EFFECT: If sort is a function sort (has sorts for parameters) create
	        return sort from the id and add it to the depot. After '('
                start parsing for the sorts of the parameters. Combination happens in
		sortdec_1. If this is a simple sort just add the sort to the depot.
       */
      s=0;
      if(dfg_Check( currentToken , DFG_IDENTIFIER)){
	id = string_StringCopy(currentToken->text);
	if(dfg_Check( NEXTTOK , DFG_OPENPBR)){
	  s++;
	  id = string_Nconc(id,string_StringCopy(currentToken->text));
	  NEXTTOK;
	  while(s>0){
	    if(dfg_Check( currentToken , DFG_IDENTIFIER)){
	      id = string_Nconc(id,string_StringCopy(currentToken->text));
	    }else{
	      misc_StartUserErrorReport();
	      misc_UserErrorReport("\n %s is not allowed for the composition of sorts",currentToken->text);
	      misc_UserErrorReport("\n At line %d start of sort was %s",currentToken->line,currentToken->text);
	      misc_FinishUserErrorReport();
	    }
	    switch(NEXTTOK->type){
	    case DFG_OPENPBR:
	      s++;
	      id = string_Nconc(id,string_StringCopy(currentToken->text));
	      NEXTTOK;
	      break;
	    case DFG_CLOSEPBR:
	      while(dfg_Check( currentToken , DFG_CLOSEPBR ) && s>0){
		s--;
		id = string_Nconc(id,string_StringCopy(currentToken->text));
		NEXTTOK;
	      }
	      break;
	    case DFG_IDENTIFIER:
	      continue;
	      break;
	    default:
	      misc_StartUserErrorReport();
	      misc_UserErrorReport("\n %s is not allowed for the composition of sorts",currentToken->text);
	      misc_UserErrorReport("\n At line %d start of sort was %s",currentToken->line,currentToken->text);
	      misc_FinishUserErrorReport();
	    }
	  }
	}
	s = symbol_Lookup(id);
	if (s == 0) {
	  misc_StartUserErrorReport();
	  misc_UserErrorReport("\n Undefined symbol %s", id);
	  misc_UserErrorReport(" in declarations at line: %d pos: %d . Should be a sort.\n",currentToken->line,currentToken->pos);
	  misc_FinishUserErrorReport();
	}
	if (!symbol_IsSort(s)) {
	  misc_StartUserErrorReport();
	  misc_UserErrorReport("\n Symbol %s isn't a sort", id);
	  misc_UserErrorReport(" in declarations at line: %d pos: %d .\n"
			       ,currentToken->line,currentToken->pos);
	  misc_FinishUserErrorReport();
	}

	string_StringFree(id);
 	array_Add(depot,(intptr_t)s);
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;

    case PAR_sortdeclist:
      /*RULE:
	sortdeclist:= sortdec sortdeclist_1
	EFFECT: add new empty sorts list to the depot
       */
      array_Add(depot,(intptr_t)list_Nil());
      array_Add(states,(intptr_t)PAR_sortdeclist_1);
      array_Add(states,(intptr_t)PAR_sortdec);
      break;

    case PAR_sortdeclist_1:
      /*RULE:
	sortdeclist_1:= *empty*
	              | ',' sortdec sortdeclist_1
	EFFECT:
       */
      s = (intptr_t)array_Pop(depot);
      list1 = (LIST)array_Pop(depot);
      sort1 = term_Create(s,list_Nil());
      array_Add(depot,(intptr_t)list_Nconc(list1,list_List(sort1)));
      if(currentToken->type == DFG_COMMA){
	array_Add(states,(intptr_t)PAR_sortdeclist_1);
	array_Add(states,(intptr_t)PAR_sortdec);
	NEXTTOK;
      }
      break;

      /****************/
      /* FORMULAE     */
      /****************/
    
    case PAR_formulalistsopt:
      if(currentToken->type == DFG_list_of_formulae){
	array_Add(states,(intptr_t)PAR_formulalistsopt);
	array_Add(states,(intptr_t)PAR_formulalist);
      }
      break;

    case PAR_formulalist:
      /*
	RULE:
	formulalist:='list_of_formulae' '(' ['axiom' | 'conjecture'] ')' '.'
	             formulalistopt
		     formulalist_end
        EFFECT: TO_DO
       */
      if(dfg_Check( currentToken , DFG_list_of_formulae ) && dfg_Check( NEXTTOK , DFG_OPENBR) 
&& (dfg_Check( NEXTTOK , DFG_axioms ) || dfg_Check( currentToken , DFG_conjectures))){
	
	origin = (intptr_t*)memory_Malloc(sizeof(intptr_t));
        *origin = currentToken->type == DFG_axioms;
        array_Add(depot,(intptr_t)origin);

	if(dfg_Check( NEXTTOK , DFG_CLOSEBR ) &&
           dfg_Check( NEXTTOK , DFG_POINT)){
          NEXTTOK;
          array_Add(states,(intptr_t)(PAR_formulalist_end));
          array_Add(states,(intptr_t)(PAR_formulalistopt));
        }else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;
      
    case PAR_formulalist_end:
      /*
	RULE:
	formulalist_end:= 'end_of_list' '.'
	EFFECT: TO_DO
       */
      if(dfg_Check( currentToken , DFG_end_of_list ) && dfg_Check( NEXTTOK , DFG_POINT)){
        NEXTTOK;
      }else {
        dfg_ErrorOnCheck(currentToken,lastToken);// flend ");
        break;
      }
      list1 = (LIST) array_Pop(depot);
      origin = (intptr_t*) array_Pop(depot);
      list_NReverse(list1);
      if (!list_Empty(list1)) {
        if (*origin) //  Axioms
          dfg_AXIOMLIST  = list_Nconc(dfg_AXIOMLIST, list1);
        else
          dfg_CONJECLIST = list_Nconc(dfg_CONJECLIST, list1);
      }
      memory_Free(origin,sizeof(intptr_t));
      break;


    case PAR_formulalistopt:
      /*
	RULE:
	formulalistopt_1:= * empty *
                       |  'formula' '(' formula labelopt CLOSEBR POINT formulalistopt_1
	EFFECT: TO_DO
       */
      array_Add(depot,(intptr_t)list_Nil());
      if(dfg_Check( currentToken , DFG_formula)){
        if(dfg_Check( NEXTTOK , DFG_OPENBR )){
          array_Add(states,(intptr_t)(PAR_formulalistopt_1));
          array_Add(states,(intptr_t)(PAR_POINT));
          array_Add(states,(intptr_t)(PAR_CLOSEBR));
          array_Add(states,(intptr_t)(PAR_labelopt));
          array_Add(states,(intptr_t)(PAR_bformula));
          NEXTTOK;
        }else{
          dfg_ErrorOnCheck(currentToken,lastToken);// fopt ");
        }
      }
      break;

    case PAR_formulalistopt_1:
      /*
	RULE:
	formulalistopt_1:= * empty *
                         | 'formula' '(' bformula labelopt CLOSEBR POINT formulalistopt_1
	EFFECT: TO_DO
      */
      if(dfg_Check( currentToken , DFG_formula)){
        if(dfg_Check( NEXTTOK , DFG_OPENBR )){
          array_Add(states,(intptr_t)(PAR_formulalistopt_1));
          array_Add(states,(intptr_t)(PAR_POINT));
          array_Add(states,(intptr_t)(PAR_CLOSEBR));
          array_Add(states,(intptr_t)(PAR_labelopt));
          array_Add(states,(intptr_t)(PAR_bformula));
          NEXTTOK;
        }else{
          dfg_ErrorOnCheck(currentToken,lastToken);// fopt1 ");
        }
      }
      label = (char*) array_Pop(depot);
      term1 = (TERM)  array_Pop(depot);
      list1  = (LIST)  array_Pop(depot);
//      pair;
      pair = list_PairCreate(label, term1);
      array_Add(depot,(intptr_t)list_Cons(pair,list1));
      
      dfg_VarCheck();
      break;


    case PAR_bformula:
      s = 0;
     
      switch(currentToken->type){
      case DFG_not:
	if(dfg_Check( NEXTTOK , DFG_OPENBR)){
	  array_Add(states,(intptr_t)PAR_not_lit_end);
	  array_Add(states,(intptr_t)PAR_bformula);

	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}
	break;
      case DFG_true:
	array_Add(depot,(intptr_t)term_Create(fol_True(),list_Nil()));
	NEXTTOK;
        break;
      case DFG_false:
	array_Add(depot,(intptr_t)term_Create(fol_False(),list_Nil()));
        NEXTTOK;
        break;
      case DFG_le:
	  s = fol_Le();
      case DFG_ls:
	if(s == 0){
	  s = fol_Ls();
	}
      case DFG_ge:
	if(s == 0){
          s = fol_Ge();
        }
      case DFG_gs:
	if(s == 0){
          s = fol_Gs();
        }
      case DFG_equal:
	if(s == 0){
	  s = fol_Equality();
	}

	if(dfg_Check( NEXTTOK , DFG_COLON)){
		DFG_TOKEN token= NEXTTOK;
		if(dfg_Check( token , DFG_lr)){
			array_Add(states,(intptr_t)PAR_annotationlr);
			NEXTTOK;
		} else if(dfg_Check( token , DFG_lt)){
			array_Add(states,(intptr_t)PAR_annotationlt);
			NEXTTOK;
		} else{
			dfg_ErrorOnCheck(currentToken,lastToken);
		}
	}

	if(dfg_Check( currentToken , DFG_OPENBR)){
	  array_Add(depot,(intptr_t)s);
	  
	  array_Add(states,(intptr_t)PAR_binfformula_end);
	  array_Add(states,(intptr_t)PAR_term);
	  array_Add(states,(intptr_t)PAR_COMMA);
	  array_Add(states,(intptr_t)PAR_term);
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}
	break;
      case DFG_and:
	s = fol_And();
      case DFG_or:
	if(s == 0){
	  s = fol_Or();
        }
	if(dfg_Check( NEXTTOK , DFG_OPENBR)){
          array_Add(depot,(intptr_t)s);

          array_Add(states,(intptr_t)PAR_nbformula_end);
          array_Add(states,(intptr_t)PAR_arglist);
          NEXTTOK;
        }else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");                                                                          
        }
	break;
      case DFG_equiv:
	s = fol_Equiv();
      case DFG_implies:
	if(s == 0){
          s = fol_Implies();
        }
      case DFG_implied:
	if(s == 0){
          s = fol_Implied();
        }

	if(dfg_Check( NEXTTOK , DFG_COLON)){
		  DFG_TOKEN token= NEXTTOK;
          if(dfg_Check( token , DFG_lr)){
            array_Add(states,(intptr_t)PAR_annotationlr);
            NEXTTOK;
          } else if(dfg_Check( token , DFG_lt)){
  			array_Add(states,(intptr_t)PAR_annotationlt);
  			NEXTTOK;
          } else{
        	dfg_ErrorOnCheck(currentToken,lastToken);// );
          }
        }
	
	if(dfg_Check( currentToken , DFG_OPENBR)){
          array_Add(depot,(intptr_t)s);

          array_Add(states,(intptr_t)PAR_binbformula_end);
          array_Add(states,(intptr_t)PAR_bformula);
          array_Add(states,(intptr_t)PAR_COMMA);
          array_Add(states,(intptr_t)PAR_bformula);
          NEXTTOK;
        }else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");                                                                          
        }
	break;
      case DFG_forall:
	s = fol_All();
      case DFG_exists:
	if(s == 0){
          s = fol_Exist();
        }

	if(dfg_Check( NEXTTOK , DFG_OPENBR ) && dfg_Check( NEXTTOK , DFG_OPENEBR)){
          array_Add(depot,(intptr_t)s);

          array_Add(states,(intptr_t)PAR_quantformula_end);
          array_Add(states,(intptr_t)PAR_bformula);
          array_Add(states,(intptr_t)PAR_COMMA);
	  array_Add(states,(intptr_t)PAR_CLOSEEBR);
          array_Add(states,(intptr_t)PAR_qtermlist);
          NEXTTOK;
        }else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");                                                                          
        }
	break;
      case DFG_IDENTIFIER:
	id = string_StringCopy(currentToken->text);

        switch(NEXTTOK->type){
        case DFG_OPENBR:
          array_Add(states,(intptr_t)(PAR_predicate_atom_end));
          array_Add(states,(intptr_t)(PAR_termlist));
          array_Add(depot,(intptr_t)id);
          NEXTTOK;
          break;
        default:
          array_Add(depot,(intptr_t)dfg_AtomCreate(id,list_Nil()));
        }
	break;
      default:
	misc_StartUserErrorReport();
	misc_UserErrorReport("\n Line %d: Expected a new term starting with acceptable Symbol,\n",dfg_LINENUMBER);
	misc_UserErrorReport(" but found: ");
	dfg_ErrorPrintToken(currentToken);
	misc_UserErrorReport(".\n");
	misc_FinishUserErrorReport();
      }
      break;


    case PAR_binbformula_end:
      if(dfg_Check( currentToken , DFG_CLOSEBR)){
        term2 =(TERM)array_Pop(depot);
        term1 =(TERM)array_Pop(depot);
        s = (SYMBOL)array_Pop(depot);

        array_Add(depot,(intptr_t)term_Create(s,list_Cons(term1,list_List(term2))));
        NEXTTOK;
      }else{
        dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;


    case PAR_nbformula_end:
      if(dfg_Check( currentToken , DFG_CLOSEBR)){
	list1 =(LIST)array_Pop(depot);
        s = (SYMBOL)array_Pop(depot);

        array_Add(depot,(intptr_t)term_Create(s,list1));
        NEXTTOK;
      }else{
        dfg_ErrorOnCheck(currentToken,lastToken);// ");                                                                            
      }
      break;


    case PAR_binfformula_end:
      if(dfg_Check( currentToken , DFG_CLOSEBR)){
        term2 =(TERM)array_Pop(depot);
        term1 =(TERM)array_Pop(depot);
        s = (SYMBOL)array_Pop(depot);

        array_Add(depot,(intptr_t)term_Create(s,list_Cons(term1,list_List(term2))));
        NEXTTOK;
      }else{
        dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;


    case PAR_quantformula_end:
      if(dfg_Check( currentToken , DFG_CLOSEBR)){
	term1 = (TERM)array_Pop(depot);
	list1 = (LIST)array_Pop(depot);
	s = (SYMBOL)array_Pop(depot);

	dfg_VarBacktrack();
	array_Add(depot,(intptr_t)dfg_CreateQuantifier(s,list1,term1));
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;

    case PAR_arglist:
      array_Add(depot,(intptr_t)list_Nil());
      if(currentToken->type != DFG_CLOSEBR){
	array_Add(states,(intptr_t)PAR_arglist_1);
	array_Add(states,(intptr_t)PAR_bformula);
      }
      break;

    case PAR_arglist_1:
      term1 = (TERM)array_Pop(depot);
      list1 = (LIST)array_Pop(depot);
      array_Add(depot,(intptr_t)list_Nconc(list1,list_List(term1)));
      if(currentToken->type == DFG_COMMA){
	array_Add(states,(intptr_t)PAR_arglist_1);
        array_Add(states,(intptr_t)PAR_bformula);
	NEXTTOK;
      }
      break;

    case PAR_annotationlr:
      term2 = (TERM)array_Pop(depot);
      //term1 = term_Create(fol_Lr(),list_List(term2));
      msorts_LR(term2);
      //TODO
      array_Add(depot,(intptr_t)term2);
      break;

    case PAR_annotationlt:
      term2 = (TERM)array_Pop(depot);
      //term1 = term_Create(fol_Lr(),list_List(term2));
      msorts_LT(term2);
      //TODO
      array_Add(depot,(intptr_t)term2);
      break;

      /****************/
      /* CLAUSES      */
      /****************/

    case PAR_clauselistsopt:
      /*
        RULE:
	clauselistsopt:= * empty *
	     | clauselist clauselistsopt
       */
      if(currentToken->type == DFG_list_of_clauses){
        array_Add(states,(intptr_t)(PAR_clauselistsopt));
        array_Add(states,(intptr_t)(PAR_clauselist));
      }
      break;
      
    case PAR_clauselist:
      /*
        RULE:
	clauselist:= 'list_of_clauses' '(' [ 'axioms' | 'conjectures' ] ')' '.'
                       cnfclausesopt  clauselist_end
	EFFECT: create new list of clauses determine if it describes axioms or
                conjectures by adding origin to the depot.
       */
      if(dfg_Check( currentToken , DFG_list_of_clauses ) &&
	 dfg_Check( NEXTTOK , DFG_OPENBR ) &&
	 (dfg_Check( NEXTTOK , DFG_axioms ) || dfg_Check( currentToken , DFG_conjectures))){
	
        origin = (intptr_t*)memory_Malloc(sizeof(intptr_t));
	*origin = currentToken->type == DFG_axioms;
        array_Add(depot,(intptr_t)origin);
        
        if(dfg_Check( NEXTTOK , DFG_COMMA ) &&
	   dfg_Check( NEXTTOK , DFG_cnf   ) &&
	   dfg_Check( NEXTTOK , DFG_CLOSEBR ) &&
	   dfg_Check( NEXTTOK , DFG_POINT)){
          NEXTTOK;
          array_Add(states,(intptr_t)(PAR_clauselist_end));
	  array_Add(states,(intptr_t)(PAR_cnfclausesopt));
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// cl2 ");
	}
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// cl1 ");
      }
      break;
      
    case PAR_clauselist_end:
      /*
        RULE:
	clauselist_end:= 'end_of_list' '.'
        EFFECT: Take the list with clauses and its origin from depot.
                Depending on origin, add clauses to axioms or conjectures.
                Plain clauses are restored from dfg_TEMPPLAINCLAUSES.
       */
      if(dfg_Check( currentToken , DFG_end_of_list ) && dfg_Check( NEXTTOK , DFG_POINT)){
        NEXTTOK;
      }else {
	dfg_ErrorOnCheck(currentToken,lastToken);// clend ");
	break;
      }
      list1 = (LIST) array_Pop(depot);
      origin = (intptr_t*) array_Pop(depot);
      if (!list_Empty(dfg_TEMPPLAINCLAUSES)) {
	/* Plain clauses are temporarily stored in the list dfg_TEMPPLAINCLAUSES.
	   They need to be concatenated to their respective axiom or conjecture 
	   plain clause lists. */
        if (*origin) /* Axioms */
          dfg_PLAINAXCLAUSES = list_Nconc(dfg_PLAINAXCLAUSES, list_NReverse(dfg_TEMPPLAINCLAUSES));
        else
          dfg_PLAINCONCLAUSES = list_Nconc(dfg_PLAINCONCLAUSES, list_NReverse(dfg_TEMPPLAINCLAUSES));
        dfg_TEMPPLAINCLAUSES = list_Nil();
      }
      if (!list_Empty(list1)) {
        if (*origin) /* Axioms */
          dfg_AXCLAUSES  = list_Nconc(dfg_AXCLAUSES, list1);
        else
          dfg_CONCLAUSES = list_Nconc(dfg_CONCLAUSES, list1);
      }
      memory_Free(origin,sizeof(intptr_t));
      break;
      
    case PAR_cnfclausesopt:
      /*
        RULE:
	cnfclausesopt:=* empty *
	     | clause '(' cnfclauseopt labelopt ')' '.' cnfclausesopt_1
       */
      array_Add(depot,(intptr_t)list_Nil());
      if(dfg_Check( currentToken , DFG_clause)){
	if(dfg_Check( NEXTTOK , DFG_OPENBR )){
	  array_Add(states,(intptr_t)(PAR_cnfclausesopt_1));
	  array_Add(states,(intptr_t)(PAR_POINT));
	  array_Add(states,(intptr_t)(PAR_CLOSEBR));
	  array_Add(states,(intptr_t)(PAR_labelopt));
	  array_Add(states,(intptr_t)(PAR_cnfclauseopt));
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// cnfopt ");
	}
      }
      break;
      
    case PAR_cnfclausesopt_1:
      /*
        RULE:
        cnfclausesopt_1:= * empty *
	     | clause '(' cnfclauseopt labelopt ')' '.' cnfclausesopt_1
        EFFECT:Take the last clause with its label from the depot and combine
               them to a pair. Add this pair to the actual list of clauses
               also found on the depot. 
      */
      if(dfg_Check( currentToken , DFG_clause)){
	if(dfg_Check( NEXTTOK , DFG_OPENBR )){
	  array_Add(states,(intptr_t)(PAR_cnfclausesopt_1));
	  array_Add(states,(intptr_t)(PAR_POINT));
	  array_Add(states,(intptr_t)(PAR_CLOSEBR));
	  array_Add(states,(intptr_t)(PAR_labelopt));
	  array_Add(states,(intptr_t)(PAR_cnfclauseopt));
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// cnfopt1 ");
	}
      }
      label = (char*) array_Pop(depot);
      clause = (TERM)  array_Pop(depot);
      list1  = (LIST)  array_Pop(depot);
//      pair;
      if (clause == NULL) { /*Plain Clause so only a placeholder was added to the depot*/
        if (label != NULL)
          string_StringFree(label);
        array_Add(depot,(intptr_t)list1);
      } else {
        pair = list_PairCreate(label, clause);
        array_Add(depot,(intptr_t)list_Nconc(list1,list_List(pair)));
      }
      dfg_VarCheck();
      break;
      
    case PAR_labelopt:
      /*RULE:
	labelopt:= *empty*
                 | ',' [ DFG_NUMBER | DFG_IDENTIFIER ]
        EFFECT: if there is a label in form of a number or identifier add it to the depot.
       */
      if(currentToken->type == DFG_COMMA){
	if(dfg_Check( NEXTTOK , DFG_NUMBER ) || dfg_Check( currentToken , DFG_IDENTIFIER)){
	  array_Add(depot,(intptr_t)string_StringCopy(currentToken->text));
	  char* Flabel= string_StringCopy(currentToken->text);
	  NEXTTOK;

	  if(currentToken->type == DFG_COMMA){
		  NEXTTOK;
		  if (dfg_Check( currentToken , DFG_NUMBER)) {
			  number = (intptr_t*)memory_Malloc(sizeof(intptr_t));
			  string_StringToInt(currentToken->text,FALSE,number); //TODO DanielW: check for failure?
			  nextclauseweights_addLabel(Flabel,(intptr_t)*number);
		  } else {
			  dfg_ErrorOnCheck(currentToken,lastToken);// TODO errormessage will be confusing
		  }
		  NEXTTOK;
	  } else {
		  nextclauseweights_addLabel(Flabel,1000); //TODO hardcoded default
	  }
        }else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// lbl ");
	}
      }else{
	array_Add(depot,(intptr_t)NULL);
      }
      break;
      
    case PAR_cnfclauseopt:
      /*
        RULE:
        cnfclauseopt:=  cnfclausebody | cnfclause | cnfshortclause
        EFFECT: If next Token is an 'or' or an 'all' clause is in CNF else
                choose plain clause ( C || A -> S ). 
       */
      switch(currentToken->type){
      case DFG_or:
	array_Add(states,(intptr_t)(PAR_cnfclausebody));
	break;
      case DFG_forall:
	array_Add(states,(intptr_t)(PAR_cnfclause));
	break;
      default:
	/* C || A -> S */
	array_Add(states,(intptr_t)(PAR_cnfshortclause));
      }
      break;
      
    case PAR_cnfclause:
      /*
        RULE:
	cnfclause:= cnfclausebody
	      | 'forall' '(' '[' qtermlist cnfclause_1 cnfclausebody cnfclause_end
       */
      if(dfg_Check( currentToken , DFG_forall ) &&
	 dfg_Check( NEXTTOK , DFG_OPENBR ) &&
	 dfg_Check( NEXTTOK , DFG_OPENEBR)){
        array_Add(states,(intptr_t)(PAR_cnfclause_end));
        array_Add(states,(intptr_t)(PAR_cnfclausebody));
        array_Add(states,(intptr_t)(PAR_cnfclause_1));
        array_Add(states,(intptr_t)(PAR_qtermlist));
        NEXTTOK;
      }else{
        dfg_ErrorOnCheck(currentToken,lastToken);// cnfcl ");
      }
      break;

    case PAR_cnfclause_1:
      /*
	RULE:
        cnfclause_1:= ']' ','
        EFFECT:Collect the list with variables for the quantifier.
       */
	if(dfg_Check( currentToken , DFG_CLOSEEBR ) && dfg_Check( NEXTTOK , DFG_COMMA)){
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}
      break;
      
    case PAR_cnfclause_end:
      /*
	RULE:
	cnfclause_end:= ')'
        EFFECT: Create new Clause.
       */
      if(dfg_Check( currentToken , DFG_CLOSEBR)){
	dfg_VarBacktrack();
	term1 = (TERM)array_Pop(depot);
	list1 = (LIST)array_Pop(depot);
	array_Add(depot,(intptr_t)dfg_CreateQuantifier(fol_All(),list1,term1));
	NEXTTOK;
      }else{
        dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;
      
    case PAR_cnfclausebody:
      /*
        RULE:
        cnfclausebody:= OR '(' litlist cnfclausebody_end
       */
      if(dfg_Check( currentToken , DFG_or ) &&
	 dfg_Check( NEXTTOK , DFG_OPENBR)){
        array_Add(states,(intptr_t)(PAR_cnfclausebody_end));
        array_Add(states,(intptr_t)(PAR_litlist));
      }else{
        dfg_ErrorOnCheck(currentToken,lastToken);// cnfclend ");
      }
      break;
      
    case PAR_cnfclausebody_end:
      /*
	RULE:
	cnfclausebody_end:= ')'
        EFFECT: Create new Clause.
       */
      if(dfg_Check( currentToken , DFG_CLOSEBR)){
	list1 = (LIST)array_Pop(depot);
	array_Add(depot,(intptr_t)term_Create(fol_Or(),list1));
	NEXTTOK;
      }else{
        dfg_ErrorOnCheck(currentToken,lastToken);// cnfbody ");
      }
      break;
      
    case PAR_cnfshortclause:
      /*
	RULE:
	cnfshortclause:= litlist_ws ARROW_DOUBLELINE selected_litlist_ws ARROW litlist_ws
        EFFECT: Start the processing of new Variables.
       */
      dfg_VarStart();
      array_Add(states,(intptr_t)(PAR_cnfshortclause_end));
      array_Add(states,(intptr_t)(PAR_litlist_ws));
      array_Add(states,(intptr_t)(PAR_ARROW));
      array_Add(states,(intptr_t)(PAR_selected_litlist_ws));
      array_Add(states,(intptr_t)(PAR_ARROW_DOUBLELINE));
      array_Add(states,(intptr_t)(PAR_litlist_ws));
      break;
      
    case PAR_cnfshortclause_end:
      /*
	RULE:
	cnfshortclause_end: *empty*
        EFFECT: Backtrack Variables. Pop literal lists constraint, antecedent
                and succedent from the depot and create a new Plain clause.
                If a literal was selected in antecedent set selected and
                reset global variable.
        WARNING: The Plainclause will be saved in dfg_TEMPPLAINCLAUSES and NULL
                 will be saved in the depot as a placeholder.
      */
      dfg_VarBacktrack();
      clause1 = dfg_PlainClauseCreate();
      clause1->succedent  = (LIST) array_Pop(depot);
      clause1->antecedent = (LIST) array_Pop(depot);
      clause1->constraint = (LIST) array_Pop(depot);
      clause1->selected = dfg_SELECTED_LITERAL;
      dfg_SELECTED_LITERAL = (TERM) NULL;
      dfg_TEMPPLAINCLAUSES = list_Cons(clause1,dfg_TEMPPLAINCLAUSES);
      array_Add(depot,(intptr_t)NULL);
      break;
      
    case PAR_litlist:
      /*
        RULE:
	litlist:= * empty *
	        | lit litlist_1
        EFFECT:Add empty literal list to the depot.
       */
      array_Add(depot,(intptr_t)list_Nil());
      if(NEXTTOK->type != DFG_CLOSEBR){
        array_Add(states,(intptr_t)(PAR_litlist_1));
        array_Add(states,(intptr_t)(PAR_lit));
      }
      break;
      
    case PAR_litlist_1:
      /*
        RULE:
	litlist_1:= * empty *
	          | ',' lit litlist_1
	EFFECT:Get last literal and current literal list from depot
               and combine them. Therafter try to parse another literal.
       */
      lit1 = (TERM)array_Pop(depot);
      list1 = (LIST)array_Pop(depot);
      array_Add(depot,(intptr_t)list_Nconc(list1, list_List(lit1)));
      if( currentToken->type == DFG_COMMA){
	array_Add(states,(intptr_t)(PAR_litlist_1));
        array_Add(states,(intptr_t)(PAR_lit));
	NEXTTOK;
      }else{
	
      }
      break;
      
    case PAR_litlist_ws:
      /*
        RULE:
	litlist_ws:= * empty *
	           | lit litlist_ws_1
        EFFECT: Add empty literal list to the depot
       */
      array_Add(depot,(intptr_t)list_Nil());
      if(currentToken->type == DFG_not  || currentToken->type == DFG_IDENTIFIER
	 || currentToken->type == DFG_equal || currentToken->type == DFG_true
	 || currentToken->type == DFG_false || currentToken->type == DFG_le
         || currentToken->type == DFG_ls || currentToken->type == DFG_ge
         || currentToken->type == DFG_gs){
	array_Add(states,(intptr_t)(PAR_litlist_ws_1));
        array_Add(states,(intptr_t)(PAR_lit));
      }
      break;
      
    case PAR_litlist_ws_1:
      /*
        RULE:
        litlist_ws_1:= * empty *
	             | lit litlist_ws_1
        EFFECT:Get last literal and current literal list from depot
               and combine them. Thereafter try to parse another literal.
	       Because of lack of seperator Check for different literal types.
       */
      lit1 = (TERM)array_Pop(depot);
      list1 = (LIST)array_Pop(depot);

      array_Add(depot,(intptr_t)list_Nconc(list1, list_List(lit1)));
      if(currentToken->type == DFG_not || currentToken->type == DFG_IDENTIFIER
         || currentToken->type == DFG_equal || currentToken->type == DFG_true
         || currentToken->type == DFG_false || currentToken->type == DFG_le
         || currentToken->type == DFG_ls || currentToken->type == DFG_ge
         || currentToken->type == DFG_gs){
        array_Add(states,(intptr_t)(PAR_litlist_ws_1));
        array_Add(states,(intptr_t)(PAR_lit));
      }
      break;
      
    case PAR_selected_litlist_ws:
      /*
        RULE:
	selected_litlist_ws:= * empty *
	                    | lit selected_litlist_ws_1
        EFFECT: Add empty literal list to the depot
       */
      array_Add(depot,(intptr_t)list_Nil());
      if(currentToken->type == DFG_not || currentToken->type == DFG_IDENTIFIER
	 || currentToken->type == DFG_equal || currentToken->type == DFG_true
	 || currentToken->type == DFG_false || currentToken->type == DFG_le
         || currentToken->type == DFG_ls || currentToken->type == DFG_ge
         || currentToken->type == DFG_gs){
	array_Add(states,(intptr_t)(PAR_selected_litlist_ws_1));
        array_Add(states,(intptr_t)(PAR_lit));
      }
      break;
      
    case PAR_selected_litlist_ws_1:
      /*
        RULE:
        selected_litlist_ws_1:= * empty *
	                      | lit selected_litlist_ws_1
			      | '+' lit selected_litlist_ws_1
        EFFECT:Get last literal and current literal list from depot.
               If '+' is next Token select last literal. If there already is a
	       selected literal in this literal list, throw error.
	       Add literal to list. Thereafter try to parse another literal.
	       Because of lack of seperator Check for different literal types.
       */
      lit1 = (TERM)array_Pop(depot);
      list1 = (LIST)array_Pop(depot);
      array_Add(depot,(intptr_t)list_Nconc(list1, list_List(lit1)));
      if(currentToken->type == DFG_SYMB_PLUS){
	if(dfg_SELECTED_LITERAL != NULL) {
          dfg_ErrorOnCheck(currentToken,lastToken);// ");
          misc_StartUserErrorReport();
          misc_UserErrorReport("\n Trying to select two literals in a clause.");
          misc_FinishUserErrorReport();
        }
        dfg_SELECTED_LITERAL = lit1;
      }
      if(currentToken->type == DFG_not || currentToken->type == DFG_IDENTIFIER
         || currentToken->type == DFG_equal || currentToken->type == DFG_true
         || currentToken->type == DFG_false || currentToken->type == DFG_le
	 || currentToken->type == DFG_ls || currentToken->type == DFG_ge
	 || currentToken->type == DFG_gs){
        array_Add(states,(intptr_t)(PAR_selected_litlist_ws_1));
        array_Add(states,(intptr_t)(PAR_lit));
      }
      break;
      
    case PAR_lit:
      /*
        RULE:
	lit:= atom
	    | 'not' '(' atom not_lit_end
       */
      if(currentToken->type == DFG_not){
	if( NEXTTOK->type == DFG_OPENBR){
	  array_Add(states,(intptr_t)(PAR_not_lit_end));
	  array_Add(states,(intptr_t)(PAR_atom));
	  NEXTTOK;
	}
      }else{
	array_Add(states,(intptr_t)(PAR_atom));
      }
      break;
      
    case PAR_atom:
      /*
	RULE:
	atom:= DFG_ID
	     | 'true'
	     | 'false'
	     | 'equal' '(' term COMMA term eq_atom_end
	     | DFG_IDENTIFIER [ '(' termlist predicate_atom_end
	                      | ':' sortdec sorted_atom_end
        EFFECT:If atom is true or false simply generate atoms. If atoms
               start with number or identifier save text. Either it is a
	       constant or if followed by '(' a predicate. Constants can
	       be generated directly. Predicates and equal not until later.
       */
      s = 0;
      switch( currentToken->type){
      case DFG_true:
	array_Add(depot,(intptr_t)term_Create(fol_True(),list_Nil()));
	NEXTTOK;
	break;
      case DFG_false:
	array_Add(depot,(intptr_t)term_Create(fol_False(),list_Nil()));
	NEXTTOK;
	break;
      case DFG_IDENTIFIER:
	id = string_StringCopy(currentToken->text);
        
	switch(NEXTTOK->type){
	case DFG_OPENBR:
	  array_Add(states,(intptr_t)(PAR_predicate_atom_end));
          array_Add(states,(intptr_t)(PAR_termlist));
	  array_Add(depot,(intptr_t)id);
	  NEXTTOK;
	  break;
	default:
	  array_Add(depot,(intptr_t)dfg_AtomCreate(id,list_Nil()));
	}
	break;
      case DFG_le:
	s = fol_Le();
      case DFG_ls:
        if(s == 0){
          s = fol_Ls();
        }
      case DFG_ge:
        if(s == 0){
          s = fol_Ge();
        }
      case DFG_gs:
        if(s == 0){
          s = fol_Gs();
        }
      case DFG_equal:
        if(s == 0){
          s = fol_Equality();
        }
        if(dfg_Check( NEXTTOK , DFG_OPENBR)){
          array_Add(depot,(intptr_t)s);

          array_Add(states,(intptr_t)PAR_binfformula_end);
          array_Add(states,(intptr_t)PAR_term);
          array_Add(states,(intptr_t)PAR_COMMA);
          array_Add(states,(intptr_t)PAR_term);
          NEXTTOK;
        }else{
          dfg_ErrorOnCheck(currentToken,lastToken);// ");                                  
        }
        break;
      default:
	misc_StartUserErrorReport();
	misc_UserErrorReport("\n Line %d: ",dfg_LINENUMBER);
	dfg_ErrorPrintToken(currentToken);
	misc_UserErrorReport("cannot be used as an atom");
	misc_FinishUserErrorReport();
      }
      break;
      
    case PAR_not_lit_end:
      /*
        RULE:
        not_lit_end:= ')'
        EFFECT: Take atom from depot and add corresponding not-literal.
       */
      if(dfg_Check( currentToken , DFG_CLOSEBR)){
        term1 = (TERM) array_Pop(depot);
	array_Add(depot,(intptr_t)term_Create(fol_Not(),list_List(term1)));
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// not ");
      }
      break;
    
    case PAR_predicate_atom_end:
      /*
        RULE:
        predicate_atom_end:= ')'
        EFFECT: Take list of terms and id of the predicate from the depot.
                Add new atom for this predicate to the depot.
        WARNING:dfg_AtomCreate checks for arity, so termlist must have
                exactly the arity as its length
       */
      if(dfg_Check( currentToken , DFG_CLOSEBR)){
        termlist = (LIST) array_Pop(depot);
	id = (char*) array_Pop(depot);
	        
	term1 = dfg_AtomCreate(id,termlist);
        
	array_Add(depot,(intptr_t)term1);
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// pred atom ");
      }
      break;
      
    case PAR_eq_atom_end:
      /*
        RULE:
        eq_atom_end:= ')'
        EFFECT: Take to terms from depot and create Atom describing their equality.
       */
      if(dfg_Check( currentToken , DFG_CLOSEBR)){
        term2 = (TERM) array_Pop(depot);
	term1 = (TERM) array_Pop(depot);
        array_Add(depot,(intptr_t)term_Create(fol_Equality(),list_Cons(term1,list_List(term2))));
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// lit ");
      }
      break;
      
      /****************/
      /* TERMS        */
      /****************/

    case PAR_term:
      /*
	RULE:
	term:= DFG_ID | DFG_NUMBER
	     | DFG_ID '(' term termlist_1 term_1
        EFFECT: If term is constant create term and add it to the depot.
	        If term is predicate add its id to the depot and check 
                for its subjects. Combine them in term_1.
       */
      numberid = NULL;
      isneg = 1;
      s = 0;
      s2 = 0;
      switch(currentToken->type){
      case DFG_IDENTIFIER:
        id = string_StringCopy(currentToken->text);
        switch(NEXTTOK->type){
	case DFG_OPENBR:
	  array_Add(depot,(intptr_t)id);
	  array_Add(depot,(intptr_t)list_Nil());
	  array_Add(states,(intptr_t)(PAR_term_1));
	  array_Add(states,(intptr_t)(PAR_termlist_1));
	  array_Add(states,(intptr_t)(PAR_term));
	  NEXTTOK;
	  break;
	case DFG_COLON:
	  array_Add(depot,(intptr_t)id);
	  array_Add(states,(intptr_t)PAR_sorted_term);
	  array_Add(states,(intptr_t)PAR_sortdec);
	  NEXTTOK;
	  break;
	default:
	  s = dfg_Symbol(id,0);
	  array_Add(depot,(intptr_t)term_Create(s,list_Nil()));
	}
	break;
      case DFG_UNARY_MINUS:
	numberid = string_StringCopy("-");
	isneg = -1;
	s2 = fol_Integer(); 
	if(dfg_Check(NEXTTOKWS,DFG_NUMBER)){
	  number = (intptr_t*)memory_Malloc(sizeof(intptr_t));
	  numberid = string_Nconc(numberid,string_StringCopy(currentToken->text));
	  string_StringToInt(currentToken->text,FALSE,number);
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);
	}
      case DFG_NUMBER:
	if(numberid == NULL){
	  number = (intptr_t*)memory_Malloc(sizeof(intptr_t));
	  numberid = string_StringCopy(currentToken->text);
	  string_StringToInt(currentToken->text,FALSE,number);
	}
	switch(NEXTTOKWS->type){
	case DFG_POINT:
	  numberid =  string_Nconc(numberid,string_StringCopy("."));
	  s2 = fol_Real(); 
	  if(dfg_Check( NEXTTOKWS , DFG_NUMBER)){
	    numberid = string_Nconc(numberid,string_StringCopy(currentToken->text));
	  }else{
	    dfg_ErrorOnCheck(currentToken,lastToken);// const number ");
	  }
	case DFG_WhiteSpace:
	case DFG_NextLine:
	  NEXTTOK;
	default:
	  if(s2 == 0)
	    s2 = fol_Natural();
	  dfg_SymbolDecl(DFG_FUNC,string_StringCopy(numberid), 0);
	  s = dfg_Symbol(numberid,0);
	  symbol_SetWeight(s,(int) isneg * (*number));

	  memory_Free(number,sizeof(intptr_t));

	  list1 = list_List(term_Create(s2, list_Nil()));
	  list1 = list_Cons(term_Create(s,list_Nil()),list1);
	  term2 = term_Create(fol_Hassort(),list1);
	  pair = list_PairCreate(NULL, term2);

	  dfg_DECLARATIONS = list_Nconc(dfg_DECLARATIONS,list_List(pair));
	  array_Add(depot,(intptr_t)term_Create(s,list_Nil()));
	  /*if(*decimal != 0){
	    list1 = term_Create((*decimal),list_Nil());
	  }else{
	    list1 = list_Nil();
	  }
	  if(*number != 0){
            term1 = term_Create(isneg*(*number),list_Nil());
	    term = term_Create(fol_Const(),list_Cons(term1,list1));
	  }else
	  array_Add(states,(intptr_t)term,depot);
	  memory_Free(number,sizeof(intptr_t));
	  memory_Free(decimal,sizeof(intptr_t));*/
	}
	break;
      default:
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;

    case PAR_sorted_term:
      s1  = (SYMBOL)array_Pop(depot);
      id = (char*)array_Pop(depot);
      s = dfg_Symbol(id,s1);
      array_Add(depot,(intptr_t)term_Create(s,list_Nil()));
      break;

    case PAR_term_1:
      /*
	RULE:
        term_1:= ')'
        EFFECT: Take predicate name and its subject (a list of terms)
                from the depot and combine them.
       */
      if(dfg_Check( currentToken , DFG_CLOSEBR)){
	list1 = (LIST)  array_Pop(depot);
	id   = (char*) array_Pop(depot);
        array_Add(depot,(intptr_t)dfg_TermCreate(id, list1));
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;
      
    case PAR_termlist:
      /*
	RULE:
        termlist:= term termlist_1
        EFFECT: Add empty list of terms to the depot.
       */
      array_Add(states,(intptr_t)(PAR_termlist_1));
      array_Add(states,(intptr_t)(PAR_term));
      array_Add(depot,(intptr_t)list_Nil());
      break;
      
    case PAR_termlist_1:
      /*
	RULE:
	termlist_1:= *empty
                   | ',' term termlist_1
        EFFECT: Take the last parsed term from depot and  add it to
                the current list of terms also taken from the depot.
       */
      term1 = (TERM) array_Pop(depot);
      list1 = (LIST) array_Pop(depot);
      array_Add(depot,(intptr_t)list_Nconc(list1,list_List(term1)));
      if(currentToken->type == DFG_COMMA){
	array_Add(states,(intptr_t)(PAR_termlist_1));
	array_Add(states,(intptr_t)(PAR_term));
        NEXTTOK;
      }
      break;
      
    case PAR_qtermlist:
      /*
	RULE:
	qtermlist:= qterm qtermlist_1
	EFFECT: Add empty list of qterms to the depot.
       */
      dfg_VarStart();
      array_Add(states,(intptr_t)(PAR_qtermlist_1));
      array_Add(states,(intptr_t)(PAR_qterm));
      array_Add(depot,(intptr_t)list_Nil());
      break;
      
    case PAR_qtermlist_1:
      /*
        RULE:
        qtermlist_1:= *empty*
             | ',' qterm qtermlist_1
        EFFECT: Take the last parsed term from depot and  add it to
                the current list of terms also taken from the depot.
       */
      term1 = (TERM) array_Pop(depot);
      list1 = (LIST) array_Pop(depot);
      array_Add(depot,(intptr_t)list_Nconc(list1,list_List(term1)));
      if(dfg_Check( currentToken , DFG_COMMA)){
	if(dfg_Check( NEXTTOK , DFG_IDENTIFIER )){
	array_Add(states,(intptr_t)(PAR_qtermlist_1));
	array_Add(states,(intptr_t)(PAR_qterm));
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// qterml ");
	}
      }else{
	dfg_VarStop();
      }
      break;
      
    case PAR_qterm:
      /*
	RULE:
	qterm:= DFG_ID
	      | DFG_ID ':' sortdef
        EFFECT: Qterms are used in quantifiers to list variables.
                This can be done in form of a Variable or in form of a predicate
		describing a sort.
       */
      if(dfg_Check( currentToken , DFG_IDENTIFIER)){
	id  = string_StringCopy(currentToken->text);
	id1 = string_StringCopy(currentToken->text);
	if(dfg_Check( NEXTTOK , DFG_COLON)){
	  if(dfg_Check( NEXTTOK , DFG_IDENTIFIER)){
	    id2 = string_StringCopy(currentToken->text);
	    if (!dfg_IGNORE) {
	      SYMBOL p, v;
              p = dfg_Symbol(id2, 1);
              if (!symbol_IsSort(p)) {
                misc_StartUserErrorReport();
                misc_UserErrorReport("\n Line %d: Symbol %s is not a sort.\n",dfg_LINENUMBER,id2);
                misc_FinishUserErrorReport();
              }
	      v = dfg_Symbol(id,p);
	      term1 = term_Create(v,list_Nil());
	      array_Add(depot,(intptr_t)term1);
	      string_StringFree(id1);
            }
	    NEXTTOK;
	  }else{
	    dfg_ErrorOnCheck(currentToken,lastToken);// ");
	  }
	}else{
	  if (!dfg_IGNORE) {
            s = dfg_Symbol(id,fol_Top());
	    if (!symbol_IsVariable(s)) {
              misc_StartUserErrorReport();
              misc_UserErrorReport("\n Line %d: Symbol %s is not a variable.\n",dfg_LINENUMBER,id1);
              misc_FinishUserErrorReport();
            }
            array_Add(depot,(intptr_t)term_Create(s, list_Nil()));
	    string_StringFree(id1);
          }
	}
      }
      break;
      
      
      /****************/
      /* SETTINGS     */
      /****************/

    case PAR_settinglistsopt:
      if(dfg_Check( currentToken , DFG_list_of_general_settings)){
	if (dfg_IGNORESETTINGS) {
          misc_StartUserErrorReport();
          misc_UserErrorReport("\n Settings not allowed in included files\n");			
          misc_FinishUserErrorReport();
        }
        if(dfg_Check( NEXTTOK , DFG_POINT)){
	  array_Add(states,(intptr_t)(PAR_POINT));
	  array_Add(states,(intptr_t)(PAR_end_of_list));
	  array_Add(states,(intptr_t)(PAR_gsettings));
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// "); 
	}
        
      }else if(dfg_Check( currentToken , DFG_list_of_settings)){
	if (dfg_IGNORESETTINGS) {
          misc_StartUserErrorReport();
          misc_UserErrorReport("\n Settings not allowed in included files\n");			
          misc_FinishUserErrorReport();
        }
        if(dfg_Check( NEXTTOK , DFG_OPENBR)){
	  if(dfg_Check( NEXTTOK , DFG_SPASS)){
	    lex->ignoreText = 0;
	    if(dfg_Check( NEXTTOK , DFG_CLOSEBR ) && dfg_Check( NEXTTOK , DFG_POINT)){
	      array_Add(states,(intptr_t)(PAR_settings_end));
              array_Add(states,(intptr_t)(PAR_spassflags));
	      NEXTTOK;
	    }else{
	      dfg_ErrorOnCheck(currentToken,lastToken);// settings ");
	    }

	  }else{
	    if(dfg_Check( NEXTTOK , DFG_CLOSEBR ) && dfg_Check( NEXTTOK , DFG_POINT
	       ) && dfg_Check( NEXTTOK , DFG_TEXT ) && dfg_Check( NEXTTOK , DFG_end_of_list
	       ) && dfg_Check( NEXTTOK , DFG_POINT)){
	      NEXTTOK;
	    }else{
	      dfg_ErrorOnCheck(currentToken,lastToken);// settings ");
	    }
	  }
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// settings "); 
	}
        
      }
      break;
      
    case PAR_settings_end:
      if(dfg_Check( currentToken , DFG_end_of_list ) &&
	 dfg_Check( NEXTTOK , DFG_POINT)){
	NEXTTOK;
	lex->ignoreText = 1;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// settings end ");
      }
      break;
    
    case PAR_spassflags:
      switch(currentToken->type){
      case DFG_set_precedence:
	if(dfg_Check( NEXTTOK , DFG_OPENBR)){
	  array_Add(states,(intptr_t)(PAR_spassflags));
	  array_Add(states,(intptr_t)(PAR_POINT));
	  array_Add(states,(intptr_t)(PAR_CLOSEBR));
	  array_Add(states,(intptr_t)(PAR_preclist_1));
	  array_Add(states,(intptr_t)(PAR_precitem));
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}
	break;
      case DFG_set_selection:
	if(dfg_Check( NEXTTOK , DFG_OPENBR)){
	  array_Add(states,(intptr_t)(PAR_spassflags));
	  array_Add(states,(intptr_t)(PAR_POINT));
	  array_Add(states,(intptr_t)(PAR_CLOSEBR));
	  array_Add(states,(intptr_t)(PAR_selectlist_1));
	  array_Add(states,(intptr_t)(PAR_selectitem));
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}
	break;
      case DFG_set_ClauseFormulaRelation:
        if(dfg_Check( NEXTTOK , DFG_OPENBR)){
	  array_Add(states,(intptr_t)(PAR_spassflags));
	  array_Add(states,(intptr_t)(PAR_POINT));
	  array_Add(states,(intptr_t)(PAR_CLOSEBR));
	  array_Add(states,(intptr_t)(PAR_clfolist_1));
	  array_Add(states,(intptr_t)(PAR_clfoitem));
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// CFR ");
	}
	break;
      case DFG_set_DomPred:
	if(dfg_Check( NEXTTOK , DFG_OPENBR)){
	  array_Add(states,(intptr_t)(PAR_spassflags));
	  array_Add(states,(intptr_t)(PAR_POINT));
	  array_Add(states,(intptr_t)(PAR_dompred_end));
	  array_Add(states,(intptr_t)(PAR_labellist));
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}
	break;
      case DFG_set_flag:
	if(dfg_Check( NEXTTOK , DFG_OPENBR ) && dfg_Check( NEXTTOK , DFG_IDENTIFIER)){
	  id = string_StringCopy(currentToken->text);
	  if(dfg_Check( NEXTTOK , DFG_COMMA ) && dfg_Check( NEXTTOK , DFG_NUMBER)){
	    number = (intptr_t*)memory_Malloc(sizeof(intptr_t));
	    if(string_StringToInt(currentToken->text,FALSE,(intptr_t*)number) &&
	       dfg_Check( NEXTTOK , DFG_CLOSEBR ) && dfg_Check( NEXTTOK , DFG_POINT)){
	      int flag = flag_Id(id);
	      if (flag == -1) {
		misc_StartUserErrorReport();
		misc_UserErrorReport("\n Found unknown flag %s", id);
		misc_FinishUserErrorReport();
	      }
	      string_StringFree(id);
	      flag_SetFlagIntValue(dfg_FLAGS, flag, *number);
	      memory_Free(number,sizeof(intptr_t));
	    }else{
	      dfg_ErrorOnCheck(currentToken,lastToken);// ");
	    }
	  }else{
	    dfg_ErrorOnCheck(currentToken,lastToken);// ");
	  }
	  array_Add(states,(intptr_t)(PAR_spassflags));
//	  array_Add(states,(intptr_t)(PAR_POINT));
//	  array_Add(states,(intptr_t)(PAR_CLOSEBR));
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}
	break;
      default:
	;
      }
      break;
      
    case PAR_dompred_end:
      if(dfg_Check( currentToken , DFG_CLOSEBR ) && dfg_Check( NEXTTOK , DFG_POINT)){
	list1 = (LIST)array_Pop(depot);
        for ( ; !list_Empty(list1); list1 = list_Pop(list1)) {
          s = symbol_Lookup(list_Car(list1));
          if (s == 0) {
            misc_StartUserErrorReport();
            misc_UserErrorReport("\n Undefined symbol %s", list_Car(list1));
            misc_UserErrorReport(" in DomPred list.\n"); 
            misc_FinishUserErrorReport(); 
          }
          if (!symbol_IsPredicate(s)) {
            misc_StartUserErrorReport();
            misc_UserErrorReport("\n Symbol %s isn't a predicate", list_Car(list1));
            misc_UserErrorReport(" in DomPred list.\n");
            misc_FinishUserErrorReport();
          }
          string_StringFree(list_Car(list1)); 
          symbol_AddProperty(s, DOMPRED);
        }
        NEXTTOK;
      }else{
        dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      
      break;
      
    case PAR_preclist_1:
      if(currentToken->type == DFG_COMMA){
	array_Add(states,(intptr_t)(PAR_preclist_1));
        array_Add(states,(intptr_t)(PAR_precitem));
	NEXTTOK;
      }
      break;
      
    case PAR_precitem:
      if(dfg_Check( currentToken , DFG_IDENTIFIER)){
	s = symbol_Lookup(currentToken->text);
        if (s == 0) {
          misc_StartUserErrorReport();
          misc_UserErrorReport("\n Undefined symbol %s ", currentToken->text);
          misc_UserErrorReport(" in precedence list.\n"); 
          misc_FinishUserErrorReport(); 
        }
        symbol_SetIncreasedOrdering(dfg_PRECEDENCE, s);
        dfg_USERPRECEDENCE = list_Cons((POINTER)s, dfg_USERPRECEDENCE);
	NEXTTOK;
      }else if(dfg_Check( currentToken , DFG_OPENBR ) && dfg_Check( NEXTTOK,DFG_IDENTIFIER)){
	s = symbol_Lookup(currentToken->text);
	if (s == 0) {
	  misc_StartUserErrorReport();
	  misc_UserErrorReport("\n Undefined symbol %s", currentToken->text);
	  misc_UserErrorReport("in precedence list.\n");
	  misc_FinishUserErrorReport(); 
	}
	number=(intptr_t*)memory_Malloc(sizeof(intptr_t));
	int ord = 0;
	if(dfg_Check( NEXTTOK , DFG_COMMA ) && dfg_Check( NEXTTOK,DFG_NUMBER ) && string_StringToInt(currentToken->text,FALSE,(intptr_t*)number)){
	  if(dfg_Check( NEXTTOK , DFG_COMMA ) && dfg_Check( NEXTTOK , DFG_IDENTIFIER)){ 
	    if(currentToken->text[1] != '\0' ||
	       (currentToken->text[0]!='l' && currentToken->text[0]!='m' && currentToken->text[0]!='r')) {
              misc_StartUserErrorReport();
              misc_UserErrorReport("\n Invalid symbol status %s", currentToken->text);
              misc_UserErrorReport(" in precedence list.");
              misc_FinishUserErrorReport();
            }
            switch (currentToken->text[0]) {
            case 'm': ord = ORDMUL;   break;
            case 'r': ord = ORDRIGHT; break;
            default:  ord = 0;
            }
	    if(dfg_Check( currentToken , DFG_CLOSEBR)){
              symbol_SetIncreasedOrdering(dfg_PRECEDENCE, s);
              dfg_USERPRECEDENCE = list_Cons((POINTER)s, dfg_USERPRECEDENCE);
              symbol_SetWeight(s, *number);
	      memory_Free(number,sizeof(intptr_t));
              if (ord != 0)
                symbol_AddProperty(s, ord);
	      
	    }else{
	      dfg_ErrorOnCheck(currentToken,lastToken);// ");
	    }
	  }else if(dfg_Check( currentToken , DFG_CLOSEBR)){
	    if(dfg_Check( currentToken , DFG_CLOSEBR)){
              symbol_SetIncreasedOrdering(dfg_PRECEDENCE, s);
              dfg_USERPRECEDENCE = list_Cons((POINTER)s, dfg_USERPRECEDENCE);
              symbol_SetWeight(s, *number);
	      memory_Free(number,sizeof(intptr_t));
	    }else{
	      dfg_ErrorOnCheck(currentToken,lastToken);// ");
	    }
	  }else{
	    dfg_ErrorOnCheck(currentToken,lastToken);// ");
	  }
	
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}
	NEXTTOK;
      }
      break;
      
    case PAR_clfolist_1:
      if(currentToken->type == DFG_COMMA){
	array_Add(states,(intptr_t)(PAR_clfolist_1));
	array_Add(states,(intptr_t)(PAR_clfoitem));
	NEXTTOK;
      }
      break;
      
    case PAR_clfoitem:
      if(dfg_Check( currentToken , DFG_OPENBR ) && dfg_Check( NEXTTOK , DFG_NUMBER)){
	number = (intptr_t*)memory_Malloc(sizeof(intptr_t));
	if(string_StringToInt(currentToken->text,FALSE,(intptr_t*)number)){
	  array_Add(depot,(intptr_t)number);
	  if(dfg_Check( NEXTTOK , DFG_COMMA)){
	    array_Add(states,(intptr_t)(PAR_clfoitem_end));
	    array_Add(states,(intptr_t)(PAR_clfoaxseq_1));
	    array_Add(states,(intptr_t)(PAR_clfoaxseqitem));
	    NEXTTOK;
	  }else{
	    dfg_ErrorOnCheck(currentToken,lastToken);// clfoitem ");
	  }
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// clfoitem ");
	}
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// clfoitem ");
      }
      break;
      
    case PAR_clfoitem_end:
      if(dfg_Check( currentToken , DFG_CLOSEBR)){
	number = (intptr_t*)array_Pop(depot);
	dfg_CLAXRELATION = list_Cons(list_Cons((POINTER)*number, dfg_CLAXAXIOMS), dfg_CLAXRELATION);
        memory_Free(number,sizeof(intptr_t));
	dfg_CLAXAXIOMS   = list_Nil();
        NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// clfoitem end ");
      }
      break;
      
    case PAR_clfoaxseq_1:
      if(currentToken->type == DFG_COMMA){
	array_Add(states,(intptr_t)(PAR_clfoaxseq_1));
	array_Add(states,(intptr_t)(PAR_clfoaxseqitem));
	NEXTTOK;
      }
      break;
      
    case PAR_clfoaxseqitem:
      if(dfg_Check( currentToken , DFG_IDENTIFIER ) || dfg_Check( currentToken , DFG_NUMBER)){
	dfg_CLAXAXIOMS = list_Cons((POINTER)string_StringCopy(currentToken->text), dfg_CLAXAXIOMS);
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// clfoaxseqitem ");
      }
      break;
      
    case PAR_selectlist_1:
      if(currentToken->type == DFG_COMMA){
	array_Add(states,(intptr_t)(PAR_selectlist_1));
	array_Add(states,(intptr_t)(PAR_selectitem));
	NEXTTOK;
      }
      break;
      
    case PAR_selectitem:
      if(dfg_Check( currentToken , DFG_IDENTIFIER)){
	s = symbol_Lookup(currentToken->text);
	if (s == 0) {
	  misc_StartUserErrorReport();
	  misc_UserErrorReport("\n Undefined symbol %s ", currentToken->text);
	  misc_UserErrorReport(" in selection list.\n"); 
	  misc_FinishUserErrorReport(); 
	}
	if (!symbol_IsPredicate(s)) {
	  misc_StartUserErrorReport();
	  misc_UserErrorReport("\n Symbol %s isn't a predicate", currentToken->text);
	  misc_UserErrorReport(" in selection list.\n");
	  misc_FinishUserErrorReport();
	}
	dfg_USERSELECTION = list_Cons((POINTER)s, dfg_USERSELECTION);
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;
      
    case PAR_gsettings:
      if(dfg_Check( currentToken , DFG_hypothesis)){
	array_Add(states,(intptr_t)(PAR_gsettings_1));
	array_Add(states,(intptr_t)(PAR_gsetting));
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;
      
    case PAR_gsettings_1:
      if(currentToken->type == DFG_hypothesis){
	array_Add(states,(intptr_t)(PAR_gsettings_1));
	array_Add(states,(intptr_t)(PAR_gsetting));
      }
      break;
      
    case PAR_gsetting:
	if(dfg_Check( currentToken , DFG_hypothesis ) && dfg_Check( NEXTTOK , DFG_OPENEBR)){
	array_Add(states,(intptr_t)(PAR_gsetting_end));
	array_Add(states,(intptr_t)(PAR_labellist));
	NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}
      break;
      
    case PAR_gsetting_end:
      if(dfg_Check( currentToken , DFG_CLOSEEBR ) && dfg_Check( NEXTTOK , DFG_POINT)){
	list1 = (LIST)array_Pop(depot);
	dfg_DeleteStringList(list1);
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;
      
    case PAR_labellist:
      if(dfg_Check( currentToken , DFG_IDENTIFIER ) || dfg_Check( currentToken , DFG_NUMBER)){
	array_Add(depot,(intptr_t)list_List(string_StringCopy(currentToken->text)));
	array_Add(states,(intptr_t)(PAR_labellist_1));
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;
      
    case PAR_labellist_1:
      if(currentToken->type == DFG_COMMA){
	if(dfg_Check( NEXTTOK , DFG_IDENTIFIER)){
	  list1 = (LIST)array_Pop(depot);
	  array_Add(depot,(intptr_t)list_Nconc(list1 , list_List(string_StringCopy(currentToken->text))));
	  array_Add(states,(intptr_t)(PAR_labellist_1));
	  NEXTTOK;
	}else{
	  dfg_ErrorOnCheck(currentToken,lastToken);// ");
	}
      }
      break;

      /****************/
      /* SIMPLE SIGNS */
      /****************/
    case PAR_end_of_list:
      if(dfg_Check( currentToken , DFG_end_of_list)){
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// signs ");
      }
      break;
    case PAR_POINT:
      if(dfg_Check( currentToken , DFG_POINT)){
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// signs ");
      }
      break;
    case PAR_COMMA:
      if(dfg_Check( currentToken , DFG_COMMA)){
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// signs ");
      }
      break;
    case PAR_OPENBR:
      if(dfg_Check( currentToken , DFG_OPENBR)){
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;
    case PAR_CLOSEBR:
      if(dfg_Check( currentToken , DFG_CLOSEBR)){
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;
    case PAR_OPENEBR:
      if(dfg_Check( currentToken , DFG_OPENEBR)){
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;
    case PAR_CLOSEEBR:
      if(dfg_Check( currentToken , DFG_CLOSEEBR)){
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;
    case PAR_ARROW:
      if(dfg_Check( currentToken , DFG_ARROW)){
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;
    case PAR_ARROW_DOUBLELINE:
      if(dfg_Check( currentToken , DFG_ARROW_DOUBLELINE)){
	NEXTTOK;
      }else{
	dfg_ErrorOnCheck(currentToken,lastToken);// ");
      }
      break;
      
    default:
      dfg_ErrorOnCheck(currentToken,lastToken);// ");
      ;
    }
  }
  freeLexer(lex);
  freeToken(lastToken);
  freeToken(currentToken);  

  if(depot->size == 0){
    array_Delete(depot);
  }else{
    printf("Depot has still %d elements\n",depot->size);
  }
  array_Delete(states);
  

}
