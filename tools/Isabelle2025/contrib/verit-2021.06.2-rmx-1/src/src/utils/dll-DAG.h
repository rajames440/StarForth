/**
   \file dll-DAG.h
   \brief The module provides circular doubly-linked list (dll) for TDAG */

/* Obtained using
   sed "s/\([^_]\)TYPE/\1TDAG/g;s/EXT/DAG/g;s/\"\"//g" dll-TYPE.tpl > dll-DAG.h */

#ifndef __DLL_DAG_H
#define __DLL_DAG_H

#include "utils/dll.h"

typedef Tdll Tdll_DAG;

#define DLL_DAG_NULL 0

/**
   \brief Constructor for dll with arguments in it.
   \param data a data of TDAG, followed by any number of TDAG datas
   \remarks Last argument should be 0 (TDAG)
   \return A dll containing the datas given as arguments, in the same order */
#ifndef PEDANTIC
#define dll_DAG_new_args(data, ...) dll_new_args((GEN_TYPE)data, __VA_ARGS__)
#endif

/**
   \brief dll destructor
   \param Pdll address of the destructed list
   \post  Pdll is DLL_NULL */
#define dll_DAG_free dll_free

/**
   \brief Accessor to first element
   \param A a dll
   \pre A is not DLL_NULL
   \return the first element of dll */
#define dll_DAG_car(A) ((TDAG)dll_car(A))

/**
   \brief Accessor to last element
   \param A a dll
   \pre A is not DLL_NULL
   \return the last element of dll */
#define dll_DAG_last(A) ((TDAG)dll_last(dll))

/**
   \brief Returns dll rotated one element left (tail to head)
   \return  a dll such that i-th elmt is i+1-th elmt of argument and
   the last element is the first element of dll
   \remarks The result can be viewed as the tail of dll
   \warning The result and dll share the same nodes
   \warning The resulting dll has the same length as the argument dll
   \remarks Constant time */
#define dll_DAG_cdr dll_cdr

/**
   \brief Returns dll rotated one element right (head to tail)
   \return  a dll such that i-th elmt is i-1-th elmt of argument and
   the first element is the last element of dll
   \warning The result and dll share the same nodes
   \warning The resulting dll has the same length as the argument dll
   \remarks Constant time */
#define dll_DAG_prev dll_prev

/**
   \brief Length of dll
   \return the number of elements in dll
   \remarks Linear time. */
#define dll_DAG_length dll_length

/**
   \brief dll copy
   \return a copy of dll, the order is preserved
   \remark Linear time */
#define dll_DAG_cpy dll_cpy

/**
   \brief Creates a dll with a single element
   \param A a data
   \return dll containing single element data
   \remarks Constant time */
#define dll_DAG_one(A) dll_one((GEN_TYPE)data)

/**
   \brief Adds element to head of dll
   \param data a data
   \param dll a dll
   \return dll with head containing data and tail equal to dll.
   \remarks Constant time
   \remarks destructive for dll */
#define dll_DAG_cons(data, dll) dll_cons((GEN_TYPE)data, dll)

/**
   \brief Adds element to tail of dll
   \param data a data
   \param dll a dll
   \return dll with elements of argument followed by data
   \remarks Constant time
   \remarks destructive for dll */
#define dll_DAG_add(dll, data) dll_add(dll, (GEN_TYPE)data)

/**
   \brief Replace by P first element of dll.
   \param dll a dll
   \param data a data
   \return dll with elements of argument dll, but data instead of first element
   \remarks The argument is modified in place and returned
   \remarks Constant time
   \remarks destructive for dll */
#define dll_set_car(dll, data) dll_set_car(dll, (GEN_TYPE)data)

/**
   \brief accessor based on position
   \param dll a dll
   \param n a position
   \return element at position n of dll if it exists, 0 otherwise
   \remark O(n) */
#define dll_DAG_n(dll, n) ((TDAG)dll_n(dll, n))

/**
   \brief removes head element of dll
   \pre dll is not DLL_NULL
   \return new head of dll or DLL_NULL
   \remarks Constant type
   \remarks destructive for dll */
#define dll_DAG_remove dll_remove

/**
   \brief removes last element of dll
   \pre dll is not DLL_NULL
   \return new head of dll or DLL_NULL
   \remarks Constant time
   \remarks destructive for dll */
#define dll_DAG_pop_back dll_pop_back

/**
   \brief inverts dll
   \pre dll is not DLL_NULL
   \return new head of dll or DLL_NULL
   \remarks O(length(dll))
   \remarks destructive for dll */
#define dll_DAG_rev dll_rev

/**
   \brief merges two dlls
   \return new head of dll 
   \remarks dll2 is added at the end of dll1
   \remarks Constant time
   \remarks destructive for dll1 and dll2, used to form result */
#define dll_DAG_merge dll_merge

/**
   \brief reverts the effect of a merge
   \pre dll was attributed the result of dll_merge(dll1, dll2)
   \return head of dll1
   \remarks the effect of the call dll_merge(dll1, dll2) is reverted
   \remarks Constant time
   \remarks destructive for dll */
#define dll_DAG_anti_merge dll_anti_merge

/**
   \brief Debugging function
   \return 1 if dll is consistent (i.e. correctly doubly-linked), 0
   otherwise. */
#define dll_DAG_consistency dll_consistency

/**
   \brief Applies a function to all elements of a dll
   \param dll a dll
   \param f a function
   \post f has been applied to each element of dll, in order
   \remarks Lin time % length of dll (assuming cost of calling f is constant) */
#define dll_DAG_apply(dll, f) (dll, (TFapply)f)

/**
   \brief Filters element in a dll
   \param dll a dll
   \param pred a test function
   \return returns a new dll of all elements satisfying pred
   \remarks Lin time % length of dll (assuming cost of calling pred is
   constant)
   \remarks non-destructive (assuming pred is side-effect free) */
#define dll_DAG_filter(dll, pred) dll_filter(dll, (TFtest)pred)

/**
   \brief Filters a dll
   \param dll a dll
   \param pred a test function
   \return removes elements from dll satisfying pred
   \remarks Lin time % length of dll (assuming cost of calling pred is
   constant).
   \remarks destructive */
#define dll_DAG_remove_all(dll, pred) dll_remove_all(dll, (TFtest)pred)

/**
   \brief checks if a dll is a permutation of another
   \return 1 iff both dlls are the same
   \remarks Lin time % length of dll 
   \remarks non destructive */
#define dll_DAG_same dll_same

#endif /* __DLL_H */
