#ifndef SMTTYPES_H
#define SMTTYPES_H

#include "symbolic/veriT-DAG.h"
#include "utils/list.h"
#include "utils/stack.h"

typedef struct TSbind
{
	Tsymb symb;
	TDAG DAG;
} * Tbind;

#define T_BIND Tbind
#define T_BIND_LIST Tlist
#define T_FUNCTION_ID Tsymb
#define T_NUMERAL_LIST Tlist
#define T_SORT Tsort
#define T_SORT_LIST Tlist
#define T_VAR Tsymb
#define T_STACK_VAR Tstack_symb
#define T_SYMBOL_LIST Tlist
#define T_TERM TDAG
#define T_TERM_LIST Tlist
#define T_ATTR Tassoc
#define T_ATTR_LIST Tlist

/** \brief extends SMT-LIB2 with macros */
#define T_MACRO struct Tbind*

#endif
