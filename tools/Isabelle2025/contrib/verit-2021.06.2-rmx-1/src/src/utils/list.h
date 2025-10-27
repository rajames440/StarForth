/**
   \file list.h

   \brief The module provides generic circular doubly-linked list.
*/

#ifndef __LIST_H
#define __LIST_H

#include "utils/types.h"

/* PF empty list is NULL pointer.
   internal structure is circular.
   To visit every element in a list, just take cdr
   until getting original pointer */

typedef struct TSlist
{
	void* P;
	struct TSlist* next;
	struct TSlist* prev;
} * Tlist; /**< The type for circular lists */

#define LIST_LOOP_BEGIN(LIST, TYPE, VAR) \
	{ \
		Tlist LIST##__ptr = LIST; \
		if (LIST) do { \
				TYPE VAR = (TYPE)list_car(LIST##__ptr);

#define LIST_LOOP_END(LIST) \
	LIST##__ptr = list_cdr(LIST##__ptr); \
	} \
	while (LIST##__ptr != LIST) \
		; \
	}

#define LIST_LOOP_BEGIN_DAG(LIST, VAR) \
	{ \
		Tlist LIST##__ptr = LIST; \
		if (LIST) do { \
				TDAG VAR = ((TDAG)(uintptr_t)list_car(LIST##__ptr));

#define LIST_LOOP_END_DAG(LIST) \
	LIST##__ptr = list_cdr(LIST##__ptr); \
	} \
	while (LIST##__ptr != LIST) \
		; \
	}

#define LIST_REVLOOP_BEGIN(LIST, TYPE, VAR) \
	{ \
		Tlist LIST##__ptr = LIST; \
		if (LIST##__ptr) do { \
				TYPE VAR; \
				LIST##__ptr = list_prev(LIST##__ptr); \
				VAR = (TYPE)list_car(LIST##__ptr);

#define LIST_REVLOOP_END(LIST) \
	} \
	while (LIST##__ptr != LIST) \
		; \
	}

#define LIST_REVLOOP_BEGIN_DAG(LIST, VAR) \
	{ \
		Tlist LIST##__ptr = LIST; \
		if (LIST##__ptr) do { \
				TDAG VAR; \
				LIST##__ptr = list_prev(LIST##__ptr); \
				VAR = ((TDAG)(uintptr_t)list_car(LIST##__ptr));

#define LIST_REVLOOP_END_DAG(LIST) \
	} \
	while (LIST##__ptr != LIST) \
		; \
	}

/**
   \brief Constructor for lists with arguments in it.
   \param P a pointer, followed by any number of pointers
   \remarks Last argument should be NULL
   \return A list containing the pointers given as arguments,
   in the same order. */
Tlist list_new_args(void* P, ...);

/**
   \brief List destructor
   \param Plist address of the destructed list
   \post  Plist points to NULL */
void list_free(Tlist* Plist);

/**
   \brief Accessor to first element
   \param list a list
   \pre list is not NULL
   \return the first element of list */
#define list_car(list) (((Tlist)(list))->P)

/**
   \brief Accessor to last element
   \param list a list
   \pre list is not NULL
   \return the last element of list */
#define list_last(list) (((Tlist)(list))->prev->P)

/**
   \brief Returns list rotated one element left (tail to head)
   \param list a list
   \pre list is not NULL
   \return  a list such that i-th elmt is i+1-th elmt of argument
   and the last element is the first element of list.
   \remarks The result can be viewed as the tail of list
   \warning The result and list share the same nodes.
   \warning The resulting list has the same length as the argument
   list.
   \remarks Constant time */
#define list_cdr(list) (((Tlist)(list))->next)

/**
   \brief Returns list rotated one element right (head to tail)
   \param list a list
   \pre list is not NULL
   \return  a list such that i-th elmt is i-1-th elmt of argument
   and the first element is the last element of list.
   \warning The result and list share the same nodes.
   \warning The resulting list has the same length as the argument
   list.
   \remarks O(1) time */
#define list_prev(list) (((Tlist)(list))->prev)

/**
   \brief Length of list
   \param list a list
   \return the number of elements in list
   \remarks Linear time. */
unsigned list_length(Tlist list);

/* TODO: see if one can get rid of list_cpy for list_cpy2, and rename */
/**
   \brief List copy
   \param list a list
   \return a copy of list, the order is not preserved
   \remark Linear time */
Tlist list_cpy(Tlist list);

/**
   \brief List copy
   \param list a list
   \return a copy of list, the order is preserved
   \remark Linear time */
Tlist list_cpy2(Tlist list);

/**
   \brief Creates a list with a single element
   \param P a pointer
   \return List containing P
   \remarks Constant time */
Tlist list_one(void* P);

/**
   \brief Adds element to head of list
   \param P a pointer
   \param list a list
   \return List with head containing P and tail equal to list.
   \remarks The argument of the list is modified in place and
   returned.
   \remarks Constant time */
Tlist list_cons(void* P, Tlist list);

/**
   \brief Adds element to tail of list
   \param P a pointer
   \param list a list
   \return List with elements of argument followed by P.
   \remarks The argument list is modified in place and returned.
   \remarks Constant time */
Tlist list_add(Tlist list, void* P);

/**
   \brief Replace by P first element of list.
   \param list a list
   \param P a pointer
   \return List with elements of argument list, but P instead
   of first element.
   \remarks The argument is modified in place and returned.
   \remarks Constant time */
Tlist list_set_car(Tlist list, void* P);

/**
   \brief accessor based on position
   \param list a list
   \param n a position
   \return element at position n of list if it exists, NULL otherwise
   \remark O(n) */
void* list_n(Tlist list, int n);

/**
   \brief removes head of list
   \param list a list
   \pre list is not NULL
   \return new head of list or NULL
   \remarks O(1)
   \remarks side effect on list */
Tlist list_remove(Tlist list);

/**
   \brief removes last of list
   \param list a list
   \pre list is not NULL
   \return new head of list or NULL
   \remarks O(1)
   \remarks side effect on list */
Tlist list_pop_back(Tlist list);

/**
   \brief inverts list
   \param list a list
   \pre list is not NULL
   \return new head of list or NULL
   \remarks O(length(list))
   \remarks side effect on list */
Tlist list_rev(Tlist list);

/**
   \brief merges two lists
   \param list1 a list
   \param list2 a list
   \return new head of list
   \remarks list2 is added at the end of list1
   \remarks O(1)
   \remarks side effect: destructive for list1 and list2, used to form result */
Tlist list_merge(Tlist list1, Tlist list2);

/**
   \brief reverts the effect of a merge
   \param list1 a list
   \param list2 a list
   \pre list1 was attributed the result of list_merge(list1, list2)
   \return head of list1
   \remarks the effect of the call list_merge(list1, list2) is reverted
   \remarks O(1)
   \remarks side effect: destructive for list */
Tlist list_anti_merge(Tlist list1, Tlist list2);

/**
   \brief Debugging function
   \param list a list
   \return 1 if list is consistent (i.e. correctly doubly-linked), 0
   otherwise. */
int list_consistency(Tlist list);

/**
   \brief Applies a function to all elements of a list
   \param list a list
   \param f a function
   \post f has been applied to each element of list, in order.
   \remarks Lin time % length of list (assuming cost of calling f is
   constant). */
void list_apply(Tlist list, TFapply f);

/**
   \brief Filters element in a list
   \param list a list
   \param pred a test function
   \return returns a new list of all elements satisfying pred
   \remarks Lin time % length of list (assuming cost of calling pred is
   constant).
   \remarks non-destructive (assuming pred is side-effect free) */
Tlist list_filter(Tlist list, TFtest pred);

/**
   \brief Filters a list
   \param list a list
   \param pred a test function
   \return removes elements from list satisfying pred
   \remarks Lin time % length of list (assuming cost of calling pred is
   constant).
   \remarks destructive */
Tlist list_remove_all(Tlist list, TFtest pred);

/**
   \brief checks if a list is a permutation of another
   \param list1 a list
   \param list2 another list
   \return 1 iff both lists are the same
   \remarks Lin time % length of list
   \remarks non destructive */
int list_same(Tlist list1, Tlist list2);

/** \brief initialize the module */
void list_init(void);

/** \brief release the module */
void list_done(void);

#endif /* __LIST_H */
