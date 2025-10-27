/**************************************************************/
/* Macros                                                     */
/**************************************************************/

#define DFG_MAX_KEY_LENGTH 27 /*longest keyword is "set_ClauseFormulaRelation" with 25 chars 
                                + 2 char for keyword seperating dollarsigns*/
#define DFG_BUFFER_SIZE DFG_MAX_KEY_LENGTH
#define CURRCHAR (buffer[buff_pos])
#define NEXTCHAR getNextChar(lex,&buff_pos)
#define UNGETCHAR ungetChar(lex,&buff_pos)
#define NEXTNOIDENT ((NEXTCHAR < 'a' || CURRCHAR > 'z') && (CURRCHAR < 'A' || CURRCHAR > 'Z') && (CURRCHAR < '0' || CURRCHAR > '9') && CURRCHAR != '_')

/**************************************************************/
/* Includes                                                   */
/**************************************************************/


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "memory.h"
#include "strings.h"


/**************************************************************/
/* Structures                                                 */
/**************************************************************/

typedef enum {
  DFG_POINT,
  DFG_COMMA,
  DFG_OPENBR,
  DFG_CLOSEBR,
  DFG_OPENEBR,
  DFG_CLOSEEBR,
  DFG_OPENPBR,
  DFG_CLOSEPBR,
  DFG_UNARY_MINUS,
  DFG_ARROW,
  DFG_SYMB_PLUS,
  DFG_ARROW_DOUBLELINE,
  DFG_COLON,
  DFG_3TAP,
  
  DFG_TEXT,

  DFG_all,
  DFG_and,
  DFG_author,
  DFG_axioms,

  DFG_begin_problem,
  DFG_box,

  DFG_clause,
  DFG_cnf,
  DFG_comp,
  DFG_concept_formula,
  DFG_conjectures,
  DFG_conv,

  DFG_datatype,
  DFG_date,
  DFG_def,
  DFG_description,
  DFG_dia,
  DFG_div,
  DFG_distinct_symbols,
  DFG_dl,
  DFG_domain,
  DFG_domrestr,

  DFG_eml,
  DFG_end_of_list,
  DFG_end_problem,
  DFG_equal,
  DFG_equiv,
  DFG_exists,

  DFG_false,
  DFG_forall,
  DFG_formula,
  DFG_fract,
  DFG_function,
  DFG_weights,
  DFG_functions,

  DFG_ge,
  DFG_gs,
  
  DFG_hypothesis,

  DFG_id,
  DFG_implied,
  DFG_implies,
  DFG_include,

  DFG_le,
  DFG_list_of_clauses,
  DFG_list_of_declarations,
  DFG_list_of_descriptions,
  DFG_list_of_formulae,
  DFG_list_of_general_settings,
  DFG_list_of_includes,
  DFG_list_of_proof,
  DFG_list_of_settings,
  DFG_list_of_special_formulae,
  DFG_list_of_symbols,
  DFG_logic,
  DFG_lr,
  DFG_ls,
  DFG_lt,

  DFG_minus,
  DFG_mult,

  DFG_name,
  DFG_not,

  DFG_or,
  
  DFG_plus,
  DFG_predicate,
  DFG_predicates,
  DFG_prop_formula,

  DFG_range,
  DFG_ranrester,
  DFG_rel_formula,
  DFG_role_formula,
  
  DFG_satisfiable,
  DFG_set_flag,
  DFG_set_precedence,
  DFG_set_selection,
  DFG_set_ClauseFormulaRelation,
  DFG_set_DomPred,
  DFG_some,
  DFG_sorts,
  DFG_splitlevel,
  DFG_status,
  DFG_step,
  DFG_subsort,
  DFG_sum,

  DFG_test,

  DFG_translpairs,
  DFG_true,

  DFG_unknown,
  DFG_unsatisfiable,

  DFG_version,

  DFG_App,
  DFG_AED,
  
  DFG_Con,
  DFG_CRW,
  
  DFG_Def,
  
  DFG_EmS,
  DFG_EqF,
  DFG_EqR,
  
  DFG_Fac,
  
  DFG_Inp,
  DFG_Integer,
  
  DFG_KIV,
  
  DFG_LEM,
  
  DFG_Mpm,
  DFG_MRR,
  
  DFG_Natural,
  
  DFG_Obv,
  DFG_Ohy,
  DFG_Opm,
  DFG_OTTER,
  
  DFG_PROTEIN,
  
  DFG_Rational,
  DFG_Real,
  DFG_Rew,
  DFG_Res,
  
  DFG_Shy,
  DFG_SoR,
  DFG_SpL,
  DFG_SpR,
  DFG_SPm,
  DFG_Spt,
  DFG_Ssi,
  DFG_SATURAT,
  DFG_SETHEO,
  DFG_SPASS,
  DFG_Ter,
  DFG_Top,
  
  DFG_UnC,
  DFG_URR,
  
  DFG_NUMBER,

  DFG_IDENTIFIER,
  DFG_WhiteSpace,
  DFG_NextLine,
  DFG_FileEnd,
  DFG_FileBegin
}dfgtokentype ;

