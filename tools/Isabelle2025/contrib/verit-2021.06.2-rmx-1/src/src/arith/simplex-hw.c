/*
   TODO: put the following documentation in a doxygen-compatible format
   TODO: enrich documentation

   <h2>Introduction</h2>

   Linear constraints are added with the sub-routine simplex_hw_constraint_push.

   The variables appearing in these constraints are numbered starting
   from 1. The value 0 is reserved to represent either undefinedness
   or the constant one. The table simplex_var is used to store
   information on these variables.

   A matrix maintains all the linear constraints that were added. This matrix
   is sparse and accessible via two variables global to this file: ROWS and
   COLS (for columns).

   Matrix elements are represented by a structure type TSmon, and referenced
   by pointer type Tmon.

   <h2>Module data</h2>

   <h3>Simplex variable data: TLASvar</h3>

   Each variable is assigned a unique identifier in the range 1..simplex_var_n.
   The array simplex_var maintains key information for each variable: current
   assignment, previous assignment, optionally its bounds, and the reasons for
   the bounds.

   Variables are classified as

   * basic (or non-basic): this classification corresponds to that of the Simplex

   * bounded or unbounded: bounded variables are those that may be set a bound to
   during the current phase of execution. An unbounded variable might become
   eventually bounded, in case new constraints are pushed to the module after
   a satisfiability check has been performed.

   * passive or active: A variable is passive when it has been found that it
   needs not be taken into account in the Simplex.

   * integer-valued or real: self-explaining.

   Additionally, attributes boundmask and mark_unsat indicate whether the
   variable is lower/upper bounded, and if its assignment is outside bounds

   <h3>Matrix elements: TSmon</h3>

   The matrix representation is sparse. Each matrix element is respresented by a
   TSmon structure with to maintaind double-linking in columns, the identifiers of
   the current line and column, and the coefficient (non-zero).

   Global variable ROWS maintains a table (Tstack) of rows. Each row is itself
   a table (Tstack) of TSmon structure.

   Global variable COLS maintains a table (Tstack) of pointers to doubly-linked
   lists of TSmon structures.

   <h3>Modified variables</h3>

   <h3>Compute store</h3>

   Routines called frequently need local resources. To reduce the processing time, such
   local resources are allocated only once, when the module is uninitialized. All such
   resources are grouped within a single structure, called store.

   <h3>Backtracking</h3>

   <h2>Operations</h2>

*/

#include "utils/general.h"
#include "utils/types.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HW_VERSION
#include "utils/stack.h"
#endif
#include "numbers-hw.h"
#include "simplex-hw.h"
#include "symbolic/veriT-status.h"
#include "undo.h"
#include "utils/options.h"
#include "utils/statistics.h"

/* Uncomment next line to print state of the module during execution
   and toggle invariant checking. */

/* #define DEBUG_SIMPLEX */

#ifdef DEBUG_SIMPLEX
static void LA_print(void);
static void LA_invariant_column(const Tsimplex_var);
static void LA_invariant(void);
#define CHECK_INVARIANT \
	LA_print(); \
	LA_invariant()
#else
#define CHECK_INVARIANT
#endif

/*
  --------------------------------------------------------------
  Options
  --------------------------------------------------------------
*/

static bool simplex_disable_pivot_heuristics = false;

/*
  --------------------------------------------------------------
  Statistics
  --------------------------------------------------------------
*/

static unsigned simplex_var_n_unbound = 0;
static unsigned simplex_var_n_const = 0;
static unsigned simplex_pivot = 0; /* number of pivots operations */
static unsigned simplex_pivot_heuristics = 0;

/*
  --------------------------------------------------------------
  Variables

  Simplex variables are identified by a positive integer.
  Identifier 0 corresponds to a constant factor.
  The identifiers are in the interval 1..simplex_var_n.
  --------------------------------------------------------------
*/

#define LAVAR_UNDEF ((Tsimplex_var)0)
#define ONE ((Tsimplex_var)0)

/**
   \var simplex_var_n
   \brief number of variables */
static Tsimplex_var simplex_var_n = 0;

/**
   \typedef TLASvar
   \brief structure for variables
   \remark for a basic variable i, the diagonal element can be found in C[i] */
typedef struct TLASvar
{
	TLAdelta_hw assign; /*< value */
#ifdef SIMPLEX_COPY
	TLAdelta_hw assign2; /*< value (backup) */
#endif
#ifdef PEDANTIC
	bool basic; /*< variable is basic */
	bool bounded; /*< variable may have bounds */
	bool passive; /*< variable is passive */
#ifdef SIMPLEX_COPY
	bool mark; /*< backup value and value differ */
#endif
	bool mark_unsat; /*< bounds are unsatisfied */
	bool integer; /*< the variable is integer */
	unsigned char
		boundmask; /*< up & lo bounds exist (3) up (2) lo (1) none (0) */
#else
	bool basic : 1; /*< variable is basic */
	bool bounded : 1; /*< variable may have bounds */
	bool passive : 1; /*< variable is passive */
#ifdef SIMPLEX_COPY
	bool mark : 1; /*< backup value and value differ */
#endif
	bool mark_unsat : 1; /*< bounds are unsatisfied */
	bool integer : 1; /*< the variable is integer */
	unsigned boundmask : 2; /*< up & lo bounds exist (3) up (2) lo (1) none (0) */
#endif
	TLAdelta_hw bound[2]; /*< lower [0] and upper [1] bound */
	Tlit reason[2]; /*< get the reason for lower [0] and upper [1] bound */
	unsigned row_count; /*< counts rows where variable appears */
} TLASvar;

/**
   \var simplex_var
   \brief array of variables */
static TLASvar* simplex_var;

/* a number of abbreviations to access variable properties */
#define PASSIVE(var) (simplex_var[var].passive)
#define ACTIVE(var) (!PASSIVE(var))
#define BASIC(var) (simplex_var[var].basic)
#define NONBASIC(var) (!BASIC(var))

/**
   \var integer_stack
   \brief array of identifiers of integer variables
   \remark If $N$ is the number of integer variables,
   iteration through integer variables is \f$ \Theta(N) \f$.
*/
extern Tstack_simplex_var integer_stack;

/*
  --------------------------------------------------------------
  Bounds
  --------------------------------------------------------------
*/

/** \brief shortcuts for testing if simplex variables are bound */
#define MASK_BOUND_LOW (1u)
#define MASK_BOUND_UPP (2u)
#define MASK_BOUND_ALL (3u)
#define IS_BOUND_LOW(svar) (simplex_var[svar].boundmask & MASK_BOUND_LOW)
#define IS_BOUND_UPP(svar) (simplex_var[svar].boundmask & MASK_BOUND_UPP)
#define IS_BOUND_ALL(svar) (IS_BOUND_LOW(svar) && IS_BOUND_UPP(svar))
#define LOW 0
#define UPP 1
#define BOUND_LOW(svar) (simplex_var[svar].bound[LOW])
#define BOUND_UPP(svar) (simplex_var[svar].bound[UPP])

/*
  --------------------------------------------------------------
  Bounds
  --------------------------------------------------------------
*/

/*
  --------------------------------------------------------------
  Black magic
  --------------------------------------------------------------
*/

/**
   \brief Absolute value for signed integers.
   \param[in] __in a signed value
   \param[out] __out a signed lvalue
*/
#ifdef HW_VERSION
#define SIGNED_ABS(__in, __out) \
	{ \
		signed __mask; \
		__mask = (__in) >> (sizeof(signed int) * CHAR_BIT - 1); \
		(__out) = (unsigned)(((__in) + __mask) ^ __mask); \
	}
#else
#define SIGNED_ABS(__in, __out) mpz_abs((__out), (__in))
#endif

/**
   \brief a comparison function defining a total order on simplex
   variables bounds
   \param[in] v1 a simplex variable
   \param[in] v2 a simplex variable
   \return negative if v1 lower bound is smaller than v2 lower bound,
   or if their lower bounds are identical and v1 upper bound is greater
   than v2 upper bound
   \return zero if the bounds are identical
   \return positive if v1 lower bound is greater than v2 lower bound,
   or if their lower bounds are identical and v1 upper bound is smaller
   than v2 lower bound
   \note robust to unbounded variables, using +/-infinity as the bound in the
   comparison
*/
int
simplex_hw_var_cmp(Tsimplex_var v1, Tsimplex_var v2)
{
	return LAdelta_hw_cmp(&simplex_var[v1].assign, &simplex_var[v2].assign);
}

/**
   \brief a comparison function defining equality between simplex variables
   based on their current assignment.
   \param[in] v1 a simplex variable
   \param[in] v2 a simplex variable
   \pre v1 and v2 have a single possible assignment
   \return true iff v1 and v2 have the same assignment.
   comparison
   \see simplex_var_fixed
*/
bool
simplex_hw_var_eq(Tsimplex_var v1, Tsimplex_var v2)
{
	return LAdelta_hw_eq(&simplex_var[v1].assign, &simplex_var[v2].assign);
}

/**
   \brief access to current variable assignment
   \param[in] v a simplex variable
   \return address where the assignment is stored
*/
TLAdelta_hw*
simplex_hw_var_assign(Tsimplex_var v)
{
	return &simplex_var[v].assign;
}

/**
   \brief tests if a variable is an integer
   \param[in] v a simplex variable
*/
bool
simplex_hw_var_integer(Tsimplex_var v)
{
	return simplex_var[v].integer;
}

static bool
simplex_hw_var_fixed(Tsimplex_var v)
{
	return IS_BOUND_ALL(v) && LAdelta_hw_eq(&BOUND_LOW(v), &BOUND_UPP(v));
}

unsigned
simplex_hw_var_n(void)
{
	return simplex_var_n;
}

/*
  --------------------------------------------------------------
  Monom Handler
  --------------------------------------------------------------
*/

/**
   \struct TSmon
   \brief element of tableau (monom)
   \remark so c_b b + ... + c n + ... = 0
   \remark a tableau row is represented as a stack of such struct
*/
typedef struct TSmon
{
	struct TSmon* ln; /*< next non-zero in column */
	struct TSmon* lp; /*< previous non-zero in column */
	Tsimplex_var l;
	Tsimplex_var c;
	TLAsigned_hw a;
} TSmon, *Tmon;

TSstack(_Smon, TSmon); /* packed linear expressions (array of monoms) */
TSstack(_stack_Smon, Tstack_Smon); /* array of packed linear expressions*/
TSstack(_mon, Tmon); /* array of pointers to monoms */

/*
  The tableau is an array (stack) of rows, and each row is an array (stack)
  of monoms. The rows are sorted in increasing order of variables. Only
  monomes with non-zero coefficients are represented. We call this a packed
  array of monoms.

  Columns may be addressed individually with a separate data structure.
  Each column is a doubly-linked list of monoms.
*/

static Tstack_stack_Smon ROWS; /*< tableau rows, indexed by variables */
static Tstack_mon COLS; /*< tableau columns, indexed by variables */

/*
  Abbreviations
*/
#define Trow Tstack_Smon
#define ROW(var) stack_get(ROWS, var)
#define ROW_LEN(line) stack_size(line)
#define ROW_COEF(line, i) stack_get(line, i).a
#define ROW_VAR(line, i) stack_get(line, i).c
#define ROW_ID(line, i) stack_get(line, i).l
#define COL(i) stack_get(COLS, i)
#define DIAG(i) COL(i)->a

/*
  Functions/macros stack_INIT, stack_inc, stack_resize and stack_free are
  "overloaded" for rows as functions row_init, row_inc and row_free,
  respectively.

  The additional processing in these functions is for resources to represent
  coefficients.

  Sub-routine row_realloc guarantees that the memory block has enough capacity
  to accomodate a given number of monomes. It does not change the size of the
  row. It may change its alloc field, and also the address of the row. The
  memory block is never shrinked.

  Each such function has a HW and SW version.
*/

static Trow
row_init(void)
{
	Trow row;
#ifndef HW_VERSION
	unsigned i;
#endif
	stack_INIT(row);
#ifndef HW_VERSION
	for (i = 0; i < row->alloc; ++i) LAsigned_hw_init(row->data[i].a);
#endif
	return row;
}

static void
row_free(Trow row)
{
#ifndef HW_VERSION
	unsigned i;
	for (i = 0; i < row->alloc; ++i) LAsigned_hw_clear(row->data[i].a);
#endif
	stack_free(row);
}

static void
row_inc(Trow row)
{
	assert(row->size < row->alloc);
	++row->size;
}

static void
row_reset(Trow row)
{
	stack_reset(row);
}

static Trow
row_realloc(Trow row, const unsigned length)
{
	unsigned i;
	Trow new;
	unsigned n = row->alloc;
	/*  printf("row_realloc %p %u\n", row, length); */
	while (n < length) n *= 2;
	if (n == row->alloc) return row;
	stack_INIT_s(new, n);
	for (i = 0; i < row->size; ++i) {
		stack_inc(new);
		new->data[i].ln = row->data[i].ln;
		if (new->data[i].ln != 0) new->data[i].ln->lp = &new->data[i];
		new->data[i].lp = row->data[i].lp;
		if (new->data[i].lp != 0)
			new->data[i].lp->ln = &new->data[i];
		else if (COL(row->data[i].c) == &row->data[i])
			COL(row->data[i].c) = &new->data[i];
		new->data[i].l = row->data[i].l;
		new->data[i].c = row->data[i].c;
		LAsigned_hw_init(new->data[i].a);
		LAsigned_hw_set(new->data[i].a, row->data[i].a);
	}
#ifndef HW_VERSION
	for (i = row->size; i < new->alloc; ++i) LAsigned_hw_init(new->data[i].a);
#endif
	row_free(row);
	return new;
}

