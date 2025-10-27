#ifndef DAG_SORT_H
#define DAG_SORT_H

#include "utils/stack.h"
#include "utils/types.h"

#include <stdarg.h>

/**
   \brief Type for sorts of symbols and DAGs

   Facilities for disjoint sorts.
   A sort is either
   - scalar : This is just a name.
   - functional : sort_1 x ... sort_{n-1} -> sort_{n}, with n > 1.
   - n-ary : sort_1 x ... (arbitrary number of times) x sort_1 -> sort_2
   arity is respectively 0, n, DAG_SORT_NARY in those cases.

   Notice that arity (for a functional sort) is not the number of arguments of
   the function having such a sort, but the number of arguments + 1 since the
   domain is also taken into account.

   Compound sorts are "non-scalar" sorts (e.g. functional or n-ary).

   There may be aliases (i.e. names) to sorts (that is, even the compound ones).
   DAG_sort_new(_args) will not fail when declaring the same sort twice,
   i.e. it fails only when declaring different sorts with the same name.

   Boolean sort being just as any other sort in SMT-LIB since quite some time,
   predicates are thus a special kind of function.

   Parametric sorts depend of other sorts.  Canonical examples are List or Array.
   Its arity is always non-zero, and it should have no subsorts (it can be applied
   to other sorts to provide an instance of the parametric sort)

   Instance sorts are instance of parametric sorts, e.g. List(Real), Array(Int,U).
   Parametric and Instance are mutually exclusive.

   Variable sorts are (polymorphic) scalar placeholder sorts.

   Parametric sorts cannot be polymorphic, but instance and function sorts can.

                   scalar function parametric instance variable 
   polymorphic          0        ?          0        ?        1
   parametric           0        0          1        0        0
   instance             0        0          0        1        0
   variable             0        0          0        0        1

   Predefined sorts are defined by the logic and do not have to be defined when
   outputing SMT-LIB formulas.  It is only used for DAG printing.

   \todo TODO: make sure functions, and quantifiers are ok with that sort  */

/*
  --------------------------------------------------------------
  Data type, values and global variables
  --------------------------------------------------------------
*/

/**
   \brief type to identify sorts
   \note a Tsort value is an index to a global table that stores the
   attributes of each sort */
typedef unsigned Tsort;

#define DAG_SORT_NULL ((Tsort)0)

#define DAG_SORT_NARY ((unsigned)-1)

/**
   \brief register type for sort attributes */
typedef struct TSsort
{
	char* name; /*< sort name; optional (may be NULL), must be unique */
	unsigned arity; /*< number of arguments, (compound, polymorphic sorts),
	                    DAG_SORT_NARY for a polyadic/n-ary sort */
#ifndef PEDANTIC
	bool mark : 1; /*< marker for user-defined processing */
	bool predefined : 1; /*< predefined sorts */
	bool variable : 1; /*< sort variable */
	bool instance : 1; /*< instances of sort constructors */
	bool parametric : 1; /*< parametric sort constructors */
	bool polymorphic : 1; /*< polymorphic sorts */
#else
	bool mark; /*< marker for user-defined processing */
	bool predefined; /*< predefined sort */
	bool variable; /*< sort variable */
	bool instance; /*< instance of sort constructors */
	bool parametric; /*< parametric sort constructors */
	bool polymorphic; /*< polymorphic sorts */
#endif
	Tsort* sub; /*< array of sub-sorts (compound, polymorphic sorts) */
	/* PFTODO: binding is dirty.  Where is it used?  Is there a better way ? */
	Tsort binding; /*< place-holder to attach pointer values */
} TSsort;

/** \brief type for the table storing sort attributes */
TSstack(_Ssort, TSsort);

/** \brief global table storing sort attributes */
extern Tstack_Ssort DAG_sort_stack;

/** data type for stacks of Tsort values */
TSstack(_sort, Tsort);

/** short cut to access contents of global table DAG_sort_stack */

#define __DAG_SORT_DATA(sort) (DAG_sort_stack->data[(Tsort)(sort)])

/*
  --------------------------------------------------------------
  Casts
  --------------------------------------------------------------
*/

