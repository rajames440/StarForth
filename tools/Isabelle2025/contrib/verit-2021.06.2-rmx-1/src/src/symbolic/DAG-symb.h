/*
                                  ***   StarForth   ***

  DAG-symb.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:02.578-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/verit-2021.06.2-rmx-1/src/src/symbolic/DAG-symb.h
 */

#ifndef DAG_SYMB_H
#define DAG_SYMB_H

#include "symbolic/DAG-sort.h"

#include <gmp.h>

#define SYMB_SIZE_LIMIT 256

/*
  --------------------------------------------------------------
  Symbol data structure
  --------------------------------------------------------------
*/

typedef unsigned Tsymb;

#define DAG_SYMB_NULL ((Tsymb)0)

TSstack(_symb, Tsymb); /* typedefs Tstack_symb */

typedef unsigned Tsymb_type;

#define SYMB_INTERPRETED (1u << 0)
#define SYMB_PREDEFINED (1u << 1)

#define SYMB_NAMED (1u << 2)
#define SYMB_NUMBER (1u << 3) /*< rational or int number */
#define SYMB_INTEGER (1u << 4) /*< int number */

#define SYMB_PREDICATE (1u << 5) /*< predicate symbol */

#define SYMB_VARIABLE (1u << 6)
#define SYMB_QUANTIFIER (1u << 7) /*< exists or forall */
#define SYMB_BOOLEAN_CONNECTOR (1u << 8)
#define SYMB_BOOLEAN_CONSTANT (1u << 9)

#define SYMB_MACRO (1u << 10) /* TODO: should remove */

#define SYMB_NULLARY (1u << 11)

/**
   \brief Structure to represent symbols of DAGs */
typedef struct TSsymb
{
	Tsymb_type type; /*< identifies the symbol kind */
	Tsort sort; /*< sort of the symbol */
	unsigned hash_key; /*< result of hash function */
	union
	{
		char* name; /*< symbol name; overloading is possible */
		mpz_t mpz;
		mpq_t mpq;
	} value;
	/* TODO: these should be removed at some point */
	int misc; /*< placeholder for clients */
} TSsymb;

TSstack(_Ssymb, TSsymb);

/** \brief stack of symbols, stored in a single chunk of memory */
extern Tstack_Ssymb DAG_symb_stack;

#define __DAG_SYMB_DATA(symb) (DAG_symb_stack->data[symb])

/**
   \brief Accesses the name of the symbol
   \param symb
   \return The name of symb */
#define DAG_symb_name2(symb) \
	(__DAG_SYMB_DATA(symb).value.name) /* TODO: REMOVE */

/**
   \brief Accesses the multi-precision integer of the symbol
   \param symb
   \return The mpz of symb */
#define DAG_symb_mpz(symb) (__DAG_SYMB_DATA(symb).value.mpz) /* TODO: REMOVE */

/**
   \brief Accesses the multi-precision rational of the symbol
   \param symb
   \return The mpq of symb */
#define DAG_symb_mpq(symb) (__DAG_SYMB_DATA(symb).value.mpq) /* TODO: REMOVE */

/**
   \brief Accesses the kind of the symbol
   \param symb
   \return The kind of symb */
#define DAG_symb_type(symb) (__DAG_SYMB_DATA(symb).type)

/**
   \brief Accesses the sort of the symbol
   \param symb
   \return The sort of symb */
#define DAG_symb_sort(symb) (__DAG_SYMB_DATA(symb).sort)

/**
   \brief Marks symbol as being interpreted.
   \param symb a symbol
   \remark Macros need to be marked as interpreted symbols */
#define DAG_symb_set_interpreted(symb) \
	(__DAG_SYMB_DATA(symb).interpreted = true)

#define DAG_symb_misc(symb) (__DAG_SYMB_DATA(symb).misc)
#define DAG_symb_set_misc(symb, v) (__DAG_SYMB_DATA(symb).misc = (v))
#define DAG_symb_reset_misc(symb) DAG_symb_set_misc(symb, 0)

#define DAG_symb_key(symb) (__DAG_SYMB_DATA(symb).hash_key)

/*
  --------------------------------------------------------------
  Initialisation-release functions
  --------------------------------------------------------------
*/

/**
   \brief Initializes DAG-symb module */
extern void DAG_symb_init(void);
/**
   \brief Closes DAG_symb module */
extern void DAG_symb_done(void);

typedef void (*TDAG_symb_hook_resize)(unsigned old, unsigned new);
typedef void (*TDAG_symb_hook_free)(Tsymb);