/** \brief a data structure to group the temporary GMP data used
    in this module sub-routines.
    \remark each element of the structure is named according to
    a sub-routine, and is itself a structure with one field for
    each variable used. In the case of recursive sub-routines
    this field is an array.
*/
static struct store
{
#ifndef HW_VERSION
	struct var_update
	{
		mpq_t tmp1;
		mpq_t tmp2;
	} var_update;
#endif
	struct normalize
	{
		TLAsigned_hw tmp;
		TLAsigned_hw tmp2;
	} normalize;
	struct eliminate_basic
	{
		TLAsigned_hw tmp;
		TLAsigned_hw tmp2;
	} eliminate_basic;
	struct conflict
	{
		TLAsigned_hw c;
	} conflict;
	struct update
	{
		TLAdelta_hw D;
	} update;
	struct pivot
	{
		TLAsigned_hw diag;
		TLAsigned_hw c;
	} pivot;
	struct solve
	{
		TLAsigned_hw c;
	} solve;
	struct assert_equality
	{
		TLAsigned_hw diag;
		TLAdelta_hw delta;
		TLAsigned_hw c;
	} assert_equality;
	struct row_update
	{
		Trow row;
		TLAsigned_hw a;
	} row_update;
	struct constraint_push
	{
		Trow row;
	} constraint_push;
} store;

/** \brief initializes the store */
static inline void
store_init(void)
{
#ifndef HW_VERSION
	mpq_init(store.var_update.tmp1);
	mpq_init(store.var_update.tmp2);
#endif
	LAsigned_hw_init(store.normalize.tmp);
	LAsigned_hw_init(store.normalize.tmp2);
	LAsigned_hw_init(store.eliminate_basic.tmp);
	LAsigned_hw_init(store.eliminate_basic.tmp2);
	LAsigned_hw_init(store.conflict.c);
	LAdelta_hw_init(&store.update.D);
	LAsigned_hw_init(store.pivot.diag);
	LAsigned_hw_init(store.pivot.c);
	LAsigned_hw_init(store.solve.c);
	LAsigned_hw_init(store.assert_equality.diag);
	LAdelta_hw_init(&store.assert_equality.delta);
	LAsigned_hw_init(store.assert_equality.c);
	store.constraint_push.row = row_init();
	store.row_update.row = row_init();
	LAsigned_hw_init(store.row_update.a);
}

/** \brief frees store resources */
static inline void
store_done(void)
{
#ifndef HW_VERSION
	mpq_clear(store.var_update.tmp1);
	mpq_clear(store.var_update.tmp2);
#endif
	LAsigned_hw_clear(store.normalize.tmp);
	LAsigned_hw_clear(store.normalize.tmp2);
	LAsigned_hw_clear(store.eliminate_basic.tmp);
	LAsigned_hw_clear(store.eliminate_basic.tmp2);
	LAsigned_hw_clear(store.conflict.c);
	LAdelta_hw_clear(&store.update.D);
	LAsigned_hw_clear(store.pivot.diag);
	LAsigned_hw_clear(store.pivot.c);
	LAsigned_hw_clear(store.solve.c);
	LAsigned_hw_clear(store.assert_equality.diag);
	LAdelta_hw_clear(&store.assert_equality.delta);
	LAsigned_hw_clear(store.assert_equality.c);
	row_free(store.constraint_push.row);
	row_free(store.row_update.row);
	LAsigned_hw_clear(store.row_update.a);
}

/**
   \brief adds a monom in the column for its c field
   \param P pointer to the monom to be inserted
   \remark insertion occurs in the first position */
static inline void
mon_C_add(Tmon P)
{
	Tsimplex_var c;
	Tmon Ph;
	assert(P);
	c = P->c;
	Ph = COL(c); /* pointer to head of column for c */
	assert(Ph == NULL || Ph->lp == NULL);
	P->lp = NULL;
	P->ln = Ph;
	if (Ph) Ph->lp = P;
	stack_set(COLS, c, P);
	++simplex_var[c].row_count;
}

/**
   \brief removes monom from the column for its c field
   \param P pointer to the monom to be removed */
static inline void
mon_C_rm(Tmon P)
{
	assert(P != NULL);
	/* according to invariants, this is taken care of later,
     since C[0] = 0 and ln and lp should be 0 for all monoms about var 0
     if (!mon[mon_id].c)
     return; */
	if (P->lp) {
		assert(P->lp->ln == P);
		P->lp->ln = P->ln;
		if (P->ln != NULL) P->ln->lp = P->lp;
	} else {
		assert(P->c == 0 || COL(P->c) == P);
		stack_set(COLS, P->c, P->ln);
		if (P->ln != NULL) P->ln->lp = NULL;
	}
	--simplex_var[P->c].row_count;
	P->ln = NULL;
	P->lp = NULL;
}

/*
  --------------------------------------------------------------
  Table of modified variables
  --------------------------------------------------------------
*/

#ifdef SIMPLEX_COPY

static unsigned modified_n = 0;
static unsigned modified_size = 0;
static Tsimplex_var* modified;

static void
modified_init(void)
{
	modified_n = 0;
	modified_size = 16;
	MY_MALLOC(modified, modified_size * sizeof(Tsimplex_var));
}

static void
modified_done(void)
{
	modified_n = 0;
	modified_size = 0;
	free(modified);
}

static inline void
modified_insert(Tsimplex_var i)
{
	if (simplex_var[i].mark) return;
	simplex_var[i].mark = true;
	if (modified_n >= modified_size) {
		modified_size <<= 1;
		MY_REALLOC(modified, modified_size * sizeof(Tsimplex_var));
	}
	modified[modified_n++] = i;
}

static void
modified_save(void)
{
	unsigned i;
	for (i = 0; i < modified_n; i++) {
		assert(simplex_var[modified[i]].mark);
		simplex_var[modified[i]].mark = false;
		LAdelta_hw_set(
			&simplex_var[modified[i]].assign2, &simplex_var[modified[i]].assign);
	}
	modified_n = 0;
}

static inline void heap_var_remove_all(void);

static inline void
modified_restore(void)
{
	unsigned i;
	for (i = 0; i < modified_n; i++) {
		assert(simplex_var[modified[i]].mark);
		simplex_var[modified[i]].mark = false;
		simplex_var[modified[i]].mark_unsat = false;
		LAdelta_hw_set(
			&simplex_var[modified[i]].assign, &simplex_var[modified[i]].assign2);
	}
	modified_n = 0;
	heap_var_remove_all();
}

#endif
/*
  --------------------------------------------------------------
  var heap and activity
  Table of unsat basics
  --------------------------------------------------------------
*/

static unsigned heap_var_n = 0;
static unsigned heap_var_size = 0;
static Tsimplex_var* heap_var = NULL;
static unsigned heap_index_size = 0;
static unsigned* heap_index = NULL;

#define HEAP_UNDEF UINT_MAX

static inline unsigned
left(unsigned i)
{
	return i * 2 + 1;
}

static inline unsigned
right(unsigned i)
{
	return i * 2 + 2;
}

static inline unsigned
parent(unsigned i)
{
	return (i - 1) >> 1;
}

static inline void
sift_up(unsigned i)
{
	Tsimplex_var var = heap_var[i];
	unsigned p = parent(i);
	while (i && var < heap_var[p]) {
		heap_var[i] = heap_var[p];
		heap_index[heap_var[p]] = i;
		i = p;
		p = parent(p);
	}
	heap_var[i] = var;
	heap_index[var] = i;
}

static inline void
sift_down(unsigned i)
{
	Tsimplex_var var = heap_var[i];
	while (left(i) < heap_var_n) {
		unsigned child;
		if (right(i) < heap_var_n && heap_var[right(i)] < heap_var[left(i)])
			child = right(i);
		else
			child = left(i);
		if (heap_var[child] >= var) break;
		heap_var[i] = heap_var[child];
		heap_index[heap_var[child]] = i;
		i = child;
	}
	heap_var[i] = var;
	heap_index[var] = i;
}

#ifndef NDEBUG

static inline int
heap_var_in(Tsimplex_var var)
{
	assert(var != LAVAR_UNDEF);
	return var < heap_index_size && heap_index[var] != HEAP_UNDEF;
}

#endif

static inline void
heap_var_insert(Tsimplex_var var)
{
	assert(var != LAVAR_UNDEF);
	assert(heap_var_n <= heap_var_size);
	assert(var >= heap_index_size || heap_index[var] == HEAP_UNDEF);
	if (heap_var_size == heap_var_n) {
		heap_var_size *= 2;
		MY_REALLOC(heap_var, heap_var_size * sizeof(Tsimplex_var));
	}
	/* DD TODO: factor with heap_var_notify_var_table_increase? */
	while (heap_index_size <= var) {
		unsigned i;
		heap_index_size *= 2;
		MY_REALLOC(heap_index, heap_index_size * sizeof(unsigned));
		for (i = heap_index_size >> 1; i < heap_index_size; ++i)
			heap_index[i] = HEAP_UNDEF;
	}
	assert(!heap_var_in(var));
	heap_var[heap_var_n] = var;
	heap_index[var] = heap_var_n;
	sift_up(heap_var_n++);
}

static inline Tsimplex_var
heap_var_remove_min(void)
{
	Tsimplex_var var = heap_var[0];
	heap_index[var] = HEAP_UNDEF;
	heap_var[0] = heap_var[--heap_var_n];
	if (heap_var_n) sift_down(0); /* index will be set in sift_down */
	return var;
}

#ifndef SIMPLEX_COPY
static inline void
heap_var_remove_all(void)
{
	unsigned i;
	for (i = 0; i < heap_var_n; i++) {
		simplex_var[heap_var[i]].mark_unsat = false;
		heap_index[heap_var[i]] = HEAP_UNDEF;
	}
	heap_var_n = 0;
}
#endif

static inline int
heap_var_empty(void)
{
	return heap_var_n == 0;
}

#if 0
static inline Tsimplex_var
heap_var_get_min(void)
{
  return heap_var[0];
}

static inline void
heap_var_build(Tsimplex_var * vs, unsigned n)
{
  int i;
  heap_var_n = 0;

  for (i = 0; i < (int) n; i++)
    {
      heap_index[vs[i]] = i;
      heap_var[heap_var_n++] = vs[i];
    }

  for (i = heap_var_n / 2 - 1; i >= 0; i--)
    sift_down(i);
}
#endif

static inline void
heap_var_init(void)
{
	unsigned i;
	heap_var_n = 0;
	heap_var_size = 16;
	MY_MALLOC(heap_var, heap_var_size * sizeof(Tsimplex_var));
	heap_index_size = 16;
	MY_MALLOC(heap_index, heap_index_size * sizeof(unsigned));
	for (i = 0; i < heap_index_size; ++i) heap_index[i] = HEAP_UNDEF;
}

static inline void
heap_var_done(void)
{
	heap_var_n = 0;
	free(heap_var);
	heap_var = NULL;
	heap_var_size = 0;
	free(heap_index);
	heap_index = NULL;
	heap_index_size = 0;
}

static inline void
heap_var_notify_var_table_increase(unsigned j)
{
	if (heap_index_size < j) {
		unsigned i;
		MY_REALLOC(heap_index, j * sizeof(unsigned));
		for (i = heap_index_size; i < j; ++i) heap_index[i] = HEAP_UNDEF;
		heap_index_size = j;
	}
}

/*
  --------------------------------------------------------------
  simplex_var
  --------------------------------------------------------------
*/

/**
   \brief initialize variables
   \remark variable 0 is set to be constant 1 */
static void
simplex_var_init(void)
{
	assert(simplex_var_n == 0);
	simplex_var_n = 1;
	MY_MALLOC(simplex_var, sizeof(TLASvar));
	stack_INIT_s(ROWS, 2 * simplex_var_n);
	stack_INIT_s(COLS, 2 * simplex_var_n);
	LAdelta_hw_init(&simplex_var[ONE].assign);
#ifdef SIMPLEX_COPY
	LAdelta_hw_init(&simplex_var[ONE].assign2);
#endif
	LAdelta_hw_init(&simplex_var[ONE].bound[LOW]);
	LAdelta_hw_init(&simplex_var[ONE].bound[UPP]);
	LAdelta_hw_set_one(&simplex_var[ONE].assign);
#ifdef SIMPLEX_COPY
	LAdelta_hw_set_one(&simplex_var[ONE].assign2);
#endif
	simplex_var[ONE].basic = false;
	simplex_var[ONE].bounded = true;
	simplex_var[ONE].passive = true;
#ifdef SIMPLEX_COPY
	simplex_var[ONE].mark = false;
#endif
	simplex_var[ONE].mark_unsat = false;
	simplex_var[ONE].boundmask = MASK_BOUND_ALL;
	stack_push(COLS, NULL);
	stack_inc(ROWS);
	ROW(ONE) = row_init();
	LAdelta_hw_set_one(&simplex_var[ONE].bound[LOW]);
	LAdelta_hw_set_one(&simplex_var[ONE].bound[UPP]);
	simplex_var[ONE].reason[LOW] = 0;
	simplex_var[ONE].reason[UPP] = 0;
}

