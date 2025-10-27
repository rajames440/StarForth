/**
   \file stack.h
   \brief The module provides generic stacks */

#ifndef __STACK_H
#define __STACK_H

#include <stdbool.h>
#include <string.h>

/* Comments
   - nothing garantees everything will always be handled by macros
     so at some point it may be REQUIRED to have differentiated "functions"
     for different stack types
   - stack_INIT() is really a macro, looks like a macro, is a bit ugly as a call
   - typedefing Tstack_EXT to be a stack of type (Tstring, unsigned, etc...)
     can be done using TSstack(EXT, TYPE)
   - see the end of file for usage example */

/* Do not use these macros for your own usage */
#define CC2(A, B) A##B
/* force prescan */
#define CC(A, B) CC2(A, B)
#define CC3(A, B, C) CC(CC(A, B), C)
#define sizeofdata(stack) sizeof((stack)->data[0])
#ifdef PEDANTIC
#define INSERT_IF_PEDANTIC(A) A
#else
#define INSERT_IF_PEDANTIC(A)
#endif

#include "utils/general.h"
#include "utils/veriT-qsort.h"

#include <assert.h>
#include <stdlib.h>

#include "utils/general.h"

/* A stack of TYPE can be defined as Tstack_EXT using
   TSstack(EXT, TYPE) */
#define TSstack(EXT, TYPE) \
	typedef struct CC(TSstack, EXT) \
	{ \
		unsigned size; /*< number of elements in the stack */ \
		unsigned alloc; /*< number of allocated elements */ \
		TYPE data[]; /*< elements in the stack */ \
	} * CC(Tstack, EXT)

TSstack(_bool, bool);
TSstack(_int, int);
TSstack(_unsigned, unsigned);
TSstack(_Pchar, char*);

/**
   \brief creates a stack
   \return the created stack */
#define stack_INIT_s(stack, s) \
	do { \
		MY_MALLOC((stack), 2 * sizeof(unsigned) + (s)*sizeofdata(stack)); \
		(stack)->size = 0; \
		(stack)->alloc = (s); \
	} while (0)

/**
   \brief creates a stack
   \return the created stack */
#define stack_INIT(stack) stack_INIT_s(stack, 4)

/**
   \brief creates a copy of stack2
   \param stack2 another stack
   \return the created stack */
#define stack_COPY(stack, stack2) \
	do { \
		unsigned stack_i; \
		assert(stack2); \
		MY_MALLOC( \
			(stack), 2 * sizeof(unsigned) + stack_size(stack2) * sizeofdata(stack)); \
		(stack)->size = stack_size(stack2); \
		(stack)->alloc = stack_size(stack2); \
		for (stack_i = 0; stack_i < (stack2)->size; ++stack_i) \
			(stack)->data[stack_i] = (stack2)->data[stack_i]; \
	} while (0)

/**
   \brief resize stack such that it contains length elements.
   New elements are set to 0/null
   \param stack a stack
   \param length a number of elements */
#ifndef DMEM
#define stack_resize(stack, length) \
	do { \
		assert(stack); \
		if (length > (stack)->alloc) { \
			while (length > (stack)->alloc) (stack)->alloc *= 2; \
			MY_REALLOC( \
				(stack), 2 * sizeof(unsigned) + (stack)->alloc * sizeofdata(stack)); \
		} \
		if (length > (stack)->size) \
			memset( \
				(stack)->data + (stack)->size, 0, \
				(length - (stack)->size) * sizeofdata(stack)); \
		(stack)->size = length; \
	} while (0)
#else
#define stack_resize(stack, length) \
	do { \
		assert(stack); \
		if (length > (stack)->alloc) { \
			void* P; \
			MY_MALLOC(P, 2 * sizeof(unsigned) + length * sizeofdata(stack)); \
			memcpy( \
				P, (stack), \
				2 * sizeof(unsigned) + (stack)->alloc * sizeofdata(stack)); \
			P->alloc = length; \
			free(stack); \
			stack = P; \
		} \
		if (length > (stack)->size) \
			memset( \
				(stack)->data + (stack)->size, 0, \
				(length - (stack)->size) * sizeofdata(stack)); \
		(stack)->size = length; \
	} while (0)
#endif

/**
   \brief adds value on top of stack
   \param stack a stack
   \param value a value */
#ifndef DMEM
#define stack_push(stack, value) \
	do { \
		assert(stack); \
		if ((stack)->size == (stack)->alloc) { \
			(stack)->alloc *= 2; \
			MY_REALLOC( \
				(stack), 2 * sizeof(unsigned) + (stack)->alloc * sizeofdata(stack)); \
		} \
		(stack)->data[(stack)->size++] = value; \
	} while (0)
#else
#define stack_push(stack, value) \
	do { \
		assert(stack); \
		if ((stack)->size == (stack)->alloc) \
			stack_resize(stack, (stack)->alloc + 1); \
		(stack)->data[(stack)->size++] = value; \
	} while (0)
#endif

/**
   \brief adds value at position i in stack
   \param stack a stack
   \param value a value */
