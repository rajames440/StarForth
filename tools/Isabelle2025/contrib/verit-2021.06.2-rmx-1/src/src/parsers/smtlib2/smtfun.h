#ifndef SMTFUN_H
#define SMTFUN_H

#include "parsers/smtlib2/smttypes.h"

#include <stdbool.h>

/*
  --------------------------------------------------------------
  Commands
  --------------------------------------------------------------
*/

void smt2_set_logic(char* symbol);
void smt2_declare_sort(char* symbol, char* numeral);
void smt2_define_sort(char* symbol, T_SYMBOL_LIST symbol_list, T_SORT sort);
void smt2_declare_fun(char* symbol, T_SORT_LIST sort_list, T_SORT sort);
void smt2_declare_poly_fun(
	char* symbol, T_SORT_LIST sort_list1, T_SORT_LIST sort_list2, T_SORT sort);
void smt2_define_fun(
	char* symbol, T_STACK_VAR stack_var, T_SORT Tsort, T_TERM term);
void smt2_define_poly_fun(
	char* symbol, /*T_SORT_LIST sort_list,*/
	T_STACK_VAR stack_var, /*T_SORT Tsort,*/ T_TERM term);
void smt2_push(char* numeral);
void smt2_pop(char* numeral);
void smt2_assert(T_TERM term);
void smt2_check_sat(void);
void smt2_get_assertions(void);
void smt2_get_proof(void);
void smt2_get_unsat_core(void);
void smt2_get_model(void);
void smt2_echo(char* str);
void smt2_get_value(T_TERM_LIST term_list);
void smt2_get_assignment(void);
void smt2_get_info(char* keyword);
void smt2_get_option(char* keyword);
void smt2_exit(void);
void smt2_init(bool option_disable_print_success);
void smt2_done(void);

/**
   \brief action executed by the parser when a command has been executed.

   \note currently any error interrupts veriT's execution and when smt2_command
   is called, we know that the command has been successful.
*/
void smt2_command(void);

void smt2_set_option(char* keyword);

/**
   \brief sets a numeric option
   \note str is a numeral, i.e. a C-string of digits */
void smt2_set_option_num(char* keyword, char* str);

void smt2_set_option_str(char* keyword, char* str);
void smt2_set_option_bool(char* keyword, const bool value);

void smt2_set_info(char* keyword);
void smt2_set_info_str(char* keyword, char* str);

void smt2_define_fun_short(char* symbol, T_TERM term);

/*
  --------------------------------------------------------------
  Terms
  --------------------------------------------------------------
*/

T_TERM smt2_term_const(char* str);
T_TERM smt2_term(char* str);
T_TERM smt2_term_app(char* str, T_TERM_LIST term_list);
T_TERM smt2_iterm(char* str, T_NUMERAL_LIST numeral_list);
T_TERM smt2_iterm_app(
	char* str, T_TERM_LIST term_list, T_NUMERAL_LIST numeral_list);

T_TERM smt2_term_s(char* str, T_SORT sort);
T_TERM smt2_term_app_s(char* str, T_TERM_LIST term_list, T_SORT sort);
T_TERM smt2_iterm_s(char* str, T_NUMERAL_LIST numeral_list, T_SORT sort);
T_TERM smt2_iterm_app_s(
	char* str, T_TERM_LIST term_list, T_NUMERAL_LIST numeral_list, T_SORT sort);

T_TERM smt2_let(T_BIND_LIST bind_list, T_TERM term);
T_TERM smt2_term_forall(T_STACK_VAR stack_var, T_TERM term);
T_TERM smt2_term_exists(T_STACK_VAR stack_var, T_TERM term);

T_TERM smt2_term_attr(T_TERM term, char* keyword);
T_TERM smt2_term_attr_str(T_TERM term, char* keyword, char* str);

T_TERM smt2_term_attr_named(T_TERM term, char* str);
T_TERM smt2_term_attr_pattern(T_TERM term, T_TERM_LIST triggers);

/* extension of SMT-LIB2 with lambda expressions */
TDAG smt2_term_lambda(T_STACK_VAR stack_var, TDAG term);
T_TERM smt2_lambda_app(T_TERM lambda, T_TERM_LIST term_list);