/**
   \brief release variables */
static void
simplex_var_done(void)
{
	unsigned i;
	for (i = 0; i < simplex_var_n; ++i) {
		LAdelta_hw_clear(&simplex_var[i].assign);
#ifdef SIMPLEX_COPY
		LAdelta_hw_clear(&simplex_var[i].assign2);
#endif
		LAdelta_hw_clear(&simplex_var[i].bound[LOW]);
		LAdelta_hw_clear(&simplex_var[i].bound[UPP]);
	}
	free(simplex_var);
	stack_free(COLS);
	for (i = 0; i < stack_size(ROWS); ++i) row_free(ROW(i));
	stack_free(ROWS);
	simplex_var_n = 0;
}

/**
   \brief create a new variable
   \return the new variable
   \remark all created variables are non-basic */
Tsimplex_var
simplex_hw_var_new(bool integer)
{
	assert(simplex_var_n);
	/* when simplex_var_n is a power of 2, data structures are
     full and need to be resized */
	if (!(simplex_var_n & (simplex_var_n - 1))) {
		unsigned j;
		MY_REALLOC(simplex_var, 2 * simplex_var_n * sizeof(TLASvar));
		heap_var_notify_var_table_increase(2 * simplex_var_n);
		stack_resize(ROWS, 2 * simplex_var_n);
		stack_resize(COLS, 2 * simplex_var_n);
		for (j = simplex_var_n; j < 2 * simplex_var_n; ++j) {
			ROW(j) = row_init();
			COL(j) = NULL;
		}
	}
	LAdelta_hw_init(&simplex_var[simplex_var_n].assign);
#ifdef SIMPLEX_COPY
	LAdelta_hw_init(&simplex_var[simplex_var_n].assign2);
#endif
	LAdelta_hw_init(&simplex_var[simplex_var_n].bound[LOW]);
	LAdelta_hw_init(&simplex_var[simplex_var_n].bound[UPP]);
	LAdelta_hw_set_zero(&simplex_var[simplex_var_n].assign);
#ifdef SIMPLEX_COPY
	LAdelta_hw_set_zero(&simplex_var[simplex_var_n].assign2);
#endif
	LAdelta_hw_set_zero(&simplex_var[simplex_var_n].bound[LOW]);
	LAdelta_hw_set_zero(&simplex_var[simplex_var_n].bound[UPP]);
	simplex_var[simplex_var_n].basic = false;
	simplex_var[simplex_var_n].bounded = integer;
	simplex_var[simplex_var_n].passive = false;
#ifdef SIMPLEX_COPY
	simplex_var[simplex_var_n].mark = false;
#endif
	simplex_var[simplex_var_n].mark_unsat = false;
	simplex_var[simplex_var_n].integer = integer;
	simplex_var[simplex_var_n].boundmask = 0;
	simplex_var[simplex_var_n].reason[LOW] = 0;
	simplex_var[simplex_var_n].reason[UPP] = 0;
	simplex_var[simplex_var_n].row_count = 0;
	if (integer) { stack_push(integer_stack, simplex_var_n); }
	return simplex_var_n++;
}

void
simplex_hw_var_bounded(Tsimplex_var var)
{
	simplex_var[var].bounded = true;
}

/**
   \brief update basic variable
   \param i a variable
   \param D a delta value
   \param num a numeric value
   \param den a numeric value
   \remark variable i will be set to its former value minus D * num /den
   \remark useful for updating basic variable on non-basic variable update */
static inline void
simplex_var_update(
	Tsimplex_var i, TLAdelta_hw* D, TLAsigned_hw num, TLAsigned_hw den)
{
	int bound;
	TLASvar* Pvar = simplex_var + i;
#ifdef HW_VERSION
	signed long den3, sign;
	signed long long num2;
	signed long long num3;
	unsigned long long den2;
#ifdef SIMPLEX_COPY
	modified_insert(i);
#endif
	/* v_i = v_i - D * num / den */
	/* (n_i', d_i') = (n_i, d_i) - (n_D, d_D) * num / den */
	/* n_i' = n_i * d_D * den - n_D * num * d_i */
	/* d_i' = d_i * d_D * den */
	/* n_i' = n_i * d_D * |den| - n_D * num * d_i * sign(den) */
	/* d_i' = d_i * d_D * |den| */
	/* num2 = den2 = d_D * |den|
     den2 *= d_i
     num2 *= n_i
     num3 = n_D * num * d_i * sign(den)
     num2 -= num3 */
	sign = den >> (sizeof(signed int) * CHAR_BIT - 1); /* 0 or -1 */
	den3 = (den + sign) ^ sign; /* den3 = |den| */
	den2 = (unsigned long long)D->val.den * (unsigned long long)den3;
	OVERFLOW_CHECK_SET(den2 > INT_MAX, den2);
	num2 = (signed long long)den2;
	den2 *= (unsigned long long)Pvar->assign.val.den;
	OVERFLOW_CHECK_SET(den2 > INT_MAX, den2);
	num2 *= (signed long long)Pvar->assign.val.num;
	OVERFLOW_CHECK(num2 > INT_MAX || num2 <= INT_MIN);

	num3 = (signed long long)D->val.num * (signed long long)num;
	OVERFLOW_CHECK(num3 > INT_MAX || num3 <= INT_MIN);
	num3 *= (signed long long)Pvar->assign.val.den;
	OVERFLOW_CHECK(num3 > INT_MAX || num3 <= INT_MIN);
	num3 = (num3 + sign) ^ sign;
	num2 -= num3;
	OVERFLOW_CHECK(num2 > INT_MAX || num2 <= INT_MIN);

	RETURN_IF_OVERFLOW();

	Pvar->assign.val.num = (signed int)num2;
	Pvar->assign.val.den = (unsigned int)den2;
	/* (Pvar->assign.val.num > SHRT_MAX ||
     Pvar->assign.val.den > SHRT_MAX) */
	LArational_hw_normalize(&Pvar->assign.val);
	/* 0 if nothing is added
     1 if lower bound may be violated (increment is negative)
     -1 if upper bound may be violated (increment is positive) */
	bound =
		(int)(((signed long)num3 >> (sizeof(signed long) * CHAR_BIT - 1)) << 1) +
		(num3 != 0);
	/* bound is -1, 0 or 1 if num3 is < 0, == 0, > 0 */

	den2 = (unsigned long long)D->delta.den * (unsigned long long)den3;
	OVERFLOW_CHECK_SET(den2 > INT_MAX, den2);
	num2 = (signed long long)den2;
	den2 *= (unsigned long long)Pvar->assign.delta.den;
	OVERFLOW_CHECK_SET(den2 > INT_MAX, den2);
	num2 *= (signed long long)Pvar->assign.delta.num;
	OVERFLOW_CHECK(num2 > INT_MAX || num2 <= INT_MIN);

	num3 = (signed long long)D->delta.num * (signed long long)num;
	OVERFLOW_CHECK(num3 > INT_MAX || num3 <= INT_MIN);
	num3 *= (signed long long)Pvar->assign.delta.den;
	OVERFLOW_CHECK(num3 > INT_MAX || num3 <= INT_MIN);
	num3 = (num3 + sign) ^ sign;
	num2 -= num3;
	OVERFLOW_CHECK(num2 > INT_MAX || num2 <= INT_MIN);

	RETURN_IF_OVERFLOW();

	Pvar->assign.delta.num = (signed int)num2;
	Pvar->assign.delta.den = (unsigned int)den2;
	/*if (Pvar->assign.delta.num > SHRT_MAX ||
    Pvar->assign.delta.den > SHRT_MAX) */
	LArational_hw_normalize(&Pvar->assign.delta);
	if (!bound) {
		bound = (int)((signed long)num3 >> (sizeof(signed long) * CHAR_BIT - 1));
		/* bound is either -1 or 0, respectively for num3 > 0 and < 0 */
		/* only testing if bound == -1 is allowed in the rest of the code */
		assert(num3);
	}
#else /* MP_VERSION */
#ifdef SIMPLEX_COPY
	modified_insert(i);
#endif
	/* IMPROVE: when in MP mode, here is the bottleneck.
     reorganize computations so that GMP has fewer work? */
	/* TODO: v_i = v_i - D * num / den */
	/* (n_i', d_i') = (n_i, d_i) - (n_D, d_D) * num / den */

	/* tmp1 := num/den */
	mpq_set_num(store.var_update.tmp1, num);
	mpq_set_den(store.var_update.tmp1, den);
	mpq_canonicalize(store.var_update.tmp1);
	/* tmp2 := n_D * num/den */
	mpq_mul(store.var_update.tmp2, D->val, store.var_update.tmp1);
	/* n_i' = n_i - n_D * num / den */
	mpq_sub(Pvar->assign.val, Pvar->assign.val, store.var_update.tmp2);
	/* bound is -1, 0 or 1 if (n_D * num / den) is < 0, == 0, > 0 */
	bound = mpq_sgn(store.var_update.tmp2);
	/* tmp2 := d_D * num/den */
	mpq_mul(store.var_update.tmp2, D->delta, store.var_update.tmp1);
	/* d_i' = d_i - d_D * num / den */
	mpq_sub(Pvar->assign.delta, Pvar->assign.delta, store.var_update.tmp2);
	/* bound is -1, 0 or 1 if (D * num / den) is < 0, == 0, > 0 */
	if (!bound) bound = mpq_sgn(store.var_update.tmp2);
#endif
	/* TODO? assert(bound); */
	if (
		!Pvar->mark_unsat &&
		((bound == -1 && (Pvar->boundmask & MASK_BOUND_UPP) &&
			LAdelta_hw_less(&Pvar->bound[UPP], &Pvar->assign)) ||
		 (bound != -1 && (Pvar->boundmask & MASK_BOUND_LOW) &&
			LAdelta_hw_less(&Pvar->assign, &Pvar->bound[LOW])))) {
		Pvar->mark_unsat = true;
		heap_var_insert(i);
	}
}

/**
   \brief update non-basic variable
   \param i a variable
   \param v a delta value
   \remark variable i will be set to v */
static inline void
simplex_var_assign(Tsimplex_var i, TLAdelta_hw* v)
{
	LAdelta_hw_set(&simplex_var[i].assign, v);
#ifdef SIMPLEX_COPY
	modified_insert(i);
#endif
}

/**
   \brief check if bound is not reached
   \param i is a non-basic variable
   \param upp flag indicating if checking against upper bound (true) or
   lower bound (false)
   \return 1 iff non-basic variable i can be increased
   (upp = 1) or decreased (upp == 0) and remain within bound */
static inline int
simplex_var_notied_bound(Tsimplex_var i, bool upp)
{
	/* non-basic variables should be within bound, the test:
     pol?
     LAdelta_hw_less(&simplex_var[i].assign, &simplex_var[i].bound[1]):
     LAdelta_hw_less(&simplex_var[i].bound[0], &simplex_var[i].assign)
     can be rewritten as
     pol?
     LAdelta_hw_neq(&simplex_var[i].assign, &simplex_var[i].bound[1]):
     LAdelta_hw_neq(&simplex_var[i].bound[0], &simplex_var[i].assign)
     i.e.
     LAdelta_hw_neq(&simplex_var[i].assign, &simplex_var[i].bound[pol]) */
	assert(NONBASIC(i));
	return !(simplex_var[i].boundmask & (1 << upp)) ||
		!LAdelta_hw_eq(&simplex_var[i].assign, &simplex_var[i].bound[upp]);
}

/*
  --------------------------------------------------------------
  Linear expression
  --------------------------------------------------------------
*/

#ifdef DEBUG_SIMPLEX
static void linear_expr_print(const Trow row);
#endif

static void
linear_expr_normalize_hw(Trow row)
{
	unsigned i;
	LAsigned_hw_abs(store.normalize.tmp, ROW_COEF(row, 0));
	/* compute gcd */
	for (i = 1; LAsigned_hw_notone(store.normalize.tmp) && i < ROW_LEN(row);
			 ++i) {
		LAsigned_hw_abs(store.normalize.tmp2, ROW_COEF(row, i));
		LAsigned_hw_gcd(store.normalize.tmp, store.normalize.tmp2);
	}
	/* divide by gcd */
	if (LAsigned_hw_notone(store.normalize.tmp))
		for (i = 0; i < ROW_LEN(row); ++i)
			LAsigned_hw_divex(ROW_COEF(row, i), store.normalize.tmp);
}

/*
  Combination of linear expressions (and tableau rows) has three
  different versions, corresponding to
  - row_update_active: combines two tableau rows k and i into k, maintaining
  the monoms of row k in their columns (k is an active variable);
  - row_update_passive: combines two tableau rows k and i into k,
  but the monoms in k are not in the columns (k is a passive variable),
  except for the monom for variable k.
  - linear_expr_update: combines a linear expression L with a
  tableau row i, updating L. The monomes in L are not in the columns.
*/

/*
  \brief stores in store.row_update.row a.R - b.L[i], where
  R is formed by the suffix of monomes in *P starting from position
  offset.
*/