#ifndef DMEM
#define stack_insert(stack, value, i) \
	do { \
		unsigned stack_j; \
		assert(stack); \
		if ((stack)->size == (stack)->alloc) { \
			(stack)->alloc *= 2; \
			MY_REALLOC( \
				(stack), 2 * sizeof(unsigned) + (stack)->alloc * sizeofdata(stack)); \
		} \
		for (stack_j = (stack)->size; stack_j > i; stack_j--) \
			(stack)->data[stack_j] = (stack)->data[stack_j - 1]; \
		(stack)->data[i] = value; \
		(stack)->size++; \
	} while (0)
#else
#define stack_insert(stack, value, i) \
	do { \
		unsigned stack_j; \
		assert(stack); \
		if ((stack)->size == (stack)->alloc) \
			stack_resize(stack, (stack)->alloc + 1); \
		for (stack_j = (stack)->size; stack_j > i; stack_j--) \
			(stack)->data[stack_j] = (stack)->data[stack_j - 1]; \
		(stack)->data[i] = value; \
		(stack)->size++; \
	} while (0)
#endif

/**
   \brief deletes value at position i in stack
   \param stack a stack
   \param i a position */
#define stack_delete(stack, i) \
	do { \
		unsigned stack_j; \
		assert(stack&& i < stack_size(stack)); \
		for (stack_j = i; stack_j < (stack)->size - 1; ++stack_j) \
			(stack)->data[stack_j] = (stack)->data[stack_j + 1]; \
		--(stack)->size; \
	} while (0)

/**
   \brief allocate space for a supplementary value on top of stack
   \param stack a stack */
#ifndef DMEM
#define stack_inc(stack) \
	do { \
		assert(stack); \
		if ((stack)->size == (stack)->alloc) { \
			(stack)->alloc *= 2; \
			MY_REALLOC( \
				(stack), 2 * sizeof(unsigned) + (stack)->alloc * sizeofdata(stack)); \
		} \
		(stack)->size++; \
	} while (0)

#else
#define stack_inc(stack) \
	do { \
		assert(stack); \
		if ((stack)->size == (stack)->alloc) \
			stack_resize(stack, (stack)->alloc + 1); \
		(stack)->size++; \
	} while (0)

#endif

/**
   \brief allocate space for n supplementary value on top of stack
   \param stack a stack */
#ifndef DMEM
#define stack_inc_n(stack, n) \
	do { \
		assert(stack); \
		if ((stack)->size + n > (stack)->alloc) { \
			while ((stack)->size + n > (stack)->alloc) (stack)->alloc *= 2; \
			MY_REALLOC( \
				(stack), 2 * sizeof(unsigned) + (stack)->alloc * sizeofdata(stack)); \
		} \
		(stack)->size += n; \
	} while (0)

#else
#define stack_inc_n(stack, n) \
	do { \
		assert(stack); \
		if ((stack)->size + n > (stack)->alloc) \
			stack_resize(stack, (stack)->size + n); \
		(stack)->size += n; \
	} while (0)

#endif

/**
   \brief allocate space for a supplementary value on top of stack
   \param stack a stack
   \param hook a function to realloc space when stack is full */
#ifndef DMEM
#define stack_inc_hook(stack, hook) \
	do { \
		assert(stack); \
		if ((stack)->size == (stack)->alloc) { \
			hook((stack)->alloc, (stack)->alloc * 2); \
			(stack)->alloc *= 2; \
			MY_REALLOC( \
				(stack), 2 * sizeof(unsigned) + (stack)->alloc * sizeofdata(stack)); \
		} \
		(stack)->size++; \
	} while (0)
#else
#define stack_inc_hook(stack, hook) \
	do { \
		assert(stack); \
		if ((stack)->size == (stack)->alloc) { \
			stack_resize(stack, (stack)->alloc + 1); \
			hook((stack)->alloc, (stack)->alloc + 1); \
		} \
		(stack)->size++; \
	} while (0)
#endif

/**
   \brief allocate space for a supplementary value on top of stack
   \param stack a stack */
#define stack_inc0(stack) \
	do { \
		stack_inc(stack); \
		memset((stack)->data + (stack)->size - 1, 0, sizeofdata(stack)); \
	} while (0)

/**
   \brief remove value on top of stack
   \param stack a stack
   \return nothing */
#define stack_dec(stack) \
	do { \
		--(stack)->size; \
	} while (0)

/**
   \brief remove n values on top of stack
   \param stack a stack
   \param n quantity of values removed
   \return nothing */
#define stack_dec_n(stack, n) \
	do { \
		(stack)->size -= n; \
	} while (0)

/**
   \brief get value on top of stack, and remove it
   \param stack a stack
   \return the value on top of the stack */
#define stack_pop(stack) ((stack)->data[--(stack)->size])

/**
   \brief pops n values from stack, and return last popped value
   \param stack a stack
   \param n quantity of values popped
   \return the value on top of the stack */
#define stack_pop_n(stack, n) ((stack)->data[(stack)->size -= n])

/**
   \brief get value on top of stack, without removing it
   \param stack a stack
   \return the value on top of the stack */