/**
   \brief cast from Tsort to void *
   \param sort */
static inline void*
DAG_ptr_of_sort(const Tsort sort)
{
	return (void*)(uintptr_t)(sort);
}

/**
   \brief cast from void * to Tsort
   \param sort */
static inline Tsort
DAG_sort_of_ptr(void* P)
{
	return (Tsort)(uintptr_t)P;
}

/*
  --------------------------------------------------------------
  Constructors
  --------------------------------------------------------------
*/

/**
   \brief old sort constructor
   \param name pointer to string naming the sort (may be NULL)
   \param n number of sub-sorts in a compound sort; if n is
   DAG_SORT_NARY, then symbol of this sort may have any number of arguments of
   sort sub[0], and returns argument of sort sub[1].
   \param sub array storing sub-sorts in compound sorts
   \return a new sort
   \remark Two sorts are equal if they have the same arity and sub-sorts
   - If an equal sort of the same name as already been created, it is returned
   - If a different sort with the same name has already been created,
   it is an error.
   - If an equal sort with a different, not null, name has already been created,
   it is an error.
   - If an equal sort with a null name has already been created, its name is
   set to the given name and it is returned.
   \remark Destructive for sub
   \remark Created sort is functional. Set functional field to 0 to change */
/* PF TODO: I suspect the above remark about functional is obsolete */
extern Tsort DAG_sort_new(
	const char* const name, const unsigned n, Tsort* const sub);

/**
   \brief Sort constructor
   \param name pointer to string naming the sort (may be NULL)
   \param arity number of sub-sorts in a compound sort; if arity is
   DAG_SORT_NARY, then symbol of this sort may have any number of
   arguments of sort sub[0], and returns argument of sort sub[1].
   \param ... for compound sorts, sub-sorts are given as arguments,
   followed by NULL
   \return a new sort
   \remark just an interface for above function */
extern Tsort DAG_sort_new_args(
	const char* const name, const unsigned arity, ...);

/**
   \brief creates a sort variable with the given name
   \param name
   \pre name must either be NULL or a valid string.
   \return It returns a sort variable:
   - when called with NULL, it generates a fresh sort variable,
   the name of this variable is '_ (single-quote underscore) followed
   by a positive integer.
   - when called with a string, it generates a sort variable of the
   given name.
   \remark If called twice with the same (non-NULL) string, then
   returns the same sort */
extern Tsort DAG_sort_new_var(const char* const name);

/**
   \brief creates a parametric sort constructor
   \param name
   \param arity
   \remark If there is already a constructor of the same name and arity,
   then it is returned.
   \remark If name is NULL, an error is printed and execution halts.
   \remark If there is already a constructor of the same name and
   different arity, an error is printed to stderr and execution halts.
   \remark If arity is 0, then the result is DAG_sort_new_func(name, 0, NULL) */
extern Tsort DAG_sort_new_param(const char* const name, const unsigned arity);

/**
   \brief creates an instance of a parametric sort constructor
   \param name the name of the resulting sort (optional: may be NULL)
   \param sort the parametric sort constructor
   \param n the number of arguments
   \param sub the arguments
   \remark If there is already an instance of the constructor and
   arguments, then it is returned
   \remark Fails if arity != sort->arity */
extern Tsort DAG_sort_new_inst(
	const char* const name, const Tsort sort, const unsigned n, Tsort* const sub);

/*
  --------------------------------------------------------------
  Accessing sorts attributes
  --------------------------------------------------------------
*/

/**
   \brief Gets sort with name
   \param name of the searched sort
   \return the sort named name, or NULL if not found */
extern Tsort DAG_sort_lookup(const char* const name);

/**
   \brief Accesses the name of the sort.
   \param s the sort
   \return a pointer to the name string of the sort, if declared and named,
   NULL otherwise */
static inline char*
DAG_sort_name(const Tsort s)
{
	return __DAG_SORT_DATA(s).name;
}

/**
   \brief Accesses the arity of the sort.
   \param s The sort
   \return the arity of the sort */
static inline unsigned
DAG_sort_arity(const Tsort s)
{
	return __DAG_SORT_DATA(s).arity;
}

