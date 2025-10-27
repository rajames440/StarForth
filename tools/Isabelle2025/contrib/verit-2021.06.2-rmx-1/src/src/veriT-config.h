/*
  --------------------------------------------------------------
  Configuration files
  --------------------------------------------------------------
*/

#ifndef __VERIT_CONFIG_H
#define __VERIT_CONFIG_H

#include "config.h"

#define HASH_ADAPTIVE_SIZE
/* PF double the size of the hash table when
   size < (nb << HASH_ADAPTIVE_RATIO) */
enum
{
	HASH_ADAPTIVE_RATIO = 2
};

/* #define DEBUG_PROOF */

/* DD+PF This is automatically set using make debug.
   No need to set it here */
/* #define DEBUG */

/* PF Outputs a cnf file for the Boolean abstraction of
   the input formula, conflict clauses, and lemmas with the
   check_deduced option.
   Should be sat iff the input is
   Options only compiled if DEBUG is set.
   Better leave it defined
   Incompatible with proof production */
#define DEBUG_RECHECK_UNSAT

/* option to filter literals according to polarity for theories */
#define POLARITY_FILTER MAGIC PARAM

/* options for (SMT) parser */
/* #define DEBUG_PARSER */

/* options for HOL treatment (lambda, apply, etc.) */
/* #define DEBUG_HOL */

/* options for Nelson & Oppen */
/* #define DEBUG_NO */
/* #define DEBUG_CONFLICT_CLAUSE */

/* options for Boolean module */
/* #define DEBUG_BOOL */
/* #define BOOL_STAT */

/* options for conjunctive normal form module */
/* #define DEBUG_CNF */
/* #define STATS_CNF */

/* options for congruence closure */
/* #define DEBUG_CC */

/* options for DAGs */
/* PF supplementary tests for DAG */
/* #define DAG_CHECK */
/* #define DEBUG_DAG */
/* PF gives some statistics about hashing within this modules */
/* #define DAG_STATS */

/* options for quantifier handling */
/* #define DEBUG_QNT_TIDY */

/* options for skolemization */
/* #define DEBUG_SKOLEM */

/* options for arith module */
/* #define DEBUG_ARITH */

/* DC Ignores the logic of the formula and forces the use of linear arithmetic module */
/* #define LINEAR_ARITHMETIC */

/* DC Ignores the logic of the formula and forces the use of difference logic module */
/* #define DIFFERENCE_LOGIC */

/* options for linear arith module */
/* #define DEBUG_LA */

/* options for difference logic module */
/* #define DEBUG_DL */
/* #define DEBUG_DL_DATA_STATE */

/* option for testing arithmetic only theory, i.e., it will not use NO */
/* #define PURE_ARITH */

/* options for E prover module */
/* #define DEBUG_E */
/* #define DEBUG_TSTP_PARSER */
/* #define DEBUG_TSTPFUN */

/* options for the Herbrand module */
/* #define DEBUG_HERBRAND */

/* PS options for Tom */
/* #define DEBUG_TOM */

/* PS+PF options for unit_simplification */
/* #define DEBUG_US */

/* PF STATS_LEVEL may be 0, 1, or 2
   0 No statistics at all
   1 Only statistics that do not consume computer time
   2 All statistics */
enum
{
	STATS_LEVEL = 1
};

#endif /* __VERIT_CONFIG_H */