/*
  --------------------------------------------------------------
  Atoms - sorts - and all that
  --------------------------------------------------------------
*/

void smt2_declare_sort_var_list(T_SORT_LIST sort_list);
void smt2_undeclare_sort_var_list(T_SORT_LIST sort_list);

T_SORT smt2_sort_var(char* symbol);

T_SORT_LIST smt2_sort_var_list_one(T_SORT sort);
T_SORT_LIST smt2_sort_var_list_add(T_SORT_LIST sort_list, T_SORT sort);

T_BIND smt2_bind(char* symbol, T_TERM term);

T_VAR smt2_var(char* symbol, T_SORT sort);

/**
    \param sort the sort to check
    \return 1 iff the sort is parametric(0 otherwise)
*/
int smt2_sort_parametric(T_SORT sort);
/**
    \param symbol sort name
    \return the sort corresponding to the symbol
*/
T_SORT smt2_sort_lookup(char* symbol);
/**
    \param symbol the identifier of the sort
    \param numeral_list list of numerals
    \return sort applying the indexed sort to the list of indexes
*/
T_SORT smt2_sort_lookup_index(char* symbol, T_NUMERAL_LIST numeral_list);
/**
    \param sort_list list of sorts of length n
    \return sort for functions from n-1 first sorts to the last
*/
T_SORT smt2_sort_functional(T_SORT_LIST sort_list);
/**
    \param sort parametric sort
    \param sort_list list of sorts
    \return application of the parametric sort to the list of sorts
*/
T_SORT smt2_sort_apply(T_SORT sort, T_SORT_LIST sort_list);

/*
  --------------------------------------------------------------
  Binders
  --------------------------------------------------------------
*/

void smt2_declare_bind_list(T_BIND_LIST bind_list);
void smt2_undeclare_bind_list(T_BIND_LIST bind_list);
void smt2_declare_stack_var(T_STACK_VAR stack_var);
void smt2_undeclare_stack_var(T_STACK_VAR stack_var);

/*
  --------------------------------------------------------------
  Attributes
  --------------------------------------------------------------
*/

T_TERM smt2_annot_term(T_TERM term, T_ATTR_LIST attr_list);
T_ATTR_LIST smt2_attr_list_one(T_ATTR);
T_ATTR_LIST smt2_attr_list_add(T_ATTR_LIST, T_ATTR);
T_ATTR smt2_attr(char*);
T_ATTR smt2_attr_value(char*, void*);

/*
  --------------------------------------------------------------
  List handling
  --------------------------------------------------------------
*/

T_BIND_LIST smt2_bind_list_new(void);
T_BIND_LIST smt2_bind_list_one(T_BIND bind);
T_BIND_LIST smt2_bind_list_add(T_BIND_LIST bind_list, T_BIND bind);

T_NUMERAL_LIST smt2_numeral_list_one(char* numeral);
T_NUMERAL_LIST smt2_numeral_list_add(
	T_NUMERAL_LIST numeral_list, char* numeral);

T_SORT_LIST smt2_sort_list_new(void);
T_SORT_LIST smt2_sort_list_one(T_SORT sort);
T_SORT_LIST smt2_sort_list_add(T_SORT_LIST sort_list, T_SORT sort);

T_SYMBOL_LIST smt2_symbol_list_new(void);
T_SYMBOL_LIST smt2_symbol_list_add(T_SYMBOL_LIST symbol_list, char* symbol);

T_TERM_LIST smt2_term_list_one(T_TERM term);
T_TERM_LIST smt2_term_list_add(T_TERM_LIST term_list, T_TERM term);

T_STACK_VAR smt2_stack_var_new(void);
T_STACK_VAR smt2_stack_var_one(T_VAR var);
T_STACK_VAR smt2_stack_var_add(T_STACK_VAR stack_var, T_VAR var);

/**
   \brief flex-generated scanner routines used in bison-generated parser */
extern int yy2lex(void);
extern int yy2lex_destroy(void);

#endif