/**
   \brief Tests if sort is predefined
   \param s the sort */
static inline bool
DAG_sort_predefined(const Tsort s)
{
	return __DAG_SORT_DATA(s).predefined == 1;
}

/**
   \brief Tests if sort is parametric
   \param s the sort
   \remarks (List 1) is parametric, (List Int) and Int are not */
static inline bool
DAG_sort_parametric(const Tsort s)
{
	return __DAG_SORT_DATA(s).parametric == 1;
}

/**
   \brief Tests if sort is polymorphic
   \param s the sort */
static inline bool
DAG_sort_polymorphic(const Tsort s)
{
	return __DAG_SORT_DATA(s).polymorphic == 1;
}

/**
   \brief Tests if sort is an instance of a parametric sort constructor
   \param s the sort */
static inline bool
DAG_sort_instance(const Tsort s)
{
	return __DAG_SORT_DATA(s).instance == 1;
}

/**
   \brief tests if a sort is a sort variable
   \param s The tested sort */
static inline bool
DAG_sort_variable(const Tsort s)
{
	return __DAG_SORT_DATA(s).variable == 1;
}

/**
   \brief Accesses the i-th sub-sort of the sort.
   \param s The accessed sort.
   \param i the index of the sub-sort
   \note This routine may be used to access the elements of a functional sort */
static inline Tsort
DAG_sort_sub(const Tsort s, const unsigned i)
{
	return __DAG_SORT_DATA(s).sub[i];
}

/**
   \brief Accesses the i-th sub-sort of the sort.
   \param sort The accessed sort.
   \return array of \c sort \c sub-sorts.
   This routine may be used to access the elements of a functional sort */
static inline Tsort*
DAG_sort_subs(const Tsort s)
{
	return __DAG_SORT_DATA(s).sub;
}

/**
   \brief mark, i.e. sets the misc attribute, sort
   \param sort a sort */
static inline void
DAG_sort_mark(const Tsort s)
{
	__DAG_SORT_DATA(s).mark = 1;
}

/**
   \brief unmark, i.e. unsets the misc attribute, sort
   \param s a sort */
static inline void
DAG_sort_unmark(const Tsort s)
{
	__DAG_SORT_DATA(s).mark = 0;
}

/**
   \brief checks if sort is marked
   \param s a sort
   \return the value of the \a misc attribute */
static inline bool
DAG_sort_is_marked(const Tsort s)
{
	return __DAG_SORT_DATA(s).mark == 1;
}

/**
   \brief Sets a sort as predefined */
static inline void
DAG_sort_set_predefined(const Tsort s)
{
	__DAG_SORT_DATA(s).predefined = 1;
}

/*
  --------------------------------------------------------------
  Module initialization and release
  --------------------------------------------------------------
*/

extern void DAG_sort_init(void);
extern void DAG_sort_done(void);

#ifdef DEBUG
extern void DAG_sort_table_print(void);
extern int DAG_sort_invariant(Tsort sort);
#endif

/*
  --------------------------------------------------------------
  PF Dirty things we'd better get rid of
  --------------------------------------------------------------
*/

/**
   \brief returns a sort bound to sort
   \param sort a sort */
static inline Tsort
DAG_sort_binding(const Tsort s)
{
	return __DAG_SORT_DATA(s).binding;
}

/**
   \brief binds a sort to another sort
   \param s a sort
   \param s2 another sort
   \pre no sort is already binded to sort s */
static inline void
DAG_sort_bind(const Tsort s, Tsort s2)
{
	assert(DAG_sort_binding(s) == DAG_SORT_NULL);
	__DAG_SORT_DATA(s).binding = s2;
}

/**
   \brief unbinds pointer from sort
   \param s a sort */
static inline void
DAG_sort_unbind(const Tsort s)
{
	__DAG_SORT_DATA(s).binding = DAG_SORT_NULL;
}

/**
   \brief recursively unbinds sort and sub-sorts
   \param s a sort */
extern void DAG_sort_unbind_rec(const Tsort s);

#endif /* DAG_SORT_H */