static void
row_update_aux(
	Trow* P, const Trow L_i, const TLAsigned_hw a, const TLAsigned_hw b,
	const unsigned offset)
{
	unsigned it = 0;
	unsigned it_i = 0;
	unsigned it_k = offset;
	assert(offset <= ROW_LEN(*P));
	assert(store.row_update.row->size == 0);
	/* printf("row_update_aux %u %u %u\n", ROW_LEN(*P), ROW_LEN(L_i), offset); */
	store.row_update.row =
		row_realloc(store.row_update.row, ROW_LEN(*P) + ROW_LEN(L_i) - offset);

	while (it_k < ROW_LEN(*P) || it_i < ROW_LEN(L_i)) {
		if (
			it_k == ROW_LEN(*P) ||
			(it_i < ROW_LEN(L_i) && ROW_VAR(L_i, it_i) < ROW_VAR(*P, it_k))) {
			row_inc(store.row_update.row);
			ROW_VAR(store.row_update.row, it) = ROW_VAR(L_i, it_i);
			LAsigned_hw_mult_neg(
				ROW_COEF(store.row_update.row, it), ROW_COEF(L_i, it_i), b);
			++it_i;
			++it;
		} else if (it_i == ROW_LEN(L_i) || ROW_VAR(*P, it_k) < ROW_VAR(L_i, it_i)) {
			row_inc(store.row_update.row);
			ROW_VAR(store.row_update.row, it) = ROW_VAR(*P, it_k);
			LAsigned_hw_set(ROW_COEF(store.row_update.row, it), ROW_COEF(*P, it_k));
			LAsigned_hw_mult(ROW_COEF(store.row_update.row, it), a);
			++it_k;
			++it;
		} else {
			assert(it_k < ROW_LEN(*P) && it_i < ROW_LEN(L_i));
			LAsigned_hw_set(store.row_update.a, ROW_COEF(*P, it_k));
			LAsigned_hw_com(store.row_update.a, ROW_COEF(L_i, it_i), a, b);
			if (LAsigned_hw_is_zero(store.row_update.a)) {
				++it_k;
				++it_i;
			} else {
				row_inc(store.row_update.row);
				ROW_VAR(store.row_update.row, it) = ROW_VAR(*P, it_k);
				LAsigned_hw_set(ROW_COEF(store.row_update.row, it), store.row_update.a);
				++it;
				++it_k;
				++it_i;
			}
		}
	}
}

/*
  \brief replace L[k] by a L[k] - b L[i] updating the C[j] accordingly
  time */
static void
row_update_active(
	Tsimplex_var k, Tsimplex_var i, TLAsigned_hw a, TLAsigned_hw b)
{
	Trow L_k, L_i;
	unsigned it_k, it_i, it;
	unsigned offset;

	L_k = ROW(k);
	L_i = ROW(i);

	offset = 0;
	while (ROW_VAR(L_k, offset) < ROW_VAR(L_i, 0)) {
		LAsigned_hw_mult(ROW_COEF(L_k, offset), a);
		++offset;
	}
	if (offset == ROW_LEN(L_k)) {
		L_k = row_realloc(L_k, ROW_LEN(L_k) + ROW_LEN(L_i));
		for (it_i = 0, it_k = offset; it_i < ROW_LEN(L_i); ++it_i, ++it_k) {
			ROW_ID(L_k, it_k) = k;
			ROW_VAR(L_k, it_k) = ROW_VAR(L_i, it_i);
			LAsigned_hw_mult_neg(ROW_COEF(L_k, it_k), ROW_COEF(L_i, it_i), b);
			mon_C_add(&stack_get(L_k, it_k));
		}
	} else {
		assert(store.row_update.row->size == 0);
		store.row_update.row =
			row_realloc(store.row_update.row, ROW_LEN(L_k) + ROW_LEN(L_i) - offset);

		for (it_k = offset; it_k < ROW_LEN(L_k); ++it_k)
			mon_C_rm(&stack_get(L_k, it_k));

		row_update_aux(&L_k, L_i, a, b, offset);

		L_k = row_realloc(L_k, offset + ROW_LEN(store.row_update.row));
		L_k->size = offset;
		for (it = 0, it_k = offset; it < ROW_LEN(store.row_update.row);
				 ++it, ++it_k) {
			row_inc(L_k);
			L_k->data[it_k].ln = NULL;
			L_k->data[it_k].lp = NULL;
			ROW_VAR(L_k, it_k) = ROW_VAR(store.row_update.row, it);
			ROW_ID(L_k, it_k) = k;
			LAsigned_hw_set(ROW_COEF(L_k, it_k), ROW_COEF(store.row_update.row, it));
			mon_C_add(&stack_get(L_k, it_k));
		}

		row_reset(store.row_update.row);
	}

	linear_expr_normalize_hw(L_k);
	ROW(k) = L_k;
}

/*
  \brief replace L[k] by a L[k] - b L[i], updating C[k]
  - k is a basic, passive variable
  - i is a basic variable
  \attention may change ROW(k)
*/
static void
row_update_passive(
	const Tsimplex_var k, const Tsimplex_var i, const TLAsigned_hw a,
	const TLAsigned_hw b)
{
	Trow L_k, L_i;
	unsigned it_k, it_i, it;
	unsigned offset;

	L_k = ROW(k);
	L_i = ROW(i);

	COL(k) = NULL; /* this assignment is to prepare for a later sanity check */

	assert(ROW_LEN(L_i) != 0);
	offset = 0;
	while (ROW_VAR(L_k, offset) < ROW_VAR(L_i, 0)) {
		LAsigned_hw_mult(ROW_COEF(L_k, offset), a);
		if (ROW_VAR(L_k, offset) == k) COL(k) = &stack_get(L_k, offset);
		++offset;
	}

	if (offset == ROW_LEN(L_k)) {
		L_k = row_realloc(L_k, ROW_LEN(L_k) + ROW_LEN(L_i));
		for (it_i = 0, it_k = offset; it_i < ROW_LEN(L_i); ++it_i, ++it_k) {
			ROW_VAR(L_k, it_k) = ROW_VAR(L_i, it_i);
			LAsigned_hw_mult_neg(ROW_COEF(L_k, it_k), ROW_COEF(L_i, it_i), b);
			if (ROW_VAR(L_k, it_k) == k) COL(k) = &stack_get(L_k, it_k);
		}
	} else {
		assert(store.row_update.row->size == 0);
		store.row_update.row =
			row_realloc(store.row_update.row, ROW_LEN(L_k) + ROW_LEN(L_i) - offset);

		it_k = offset;
		it_i = 0;
		it = 0;

		while (it_k < ROW_LEN(L_k) || it_i < ROW_LEN(L_i)) {
			if (
				it_k == ROW_LEN(L_k) ||
				(it_i < ROW_LEN(L_i) && ROW_VAR(L_i, it_i) < ROW_VAR(L_k, it_k))) {
				row_inc(store.row_update.row);
				ROW_VAR(store.row_update.row, it) = ROW_VAR(L_i, it_i);
				LAsigned_hw_mult_neg(
					ROW_COEF(store.row_update.row, it), ROW_COEF(L_i, it_i), b);
				++it_i;
				++it;
			} else if (
				it_i == ROW_LEN(L_i) || ROW_VAR(L_k, it_k) < ROW_VAR(L_i, it_i)) {
				row_inc(store.row_update.row);
				ROW_VAR(store.row_update.row, it) = ROW_VAR(L_k, it_k);
				LAsigned_hw_set(
					ROW_COEF(store.row_update.row, it), ROW_COEF(L_k, it_k));
				LAsigned_hw_mult(ROW_COEF(store.row_update.row, it), a);
				++it_k;
				++it;
			} else {
				assert(it_k < ROW_LEN(L_k) && it_i < ROW_LEN(L_i));
				LAsigned_hw_set(store.row_update.a, ROW_COEF(L_k, it_k));
				LAsigned_hw_com(store.row_update.a, ROW_COEF(L_i, it_i), a, b);
				if (!LAsigned_hw_is_zero(store.row_update.a)) {
					row_inc(store.row_update.row);
					ROW_VAR(store.row_update.row, it) = ROW_VAR(L_k, it_k);
					LAsigned_hw_set(
						ROW_COEF(store.row_update.row, it), store.row_update.a);
					++it;
				}
				++it_k;
				++it_i;
			}
		}
		L_k = row_realloc(L_k, offset + ROW_LEN(store.row_update.row));
		L_k->size = offset;
		for (it = 0, it_k = offset; it < ROW_LEN(store.row_update.row);
				 ++it, ++it_k) {
			row_inc(L_k);
			L_k->data[it_k].ln = NULL;
			L_k->data[it_k].lp = NULL;
			ROW_VAR(L_k, it_k) = ROW_VAR(store.row_update.row, it);
			ROW_ID(L_k, it_k) = k;
			LAsigned_hw_set(ROW_COEF(L_k, it_k), ROW_COEF(store.row_update.row, it));
			if (ROW_VAR(L_k, it_k) == k) COL(k) = &stack_get(L_k, it_k);
		}
		row_reset(store.row_update.row);
	}
	assert(COL(k) != NULL);
	linear_expr_normalize_hw(L_k);
	ROW(k) = L_k;
}

/*
  \brief replace row by a row - b L[i]
  - the row replaced is store.contraint_push.row
  - i is a basic variable */
static void
linear_expr_update(Tsimplex_var i, TLAsigned_hw a, TLAsigned_hw b)
{
	Trow* Prow = &store.constraint_push.row;
	Trow L_i;
	unsigned it_k, it_i, it;
	unsigned offset;

	L_i = ROW(i);

	offset = 0;
	while (ROW_VAR(*Prow, offset) < ROW_VAR(L_i, 0)) {
		LAsigned_hw_mult(ROW_COEF(*Prow, offset), a);
		++offset;
	}

	if (offset == ROW_LEN(*Prow)) {
		*Prow = row_realloc(*Prow, ROW_LEN(*Prow) + ROW_LEN(L_i));
		for (it_i = 0, it_k = offset; it_i < ROW_LEN(L_i); ++it_i, ++it_k) {
			(*Prow)->data[it_k].lp = NULL;
			(*Prow)->data[it_k].ln = NULL;
			ROW_ID(*Prow, it_k) = LAVAR_UNDEF;
			ROW_VAR(*Prow, it_k) = ROW_VAR(L_i, it_i);
			LAsigned_hw_mult_neg(ROW_COEF(*Prow, it_k), ROW_COEF(L_i, it_i), b);
		}
	} else {
		assert(store.row_update.row->size == 0);
		store.row_update.row =
			row_realloc(store.row_update.row, ROW_LEN(*Prow) + ROW_LEN(L_i) - offset);

		it_k = offset;
		it_i = 0;
		it = 0;

		while (it_k < ROW_LEN(*Prow) || it_i < ROW_LEN(L_i)) {
			if (
				it_k == ROW_LEN(*Prow) ||
				(it_i < ROW_LEN(L_i) && ROW_VAR(L_i, it_i) < ROW_VAR(*Prow, it_k))) {
				row_inc(store.row_update.row);
				ROW_VAR(store.row_update.row, it) = ROW_VAR(L_i, it_i);
				LAsigned_hw_mult_neg(
					ROW_COEF(store.row_update.row, it), ROW_COEF(L_i, it_i), b);
				++it_i;
				++it;
			} else if (
				it_i == ROW_LEN(L_i) || ROW_VAR(*Prow, it_k) < ROW_VAR(L_i, it_i)) {
				row_inc(store.row_update.row);
				ROW_VAR(store.row_update.row, it) = ROW_VAR(*Prow, it_k);
				LAsigned_hw_set(
					ROW_COEF(store.row_update.row, it), ROW_COEF(*Prow, it_k));
				LAsigned_hw_mult(ROW_COEF(store.row_update.row, it), a);
				++it_k;
				++it;
			} else {
				assert(it_k < ROW_LEN(*Prow) && it_i < ROW_LEN(L_i));
				LAsigned_hw_set(store.row_update.a, ROW_COEF(*Prow, it_k));
				LAsigned_hw_com(store.row_update.a, ROW_COEF(L_i, it_i), a, b);
				if (!LAsigned_hw_is_zero(store.row_update.a)) {
					row_inc(store.row_update.row);
					ROW_VAR(store.row_update.row, it) = ROW_VAR(*Prow, it_k);
					LAsigned_hw_set(
						ROW_COEF(store.row_update.row, it), store.row_update.a);
					++it;
				}
				++it_k;
				++it_i;
			}
		}
		*Prow = row_realloc(*Prow, offset + ROW_LEN(store.row_update.row));
		(*Prow)->size = offset;
		for (it = 0, it_k = offset; it < ROW_LEN(store.row_update.row);
				 ++it, ++it_k) {
			row_inc(*Prow);
			(*Prow)->data[it_k].lp = NULL;
			(*Prow)->data[it_k].ln = NULL;
			ROW_VAR(*Prow, it_k) = ROW_VAR(store.row_update.row, it);
			ROW_ID(*Prow, it_k) = LAVAR_UNDEF;
			LAsigned_hw_set(
				ROW_COEF(*Prow, it_k), ROW_COEF(store.row_update.row, it));
		}
		row_reset(store.row_update.row);
	}
	linear_expr_normalize_hw(*Prow);
}

/**
   \param var a simplex variable marked as passive
   \brief remove all basic variables from the equation defining var */