/**
   \brief adds a function to be notified of the resizing of the symb stack
   \param hook_resize the function to call at each resize
   \remark new size is given as argument of hook_resize
   \remark if used to allocate side information, hook_resize should be used
   to allocate and initialize this information */
extern void DAG_symb_set_hook_resize(TDAG_symb_hook_resize hook_resize);

/**
   \brief adds a function to be notified of the freeing of a symb
   \param hook_free the function to call at each DAG free
   \remark symb which is free given as argument of hook_free
   \remark use as reinitialization of side information linked with symb */
extern void DAG_symb_set_hook_free(TDAG_symb_hook_free hook_free);

/*
  --------------------------------------------------------------
  Constructors
  --------------------------------------------------------------
*/

/**
   \brief Constructor for a named symbol
   \param name string naming the symbol
   \param type identifies the kind of symbols that needs to be created
   \param sort the symbol sort
   \return returns the declared symbol
   \note Declares a new symbol
   \note name is not freed in the call */
extern Tsymb DAG_symb_new(const char* name, Tsymb_type type, Tsort sort);

/**
   \brief Gets symbol with given name and sort.
   \param name string naming the symbol
   \param sort the symbols sort
   \return A symbol <b>s1</b> of sort <b>sort1</b> is candidate for
   the result if <b>s1</b> subsumes <b>sort</b> and there is no other
   symbol <b>s2</b> of sort <b>sort2</b> such that <b>sort1</b>
   subsumes <b>sort2</b> and <b>sort2</b> subsumes <b>sort</b>.
   Returns NULL if there are 0 or several candidates, and the
   candidate symbol otherwise */
extern Tsymb DAG_symb_lookup_sort(char* name, Tsort sort);

/**
   \brief Specialized constructor
   \param sort sort of the symbol to create
   \return symbol of a fresh skolem symbol of the given sort
   \remark used in the skolem module only */
extern Tsymb DAG_symb_skolem(Tsort sort);

/**
   \brief Specialized constructor
   \param sort sort of the symbol to create
   \return symbol of a fresh constant symbol of the given sort
   \remark used in the rare-symb module only */
extern Tsymb DAG_symb_const(Tsort sort);

/**
   \brief Specialized constructor
   \param sort sort of the symbol to create
   \return symbol of a fresh variable of the given sort */
extern Tsymb DAG_symb_variable(Tsort sort);

/**
   \brief Specialized constructor
   \param sort sort of the symbol to create
   \return symbol of a fresh function symbol of the given sort */
extern Tsymb DAG_symb_function(Tsort sort);

/**
   \brief Specialized constructor
   \param sort sort of the symbol to create
   \return symbol of a fresh predicate of the given sort */
extern Tsymb DAG_symb_predicate(Tsort sort);

/**
   \brief Gets symbol with given name.
   \param name string naming the symbol
   \param nb_arg the number of subterms
   \param Psort the array of nb_arg argument sorts (optional)
   \param sort the symbols sort (optional)
   \return Returns the appropriate Tsymb for name, if declared, or
   DAG_SYMB_NULL if zero or several symbols match.
   \remark PDAG and sort are used for taking the right symbol if name
   is overloaded */
extern Tsymb DAG_symb_lookup(
	char* name, unsigned nb_arg, Tsort* Psort, Tsort sort);

/**
   \brief returns sort of application of symb to arguments of sort
   Psort[0] ...Psort[n-1]
   \param symb the symbol
   \param n the number of arguments
   \param Psort the argument sorts
   \return DAG_SORT_NULL if it cannot be applied */
extern Tsort DAG_symb_check(Tsymb, unsigned, Tsort*);

extern Tsymb DAG_symb_integer(long value);
extern Tsymb DAG_symb_integer_mpz(mpz_t mpz);
extern Tsymb DAG_symb_integer_str(char* value);
extern Tsymb DAG_symb_binary_str(char* value);
extern Tsymb DAG_symb_hex_str(char* value);
extern Tsymb DAG_symb_rational(long num, unsigned long den);
extern Tsymb DAG_symb_rational_mpq(mpq_t mpq);
extern Tsymb DAG_symb_decimal_str(char* value);
extern Tsymb DAG_symb_rational_str(char* value);
extern Tsymb DAG_symb_str(char* value);

/*
  --------------------------------------------------------------
  Function symbols bitmasks
  --------------------------------------------------------------
*/

/** \brief stores the bitmask of each function symbol */
extern unsigned long long int* symb_mask;

/**
   \brief prints the symbol as it should appear in a text file
   \param symb the symbol to print
   \param n the number of characters available in the buffer
   \param str the buffer to print to
   \remark generates an error if does not fit the buffer */
extern void DAG_symb_snprint(Tsymb symb, unsigned n, char* str);

#endif