typedef enum {
  LEX_start,
  LEX_END,

  LEX_a_,
  LEX_all,
  LEX_and,
  LEX_author,
  LEX_axioms,
  LEX_b_,
  LEX_begin_problem,
  LEX_box,

  LEX_c_,
  LEX_clause,
  LEX_cnf,
  LEX_co_,
  LEX_comp,
  LEX_con_,
  LEX_concept_formula,
  LEX_conjectures,
  LEX_conv,

  LEX_d_,
  LEX_dat_,
  LEX_datatype,
  LEX_date,
  LEX_de_,
  LEX_def,
  LEX_description,
  LEX_di_,
  LEX_dia,
  LEX_div,
  LEX_distinct_symbols,
  LEX_dl,
  LEX_dom_,
  LEX_domain,
  LEX_domrestr,

  LEX_e_,
  LEX_eml,
  LEX_end__,
  LEX_end_of_list,
  LEX_end_problem,
  LEX_equ_,
  LEX_equal,
  LEX_equiv,
  LEX_exists,

  LEX_f_,
  LEX_false,
  LEX_for_,
  LEX_forall,
  LEX_formula,
  LEX_fract,
  LEX_function,
  LEX_weights,
  LEX_functions,

  LEX_g_,
  LEX_ge,
  LEX_gs,

  LEX_i_,
  LEX_id,
  LEX_implie_,
  LEX_implied,
  LEX_implies,
  LEX_include,
  
  LEX_hypothesis,

  LEX_l_,
  LEX_le,
  LEX_list_of__,
  LEX_list_of_clauses,
  LEX_list_of_de_,
  LEX_list_of_declarations,
  LEX_list_of_descriptions,
  LEX_list_of_formulae,
  LEX_list_of_general_settings,
  LEX_list_of_includes,
  LEX_list_of_proof,
  LEX_list_of_s_, 
  LEX_list_of_settings,
  LEX_list_of_special_formulae,
  LEX_list_of_symbols,
  LEX_logic,
  LEX_lr,
  LEX_ls,
  LEX_lt,

  LEX_m_,
  LEX_minus,
  LEX_mult,

  LEX_n_,
  LEX_name,
  LEX_not,

  LEX_or,
  
  LEX_p_,
  LEX_plus,
  LEX_pr_,
  LEX_predicate,
  LEX_predicates,
  LEX_prop_formula,

  LEX_r_,
  LEX_ran_,
  LEX_range,
  LEX_ranrester,
  LEX_rel_formula,
  LEX_role_formula,
  
  LEX_s_,
  LEX_satisfiable,
  LEX_set__,
  LEX_set_flag,
  LEX_set_precedence,
  LEX_set_selection,
  LEX_set_ClauseFormulaRelation,
  LEX_set_DomPred,
  LEX_so_,
  LEX_some,
  LEX_sorts,
  LEX_splitlevel,
  LEX_st_,
  LEX_status,
  LEX_step,
  LEX_su_,
  LEX_subsort,
  LEX_sum,

  LEX_t_,
  LEX_test,
  LEX_tr_,
  LEX_translpairs,
  LEX_true,

  LEX_un_,
  LEX_unknown,
  LEX_unsatisfiable,

  LEX_version,

  LEX_A_,
  LEX_App,
  LEX_AED,
  
  LEX_C_,
  LEX_Con,
  LEX_CRW,
  
  LEX_Def,
  
  LEX_E_,
  LEX_EmS,
  LEX_Eq_,
  LEX_EqF,
  LEX_EqR,
  
  LEX_Fac,
  
  LEX_In_,
  LEX_Inp,
  LEX_Integer,
  
  LEX_KIV,
  
  LEX_LEM,
  
  LEX_M_,
  LEX_Mpm,
  LEX_MRR,
  
  LEX_Natural,
  
  LEX_O_,
  LEX_Obv,
  LEX_Ohy,
  LEX_Opm,
  LEX_OTTER,
  
  LEX_PROTEIN,
  
  LEX_R_,
  LEX_Rational,
  LEX_Re_,
  LEX_Real,
  LEX_Rew,
  LEX_Res,  
  
  LEX_S_,
  LEX_Shy,
  LEX_SoR,
  LEX_Sp_,
  LEX_SpL,
  LEX_SpR,
  LEX_Spt,
  LEX_Ssi,
  LEX_SATURAT,
  LEX_SETHEO,
  LEX_SP_,
  LEX_SPASS,
  LEX_SPm,
  
  LEX_T_,
  LEX_Ter,
  LEX_Top,
  
  LEX_U_,
  LEX_UnC,
  LEX_URR,

  LEX_identifier
}lexerstate;