static void
update_passive_var(Tsimplex_var var)
{
	unsigned i;
	for (i = 0; i < ROW_LEN(ROW(var));) {
		Tsimplex_var bvar = ROW_VAR(ROW(var), i);
		if (bvar == var || bvar == ONE || NONBASIC(bvar)) {
			i++;
			continue;
		}
		if (PASSIVE(bvar)) update_passive_var(bvar);
		/* copy since mp number will be erased in row_update_passive */
		LAsigned_hw_set(store.eliminate_basic.tmp, COL(bvar)->a);
		LAsigned_hw_set(store.eliminate_basic.tmp2, ROW_COEF(ROW(var), i));
		row_update_passive(
			var, bvar, store.eliminate_basic.tmp, store.eliminate_basic.tmp2);
		i = (i < ROW_LEN(ROW(var))) ? i : ROW_LEN(ROW(var)) - 1;
		while (ROW_VAR(ROW(var), i) > bvar && i) i--;
	}
#ifdef DEBUG_SIMPLEX
	for (i = 0; i < ROW_LEN(ROW(var)); ++i) {
		Tsimplex_var v = ROW_VAR(ROW(var), i);
		assert(v == ONE || v == var || (ACTIVE(v) && !BASIC(v)));
	}
#endif
}

/**
   \param row a packed array of struct Smon
   \brief remove all basic variables from equation */
static void
linear_expr_eliminate_basic(void)
{
	unsigned i;
	Trow* P = &store.constraint_push.row;
	i = 0;
	while (i < ROW_LEN(*P)) {
		Tsimplex_var bvar = ROW_VAR(*P, i);
		if (BASIC(bvar)) {
			if (PASSIVE(bvar)) update_passive_var(bvar);
			LAsigned_hw_set(store.eliminate_basic.tmp, COL(bvar)->a);
			LAsigned_hw_set(store.eliminate_basic.tmp2, ROW_COEF(*P, i));
			linear_expr_update(
				bvar, store.eliminate_basic.tmp, store.eliminate_basic.tmp2);
			/* the previous call may add basic variable at any position in *P so
             one shall reset i to zero. */
			i = 0;
		} else
			++i;
	}
#ifdef DEBUG_SIMPLEX
	for (i = 0; i < ROW_LEN(*P); ++i) assert(NONBASIC(ROW_VAR(*P, i)));
#endif
	return;
}

/**
   \brief compute value of variable
   \param var variable
   \param[out] Pdelta pointer to the value */
static void
linear_expr_val(Tsimplex_var var, TLAdelta_hw* Pdelta)
{
	unsigned i, i_var = 0;
	Trow row = ROW(var);
	LAdelta_hw_set_zero(Pdelta);
	for (i = 0; i < ROW_LEN(row); ++i) {
		Tsimplex_var vi = ROW_VAR(row, i);
		if (vi != var)
			LAdelta_hw_addmult(Pdelta, &simplex_var[vi].assign, ROW_COEF(row, i));
		else
			i_var = i;
	}
	assert(ROW_VAR(row, i_var) == var);
	LAdelta_hw_div_opp(Pdelta, ROW_COEF(row, i_var));
}

#ifdef SIMPLEX_COPY
/**
   \brief compute BACKUP value of variable
   \param var variable
   \param[out] delta the value */
static void
linear_expr_val2(Tsimplex_var var, TLAdelta_hw* delta)
{
	unsigned i, i_var = 0;
	Trow row = ROW(var);
	LAdelta_hw_set_zero(delta);
	for (i = 0; i < ROW_LEN(row); ++i) {
		Tsimplex_var vi = ROW_VAR(row, i);
		if (vi != var)
			LAdelta_hw_addmult(delta, &simplex_var[vi].assign2, ROW_COEF(row, i));
		else
			i_var = i;
	}
	assert(ROW_VAR(row, i_var) == var);
	LAdelta_hw_div_opp(delta, ROW_COEF(row, i_var));
}
#endif

#ifdef DEBUG_SIMPLEX
/**
   \brief print linear expression
   \param m linear expression */

static void
linear_expr_print(const Trow row)
{
	unsigned i;
	assert(row);
	for (i = 0; i < ROW_LEN(row); ++i) {
		LAsigned_hw_print(ROW_COEF(row, i));
		if (ROW_VAR(row, i) != ONE) printf("v_%u", ROW_VAR(row, i));
		printf("%s", i + 1 < ROW_LEN(row) ? " + " : "");
	}
	printf("\n");
}
#endif

/*
  --------------------------------------------------------------
  Conflict
  --------------------------------------------------------------
*/

/* PF
   there are two ways to have a conflict:
   a new bound that contradicts an old one
   a variable that cannot be updated */

static bool simplex_conflict_direct =
	false; /*< a new bound contradicts old one */
static bool simplex_conflict_upper = false; /*< an upper bound */
static Tlit simplex_conflict_new_lit =
	LIT_UNDEF; /*< the literal producing the conflict */
static Tsimplex_var simplex_conflict_var =
	0; /*< the variable in the conflict */

static inline void
dispatch_reason(
	Tsimplex_var var, bool upper, Tstack_lit* Pconflict,
	Tstack_simplex_var* Pvar_eq)
{
	Tlit reason = simplex_var[var].reason[upper];
	/* reason is a literal, or an equality taken care of by LA interface,
     or a bound to branch around non-integral values for integer variables */
	if (reason)
		stack_push(*Pconflict, reason);
	else
		stack_push(*Pvar_eq, var);
}

void
simplex_hw_conflict(Tstack_lit* Pconflict, Tstack_simplex_var* Pvar_eq)
{
	assert(simplex_status == UNSAT); /* IMPROVE: UNIFY STATUS EVERYWHERE */
	if (simplex_conflict_direct) {
		if (simplex_conflict_new_lit)
			stack_push(*Pconflict, simplex_conflict_new_lit);
		else
			stack_push(*Pvar_eq, simplex_conflict_var);
	} else {
		unsigned i;
		Trow row = ROW(simplex_conflict_var);
		LAsigned_hw_set(store.conflict.c, COL(simplex_conflict_var)->a);
		i = 0;
		if (ROW_VAR(row, i) == ONE) ++i;
		while (i < ROW_LEN(row)) {
			if (ROW_VAR(row, i) != simplex_conflict_var) {
				bool opposite =
					LAsigned_hw_sign_diff(ROW_COEF(row, i), store.conflict.c);
				/* using xor to negate
                 LAsigned_hw_sign_diff(Pmon->a, store.conflict.c) when
                 simplex_conflict_upper is true */
				dispatch_reason(
					ROW_VAR(row, i), opposite ^ simplex_conflict_upper, Pconflict,
					Pvar_eq);
			}
			++i;
		}
	}
	dispatch_reason(
		simplex_conflict_var, simplex_conflict_upper, Pconflict, Pvar_eq);
}

static inline void
dispatch_reason_proof(
	Tsimplex_var var, bool upper, Tstack_lit* Pconflict,
	Tstack_simplex_var* Pvar_eq, Tstack_LAsigned_hw* Pconflict_coeffs,
	Tstack_LAsigned_hw* Peq_coeffs,	TLAsigned_hw coeff)
{
	Tlit reason = simplex_var[var].reason[upper];
	/* reason is a literal, or an equality taken care of by LA interface,
     or a bound to branch around non-integral values for integer variables */
	TLAsigned_hw b;
	LAsigned_hw_init(b);
	LAsigned_hw_abs(b, coeff);
	if (reason) {
		stack_push(*Pconflict, reason);

		stack_inc(*Pconflict_coeffs);
		LAsigned_hw_init(stack_top(*Pconflict_coeffs));
		LAsigned_hw_set(stack_top(*Pconflict_coeffs), b);
	} else {
		stack_push(*Pvar_eq, var);

		stack_inc(*Peq_coeffs);
		LAsigned_hw_init(stack_top(*Peq_coeffs));

		LAsigned_hw_set(stack_top(*Peq_coeffs), b);

		if(!upper)
			LAsigned_hw_set(stack_top(*Peq_coeffs), b);
		else
			LAsigned_hw_neg(stack_top(*Peq_coeffs), b);
	}
	LAsigned_hw_clear(b);
}

static inline void
push_num(Tstack_LAsigned_hw* Pcoeffs, TLAsigned_hw coeff)
{
	stack_inc(*Pcoeffs);
	LAsigned_hw_init(stack_top(*Pcoeffs));
	if (!LAsigned_hw_is_zero(coeff)) {
		LAsigned_hw_set(stack_top(*Pcoeffs), coeff);
	}
	else {
		LAsigned_hw_set_si(stack_top(*Pcoeffs), 1L);
	}
}

void
simplex_hw_conflict_proof(
	Tstack_lit* Pconflict, Tstack_simplex_var* Pvar_eq,
	Tstack_LAsigned_hw* Pcoeffs)
{
	Tstack_LAsigned_hw eq_coeffs = NULL;
	assert(simplex_status == UNSAT); /* IMPROVE: UNIFY STATUS EVERYWHERE */

	if (simplex_conflict_direct) {
		if (simplex_conflict_new_lit) {
			stack_push(*Pconflict, simplex_conflict_new_lit);
			push_num(Pcoeffs, store.conflict.c);
		}
		else {
			dispatch_reason_proof(
				simplex_conflict_var, simplex_conflict_upper, Pconflict, Pvar_eq,
				Pcoeffs, &eq_coeffs, store.conflict.c);

			push_num(Pcoeffs, store.conflict.c);
			stack_push(*Pvar_eq, simplex_conflict_var);
			return;
		}
	} else {
		stack_INIT(eq_coeffs);
		Trow row = ROW(simplex_conflict_var);
		LAsigned_hw_set(store.conflict.c, COL(simplex_conflict_var)->a);

		unsigned i = 0;
		if (ROW_VAR(row, i) == ONE) ++i;
		while (i < ROW_LEN(row)) {
			if (ROW_VAR(row, i) != simplex_conflict_var) {
				bool opposite =
					LAsigned_hw_sign_diff(ROW_COEF(row, i), store.conflict.c);
				dispatch_reason_proof(
					ROW_VAR(row, i), opposite ^ simplex_conflict_upper, Pconflict,
					Pvar_eq, Pcoeffs, &eq_coeffs, ROW_COEF(row, i));
			}
			++i;
		}
	}
	TLAsigned_hw c;
	LAsigned_hw_init(c);
	if (!LAsigned_hw_is_zero(store.conflict.c)) {
		LAsigned_hw_set(c, store.conflict.c);
		dispatch_reason_proof(
			simplex_conflict_var, simplex_conflict_upper, Pconflict, Pvar_eq,
			Pcoeffs, &eq_coeffs, c);
	}
	else {
		/* Is this correct? */
		LAsigned_hw_set_si(c, 1L);
		dispatch_reason_proof(
			simplex_conflict_var, simplex_conflict_upper, Pconflict, Pvar_eq,
			Pcoeffs, &eq_coeffs, c);
	}
	LAsigned_hw_clear(c);

	if(eq_coeffs) {
		/* Add the coefficients for equalities to the end of the coefficients */
		for (unsigned i = 0; i < stack_size(eq_coeffs); ++i) {
			stack_inc(*Pcoeffs);
			LAsigned_hw_init(stack_top(*Pcoeffs));
			LAsigned_hw_set(stack_top(*Pcoeffs), stack_get(eq_coeffs, i));
			LAsigned_hw_clear(stack_get(eq_coeffs, i));
		}
		stack_free(eq_coeffs);
	}

}

/*
  --------------------------------------------------------------
  Backtracking
  --------------------------------------------------------------
*/

enum
{
	ARITH_LOWER_BOUND = UNDO_ARITH,
	ARITH_UPPER_BOUND,
	ARITH_STATUS
};

typedef struct Tsimplex_bound
{
	Tsimplex_var var;
	TLAdelta_hw bound;
	Tlit reason;
#ifdef PEDANTIC
	unsigned char mask;
#else
	unsigned mask : 2;
#endif
} Tsimplex_bound;

static inline void
backtrack_bound(
	Tsimplex_bound* Psimplex_bound, Tsimplex_var var, Tlit reason,
	TLAdelta_hw* bound, unsigned char mask)
{
	Psimplex_bound->var = var;
	Psimplex_bound->reason = reason;
	LAdelta_hw_init(&Psimplex_bound->bound);
	LAdelta_hw_set(&Psimplex_bound->bound, bound);
	Psimplex_bound->mask = mask;
}

static void
backtrack_upper_bound(
	Tsimplex_var var, Tlit reason, TLAdelta_hw* bound, unsigned char mask)
{
	Tsimplex_bound* Psimplex_bound =
		(Tsimplex_bound*)undo_push(ARITH_UPPER_BOUND);
	backtrack_bound(Psimplex_bound, var, reason, bound, mask);
}

static void
backtrack_lower_bound(
	Tsimplex_var var, Tlit reason, TLAdelta_hw* bound, unsigned char mask)
{
	Tsimplex_bound* Psimplex_bound =
		(Tsimplex_bound*)undo_push(ARITH_LOWER_BOUND);
	backtrack_bound(Psimplex_bound, var, reason, bound, mask);
}

static void
backtrack_status(void)
{
	undo_push(ARITH_STATUS);
}

