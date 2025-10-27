/*
  --------------------------------------------------------------
  Module for printing formulas and terms
  --------------------------------------------------------------
*/

#ifndef DAG_PRINT_H
#define DAG_PRINT_H

#include "symbolic/veriT-DAG.h"

#include <stdio.h>

typedef struct TDAG_fprint_opt
{
	bool newlines; /**< (true/false) break lines */
	unsigned columns; /**< number of columns for printing */
	unsigned column_to_indent; /**< number of columns of indentation */
	bool flat; /**< (true/false) flatten printing
                                 (for terms used multiple times) */
} TDAG_fprint_opt;
extern TDAG_fprint_opt DAG_fprint_opt;

/**
  \brief Updateds the symbol prefix used for rectification such
    that it is not a prefix of any symbols appearing in `src`.
  \param src The recursively traversed DAG to update the prefix.
  \see DAG_symb_name_rectify
	*/
void update_symbol_prefix_DAG(TDAG src);

/**
   \param str the name of a symbol
   \returns Returns the the string represtentation of the parameter.
    If the first character of this string is '@' it is replaced by
    a globally stored prefix. This prefix can be updated with
    `update_symbol_prefix`. Returns a pointer to a globale storage
    of size SYMB_SIZE_LIMIT. */
char* DAG_symb_name_rectify(Tsymb symb);

/**
   \brief prints the syntactic tree, in lisp-notation (for debugging purposes), into string
   \param file the string
   \param DAG the formula to print */
void DAG_sprint(char* file, TDAG DAG);

/**
   \brief prints the syntactic tree, in lisp-notation (for debugging purposes)
   \param file the output file
   \param DAG the formula to output */
void DAG_fprint(FILE* file, TDAG DAG);

/**
   \brief prints the syntactic tree, in lisp-notation (for debugging purposes) on stdout
   \param DAG the formula to output */
void DAG_print(TDAG DAG);

/**
   \brief prints a sort to stdout (useful for debugging)
   \param sort the sort to output */
void DAG_sort_print(Tsort sort);

/** TODO: document */
void DAG_sort_fprint(FILE* file, Tsort sort);

/**
   \brief prints the syntactic tree, in lisp-notation (for debugging purposes) on stdout, annotated with sort information
   \param DAG the formula to output */
void DAG_print_sort(TDAG DAG);

/** Helper functions for debugging */
void print_PDAG(TDAG* PDAG, unsigned arity);
void print_Tstack_DAG(Tstack_DAG stack);
void print_Tstack_DAGstack(Tstack_DAGstack stack);

/**
   \brief prints formula in Isabelle language (deprecated)
   \param file the output file
   \param DAG the formula to output */
void DAG_isa_fprint(FILE* file, TDAG DAG);

/**
   \brief prints formula in SMT-LIB 2 notation, as a full benchmark file
   \param file the output file
   \param DAG the formula to output
   \param status the status of the formula */
void DAG_fprint_smt2_bench(FILE* file, TDAG DAG, char* status);

/**
   \brief prints a set of formulas in SMT-LIB 1 notation, as a full benchmark file
   \param file the output file
   \param PDAG address of an array of DAGs to output
   \param n the number of DAGs to output
   \param status the status of the formula */
void DAG_fprint_smt2_set(FILE* file, TDAG* PDAG, unsigned n, char* status);

void DAG_fprint_smt2_set_gr_DAG(
	FILE* file, TDAG* DAG, unsigned n, TDAG CI, char* status);

/**
   \brief prints formula in B notation, as a full machine file
   \param file the output file
   \param DAG the formula to output
   \note only for benchmarks with Bool and Int as sole sorts */
void DAG_fprint_b(FILE* file, TDAG DAG);

/**
   \brief prints a custom error with a printf-like format to stderr.
   Supports all printf-like formats (except $ and %n), and %D for DAGs
   \param format the format string */
void my_DAG_error(char* format, ...);

/**
   \brief prints a custom message with a printf-like format to stderr.
   \remark Supports all printf-like formats (except $ and %n), and %D for DAGs
   \param format the format string */
void my_DAG_message(char* format, ...);

/**
   \brief prints a custom waring with a printf-like format to stderr.
   \remark supports all printf-like formats (except $ and %n), and %D for DAGs
   \param format the format string */
void my_DAG_warning(char* format, ...);
#endif