typedef struct{
  dfgtokentype type;
  int line;
  int pos;
  char* text;
}DFG_TOKEN_NODE, *DFG_TOKEN;


typedef struct{
  FILE* input;
  int line;
  int pos;
  char* buffer;
  int buffered;
  int ignoreText;
  int ignoreWS;
}DFG_LEXER_NODE, *DFG_LEXER;


/**************************************************************/
/* Scanner Functions                                          */
/**************************************************************/


DFG_TOKEN nextTokenorWS(DFG_LEXER lex);

DFG_TOKEN nextToken(DFG_LEXER lex);

DFG_TOKEN helpnextToken(DFG_LEXER lex);

/**************************************************************/
/* Lexer and Token reation                                    */
/**************************************************************/

DFG_TOKEN createToken(dfgtokentype type, int line, int pos, char* text);

DFG_LEXER createLexer(FILE* file);

void restoreLex(DFG_LEXER lex,int buff_pos);

void freeToken(DFG_TOKEN tok);

void freeLexer(DFG_LEXER lex);

char* createText(const char* c , int length);

/**************************************************************/
/* Lexer and Token reation                                    */
/**************************************************************/

int getNextChar(DFG_LEXER lex, int* pos);

void ungetChar(DFG_LEXER lex,int* pos);

void eraseCommentline(DFG_LEXER lex);


/**************************************************************/
/* Helpers that can scan different types of words             */
/**************************************************************/


DFG_TOKEN getNumber(DFG_LEXER lex,int* pbuff_pos);

DFG_TOKEN getText(DFG_LEXER lex,int* pbuff_pos);

DFG_TOKEN getIdentifier(DFG_LEXER lex,int* pbuff_pos);

DFG_TOKEN getKeyword(DFG_LEXER lex,int* pbuff_pos);

void dfg_ErrorPrintType(dfgtokentype);

void dfg_ErrorPrintToken(DFG_TOKEN);