static inline void
arith_hook_set_bound(Tsimplex_bound* Psimplex_bound, unsigned B)
{
	LAdelta_hw_set(
		&simplex_var[Psimplex_bound->var].bound[B], &Psimplex_bound->bound);
	simplex_var[Psimplex_bound->var].boundmask = Psimplex_bound->mask;
	simplex_var[Psimplex_bound->var].reason[B] = Psimplex_bound->reason;
	LAdelta_hw_clear(&Psimplex_bound->bound);
}

static void
arith_hook_set_lower_bound(Tsimplex_bound* Psimplex_bound)
{
	assert(IS_BOUND_LOW(Psimplex_bound->var));
	arith_hook_set_bound(Psimplex_bound, LOW);
}

static void
arith_hook_set_upper_bound(Tsimplex_bound* Psimplex_bound)
{
	assert(IS_BOUND_UPP(Psimplex_bound->var));
	arith_hook_set_bound(Psimplex_bound, UPP);
}

static void
arith_hook_status(void* ignored)
{
	simplex_conflict_direct = false;
	/* simplex_conflict_new_lit = LIT_UNDEF;
     simplex_conflict_var = 0 */
	simplex_status = UNDEF;
#ifdef SIMPLEX_COPY
	modified_restore();
#endif
#ifdef DEBUG_SIMPLEX
	my_message("simplex backtracked\n");
#endif
}

static void
backtrack_init(void)
{
	undo_set_hook(
		ARITH_LOWER_BOUND, (Tundo_hook)arith_hook_set_lower_bound,
		sizeof(Tsimplex_bound));
	undo_set_hook(
		ARITH_UPPER_BOUND, (Tundo_hook)arith_hook_set_upper_bound,
		sizeof(Tsimplex_bound));
	undo_set_hook(ARITH_STATUS, (Tundo_hook)arith_hook_status, 0);
}

static void
backtrack_done(void)
{
	undo_unset_hook(ARITH_LOWER_BOUND);
	undo_unset_hook(ARITH_UPPER_BOUND);
	undo_unset_hook(ARITH_STATUS);
}

/*
  --------------------------------------------------------------
  Simplex internal
  --------------------------------------------------------------
*/

/**
   \brief update value of a non-basic variable, propagating
   change to dependent basic variables
   \param i non-basic variable that is being assigned
   \param v new value assigned to i */
static inline void
update(Tsimplex_var var, TLAdelta_hw* v)
{
	Tmon Pmon;
	LAdelta_hw_minus(&store.update.D, v, &(simplex_var[var].assign));
	if (LAdelta_hw_is_zero(&store.update.D)) return;
	Pmon = COL(var);
	while (Pmon) {
		Tsimplex_var basic = Pmon->l; /* a basic variable depending on var */
		Tmon Pmon2 = COL(basic);
		assert(BASIC(basic));
		/* basic occurs in a single row, where var also appears */
		assert(Pmon2->l == basic);
		assert(Pmon2->c == basic);
		simplex_var_update(basic, &store.update.D, Pmon->a, Pmon2->a);
		RETURN_IF_OVERFLOW();
		Pmon = Pmon->ln;
	}
	simplex_var_assign(var, v);
}

static void
pivot(Tsimplex_var i, Tsimplex_var j)
{
	Tmon Pmon;
	Trow L_j;
	unsigned k;
	assert(ROW_LEN(ROW(j)) == 0);
	assert(COL(i) && COL(i)->c == i && COL(i)->l == i && COL(i)->ln == NULL);
	assert(BASIC(i) && NONBASIC(j));
	simplex_var[i].basic = false;
	simplex_var[j].basic = true;
	assert(ROW(j));
	L_j = ROW(j);
	stack_set(ROWS, j, ROW(i));
	stack_set(ROWS, i, L_j);
	L_j = ROW(j);
	for (k = 0; k < ROW_LEN(L_j); ++k) {
		if (ROW_VAR(L_j, k) == j)
			LAsigned_hw_set(store.pivot.diag, ROW_COEF(L_j, k)); /* diag = a_jj */
		ROW_ID(L_j, k) = j;
	}
	Pmon = COL(j);
	while (Pmon) {
		Tsimplex_var k = Pmon->l;
		LAsigned_hw_set(store.pivot.c, Pmon->a);
		assert(Pmon->c == j);
		Pmon = Pmon->ln;
		if (k != j) row_update_active(k, j, store.pivot.diag, store.pivot.c);
	}
}

/**
   \brief exchange role of a basic and a non-basic variable, and update
   \param i basic variable
   \param j non-basic variable
   \param v value of variable i */
static void
pivot_and_update(Tsimplex_var i, Tsimplex_var j, TLAdelta_hw* v)
{
#ifdef DEBUG_SIMPLEX
	my_message("pivot_and_update %u %u\n", i, j);
#endif
	simplex_pivot++;
	pivot(i, j);
	update(i, v);
}

/**
   \brief tests if basic var i may be pivoted with non-basic var j
   \param i basic variable
   \param j non-basic variable
   \param ci coefficient of i
   \param cj coefficient of j
   \param upp flags if basic var i violates its upper bound (true, UPP)
   or its lower bound (false, LOW) */

static inline bool
pivot_candidate(
	Tsimplex_var i, Tsimplex_var j, TLAsigned_hw ci, TLAsigned_hw cj, bool upp)
{
	if (i == j) return false;
	if (LAsigned_hw_sign_diff(ci, cj)) upp = !upp;
	return simplex_var_notied_bound(j, upp);
}

/**
   \brief selects a non-baisc variable to pivot with a given basic variable
   according to Bland's rule (smallest in variable order).
   \param v basic variable
   \param ci coefficient of i in defining equation
   \param upp flags if basic variable i violates its upper bound
   (true, UPP) or its lower bound (false, LOW) */

static inline Tsimplex_var
pivot_select_Bland_s_rule(Tsimplex_var v, /*TLAsigned_hw ci,*/ bool upp)
{
	unsigned i;
	Trow row = ROW(v);
	i = 0;
	/* Skip constant term */
	if (ROW_VAR(row, i) == ONE) ++i;
	while (i < ROW_LEN(row)) {
		if (pivot_candidate(
					v, ROW_VAR(row, i), store.solve.c, ROW_COEF(row, i), upp))
			return ROW_VAR(row, i);
		++i;
	}

	return LAVAR_UNDEF;
}

/**
   \brief heuristic selection of a non-basic variable to pivot with a
   given basic variable (appearing in fewer lines).
   \param i basic variable
   \param ci coefficient of i in defining equation
   \param upp flags if basic variable i violates its upper bound
   (true, UPP) or its lower bound (false, LOW) */

static inline Tsimplex_var
pivot_select_heuristics(Tsimplex_var v, TLAsigned_hw ci, bool upp)
{
	Trow row = ROW(v);
	Tsimplex_var j = LAVAR_UNDEF;
	unsigned min;
	unsigned i;
	i = 0;
	/* Skip constant term */
	if (ROW_VAR(row, i) == ONE) ++i;
	while (i < ROW_LEN(row)) {
		Tsimplex_var vi = ROW_VAR(row, i);
		if (pivot_candidate(v, vi, ci, ROW_COEF(row, i), upp)) {
			if (j == LAVAR_UNDEF) {
				j = vi;
				min = simplex_var[vi].row_count;
			} else {
				unsigned tmp = simplex_var[vi].row_count;
				if (tmp < min) {
					j = vi;
					min = tmp;
				}
			}
		}
		++i;
	}
	return j;
}

/**
   \param i basic variable
   \param ci coefficient of i in defining equation
   \param Pnb_h_choices address of the counter of heuristic choices
   \param upp flags if basic variable i violates its upper bound
   (true, UPP) or its lower bound (false, LOW)
   \remark This routine is part of simplex_hw_solve */

static inline void
solve_bound(
	Tsimplex_var i, /*TLAsigned_hw ci,*/
	unsigned* Pnb_h_choices, bool upp)
{
	Tsimplex_var j;
	if (simplex_disable_pivot_heuristics || *Pnb_h_choices == simplex_var_n) {
		j = pivot_select_Bland_s_rule(i, upp);
	} else {
		j = pivot_select_heuristics(i, store.solve.c, upp);
		++*Pnb_h_choices;
		if (j != LAVAR_UNDEF) simplex_pivot_heuristics++;
	}
	if (j != LAVAR_UNDEF) {
		pivot_and_update(i, j, &simplex_var[i].bound[upp]);
	} else {
		backtrack_status();
#ifndef SIMPLEX_COPY
		if (!simplex_var[i].mark_unsat) {
			simplex_var[i].mark_unsat = true;
			heap_var_insert(i);
		}
#endif
		simplex_conflict_var = i;
		simplex_conflict_upper = upp;
		simplex_status = UNSAT;
#ifdef DEBUG_SIMPLEX
		LA_print();
		my_message("simplex_hw_solve returned UNSAT\n");
#endif
	}
}

/*
  --------------------------------------------------------------
  Simplex interface
  --------------------------------------------------------------
*/
/**
   \brief find an assignment satisfying all constraints
   \return SAT or UNSAT depending of the status */

Tstatus
simplex_hw_solve(void)
{
	Tsimplex_var i;
	unsigned nb_heuristic_choices = 0;
	if (simplex_status == UNSAT) return simplex_status;
#ifdef DEBUG_SIMPLEX
	my_message("simplex_hw_solve\n");
	CHECK_INVARIANT;
#endif
	while (1) {
		RETURN_IF_OVERFLOW(UNDEF);
		if (heap_var_empty()) break;
		i = heap_var_remove_min();
		assert(BASIC(i) && simplex_var[i].mark_unsat);
		simplex_var[i].mark_unsat = false;
		/* TODO: check if store.solve.c is really needed */
		LAsigned_hw_set(store.solve.c, COL(i)->a);
		/* IMPROVE
         if i is an integer variable, the value shall be integer */
		if (
			IS_BOUND_LOW(i) &&
			LAdelta_hw_less(&simplex_var[i].assign, &simplex_var[i].bound[LOW]))
			solve_bound(i, /*store.solve.c,*/ &nb_heuristic_choices, LOW);
		else if (
			IS_BOUND_UPP(i) &&
			LAdelta_hw_less(&simplex_var[i].bound[UPP], &simplex_var[i].assign))
			solve_bound(i, /*store.solve.c,*/ &nb_heuristic_choices, UPP);
		if (simplex_status == UNSAT) return simplex_status;
	}
	RETURN_IF_OVERFLOW(UNDEF);
	simplex_status = SAT;
#ifdef SIMPLEX_COPY
	modified_save();
#endif
	CHECK_INVARIANT;
#ifdef DEBUG_SIMPLEX
	my_message("simplex_hw_solve returned SAT\n");
#endif
	return simplex_status;
}

void
simplex_hw_repair(void)
{
#ifdef SIMPLEX_COPY
	modified_restore();
#endif
}

Tstatus
simplex_hw_assert_upper_bound(Tsimplex_var i, TLAdelta_hw delta, Tlit reason)
{
	if (simplex_status == UNSAT) return simplex_status;
#ifdef DEBUG_SIMPLEX
	printf("\nsimplex_hw_assert_upper_bound v_%u (reason %d)\n", i, reason);
	LAdelta_hw_print(&delta);
	printf("\n");
#endif
	assert(simplex_var[i].bounded);
	simplex_status = UNDEF;

	assert(!simplex_hw_var_integer(i) || LAdelta_hw_is_int(&delta));
	if (IS_BOUND_UPP(i) && LAdelta_hw_leq(&simplex_var[i].bound[UPP], &delta))
		return simplex_status;
	if (IS_BOUND_LOW(i) && LAdelta_hw_less(&delta, &simplex_var[i].bound[LOW])) {
		backtrack_status();
		simplex_conflict_direct = true;
		simplex_conflict_var = i;
		simplex_conflict_new_lit = reason;
		simplex_conflict_upper = 0;
		return (simplex_status = UNSAT);
	}
	CHECK_INVARIANT;
	if (PASSIVE(i)) {
		Trow row;
		unsigned j;
		update_passive_var(i);
#ifdef SIMPLEX_COPY
		modified_insert(i);
#endif
		linear_expr_val(i, &simplex_var[i].assign);
#ifdef SIMPLEX_COPY
		linear_expr_val2(i, &simplex_var[i].assign2);
#endif
		simplex_var_n_unbound--;
		row = ROW(i);
		for (j = 0; j < ROW_LEN(row); ++j) {
			ROW_ID(row, j) = i;
			if (ROW_VAR(row, j) != i) /* We keep the variable in its column */
			{
				simplex_var_n_unbound -= !ROW_VAR(row, j);
				mon_C_add(&stack_get(row, j));
			}
		}
		simplex_var[i].passive = false;
		CHECK_INVARIANT;
	}
	if (LAdelta_hw_less(&delta, &simplex_var[i].assign)) {
		if (BASIC(i)) {
			if (!simplex_var[i].mark_unsat) {
				simplex_var[i].mark_unsat = true;
				heap_var_insert(i);
			}
		} else
			update(i, &delta);
		RETURN_IF_OVERFLOW(UNDEF);
	}
	backtrack_upper_bound(
		i, simplex_var[i].reason[UPP], &simplex_var[i].bound[UPP],
		simplex_var[i].boundmask);
	LAdelta_hw_set(&simplex_var[i].bound[UPP], &delta);
	simplex_var[i].reason[UPP] = reason;
	simplex_var[i].boundmask |= MASK_BOUND_UPP;
	CHECK_INVARIANT;
	return simplex_status;
}

