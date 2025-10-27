/**
  \file DAG-tmp.h
  \brief Datastructure to associate temporary data to DAGs

  This file provides a datastructure and related methods to associate
  temporary data to specific DAGs. The storage is allocated globally
  and resized as needed. To avoid conflicting usage in different parts
  of the code the `DAG_tmp_reserve` and `DAG_tmp_release` functions
  should be used (note however that these only perform actual checks
  in debug mode). Furthermore, users are required to reset the storage
  before calling `DAG_tmp_release`. Note that the helper functions for
  different types are backed by the same storage array.

  The following is a trivial example on how to use this:

      void 
      example(TDAG DAG) 
      {
        DAG_tmp_reserve();
        DAG_tmp_bool[DAG] = 1;
        DAG_tmp_reset_bool(DAG);
        DAG_tmp_release();
      }
*/

#ifndef DAG_TMP_H
#define DAG_TMP_H

#include "symbolic/DAG.h"

/*
  --------------------------------------------------------------
  DAG_tmp functions
  --------------------------------------------------------------
*/

extern char* DAG_tmp;

/** \brief Used to associate Booleans to DAGs */
#define DAG_tmp_bool ((unsigned char*)DAG_tmp)
/** \brief Used to associate unsigned integers to DAGs */
#define DAG_tmp_unsigned ((unsigned*)DAG_tmp)
/** \brief Used to associate integers to DAGs */
#define DAG_tmp_int ((int*)DAG_tmp)
/** \brief Used to associate DAGs to DAGs */
#define DAG_tmp_DAG ((TDAG*)DAG_tmp)
/** \brief Used to associate DAG stacks to DAGs */
#define DAG_tmp_stack_DAG ((Tstack_DAG*)DAG_tmp)

#ifdef DEBUG
extern void DAG_tmp_reserve(void);
extern void DAG_tmp_release(void);
#else
/**
 	\brief Reserves the temporary storage.

  The function fails with an assertion if the temporary
  storage was not released with `DAG_tmp_release` before.

  When compiled without `DEBUG` the function does nothing.
 */
static inline void
DAG_tmp_reserve(void)
{}

/**
 	\brief Releases the temporary storage.

  The function fails with an assertion if the temporary
  storage was not reserved with `DAG_tmp_release` before.
  Furthermore, it checks if all associated data was reset
  before calling `DAG_tmp_release`.

  When compiled without `DEBUG` the function does nothing.
 */
static inline void
DAG_tmp_release(void)
{}
#endif

/** 
  \brief Recursively sets the Boolean associated with a DAG
    to false.
  \param DAG The DAG.

  Sets `DAG_tmp_bool[DAG]` to false and recurses into the
  arguments of `DAG`.
 */
extern void DAG_tmp_reset_bool(TDAG DAG);

/** 
  \brief Recursively sets the unsigned integer associated with a DAG
    to 0.
  \param DAG The DAG.

  Sets `DAG_tmp_unsigned[DAG]` to 0 and recurses into the
  arguments of `DAG`.
 */
extern void DAG_tmp_reset_unsigned(TDAG DAG);

/** 
  \brief Recursively sets the integer associated with a DAG
    to 0.
  \param DAG The DAG.

  Sets `DAG_tmp_int[DAG]` to 0 and recurses into the
  arguments of `DAG`.
 */
extern void DAG_tmp_reset_int(TDAG DAG);

/** 
  \brief Recursively sets the DAG associated with a DAG
    to `DAG_NULL`.
  \param DAG The DAG.

  Sets `DAG_tmp_DAG[DAG]` to `DAG_NULL` and recurses into the
  arguments of `DAG`.
 */
extern void DAG_tmp_reset_DAG(TDAG DAG);

/** 
  \brief Recursively frees the DAG stack associated with a DAG.
  \param DAG The DAG.

  Calls `stack_free` on `DAG_tmp_stack_DAG[DAG]` and recurses into the
  arguments of `DAG`.
 */
extern void DAG_tmp_reset_stack_DAG(TDAG DAG);

/** 
  \brief Recursively frees the DAG associated with a DAG and sets
    the saved DAG to `DAG_NULL`.
  \param DAG The DAG.

  First calls `DAG_free` on `DAG_tmp_DAG[DAG]` and then sets
  `DAG_tmp_DAG[DAG]` to `DAG_NULL`. Recurses into the
  arguments of `DAG`.
 */
extern void DAG_tmp_free_DAG(TDAG DAG);

/*
  --------------------------------------------------------------
  Initialization-release functions
  --------------------------------------------------------------
*/

/**
  \brief Initializes the module.
*/
extern void DAG_tmp_init(void);

/**
  \brief Releases the module.

  Currently does nothing.
*/
extern void DAG_tmp_done(void);

#ifdef DEBUG
/**
  \brief First prints all DAGS for which is `DAG_tmp_bool` is true and
    than prints all DAGs for which it is false.
*/
extern void DAG_tmp_bool_debug(void);

/**
  \brief Prints all DAGs not mapped to DAG_NULL
*/
extern void DAG_tmp_DAG_debug(void);
#endif

#endif
