/**************************************************************/
/* Macros                                                   */
/**************************************************************/

#define NEXTTOK getNextTok(lex, &currentToken, &lastToken)
#define NEXTTOKWS getNextTokWS(lex, &currentToken, &lastToken)

#define DFG_PRDICAT 304
#define DFG_FUNC 286
#define DFG_OPERAT 299
#define DFG_QUANTIF 306
#define DFG_SRT 310
/**************************************************************/
/* Includes                                                   */
/**************************************************************/

#include <ctype.h>
#include "dfg.h"
#include "symbol.h"
#include "term.h"
#include "foldfg.h"
#include "strings.h"
#include "array.h"
#include "dfgscanner.h"
#include "eml.h"


/*dfg_parse() works like a Pushdown Automaton. The following
  enum depicts the states of this automaton.
*/
   typedef enum{
/****************/
/* TOP RULE     */
/****************/
     PAR_problem,
     PAR_problem_end,
/****************/
/* DESCRIPTION  */
/****************/
     PAR_description,
     PAR_name,
     PAR_author,
     PAR_versionopt,
     PAR_logicopt,
     PAR_status,
     PAR_desctext,
     PAR_dateopt,
/****************/
/* LOGICAL PART */
/****************/

     PAR_logicalpart,

/****************/
/* SYMBOLS      */
/****************/

     PAR_declarationlistopt,
     PAR_decllistopt,
     PAR_symbollistopt,
     PAR_functionsopt,
     PAR_functionlist_1,
     PAR_func,
     PAR_weightsopt,
     PAR_weightlist_1,
     PAR_weight,
     PAR_predicatesopt,
     PAR_predicatelist_1,
     PAR_pred,
     PAR_sortsopt,
     PAR_sortlist_1,
     PAR_sort,
     PAR_translpairsopt,
     PAR_translpairlist_1,
     PAR_translpair,

/****************/
/* DECLARATIONS */
/****************/

     PAR_subsortdec,
     PAR_subsortdec_end,
     PAR_predicatedec,
     PAR_predicatedec_end,
     PAR_datatypedec,
     PAR_datatypedec_end,
     PAR_distinctdec,
     PAR_distinctdec_end,
     PAR_functiondec,
     PAR_functiondec_end,
     PAR_fundeclist,
     PAR_fundeclist_1,
     PAR_sortdec,
     PAR_sortdec_1,
     PAR_sortdeclist,
     PAR_sortdeclist_1,

/****************/
/* FORMULAE     */
/****************/
     PAR_formulalistsopt,
     PAR_formulalist,
     PAR_formulalistopt,
     PAR_formulalist_end,
     PAR_formulalistopt_1,
     PAR_bformula,
     PAR_binbformula_end,
     PAR_nbformula_end,
     PAR_binfformula_end,
     PAR_quantformula_end,
     PAR_arglist,
     PAR_arglist_1,
     PAR_annotationlr,
     PAR_annotationlt,

/****************/
/* CLAUSES      */
/****************/
     
     PAR_clauselistsopt,
     PAR_clauselist,
     PAR_clauselist_end,
     PAR_cnfclausesopt,
     PAR_cnfclausesopt_1,
     PAR_cnfclauseopt,
     PAR_cnfclause,
     PAR_cnfclausebody,
     PAR_cnfshortclause,
     PAR_cnfclause_1,  
     PAR_cnfclause_end,
     PAR_cnfclausebody_end,
     PAR_cnfshortclause_end,
     PAR_litlist,
     PAR_litlist_1,
     PAR_litlist_ws,
     PAR_litlist_ws_1,
     PAR_selected_litlist_ws,
     PAR_selected_litlist_ws_1,
     PAR_lit,
     PAR_atomlist,
     PAR_atom,
     PAR_not_lit_end,
     PAR_predicate_atom_end,
     PAR_eq_atom_end,
     
     PAR_labelopt,
     
/****************/
/* TERMS        */
/****************/
     
     PAR_term,
     PAR_sorted_term,
     PAR_term_1,
     PAR_termlist,
     PAR_termlist_1,
     PAR_qterm,
     PAR_qtermlist,
     PAR_qtermlist_1,
     
/****************/
/* SETTINGS     */
/****************/
     PAR_settinglistsopt,
     PAR_settings_end,
     PAR_spassflags,
     PAR_dompred_end,
     PAR_preclist_1,
     PAR_precitem,
     PAR_clfolist_1,
     PAR_clfoitem,
     PAR_clfoitem_end,
     PAR_clfoaxseq_1,
     PAR_clfoaxseqitem,
     PAR_selectlist_1,
     PAR_selectitem,
     PAR_gsettings,
     PAR_gsettings_1,
     PAR_gsetting,
     PAR_gsetting_end,
     PAR_labellist,
     PAR_labellist_1,
/****************/
/* SIMPLE SIGNS */
/****************/
     PAR_end_of_list,
     PAR_POINT,
     PAR_COMMA,
     PAR_OPENBR,
     PAR_CLOSEBR,
     PAR_OPENEBR,
     PAR_CLOSEEBR,
     PAR_ARROW,
     PAR_ARROW_DOUBLELINE
   }parserstate;


/**************************************************************/
/* Helpers                                                    */
/**************************************************************/

/*Interfaces between Parser and Lexer. It is recommended to use 
  the Macros NEXTTOK and NEXTTOKWS instead of directly invoking
  these Methods inside dfg_parse(). Warning: Last pcurrentToken
  will be freed. That also includes the scanned text! So it is
  not advisable to point to  this component.
 */
DFG_TOKEN getNextTok(DFG_LEXER lex, DFG_TOKEN* pcurrentToken, DFG_TOKEN* plastToken);

DFG_TOKEN getNextTokWS(DFG_LEXER lex, DFG_TOKEN* pcurrentToken, DFG_TOKEN* plastToken);

/**************************************************************/
/* Parsing Logic                                              */
/**************************************************************/

static void dfg_parse(FILE* file);