Tstatus
simplex_hw_assert_lower_bound(Tsimplex_var i, TLAdelta_hw delta, Tlit reason)
{
	if (simplex_status == UNSAT) return simplex_status;
#ifdef DEBUG_SIMPLEX
	CHECK_INVARIANT;
	printf("\nsimplex_hw_assert_lower_bound v_%d (reason %d)\n", i, reason);
	LAdelta_hw_print(&delta);
	printf("\n");
#endif
	assert(simplex_var[i].bounded);
	simplex_status = UNDEF;

	assert(!simplex_hw_var_integer(i) || LAdelta_hw_is_int(&delta));
	if (IS_BOUND_LOW(i) && LAdelta_hw_leq(&delta, &simplex_var[i].bound[LOW]))
		return simplex_status;
	if (IS_BOUND_UPP(i) && LAdelta_hw_less(&simplex_var[i].bound[UPP], &delta)) {
		backtrack_status();
		simplex_conflict_direct = true;
		simplex_conflict_var = i;
		simplex_conflict_new_lit = reason;
		simplex_conflict_upper = 1;
		return (simplex_status = UNSAT);
	}
	CHECK_INVARIANT;
	if (PASSIVE(i)) {
		Trow row;
		unsigned j;
		update_passive_var(i);
#ifdef SIMPLEX_COPY
		modified_insert(i);
#endif
		linear_expr_val(i, &simplex_var[i].assign);
#ifdef SIMPLEX_COPY
		linear_expr_val2(i, &simplex_var[i].assign2);
#endif
		simplex_var_n_unbound--;
		row = ROW(i);
		for (j = 0; j < ROW_LEN(row); ++j) {
			ROW_ID(row, j) = i;
			if (ROW_VAR(row, j) != i) /* We keep the variable in its column */
			{
				simplex_var_n_unbound -= !ROW_VAR(row, j);
				mon_C_add(&stack_get(row, j));
			}
		}
		simplex_var[i].passive = false;
		CHECK_INVARIANT;
	}
	if (LAdelta_hw_less(&simplex_var[i].assign, &delta)) {
		if (BASIC(i)) {
			if (!simplex_var[i].mark_unsat) {
				simplex_var[i].mark_unsat = true;
				heap_var_insert(i);
			}
		} else
			update(i, &delta);
		RETURN_IF_OVERFLOW(UNDEF);
	}
	backtrack_lower_bound(
		i, simplex_var[i].reason[LOW], &simplex_var[i].bound[LOW],
		simplex_var[i].boundmask);
	LAdelta_hw_set(&simplex_var[i].bound[LOW], &delta);
	simplex_var[i].reason[LOW] = reason;
	simplex_var[i].boundmask |= MASK_BOUND_LOW;
	CHECK_INVARIANT;
	return simplex_status;
}

/**
   \brief assert a new equality
   \remark destructive for m
   \remark simplex_solve() is required for completeness if UNDEF is returned */
static void
assert_equality(void)
{
	Tsimplex_var v;
	Trow row_v;
	unsigned j, pv;

	/* Eliminate basic variables. */
	linear_expr_eliminate_basic();
	/* 0 = 0 */
	if (ROW_LEN(store.constraint_push.row) == 0) return;
	/* ct = 0, with ct being non zero number */
	if (
		ROW_LEN(store.constraint_push.row) == 1 &&
		ROW_VAR(store.constraint_push.row, 0) == ONE) {
		backtrack_status();
		simplex_status = UNSAT;
		return;
	}
	j = ROW_VAR(store.constraint_push.row, 0) == ONE ? 1 : 0;

	/* Choose one non-basic variable v and make it basic */
	{
		unsigned k;
		unsigned min;
		pv = j;
		v = ROW_VAR(store.constraint_push.row, j);
		min = simplex_var[v].row_count;
		for (k = j + 1; k < ROW_LEN(store.constraint_push.row); ++k) {
			Tsimplex_var w = ROW_VAR(store.constraint_push.row, k);
			unsigned count = simplex_var[w].row_count;
			if (count < min) {
				min = count;
				pv = k;
				v = w;
			}
		}
	}
	simplex_var[v].basic = true;

	/* TODO: check if store.assert_equality.diag is really necessary */
	LAsigned_hw_set(
		store.assert_equality.diag, ROW_COEF(store.constraint_push.row, pv));
	ROW(v) = row_realloc(ROW(v), ROW_LEN(store.constraint_push.row));
	row_v = ROW(v);
	assert(ROW_LEN(row_v) == 0);
	for (j = 0; j < ROW_LEN(store.constraint_push.row); ++j) {
		stack_inc(row_v);
		ROW_VAR(row_v, j) = ROW_VAR(store.constraint_push.row, j);
		ROW_ID(row_v, j) = v;
		LAsigned_hw_set(ROW_COEF(row_v, j), ROW_COEF(store.constraint_push.row, j));
		mon_C_add(&stack_get(row_v, j));
	}
	assert(COL(v)->l == v);

	/* Compute value of i and store it into delta. */
	linear_expr_val(v, &store.assert_equality.delta);
	update(v, &store.assert_equality.delta);
	RETURN_IF_OVERFLOW();

	/* Remove variable v from all constraints (but its definition). */
	{
		Tmon Pmon = COL(v);
		assert(Pmon->l == v);
		Pmon = Pmon->ln;
		while (Pmon) {
			Tsimplex_var k = Pmon->l; /* Basic var k definition is modified. */
			/* TODO: check if store.assert_equality.c is really necessary */
			LAsigned_hw_set(store.assert_equality.c, Pmon->a); /* c = a_kj*/
			assert(k && k != v);
			Pmon = Pmon->ln; /* must be before call to linear_expr_update.*/
			/* L_k = a_jj L_k - a_kj L_j */
			row_update_active(
				k, v, store.assert_equality.diag, store.assert_equality.c);
		}
	}
	if (
		!simplex_var[v].mark_unsat &&
		((IS_BOUND_LOW(v) &&
			LAdelta_hw_less(&simplex_var[v].assign, &simplex_var[v].bound[LOW])) ||
		 (IS_BOUND_UPP(v) &&
			LAdelta_hw_less(&simplex_var[v].bound[UPP], &simplex_var[v].assign)))) {
		simplex_var[v].mark_unsat = true;
		heap_var_insert(v);
	}
	if (!heap_var_empty()) simplex_status = UNDEF;
#ifdef SIMPLEX_COPY
	else
		modified_save();
#endif
}

/*
  --------------------------------------------------------------
  Internal interface
  --------------------------------------------------------------
*/

void
simplex_hw_constraint_push(unsigned n, Tsimplex_var* vars, TLAsigned_hw* coefs)
{
	unsigned i;
	assert(simplex_status != UNSAT);
	if (!n) my_error("simplex_hw_constraint_push: inconsistency\n");
	store.constraint_push.row = row_realloc(store.constraint_push.row, n);
	row_reset(store.constraint_push.row);
	for (i = 0; i < n; ++i) {
		Tmon P;
		stack_inc(store.constraint_push.row);
		P = &stack_get(store.constraint_push.row, i);
		P->l = LAVAR_UNDEF;
		P->c = vars[i];
		LAsigned_hw_set(P->a, coefs[i]);
		P->ln = P->lp = NULL;
	}
#ifdef DEBUG_SIMPLEX
	printf("simplex_hw_constraint_push\n");
	linear_expr_print(store.constraint_push.row);
#endif
	assert_equality();
	RETURN_IF_OVERFLOW();
	CHECK_INVARIANT;
}

/*
  --------------------------------------------------------------
  Simplification
  --------------------------------------------------------------
*/

/*
  If two dummy (slack) variables x, y, are such that a x + b y = c, then one of
  them (e.g.x) can be replaced by the other, and be removed for ever, all
  references to x updated to refer to y

  If two non-dummy variables x, y are equal, then one of them can be replaced by
  the other, and be removed for ever, with other DPs being notified of equality

  If a dummy variable x and a non-dummy variable y are such that a x + b y = c,
  then x can be removed for ever, all references to x updated to refer to y

  If a dummy variable x is such that a x = c, then x can be removed; if the same
  occurs for a non-dummy variable, not much can be said

  If a variable is such that a <= x and x <= a, then x can be replaced by a

  To check if two variables are such that a x + b y = c,
  - if one is basic and the other is non-basic, it suffices to check 2
  variables contraints
  - if both are basic, if comes to check if they belong to constraints
  a L + b x + c = 0 and a'L + b'y + c' = 0
  - the case where both are non-basic cannot happen
*/

void
simplex_hw_simp_unbound(void)
{
	Tsimplex_var i, k;
#ifdef DEBUG_SIMPLEX
	printf("simplex_hw_simp_unbound\n");
	LA_print();
#endif
	/* First put all non-bounded variables as basic ones */
	for (i = 1; i < simplex_var_n; i++) {
		Tmon Pmon;
		/* Check all non-basic non-bounded active variables */
		if (PASSIVE(i) || simplex_var[i].bounded || BASIC(i)) continue;
		for (Pmon = COL(i); Pmon != NULL; Pmon = Pmon->ln) {
			k = Pmon->l; /* k is a basic variable... */
			if (simplex_var[k].bounded) break; /* ... and it is bounded */
		}
		if (Pmon) pivot(k, i);
	}
	/* Then remove all basic non-bounded variables from the game */
	for (i = 1; i < simplex_var_n; i++)
		if (ACTIVE(i) && !simplex_var[i].bounded && BASIC(i)) {
			Trow row = ROW(i);
			unsigned j;
			simplex_var_n_unbound++;
			simplex_var[i].passive = true;
			for (j = 0; j < ROW_LEN(row); ++j)
				if (ROW_VAR(row, j) != i) /* We keep the variable in its column */
				{
					mon_C_rm(&stack_get(row, j));
					simplex_var_n_unbound += (COL(ROW_VAR(row, j)) == 0);
				}
		}
#ifdef DEBUG_SIMPLEX
	LA_print();
#endif
}

void
simplex_hw_simp_const(void)
{
	Tsimplex_var var;
#ifdef DEBUG_SIMPLEX
	printf("simplex_hw_simp_const\n");
	LA_print();
#endif
	/* First put all basic, fixed variables as non-basic ones */
	for (var = 1; var < simplex_var_n; ++var) {
		Trow row;
		Tsimplex_var var2;
		unsigned i;
		/* Skip passive, non-basic, non-fixed variables */
		if (PASSIVE(var) || NONBASIC(var) || !simplex_hw_var_fixed(var)) continue;
		/* var is active, basic and has a fixed value */
		row = ROW(var);
		/* find a non-basic, non-fixed variable to pivot var with */
		for (i = 0; i < ROW_LEN(row); ++i) {
			var2 = ROW_VAR(row, i);
			if (var2 == ONE || var2 == var) continue;
			/* var2 is a non-basic variable... */
			if (!simplex_hw_var_fixed(var2)) break; /* ... and it is not fixed */
		}
		if (i != ROW_LEN(row)) pivot(var, var2);
	}
	/* Then remove all non-basic fixed variables from the game */
	for (var = 1; var < simplex_var_n; var++)
		if (ACTIVE(var) && NONBASIC(var) && simplex_hw_var_fixed(var)) {
			Tmon Pmon, Pmon2;
			Trow row = ROW(var);
			assert(ROW_LEN(row) == 0);
			simplex_var_n_const++;
			/* a * v_0 - b v_i = 0 if v_i = a / b */
			if (LAdelta_hw_is_zero(
						&simplex_var[var].assign)) { /* variable assignment is 0 */
				row_inc(row);
				ROW_VAR(row, ONE) = var;
				ROW_ID(row, ONE) = var;
				/*      LAsigned_hw_init(ROW_COEF(row, ONE)); */
				LAsigned_hw_set_si(ROW_COEF(row, ONE), 1L);
				mon_C_add(&stack_get(row, ONE));
			} else {
#ifdef HW_VERSION
				TLAsigned_hw num, den;
#else
				mpz_ptr num, den;
				/* mpz is typedefed mpz_struct mpz[1] so this is not a lvalue */
#endif
				row_inc(row);
				ROW_VAR(row, ONE) = ONE;
				ROW_ID(row, ONE) = var;
				LAdelta_hw_get_rat(simplex_var[var].assign, num, den);
				/* LAsigned_hw_init(ROW_COEF(row, ONE)); */
				LAsigned_hw_set(ROW_COEF(row, ONE), num);
				LAsigned_hw_neg(ROW_COEF(row, ONE), ROW_COEF(row, ONE));
				row_inc(row);
				ROW_VAR(row, 1) = var;
				ROW_ID(row, 1) = var;
				/* LAsigned_hw_init(ROW_COEF(row, 1)); */
				LAsigned_hw_set(ROW_COEF(row, 1), den);
				mon_C_add(&stack_get(row, 1));
			}
			simplex_var[var].basic = true;

			for (Pmon = COL(var); Pmon != NULL; Pmon = Pmon2) {
				Tsimplex_var var2 = Pmon->l;
				Pmon2 = Pmon->ln;
				if (var2 != var) /* We keep the variable in its row */
				{
					LAsigned_hw_set(store.assert_equality.diag, COL(var)->a);
					LAsigned_hw_set(store.assert_equality.c, Pmon->a);
					row_update_active(
						var2, var, store.assert_equality.diag, store.assert_equality.c);
					if (
						ROW_LEN(ROW(var2)) == 1 ||
						(ROW_VAR(ROW(var2), 0) == ONE && ROW_LEN(ROW(var2)) == 2))
						simplex_var_n_const += 1;
				}
			}
		}
#ifdef DEBUG_SIMPLEX
	printf("[END] simplex_hw_simp_const\n");
	LA_print();
#endif
}