#define stack_top(stack) ((stack)->data[(stack)->size - 1])

/**
   \brief get value at n from top of stack, without touching stack
   \param stack a stack
   \param n how far from the top is the access
   \return the value */
#define stack_top_n(stack, n) ((stack)->data[(stack)->size - n])

/**
   \brief get an elements in a stack
   \param stack a stack
   \param i the index of the element
   \return the element at given index */
#define stack_get(stack, i) ((stack)->data[i])

/**
   \brief set an elements in a stack
   \param stack a stack
   \param i the index of the element
   \param value the element to put at given index */
#define stack_set(stack, i, value) ((stack)->data[i] = (value))

/**
   \brief free a stack
   \param stack a stack */
#define stack_free(stack) \
	do { \
		free(stack); \
		(stack) = NULL; \
	} while (0)

/**
   \brief empty a stack
   \param stack a stack */
#define stack_reset(stack) \
	do { \
		(stack)->size = 0; \
	} while (0)

/**
   \brief check if a stack is empty
   \param stack a stack
   \return 1 iff there is no element in the stack */
#define stack_is_empty(stack) ((stack)->size == 0)

/**
   \brief get number of elements in a stack
   \param stack a stack
   \return the number of elements in the stack */
#define stack_size(stack) ((stack)->size)

/**
   \brief apply a function to all elements in a stack
   \param stack a stack
   \param f a function */
#define stack_apply(stack, f) \
	do { \
		unsigned stack_i; \
		assert(stack); \
		for (stack_i = 0; stack_i < (stack)->size; ++stack_i) \
			f((stack)->data[stack_i]); \
	} while (0)

/**
   \brief copy all elements of stack2 into stack
   \param stack a stack
   \param stack2 another stack */
#define stack_merge(stack, stack2) \
	do { \
		unsigned stack_i; \
		assert(stack); \
		assert(stack2); \
		for (stack_i = 0; stack_i < (stack2)->size; ++stack_i) \
			stack_push(stack, (stack2)->data[stack_i]); \
	} while (0)

/**
   \brief sort a stack
   \param stack a stack
   \param compare a compare function */
#define stack_sort(stack, f) \
	do { \
		assert(stack); \
		if ((stack)->size > 1) \
			veriT_qsort( \
				(stack)->data, (stack)->size, sizeofdata(stack), \
				(int (*)(const void*, const void*))f); \
	} while (0)

/**
   \brief removes duplicates in sorted stack
   \param stack a stack */
#define stack_uniq(stack) \
	do { \
		unsigned stack_i = 0, stack_j = 1; \
		assert(stack); \
		if ((stack)->size > 1) { \
			while (stack_j < (stack)->size) \
				if ((stack)->data[stack_j] == (stack)->data[stack_i]) \
					stack_j++; \
				else \
					(stack)->data[++stack_i] = (stack)->data[stack_j++]; \
			(stack)->size = stack_i + 1; \
		} \
	} while (0)

#define stack_uniq_f(stack, f) \
	do { \
		unsigned stack_i = 0, stack_j = 1; \
		assert(stack); \
		if ((stack)->size > 1) { \
			while (stack_j < (stack)->size) \
				if ((stack)->data[stack_j] == (stack)->data[stack_i]) { \
					f((stack)->data[stack_j]); \
					stack_j++; \
				} else \
					(stack)->data[++stack_i] = (stack)->data[stack_j++]; \
			(stack)->size = stack_i + 1; \
		} \
	} while (0)

#endif /* __STACK_H */

/*
  #include "stdio.h"

  #define MY_MALLOC(A, B) A = malloc(B)
  #define MY_REALLOC(A, B) A = realloc(A, B)

  #include "utils/stack.h"

  void
  print_c(char * str)
  {
  printf("%s\n", str);
  }

  void
  print_u(unsigned u)
  {
  printf("%d\n", u);
  }

  TSstack(_c, char *);

  int
  main(void)
  {
  unsigned i;
  Tstack(unsigned) stack_u;
  Tstack_c stack_c;
  stack_INIT(stack_u);
  stack_push(stack_u, 1);
  stack_push(stack_u, 2);
  stack_push(stack_u, 3);
  stack_push(stack_u, 3);
  stack_push(stack_u, 4);
  stack_push(stack_u, 5);
  stack_push(stack_u, 6);
  for (i = 0; i < stack_u->size; i++)
  printf("%d\n", stack_u->data[i]);

  stack_apply(stack_u, print_u);

  stack_free(stack_u);

  stack_INIT(stack_c);
  stack_push(stack_c, "c 1");
  stack_push(stack_c, "c 2");
  stack_push(stack_c, "c 3");
  stack_push(stack_c, "c 3");
  stack_push(stack_c, "c 4");
  stack_push(stack_c, "c 5");
  stack_push(stack_c, "c 6");
  for (i = 0; i < stack_c->size; i++)
  printf("%s\n", stack_c->data[i]);

  stack_apply(stack_c, print_c);

  stack_free(stack_c);

  return 0;
  }
*/
