#ifndef __INST_INDEX_H
#define __INST_INDEX_H

/*
  --------------------------------------------------------------
  Auxiliary function for computing instances
  --------------------------------------------------------------
*/

#include "bool/literal.h"
#include "instantiation/unify.h"
#include "symbolic/DAG-prop.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/DAG.h"

/*
  --------------------------------------------------------------
  Data structures for ground model indexes
  --------------------------------------------------------------
*/

/**
   \brief for a given function symbol stores every signature in the currently
   asserted literals; the terms occurring in asserted disequalities are also
   stored */
typedef struct Findex
{
	Tsymb symbol; /*< function symbol */
	Tstack_DAG signatures;
	Tstack_DAG diseq_terms; /*< useful for Ematching; may have terms not in SIG */
} Findex;

/**
   \brief for a given predicate symbol stores all currently negatively ([0]) and
   positively ([1]) asserted occurences of its atoms */
typedef struct Pindex
{
	Tsymb symbol; /*< predicate symbol */
	Tstack_DAG signatures[2];
} Pindex;

/*
  --------------------------------------------------------------
  Interface for setting/unsetting indexes
  --------------------------------------------------------------
*/

/**
   \brief initializes module */
extern void inst_index_init(void);

/**
   \brief finishes module */
extern void inst_index_done(void);

/**
   \brief indexes the whole signature table from CC
   \remark sets the global symbol index table used throughout the instantiation
   module */
extern void set_SIG_index(void);

/**
   \brief indexes terms and predicate applications from prime model
   \param filter whether literals should be filtered
   \remark if filter is on, literals from instantiations which happend
   downstream are not considered
   \remark sets the global symbol index table used throughout the instantiation
   module */
extern void set_SAT_index(unsigned delete_lvl);

/**
   \brief frees model built for ground model */
extern void unset_model_index(void);

extern void set_SAT_lit_index(unsigned delete_lvl);
extern void unset_model_lit_index(void);

/*
  --------------------------------------------------------------
  Handling ground indexes
  --------------------------------------------------------------
*/

/**
   \brief retrieves term index of given function symbol
   \param symbol the function symbol
   \param f_index a pointer to the term index to be retrieved
   \return true if there is a term index computed for symbol, false otherwise */
extern bool get_Findex(Tsymb symbol, Findex* f_index);

/**
   \brief retrieves predicate index of given predicate symbol
   \param symbol the predicate symbol
   \param p_index a pointer to the predicate index to be retrieved
   \return true if there is a predicate index computed for symbol, false
   otherwise */
extern bool get_Pindex(Tsymb symbol, Pindex* p_index);

/**
   \brief collecs all terms in array with the same class of DAG
   \param terms the array of terms
   \param DAG the term whose class will be searched for
   \return all terms with same class, if any
   \remark assumes that array is sorted by class */
extern Tstack_DAG find_class_terms(Tstack_DAG terms, TDAG DAG);

/**
   \brief collects, from all classes the class of DAG is disequal to, the
   respective terms in the index
   \param index the term index
   \param DAG the term
   \return all terms from the disequal classes, if any
   \remark the classes to have terms retrieved from are filtered by the index
   symbol (if they are in the bitmask) */
extern Tstack_DAG find_class_terms_diseq(Findex index, TDAG DAG);

/*
  --------------------------------------------------------------
  Sorts indexes
  --------------------------------------------------------------
*/

/**
   \brief retrieve terms with minimal depth in given sort's class
   \param sort a sort
   \return a non-empty set of terms, if any, otherwise NULL */
extern Tstack_DAG get_sort_terms_shallow(Tsort sort);

/**
   \brief retrieve all terms in given sort's class
   \param sort a sort
   \return a non-empty set of terms, if any, otherwise NULL */
extern Tstack_DAG get_sort_terms(Tsort sort);

extern void set_sorts_index(unsigned delete_lvl);
extern void unset_sorts_index(void);

/*
  --------------------------------------------------------------
  Instances deletion machinery
  --------------------------------------------------------------
*/

extern unsigned get_var_lvl(Tvar var);

/**
   \brief sets the instantiation level of a literal to last
   \param lit a literal
   \remark should be called *after* inst lvl was updated */
extern void set_var_lvl(Tvar var);
extern void set_var_lvl_arg(Tvar var, unsigned lvl);

/**
   \brief sets the instantiation level of a literal to 0
   \param lit a literal
   \remark called on literals which were part of a conflict */
extern void promote_var_lvl(Tvar var);
/**
   \brief retrieve current last instantiation level
   \return value of last instantiation level
   \remark only used for debugging?? */
extern unsigned get_last_lvl(void);

extern unsigned get_deepest_lvl(void);

/**
   \brief increments last instantiation level if below deepest
   \return true if there is a deeper level to try, false otherwise
   \remark invoked by the instantiation module when no instances were found at
   current level */
extern bool update_lvl_next(void);

/**
   \brief increments last instantiation level
   \remark invoked by the instantiation module when instances were found */
extern void update_lvl_up(void);

#endif