void
simplex_hw_update_assign(void)
{
	Tsimplex_var i;
	for (i = 1; i < simplex_var_n; i++)
		if (PASSIVE(i)) {
			update_passive_var(i);
			linear_expr_val(i, &simplex_var[i].assign);
		}
}

/*
  --------------------------------------------------------------
  Public interface
  --------------------------------------------------------------
*/

void
simplex_hw_init(void)
{
	if (
		(-1 >> 1) != -1 || sizeof(long long) != 2 * sizeof(int) ||
		((((unsigned long)-253) >> (sizeof(signed long) * CHAR_BIT - 1)) != 1))
		my_error("Linear arithmetic incompatible with compiler or architecture\n");
	store_init();
	simplex_var_init();
	heap_var_init();
#ifdef SIMPLEX_COPY
	modified_init();
#endif
	backtrack_init();
	stack_INIT(integer_stack);
	options_new(
		0, "disable-pivot-heuristics",
		"disable variable selection heuristic for simplex pivoting",
		&simplex_disable_pivot_heuristics);
}

void
simplex_hw_done(void)
{
	stats_unsigned(
		"simplex_var_hw", "Number of variables for simplex (HW version)", "%9u",
		simplex_var_n);
	stats_unsigned(
		"simplex_var_hw_unbound",
		"Number of unbounded eliminated variables for simplex (HW version)", "%9u",
		simplex_var_n_unbound);
	stats_unsigned(
		"simplex_var_hw_const",
		"Number of constant eliminated variables for simplex (HW version)", "%9u",
		simplex_var_n_const);
	stats_unsigned(
		"simp_HW_piv", "simplex HW pivoting operations", "%9u", simplex_pivot);
	stats_unsigned(
		"simp_HW_piv_h", "simplex HW pivot heuristics", "%9u",
		simplex_pivot_heuristics);
	stack_free(integer_stack);
	backtrack_done();
	heap_var_done();
	simplex_var_done();
#ifdef SIMPLEX_COPY
	modified_done();
#endif
	store_done();
}

/*
  --------------------------------------------------------------
  Invariant
  --------------------------------------------------------------
*/

#ifdef DEBUG_SIMPLEX
/* \brief check that column for variable v is well-formed */
static void
LA_invariant_column(const Tsimplex_var v)
{
	Tmon Ci = COL(v);
	/* First element in column should have no previous element */
	assert(Ci == NULL || Ci->lp == NULL);
	if (BASIC(v)) {
		assert(Ci != NULL);
		assert(Ci->ln == NULL);
		assert(Ci->c == v);
		assert(Ci->l == v);
	} else {
		Tmon Pmon;
		for (Pmon = COL(v); Pmon != NULL; Pmon = Pmon->ln) {
			/* Column C[v] only contains monoms with column index v */
			assert(Pmon->c == v);
			if (Pmon->ln != NULL) assert(Pmon->ln->lp == Pmon);
			if (Pmon->lp != NULL) assert(Pmon->lp->ln == Pmon);
		}
	}
}

static void
LA_invariant(void)
{
	RETURN_IF_OVERFLOW();
	Tsimplex_var i;
	for (i = 1; i < simplex_var_n; ++i) LA_invariant_column(i);
	for (i = 1; i < simplex_var_n; i++) {
		Trow Li = ROW(i);
		unsigned k;
		unsigned tmp = 0;
		/* Basic variables should be refered in one and only one constraint */
		if (BASIC(i)) assert(ROW_LEN(Li) > 0);
		if (PASSIVE(i)) {
			for (k = 0; k < ROW_LEN(Li); ++k) {
				/* L[i] only contains monoms with line index i */
				assert(stack_get(Li, k).lp == NULL);
				assert(stack_get(Li, k).ln == NULL);
				/* L[i] contains monoms sorted with respect to column index */
				if (k + 1 < ROW_LEN(Li)) assert(ROW_VAR(Li, k) < ROW_VAR(Li, k + 1));
			}
			continue;
		}
		if (Li)
			for (k = 0; k < ROW_LEN(Li); ++k) {
				/* Line L[i] only contains monoms with line index i */
				assert(ROW_ID(Li, k) == i);
				/* Line L[i] contains monoms sorted with respect to column index */
				if (k + 1 < ROW_LEN(Li)) assert(ROW_VAR(Li, k) < ROW_VAR(Li, k + 1));
				tmp += simplex_var[ROW_VAR(Li, k)].basic;
			}
		/* Line L[i] should countain at most one basic variable */
		assert(!Li || ROW_LEN(Li) == 0 || tmp == 1);
		/* Bounds should be consistent */
		if (simplex_var[i].boundmask == 3)
			assert(
				LAdelta_hw_leq(&simplex_var[i].bound[LOW], &simplex_var[i].bound[UPP]));
		/* if SAT status, no variable should be marked (unsat) */
		if (simplex_status == SAT) {
			assert(!simplex_var[i].mark_unsat);
#ifdef SIMPLEX_COPY
			assert(!simplex_var[i].mark);
#endif
		}
		if (NONBASIC(i) || !simplex_var[i].mark_unsat) {
			/* Nonbasic vars should be within bounds */
			/* Basic vars should be within bounds or marked unsat */
			if (IS_BOUND_LOW(i))
				assert(
					LAdelta_hw_leq(&simplex_var[i].bound[LOW], &simplex_var[i].assign));
			if (IS_BOUND_UPP(i))
				assert(
					LAdelta_hw_leq(&simplex_var[i].assign, &simplex_var[i].bound[UPP]));
		}
#ifdef SIMPLEX_COPY
		/* non marked variables should have backup and current assign equal */
		if (!simplex_var[i].mark)
			assert(LAdelta_hw_eq(&simplex_var[i].assign, &simplex_var[i].assign2));
#endif
		/* equality constraints should always be satisfied */
		if (BASIC(i) && ACTIVE(i)) {
			TLAdelta_hw delta;
			LAdelta_hw_init(&delta);
			LAdelta_hw_set_zero(&delta);
			for (k = 0; k < ROW_LEN(Li); ++k)
				LAdelta_hw_addmult(
					&delta, &simplex_var[ROW_VAR(Li, k)].assign, ROW_COEF(Li, k));
			assert(LAdelta_hw_is_zero(&delta));
			LAdelta_hw_clear(&delta);
		}
	}
}
#endif

/*
  --------------------------------------------------------------
  Tests
  --------------------------------------------------------------
*/

#ifdef DEBUG_SIMPLEX
static void
LA_print(void)
{
	Tsimplex_var i;
	printf("=== SIMPLEX_hw ===\n");
	for (i = 1; i < simplex_var_n; i++)
		if (BASIC(i)) {
			printf("v_%d %s: ", i, (i < 10) ? "  " : (i < 100) ? " " : "");
			linear_expr_print(ROW(i));
		}
	for (i = 1; i < simplex_var_n; i++) {
		Tmon P = COL(i);
		printf("v_%u ==>", i);
		while (P) {
			printf(" v_%u", P->l);
			P = P->ln;
		}
		printf("\n");
	}
	for (i = 1; i < simplex_var_n; i++) {
		printf("v_%d %s= ", i, (i < 10) ? "  " : (i < 100) ? " " : "");
		LAdelta_hw_print(&simplex_var[i].assign);
#ifdef SIMPLEX_COPY
		printf(" (");
		LAdelta_hw_print(&simplex_var[i].assign2);
		printf(")");
#endif
		printf(
			" \t{%u,%u}\t[%c], basic:%s passive:%s\n",
			(IS_BOUND_LOW(i)) ? simplex_var[i].reason[LOW] : 0u,
			(IS_BOUND_UPP(i)) ? simplex_var[i].reason[UPP] : 0u,
			simplex_var[i].mark_unsat ? 'u' : 's', simplex_var[i].basic ? "y" : "n",
			simplex_var[i].passive ? "y" : "n");
	}
	for (i = 1; i < simplex_var_n; i++)
		if (simplex_var[i].boundmask) {
			if (IS_BOUND_LOW(i)) {
				LAdelta_hw_print(&simplex_var[i].bound[LOW]);
				printf(" <= ");
			}
			printf("v_%d", i);
			if (IS_BOUND_UPP(i)) {
				printf(" <= ");
				LAdelta_hw_print(&simplex_var[i].bound[UPP]);
			}
			printf(
				" \t{%u,%u} : %s\n",
				(IS_BOUND_LOW(i)) ? simplex_var[i].reason[LOW] : 0u,
				(IS_BOUND_UPP(i)) ? simplex_var[i].reason[UPP] : 0u,
				simplex_var[i].integer ? "Z" : "Q");
		}
}
#endif

/*
  PF


  VARIOUS REMARKS

  One could do constraints strengthening with root-level constraints

  Compare simplex with propagation: simplex modifies a model whereas
  propagation only deals about bounds.  However there may be some
  links because the model is at bounds

  It may be more advantageous to consider only non-strict
  inequalities and repair the values afterwards?  Consider the number
  of operations required to repair (pivots???) vs. avoid need of
  repairing

  The algorithm should be designed such that equalities should not
  introduce inefficiency.

  A hash table may be used to detect variables that are equal
  according to the set of equalities?

  ABOUT STRICT VS NON STRICT DISEQ

  I have a slight feeling that strict disequalities are not
  necessary.  On reals only, because of the convexity, this is clear.
  Assume S is unsatisfiable and contains constraint s1 < a1 ... sn <
  an.  Then s1 = a1 or s2 = a2 or ... sn = an is a logical
  consequence of S where si < ai has been replaced by si <= ai.  Thus
  si = ai should be generated for some i, and a contradiction would
  result in CC.  Completeness in equality generation allows not to
  handle strict disequalities.

  For mixed integer and real arithm, using Branch and Bound and
  Cutting planes, maybe this still not holds?  The thing is, some
  strict ineqs can be stengthened directly into non-strict ones


  SOME NOTES ABOUT CVC4 IMPLEMENTATION (20120301)

  arith_priority_queue: heap
  They have three modes
  - collection (unsorted)
  - difference (sorted by difference between var and unsatisfied extremum)
  - variable (sorted by variable id)
  In difference mode, they furthermore have three pivot rules
  - minimum
  - break_ties
  - maximum
  The minimum, that takes the basic var with the minimum diff is by default
  Modes are changed in simplex.cpp (which strategy???)

  arith_prop_manager: propagations

  arith_rewriter: basic rewriting.  Do not understand everything, but
  I think there is nothing deep in this.

  arith_static_learner: preprocessing black magic.  Maybe along Kim2.

  arith_utilities: hash to map terms to arith variable and back,
  various simple utility functions, recognisers...

  arithvar_node_map: functions to map terms to arith variables and vice-versa

  arithvar_set: efficient datastructures (?, which?) for sets of variables

  atom_database: keeps track of all atoms, and does theory
  propagation on them.  Clauses are added to SAT solver and forgot
  after that.  So this module basically sleeps after every atom has
  been registered
  x < a ==> x < b if a < b.

  delta_rational: operations on DeltaRational c + d delta

  difference_manager: maintains links between vars such that s = x -
  y.  Useful for propagation of equalities.

  dio_solver: Diophantine equation solver.  Not read

  linear_equality: update(), pivotAndUpdate()

  normal_form: ???

  ordered_set:

  partial_model: ???

  simplex: the real thing

  tableau: the data-structure for simplex

  theory_arith: main class

  theory_arith_type_rules

  The assertup/low are similar to D&dM06, but for a check for an
  inequality x != u if x>=u and x<=u are asserted.  Seem to be
  implemented as a list.  I believe it is better to implement as a hash
  (x u) for each x != u.  There is also a assert equal to assert x = c

  The differenceManager may be to handle new equalities???

  findShortestBasicRow.  For some reason, there is a function to
  extract the basic variable x with the shortest row L(x)...  Maybe a
  simplex acceleration?

  TheoryArith::propagate

  TheoryArith::notifyEq : empty function.

  TheoryArith::propagateCandidateBound
  propagateCandidateLowerBound

  It seems that for each var, they retain which bound was propagated
  (in the sense of Theory propagation).  If bound is not improved, no
  need to try to propagate again

  Safe arithmetic in C
  http://www.informit.com/content/images/0321335724/samplechapter/seacord_ch05.pdf
  https://www.securecoding.cert.org/confluence/display/seccode/INT32-C.+Ensure+that+operations+on+signed+integers+do+not+result+in+overflow?showComments=false

  There exists flags in gcc to capture overflow ftrapv, fwrapv

  Use divexact in gmp when the division is known to be exact.  The
  exact division works transforming division to multiplication, based
  on the fact that we are working in representation modulo 2^64, and
  using the extended gcd

  mpz_swap to swap two mpz, for sorting

  for gcd, it is better to start with the smallest number

*/
