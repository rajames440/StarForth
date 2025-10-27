/* MAGIC PARAMETERS
   define SIMP
   define RESTART_MIN_INTERVAL 7
   define LEARNTS_ADJ_FACT 1.5
   define LEARNTS_MAX_FACT 1.1
   define LEARNTS_ADJ_INIT 100
   define LEARNTS_FACT_INIT 0.33
*/

/*
  --------------------------------------------------------------
  Cpp conditions
  --------------------------------------------------------------

  ## Conditions related to optional functionalities

  - EXPERIMENT_WITH_ACTIVITY

  Tentative heuristics that consists in increasing the activity of
  propositional variables appearing in a learnt clause. Realized while
  visiting Daniel Leberre and other SAT experts at CRIL, it has not
  shown useful in practice.

  - HINTS

  A hint is an external information that the SAT solver shall consider
  that some literal is set to true (for another reason than boolean
  propagation). This information is provided with a call to SAT_hint.

  - HINT_AS_DECISION

  This cpp condition indicates that hints shall be treated as
  decisions in the SAT solver (and not propagations).

  - SAT_STANDALONE

  Indicates that the SAT-solver is not a component of the SMT solver
  veriT. If it is, several cpp conditions are automatically set.

  - PROOF_PRINT

  Enables printing proof information to stdout.

  - PROOF_PRINT_CLAUSES < PROOF_PRINT

  Enables printing individual clauses when printing proof
  information.

  - RANDOMIZE_DECISION

  Enables random choice of next decision variable. Turned
  off by default.

  - REUSE_TRAIL

  Enables a technique whereby solver restarts take place not
  necessarily at the root decision level, but at the level
  of an assigned variable with lesser activity.

  - SIMP

  Turns on the simplication implemented in function purge_valid.
  Set by default.

  ## Conditions related to debugging, statistics, compilation

  - DEBUG

  The value of this conditional is used to set DEBUG_SAT to 1
  (when defined) or 0 (otherwise).

  - DEBUG_SAT {0, 1, 2, 3+}

  Various levels of debugging are available (see below).

  - STAT_LEVEL {0, 1, 2-3, 4+}

  Indicates the amount of statistics to be collected. If not set
  externally, then it is set to 0 (no statistics collected).

  - PEDANTIC

  Turns off some low-level optimizations.
  Used to turn off spurious pedantic compiler warning

  --------------------------------------------------------------
*/

/* Debugging
   0: no debugging
   1: basic assertion checking
   2: 1 + invariant tests
   3: 2 + printing loads of information */
#ifdef DEBUG
#define DEBUG_SAT 1
#else
#define DEBUG_SAT 0
#endif

#ifndef SAT_STANDALONE
#define INSIDE_VERIT
#endif

/* INSIDE_VERIT */
/* if defined, some functions are assumed to be available somewhere
   otherwise they are defined internally */

/* #define PROOF_PRINT */ /* print proofs on stdout */
/* #define PROOF_PRINT_CLAUSES */ /* explicitely print clauses in proofs */

#define REUSE_TRAIL

#define SIMP

#include "SAT/veriT-SAT.h"

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef INSIDE_VERIT
#include "utils/stack.h"
#include "utils/types.h"
#include "utils/veriT-qsort.h"
#include "veriT-config.h"
/* hints can either be used as decision or as propogated literals */
/* #define HINT_AS_DECISION */
#define STAT_LEVEL 0
#else /* INSIDE_VERIT */
#ifndef STAT_LEVEL
#define STAT_LEVEL 0
#endif
/* Level 1 store basic stats */
/* Level 4 printf quite a lot of internal information */
#if STAT_LEVEL >= 1
#include "utils/general.h"
#include "utils/statistics.h"
#endif /* STAT_LEVEL >= 1 */
#endif /* INSIDE_VERIT */

/**
   \brief coefficient of luby suite for number of conflicts between restarts
   \remark tunable constant
   \remark actual numbers are 1<<X
   \remark tried 6 7 8 9 (7 is best) */
#define RESTART_MIN_INTERVAL 7

#define LEARNTS_ADJ_FACT 1.5
#define LEARNTS_MAX_FACT 1.1
#define LEARNTS_ADJ_INIT 100
#define LEARNTS_FACT_INIT 0.33

/*
  --------------------------------------------------------------
  Internal simplification
  --------------------------------------------------------------
*/

#define Tvar SAT_Tvar
#define Tlit SAT_Tlit
#define Tclause SAT_Tclause
#define Tlevel SAT_Tlevel
#define Tvalue SAT_Tvalue

#define Tstatus SAT_Tstatus

#define VAL_FALSE SAT_VAL_FALSE
#define VAL_TRUE SAT_VAL_TRUE
#define VAL_UNDEF SAT_VAL_UNDEF

#define VAR_UNDEF SAT_VAR_UNDEF
#define LIT_UNDEF SAT_LIT_UNDEF
#define CLAUSE_UNDEF SAT_CLAUSE_UNDEF
#define CLAUSE_LAZY UINT_MAX

/*
  --------------------------------------------------------------
  Miscellaneous early declarations
  --------------------------------------------------------------
*/

#define ROOT_LEVEL 0

Tstatus SAT_status = SAT_STATUS_UNDEF;
Tlevel SAT_level = ROOT_LEVEL;
static unsigned conflict_nb = 0;

bool SAT_proof = false;

#if STAT_LEVEL >= 1
unsigned stat_n_conflict = 0;
unsigned stat_n_conflict_lit = 0;
unsigned stat_n_decision = 0;
unsigned stat_n_tp = 0;
unsigned stat_n_delete = 0;
unsigned stat_n_restart = 0;
unsigned stat_n_purge = 0;
unsigned stat_n_clauses = 0;
unsigned stat_n_prop = 0;
#if STAT_LEVEL >= 2
unsigned stat_n_watched = 0;
unsigned stat_prop_lit_call_nowatch = 0;
unsigned stat_prop_call = 0;
unsigned stat_prop_call_waste = 0;
unsigned stat_prop_call_noprop = 0;
#endif
#endif

static unsigned misc_stack_size = 0;
static unsigned misc_stack_n = 0;
static Tlit* misc_stack = NULL;

/*
  --------------------------------------------------------------
  Miscellaneous early declarations
  --------------------------------------------------------------
*/

#if DEBUG_SAT > 1
static void check_consistency(void);
static void check_consistency_final(void);
static void check_consistency_propagation(void);
static void check_consistency_heap(void);
#endif
#if DEBUG_SAT > 2
void print_stack(void);
#endif

static inline void var_order_insert(Tvar var);

static void SAT_proof_begin(Tclause clause);
static void SAT_proof_resolve(Tlit lit, Tclause clause);
static void SAT_proof_end(Tclause clause);
#ifdef PROOF_PRINT
static void proof_print(Tclause clause);
#endif

#ifdef HINTS
extern void (*hint_explain)(Tlit lit);
#endif

/*
  --------------------------------------------------------------
  Utilities to compile independently of veriT
  --------------------------------------------------------------
*/

#ifndef INSIDE_VERIT

#include <stdarg.h>
#define SWAP(V1, V2) \
	do { \
		typeof(V1) __tmp = V1; \
		V1 = V2; \
		V2 = __tmp; \
	} while (0)
#define MY_MALLOC(v, s) \
	v = malloc(s); \
	if ((s != 0) && !v) \
	my_error("malloc error on line %d in file " __FILE__ "\n", __LINE__)
#define MY_REALLOC(v, s) \
	v = realloc(v, s); \
	if ((s != 0) && !v) \
	my_error("realloc error on line %d in file " __FILE__ "\n", __LINE__)

#if DEBUG_SAT > 0
#define MY_BREAK_N(n) \
	{ \
		static int i = 0; \
		if (++i == n) breakpoint(); \
		fprintf(stderr, __FILE__ ", %d : %d pass\n", __LINE__, i); \
	}
#endif

static void
my_error(char* format, ...)
{
	va_list params;
	va_start(params, format);
	fprintf(stderr, "error : ");
	vfprintf(stderr, format, params);
	va_end(params);
#ifdef DEBUG
	abort();
#else
	exit(EXIT_FAILURE);
#endif
}

#if DEBUG_SAT > 0
static void
breakpoint(void)
{
	fprintf(stderr, "breakpoint\n");
}
#endif /* DEBUG_SAT */

typedef int (*TFcmp)(const void*, const void*);

#define veriT_qsort qsort

#endif /* #ifndef INSIDE_VERIT */

/*
  --------------------------------------------------------------
  Randomization functions
  --------------------------------------------------------------
*/

/* #define RANDOMIZE_DECISION */
#ifdef RANDOMIZE_DECISION

#define RANDOMIZE_SEED 123456
#define RANDOMIZE_FREQ 100

static unsigned int seed = RANDOMIZE_SEED;

/* Taken from http://software.intel.com/en-us/articles/fast-random-number-generator-on-the-intel-pentiumr-4-processor/
   http://en.wikipedia.org/wiki/Linear_congruential_generator
   And modified.
   This is certainly not good random at all, but good enough. */

static inline unsigned
fastrand(unsigned upper)
{
	seed = (214013 * seed + 2531011);
	return (seed >> 1) % upper;
}
#endif

/*
  --------------------------------------------------------------
  Stack generic functions
  --------------------------------------------------------------
*/

#define STACK_RESIZE_EXP(Pstack, n, size, type_size) \
	if (size < n) { \
		if (!size) size = 2; \
		while (size < n) size *= 2; \
		MY_REALLOC(Pstack, size* type_size); \
	}

#define STACK_RESIZE_LIN(Pstack, n, size, type_size) \
	if (size < n) { \
		size = n; \
		MY_REALLOC(Pstack, size* type_size); \
	}

/*
  --------------------------------------------------------------
  Literal watch
  --------------------------------------------------------------
*/

/**
   \brief structure to store the watched clauses */
typedef struct Twatch
{
	unsigned n; /**< number of clauses */
	/* TODO: Check if many watchers have n <= 3.  If so, it would be
     beneficial to put everything in the structure */
	unsigned size; /**< available size in the array */
	Tclause* Pclause; /**< array of watched clauses */
} Twatch;

/**
   \brief array of watched clauses indexed by literal */
static Twatch* watch = NULL;

/**
   \brief adds a clause to the watched clauses of literal
   \param lit the literal
   \param clause the clause */
static inline void
lit_watch(Tlit lit, Tclause clause)
{
	/* Not using STACK_RESIZE_* for efficiency */
	if (watch[lit].n == watch[lit].size) {
		watch[lit].size <<= 1;
		MY_REALLOC(watch[lit].Pclause, watch[lit].size * sizeof(Tclause));
	}
	watch[lit].Pclause[watch[lit].n++] = clause;
}

/*
  --------------------------------------------------------------
  Variable
  --------------------------------------------------------------
*/

/* State machine for variables.  States: */
typedef enum Evar_mode
{
	STATE_INIT = 0,
	STATE_POSITIVE = 1,
	STATE_NEGATIVE = 2,
	STATE_POSITIVE_RESOLVING = 3,
	STATE_NEGATIVE_RESOLVING = 4,
	STATE_RESOLVED = 5,
	STATE_FAILED = 6
} Tvar_mode;

/* Transitions
   0 --  p --> 1
   0 -- -p --> 2
   1 --  p --> 1
   1 -- -p --> 6
   2 --  p --> 6
   2 -- -p --> 2
   3 --  p --> 6
   3 -- -p --> 5
   4 --  p --> 5
   4 -- -p --> 6
   5 --  * --> 6
   6 --  * --> 6
   0 --  R --> 6
   1 --  R --> 3
   2 --  R --> 4
   3 --  R --> 6
   4 --  R --> 6
   5 --  R --> 6
   Accepting states 1-2-5
   Two passes :
   - first pass to compute final state for every variable
   - second pass to collect all variables in state 1-2.
   Any variable in state 0-3-4-7 generates an error */

/**
   \brief container for variable information
   \note memory footprint (4 or 5 words):
   - level: 1 word
   - reason: 1 word
   - double: 1 64-bit word, 2 32-bit words
   - others: 7 bits */
typedef struct TSvar
{
	SAT_Tlevel level; /**< level of assignment */
	SAT_Tclause reason; /**< clause responsible for propagation */
	double activity; /**< variable activity */
#ifndef PEDANTIC
	unsigned phase_cache : 1; /**< previous polarity assignment */
	Tvar_mode state : 3; /**< variable state */
	unsigned seen : 1; /**< helper bit for conflict analyse */
	/* IMPROVE experiment with decide set only on real SMT atoms, not Tseitin */
	unsigned decide : 1; /**< 1 iff decision on var is allowed */
	unsigned misc : 1; /**< for local flags */
#else
	unsigned phase_cache; /**< previous polarity assignment */
	Tvar_mode state; /**< variable state */
	unsigned seen; /**< helper bit for conflict analyse */
	/* IMPROVE experiment with decide set only on real SMT atoms, not Tseitin */
	unsigned decide; /**< 1 iff decision on var is allowed */
	unsigned misc; /**< for local flags */
#endif
} TSvar;

/**
   \defgroup SAT_stack_var variable stack
   \brief these fields define the stack for variables
   \invariant SAT_stack_var_n is always the maximum id of variables
   \invariant SAT_stack_var_size (the allocated size) >= SAT_stack_var_n + 1
   @{ */
unsigned SAT_stack_var_n = 0; /**< highest var id in the stack */
static unsigned SAT_stack_var_size = 0; /**< size of allocated stack for vars */
static TSvar* SAT_stack_var = NULL; /**< array of vars */
/** @} */

static Tvalue* assign = NULL; /**< assignment */

#ifdef HINT_AS_DECISION
static unsigned hint_n = 0;
static unsigned hint_p = 0;
static unsigned hint_size = 0;
static Tlit* hints = NULL;
#endif

Tvar
SAT_var_new(void)
{
	if (SAT_status != SAT_STATUS_UNSAT) SAT_status = SAT_STATUS_UNDEF;
	SAT_stack_var_n++; /* PF var start at 1 */
	if (SAT_stack_var_size < SAT_stack_var_n + 1) {
		unsigned i;
		if (!SAT_stack_var_size) SAT_stack_var_size = 2;
		while (SAT_stack_var_size < SAT_stack_var_n + 1) SAT_stack_var_size *= 2;
		MY_REALLOC(SAT_stack_var, SAT_stack_var_size * sizeof(TSvar));
		MY_REALLOC(SAT_literal_stack, SAT_stack_var_size * sizeof(Tlit));
		MY_REALLOC(SAT_level_stack, SAT_stack_var_size * sizeof(Tlit));
		MY_REALLOC(assign, SAT_stack_var_size * sizeof(Tvalue));
		MY_REALLOC(watch, (2 * SAT_stack_var_size * sizeof(Twatch)));
		for (i = SAT_stack_var_n * 2; i < 2 * SAT_stack_var_size; i++) {
			watch[i].n = 0;
			watch[i].size = 2;
			MY_MALLOC(watch[i].Pclause, 2 * sizeof(Tclause));
		}
	}
	assign[SAT_stack_var_n] = VAL_UNDEF;
	SAT_stack_var[SAT_stack_var_n].level = 0;
	SAT_stack_var[SAT_stack_var_n].reason = CLAUSE_UNDEF;
	SAT_stack_var[SAT_stack_var_n].activity = 0.0f;
	SAT_stack_var[SAT_stack_var_n].phase_cache = 0;
	SAT_stack_var[SAT_stack_var_n].state = STATE_INIT;
	SAT_stack_var[SAT_stack_var_n].seen = 0;
	SAT_stack_var[SAT_stack_var_n].decide = 1;
	SAT_stack_var[SAT_stack_var_n].misc = 0;
	var_order_insert(SAT_stack_var_n);
	return SAT_stack_var_n;
}

static inline void
SAT_lit_free(Tlit lit)
{
	free(watch[lit].Pclause);
	watch[lit].n = 0;
	watch[lit].size = 0;
	watch[lit].Pclause = NULL;
}

static inline void
SAT_var_free(Tvar var)
{
	SAT_lit_free(SAT_lit(var, 0));
	SAT_lit_free(SAT_lit(var, 1));
}

void
SAT_var_new_id(const unsigned id)
{
	while (SAT_stack_var_n < id) SAT_var_new();
}

Tvalue
SAT_var_value(const Tvar var)
{
	assert(var <= SAT_stack_var_n);
	return assign[var];
}

void
SAT_var_block_decide(Tvar var)
{
	if (SAT_level != ROOT_LEVEL)
		my_error("SAT_var_block_decide call not at root level");
	SAT_stack_var[var].decide = 0;
}

void
SAT_var_unblock_decide(Tvar var)
{
	if (SAT_level != ROOT_LEVEL)
		my_error("SAT_var_unblock_decide call not at root level");
	SAT_stack_var[var].decide = 1;
	var_order_insert(var);
}

/**
   \brief check if variable can be chosen as a decision variable
   \param var the variable
   \return 1 if suitable for decision, 0 otherwise */
static inline unsigned
SAT_var_decision(const Tvar var)
{
	assert(var <= SAT_stack_var_n);
	return SAT_stack_var[var].decide;
}

/**
   \brief get the phase cache for variable
   \param var the variable
   \return 1 if positive polarity, 0 otherwise
   \remark this is set in var_set_value */
static inline unsigned
SAT_var_phase_cache(const Tvar var)
{
	assert(var <= SAT_stack_var_n);
	return SAT_stack_var[var].phase_cache;
}

/**
   \brief check if variable seen
   \param var the variable
   \return 1 iff seen */
static inline unsigned
SAT_var_seen(const Tvar var)
{
	assert(var <= SAT_stack_var_n);
	return SAT_stack_var[var].seen;
}

/**
   \brief set variable as seen
   \param var the variable */
static inline void
SAT_var_set_seen(Tvar var)
{
	assert(var <= SAT_stack_var_n);
	SAT_stack_var[var].seen = 1;
}

/**
   \brief set variable as unseen
   \param var the variable */
static inline void
SAT_var_set_unseen(Tvar var)
{
	assert(var <= SAT_stack_var_n);
	SAT_stack_var[var].seen = 0;
}

/**
   \brief access variable activity
   \param v the variable */
#define SAT_var_activity(v) SAT_stack_var[v].activity

/**
   \brief order for variable decision
   \param v1 the first variable
   \param v2 the second variable
   \return 1 if first variable should be higher in the heap (less) */
static inline int
SAT_var_less(const Tvar v1, const Tvar v2)
{
	return SAT_var_activity(v1) > SAT_var_activity(v2);
}

/**
   \brief get the variable level
   \param var the variable
   \return the level at which variable has been assigned */
inline Tlevel
SAT_var_level(const Tvar var)
{
	return SAT_stack_var[var].level;
}

/**
   \brief get the reason for variable value
   \param var the variable
   \return the clause that propagated the variable */
static inline Tclause
SAT_var_reason(const Tvar var)
{
	return SAT_stack_var[var].reason;
}

/**
   \brief set the reason for variable value (lazy clause)
   \param var the variable
   \param reason the clause */
static inline void
SAT_var_set_reason(Tvar var, const Tclause reason)
{
	assert(SAT_stack_var[var].reason == CLAUSE_LAZY);
	SAT_stack_var[var].reason = reason;
}

/**
   \brief set the value associated with the variable
   \param var the variable
   \param value the value to set
   \param level the level at which the variable is asserted
   \param reason the clause (CLAUSE_UNDEF for decisions) propagating the val
   \return 1 if succeeded, 0 if conflict */
static inline void
var_set_value(
	Tvar var, const Tvalue value, const Tlevel level, const Tclause reason)
{
	assert(assign[var] == VAL_UNDEF);
	assign[var] = value;
	assert(SAT_stack_var[var].reason == CLAUSE_UNDEF);
	SAT_stack_var[var].reason = reason;
	SAT_stack_var[var].level = level;
	SAT_stack_var[var].phase_cache = value & 1u; /* (value == VAL_TRUE)?1:0; */
}

/**
   \brief unset the value associated with the variable
   \param var the variable to unset */
static inline void
var_unset(Tvar var)
{
	assert(assign[var] != VAL_UNDEF);
	assign[var] = VAL_UNDEF;
	SAT_stack_var[var].reason = CLAUSE_UNDEF;
	SAT_stack_var[var].level = 0;
	var_order_insert(var);
}

/*
  --------------------------------------------------------------
  var heap
  --------------------------------------------------------------
*/

static unsigned heap_var_n = 0;
static unsigned heap_var_size = 0;
static Tvar* heap_var = NULL;
static unsigned heap_index_size = 0;
static unsigned* heap_index = NULL;

#define HEAP_INDEX_UNDEF UINT_MAX

static inline unsigned
left(const unsigned i)
{
	return i * 2 + 1;
}

static inline unsigned
right(const unsigned i)
{
	return i * 2 + 2;
}

static inline unsigned
parent(const unsigned i)
{
	return (i - 1) >> 1;
}

static inline void
sift_up(unsigned i)
{
	Tvar var = heap_var[i];
	unsigned p = parent(i);
	while (i && SAT_var_less(var, heap_var[p])) {
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
	Tvar var = heap_var[i];
	while (left(i) < heap_var_n) {
		unsigned child;
		if (
			right(i) < heap_var_n &&
			SAT_var_less(heap_var[right(i)], heap_var[left(i)]))
			child = right(i);
		else
			child = left(i);
		if (!SAT_var_less(heap_var[child], var)) break;
		heap_var[i] = heap_var[child];
		heap_index[heap_var[child]] = i;
		i = child;
	}
	heap_var[i] = var;
	heap_index[var] = i;
}

static inline int
heap_var_in(const Tvar var)
{
	assert(var != VAR_UNDEF);
	return var < heap_index_size && heap_index[var] != HEAP_INDEX_UNDEF;
}

static inline void
heap_var_insert(const Tvar var)
{
	assert(var != VAR_UNDEF);
	if (!heap_var_size) {
		MY_MALLOC(heap_var, 2 * sizeof(Tvar));
		heap_var_size = 2;
	}
	while (heap_var_size < heap_var_n + 1) {
		heap_var_size *= 2;
		MY_REALLOC(heap_var, heap_var_size * sizeof(Tvar));
	}

	if (heap_index_size < SAT_stack_var_size) {
		unsigned i;
		MY_REALLOC(heap_index, SAT_stack_var_size * sizeof(unsigned));
		for (i = heap_index_size; i < SAT_stack_var_size; ++i)
			heap_index[i] = HEAP_INDEX_UNDEF;
		heap_index_size = SAT_stack_var_size;
	}
	assert(!heap_var_in(var));
	heap_var[heap_var_n] = var;
	heap_index[var] = heap_var_n;
	sift_up(heap_var_n++);
}

static inline void
heap_var_decrease(Tvar var)
{
	assert(heap_var_in(var));
	sift_up(heap_index[var]);
}

#if 0
static inline void
heap_var_increase(Tvar var)
{
  assert(heap_var_in(var));
  sift_down(heap_index[var]);
}
#endif

static inline Tvar
heap_var_remove_min(void)
{
	Tvar var = heap_var[0];
	heap_index[var] = HEAP_INDEX_UNDEF;
	heap_var[0] = heap_var[--heap_var_n];
	if (heap_var_n) sift_down(0); /* index will be set in sift_down */
	return var;
}

static inline Tvar
heap_var_get_min(void)
{
	return heap_var[0];
}

#if 0
static inline void
heap_var_update(Tvar var)
{
  if (!heap_var_in(var))
    heap_var_insert(var);
  else
    {
      sift_up(heap_index[var]);
      sift_down(heap_index[var]);
    }
}
#endif

static inline int
heap_var_empty(void)
{
	return heap_var_n == 0;
}

#if 0
static void
heap_var_build(Tvar * vs, unsigned n)
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
heap_var_free(void)
{
	heap_var_n = 0;
	free(heap_var);
	heap_var = NULL;
	heap_var_size = 0;
	free(heap_index);
	heap_index = NULL;
	heap_index_size = 0;
}

/*
  --------------------------------------------------------------
  variable activity
  --------------------------------------------------------------
*/

/**
   \brief increment for variable activity */
static double var_inc = 1.0f;
/**
   \brief decay factor for variable activity */
static double var_decay = 0.95f;

/**
   \brief insert variable into heap
   \param var the variable */
static inline void
var_order_insert(Tvar var)
{
	if (!heap_var_in(var) && SAT_var_decision(var)) heap_var_insert(var);
}

/**
   \brief decrease activity of all variables
   \remark rather than multiplying all activities by decay factor,
   devide var_inc by decay factor (<1)
   \remark multiplying all activities by a factor does not change the
   order, thus rather change the new increment to make it larger */
static inline void
var_decrease_activity(void)
{
	var_inc /= var_decay;
}

/**
   \brief increase activity of a variable
   \param var the variable
   \remark if activity reaches 1e100, divide all activities and increment
   This does not change the order */
static inline void
var_increase_activity(Tvar var)
{
	if ((SAT_var_activity(var) += var_inc) > 1e100) {
		/* rescale all activities */
		for (unsigned i = 1; i <= SAT_stack_var_n; i++)
			SAT_var_activity(i) *= 1e-100;
		var_inc *= 1e-100;
	}
	/* Update order_heap with respect to new activity */
	if (heap_var_in(var)) heap_var_decrease(var);
}

double
SAT_var_get_activity(Tvar var)
{
	assert(var <= SAT_stack_var_n);
	/* TODO: what does it mean for a variable to be active? */
	return SAT_var_activity(var);
}

/*
  --------------------------------------------------------------
  Literal
  --------------------------------------------------------------
*/

/**
   \brief get the value associated with the literal
   \param lit the literal
   \return the value (SAT_VAL_FALSE, SAT_VAL_TRUE, or SAT_VAL_UNDEF) */
static inline Tvalue
SAT_lit_value(const Tlit lit)
{
#ifdef PEDANTIC
	Tvalue tmp = SAT_lit_pol(lit) ^ 1;
	tmp ^= SAT_var_value(SAT_lit_var(lit));
	return tmp;
#else
	return SAT_var_value(SAT_lit_var(lit)) ^ (SAT_lit_pol(lit) ^ 1);
#endif
}

/**
   \brief check if value is undefined
   \param lit the literal
   \return true if undefined */
static inline bool
SAT_lit_value_undef(const Tlit lit)
{
	return SAT_var_value(SAT_lit_var(lit)) == VAL_UNDEF;
}

/**
   \brief get the value associated with the literal
   \param lit the literal
   \return the value (VAL_FALSE, VAL_TRUE, or VAL_UNDEF) */
static inline bool
SAT_lit_value_is_true(const Tlit lit)
{
	return (SAT_var_value(SAT_lit_var(lit)) ^ SAT_lit_pol(lit)) == VAL_FALSE;
}

/**
   \brief get the literal level
   \param lit the literal
   \return the level at which literal has been assigned */
inline Tlevel
SAT_lit_level(const Tlit lit)
{
	return SAT_var_level(SAT_lit_var(lit));
}

/**
   \brief check if literal has been seen
   \param lit the literal
   \return 1 if seen, 0 otherwise */
static inline unsigned
SAT_lit_seen(const Tlit lit)
{
	return SAT_var_seen(SAT_lit_var(lit));
}

/**
   \brief set variable as seen
   \param lit the literal */
static inline void
SAT_lit_set_seen(Tlit lit)
{
	SAT_var_set_seen(SAT_lit_var(lit));
}

/**
   \brief set literal as unseen
   \param lit the literal */
static inline void
SAT_lit_set_unseen(Tlit lit)
{
	SAT_var_set_unseen(SAT_lit_var(lit));
}

/**
   \brief get the reason for literal value
   \param lit the literal
   \return the clause that propagated the literal */
static inline Tclause
SAT_lit_reason(const Tlit lit)
{
	return SAT_var_reason(SAT_lit_var(lit));
}

/**
   \brief compare two literals (for qsort)
   \param lit1 pointer to the first literal
   \param lit2 pointer to the second literal
   \return an integer less than, equal to, or greater than zero if the
   first argument is considered to be respectively less than,
   equal to, or greater than the second */
static int
SAT_lit_compare(const Tlit* lit1, const Tlit* lit2)
{
	Tvar var1 = SAT_lit_var(*lit1);
	Tvar var2 = SAT_lit_var(*lit2);
	if (var1 != var2) return (int)var1 - (int)var2;
	if (*lit1 == *lit2) return 0;
	if (SAT_lit_pol(*lit1)) return 1;
	return -1;
}

/**
   \brief compare two literals (for qsort), according to an order for
   watch list
   \param Plit1 pointer to the first literal
   \param Plit2 pointer to the second literal
   \return an integer less than, equal to, or greater than zero if the
   first argument is considered to be respectively less than, equal
   to, or greater than the second
   \remark true literals should be first by increasing level
   \remark undefined literals should be in the middle, in whatever order
   \remark false literals should be last in decreasing level */
static int
SAT_lit_compare_level(const Tlit* Plit1, const Tlit* Plit2)
{
	switch (SAT_lit_value(*Plit1)) {
		case VAL_TRUE:
			if (SAT_lit_value(*Plit2) == VAL_TRUE)
				return ((int)SAT_lit_level(*Plit1)) - ((int)SAT_lit_level(*Plit2));
			return -1;
		case VAL_FALSE:
			if (SAT_lit_value(*Plit2) == VAL_FALSE)
				return ((int)SAT_lit_level(*Plit2)) - ((int)SAT_lit_level(*Plit1));
			return 1;
		default:
			switch (SAT_lit_value(*Plit2)) {
				case VAL_TRUE: return 1;
				case VAL_FALSE: return -1;
				default: return ((int)SAT_lit_var(*Plit1)) - ((int)SAT_lit_var(*Plit2));
			}
	}
	return 0;
}

/*
  --------------------------------------------------------------
  Literal stack
  --------------------------------------------------------------
*/

#define stack_lit SAT_literal_stack
#define stack_lit_n SAT_literal_stack_n
#define stack_lit_hold SAT_literal_stack_hold
#define stack_lit_unit SAT_literal_stack_unit
#define stack_lit_to_propagate SAT_literal_stack_to_propagate

/**
   \defgroup stack_lit literal stack
   \brief these fields define the stack for literals
   \invariant stack_lit_n is the index of the next literal
   @{ */
unsigned stack_lit_n = 0; /**< index of next position */
Tlit* stack_lit = NULL; /**< array of literals */
unsigned stack_lit_to_propagate = 0; /**< index to next literal to propagate */
unsigned stack_lit_hold = 0; /**< index to unmodified literals */
unsigned stack_lit_unit = 0; /**< index to permanent literals */

/**
   \brief add literal to the stack
   \param lit the literal to add
   \param reason clause propagating the literal (CLAUSE_UNDEF if decision) */
static inline void
stack_lit_add(Tlit lit, Tclause reason)
{
	var_set_value(SAT_lit_var(lit), SAT_lit_pol(lit), SAT_level, reason);
	stack_lit[stack_lit_n++] = lit;
	if (SAT_level == ROOT_LEVEL) {
		if (!SAT_proof) SAT_lit_set_seen(lit);
		stack_lit_unit = stack_lit_n;
	}
}

static inline Tlit
stack_lit_get(const unsigned index)
{
	return stack_lit[index];
}

/** @} */

/*
  --------------------------------------------------------------
  Markups
  --------------------------------------------------------------
*/

#ifdef INSIDE_VERIT
void (*SAT_markup_function)(void) = NULL;

static unsigned highest_markup = 0;
static Tstack_unsigned SAT_markups = NULL;

bool
SAT_set_markup(void)
{
	assert(
		!highest_markup || stack_is_empty(SAT_markups) ||
		highest_markup >= stack_top(SAT_markups));
	if (!SAT_level) return false;
	assert(stack_lit_n);
	if (highest_markup) stack_push(SAT_markups, highest_markup);

	highest_markup = stack_lit_n;
	return true;
}
#endif

/*
  --------------------------------------------------------------
  Level
  --------------------------------------------------------------
*/

#define stack_level SAT_level_stack
#define stack_level_hold SAT_level_stack_hold

/**
   \defgroup stack_level level stack
   \brief these fields define the stack for levels.  It is remembered
   which position in the stack is the first to correspond to next level
   \invariant SAT_level is the index of the next stack_lit index
   \invariant SAT_var_n (allocated size) > SAT_level
   \invariant stack_level[i] is the first literal asserted at level i + 1
   @{ */
unsigned* stack_level = NULL;
unsigned stack_level_hold = 0;

/**
   \brief add decision literal to the stack, changing the level
   \param lit the literal to add */
static inline void
level_push(Tlit lit)
{
	/* resized in SAT_var_new */
	stack_level[SAT_level] = stack_lit_n;
	SAT_level++;
	stack_lit_add(lit, CLAUSE_UNDEF);
}

/**
   \brief backtrack to a given level
   \param level the level not to backtrack */
static inline void
level_backtrack(const Tlevel level)
{
	unsigned stack_lit_bt;
	if (level >= SAT_level) return;
#ifdef HINT_AS_DECISION
	hint_n = hint_p = 0;
#endif
	stack_lit_bt = stack_level[level];
	assert(stack_lit_to_propagate >= stack_lit_bt);
	SAT_level = level;
	/* PF there is no particular reason to do this backwards but easier for
     debugging purposes */
	while (stack_lit_n > stack_lit_bt) {
		stack_lit_n--;
		var_unset(SAT_lit_var(stack_lit_get(stack_lit_n)));
	}
	stack_lit_to_propagate = stack_lit_n;
	if (stack_lit_n < stack_lit_hold) stack_lit_hold = stack_lit_n;
	if (level < stack_level_hold) stack_level_hold = level;
#ifdef INSIDE_VERIT
	while (stack_lit_n < highest_markup) {
		if (SAT_markup_function) SAT_markup_function();
		highest_markup = stack_size(SAT_markups) ? stack_pop(SAT_markups) : 0;
	}
#endif
}

/** @} */

/**
   \brief Find a decision literal in the trail with lower score than the
   next decision literal (top of the heap, discarding already assigned)
   and return its level
   \remark returns UINT_MAX if there's none */
#ifdef REUSE_TRAIL
static inline Tlevel
find_level_on_restart(void)
{
	unsigned i;
	double next_decision_activity;
	Tvar var;
	/* Get activity of the next decision */
	while (1) {
		assert(!heap_var_empty());
		var = heap_var_get_min();
		if (SAT_var_value(var) == VAL_UNDEF && SAT_var_decision(var)) break;
		heap_var_remove_min();
	}
	next_decision_activity = SAT_var_activity(var);
	/* Iterate over all literals in the trail (literal stack) */
	for (i = 0; i < stack_lit_n; ++i)
		if (
			!SAT_var_reason(var = SAT_lit_var(stack_lit[i])) &&
			SAT_var_activity(var) < next_decision_activity)
			return SAT_var_level(var) - 1;
	return UINT_MAX;
}
#else
static inline Tlevel
find_level_on_restart(void)
{
	return ROOT_LEVEL;
}
#endif /* REUSE_TRAIL */

/*
  --------------------------------------------------------------
  Clause
  --------------------------------------------------------------
*/

/**
   \brief Main data-structure for representing clauses */
typedef struct TSclause
{
#ifdef PEDANTIC
	unsigned saved_pos;
	unsigned n;
	unsigned char deleted;
	unsigned char learnt;
	unsigned char watched;
	unsigned char external;
#else
	unsigned saved_pos; /**< saved position for faster BLIT finding */
	unsigned n : 28; /**< number of literals */
	unsigned deleted : 1; /**< deleted */
	unsigned learnt : 1; /**< learnt clause */
	unsigned watched : 1; /**< clause is in watch lists
                           (any but empty, unit, or valid) */
	unsigned external : 1; /**< clause come from outside */
#endif
	double activity; /**< clause activity */
	Tlit blocker;
	Tlit* lit; /**< literals */
} TSclause;
/* MiniSAT uses memory after clause to store literals.  Here it is not
   possible since clause stack is an array */

static Tclause first_free_clause = CLAUSE_UNDEF;

/**
   \defgroup stack_clause clause stack
   \brief these fields define the stack for clauses
   \invariant all clause ids are smaller than stack_clause_size
   @{ */
static unsigned stack_clause_size = 0; /**< size of allocated stack */
static unsigned stack_clause_n = 0; /**< highest clause id in the stack */
static TSclause* stack_clause = NULL; /**< array of clauses */
/** @} */

/* TODO: DIRTY TO SEE THIS HERE */
static inline void clause_learnts_push(Tclause clause);

#if STAT_LEVEL >= 4
static int
check_subsumed_unit(const Tlit lit)
{
	unsigned i, j, n = 0;
	for (i = 1; i < stack_clause_size; i++)
		if (!stack_clause[i].deleted && stack_clause[i].watched)
			for (j = 0; j < stack_clause[i].n; j++)
				if (lit == stack_clause[i].lit[j]) n++;
	return n;
}
#endif

/**
   \brief adds a clause
   \param n the number of literals
   \param lit an array of n literals
   \param learnt tag indicates if clause can be deleted
   \param watched add in watch lists
   \param external tag indicates if clause is external
   \return clause id
   \remark watches are the first two literals.  Choose adequately (IMPROVE)
   \remark memory allocated by lit is confiscated, but can still be used (not freed or moved)
   while no simplification occur */
static inline Tclause
clause_new(
	const unsigned n, Tlit* lit, const bool learnt, const bool watched,
	const bool external)
{
	TSclause* PSclause;
	Tclause clause;
#if STAT_LEVEL >= 4
	if (n == 1) {
		static int j = 1;
		fprintf(stderr, "Unit clause added %d\n", j++);
		fprintf(
			stderr, "Subsumed %d, Strengthened %d\n", check_subsumed_unit(lit[0]),
			check_subsumed_unit(SAT_lit_neg(lit[0])));
	}
	if (n == 2) {
		static j = 1;
		fprintf(stderr, "Binary clause added %d\n", j++);
	}
#endif
	if (first_free_clause == CLAUSE_UNDEF) {
		stack_clause_size *= 2;
		if (stack_clause_size == 1 << 30) my_error("too many clauses\n");
		MY_REALLOC(stack_clause, stack_clause_size * sizeof(TSclause));
		for (clause = stack_clause_size / 2; clause < stack_clause_size; clause++) {
			stack_clause[clause].n = clause + 1;
			stack_clause[clause].deleted = 1;
			stack_clause[clause].activity = 0;
			stack_clause[clause].watched = 0;
			stack_clause[clause].blocker = LIT_UNDEF;
			stack_clause[clause].saved_pos = 2;
		}
		stack_clause[stack_clause_size - 1].n = CLAUSE_UNDEF;
		first_free_clause = stack_clause_size / 2;
	}
	clause = first_free_clause;
	PSclause = stack_clause + clause;
	first_free_clause = PSclause->n;
	if (stack_clause_n < clause) stack_clause_n = clause;
#if STAT_LEVEL >= 1
	stats_counter_inc(stat_n_clauses);
#endif
	if (n >= 1 << 27) my_error("too many literals in clause\n");
	PSclause->n = n;
	PSclause->lit = lit;
	PSclause->learnt = learnt;
	PSclause->saved_pos = 2;
	if (learnt) clause_learnts_push(clause);
	PSclause->deleted = 0;
	PSclause->external = external;
	if (watched && n >= 2) {
		PSclause->watched = 1;
		lit_watch(PSclause->lit[0], clause);
		lit_watch(PSclause->lit[1], clause);
	}
	return clause;
}

/**
   \brief remove a clause
   \param clause the clause to remove
   \remark this is only used in clause_purge */
/* IMPROVE sanitize order of free clauses might be productive */
static inline void
clause_del(Tclause clause)
{
	TSclause* PSclause = stack_clause + clause;
	assert(clause < stack_clause_size);
	if (PSclause->deleted) return;
	PSclause->n = 0;
	PSclause->deleted = 1;
	PSclause->activity = 0;
	PSclause->blocker = LIT_UNDEF;
	PSclause->watched = 0;
	PSclause->saved_pos = 0;
	free(PSclause->lit);
	PSclause->lit = NULL;
	PSclause->learnt = 0;
	if (SAT_proof) return;
	PSclause->n = first_free_clause;
	first_free_clause = clause;
#if STAT_LEVEL >= 1
	stats_counter_inc(stat_n_delete);
#endif
}

/**
   \brief remove a clause
   \param clause the clause to remove
   \remark this is only used in SAT_done, so minimalistic work */
static void
SAT_clause_free(Tclause clause)
{
	if (!stack_clause[clause].deleted) free(stack_clause[clause].lit);
}

unsigned
SAT_clause_get_n(SAT_Tclause clause)
{
	assert(clause <= stack_clause_n);
	return stack_clause[clause].n;
}

SAT_Tlit*
SAT_clause_get_lit(SAT_Tclause clause)
{
	assert(clause <= stack_clause_n);
	return stack_clause[clause].lit;
}

unsigned
SAT_clause_get_glue(SAT_Tclause clause)
{
	unsigned i, glue = 0;
	Tlevel level;
	assert(clause <= stack_clause_n);
	for (i = 0; i < stack_clause[clause].n; i++)
		/* Abuse the misc field of vars as helper to count duplicates */
		if (!SAT_stack_var[level = SAT_lit_level(stack_clause[clause].lit[i])]
					 .misc) {
			glue++;
			SAT_stack_var[level].misc |= 1;
		}
	for (i = 0; i < stack_clause[clause].n; i++)
		SAT_stack_var[level = SAT_lit_level(stack_clause[clause].lit[i])].misc = 0;
	return glue;
}

#if 0
/**
   \brief desactivate a clause
   \param clause the clause to desactivate */
static void
clause_unset_watched(Tclause clause)
{
  TSclause * PSclause = stack_clause + clause;
  if (!PSclause->watched)
    return;
  lit_watch_remove(PSclause->lit[0], clause);
  lit_watch_remove(PSclause->lit[1], clause);
  PSclause->watched = 0;
}
#endif

#if defined(PROOF_PRINT_CLAUSES) || DEBUG_SAT > 0
static void
clause_print(const Tclause clause)
{
	unsigned i;
	TSclause* PSclause = stack_clause + clause;
	if (PSclause->deleted) {
		fprintf(stderr, "Deleted");
		return;
	}
	for (i = 0; i < PSclause->n; i++)
		fprintf(stderr, i ? " %d" : "%d", PSclause->lit[i]);
	if (!PSclause->watched) fprintf(stderr, " Unwatched");
	if (PSclause->external) fprintf(stderr, " External");
	if (PSclause->learnt) fprintf(stderr, " Learnt");
}
#endif

#if DEBUG_SAT > 0
void clause_print_all(void); /* PF to avoid compilation warning */
void
clause_print_all(void)
{
	Tclause i;
	for (i = 1; i < stack_clause_size; i++)
		if (!stack_clause[i].deleted) {
			fprintf(stderr, "%d : ", i);
			clause_print(i);
			fprintf(stderr, "\n");
		}
}
#endif

/*
  --------------------------------------------------------------
  Clause activity/purge
  --------------------------------------------------------------
*/

static double clause_inc = 1.0f;
static double clause_decay = 0.999f;

static Tclause* learnts = NULL;
static unsigned learnts_n = 0;
static unsigned learnts_size = 0;

/**
   \brief access clause activity
   \param clause the clause */
#define clause_activity(clause) stack_clause[clause].activity

static inline void
clause_decrease_activity(void)
{
	clause_inc /= clause_decay;
}

static inline void
clause_increase_activity(Tclause clause)
{
	if ((clause_activity(clause) += clause_inc) > 1e20) {
		unsigned i;
		/* rescale all activities */
		for (i = 0; i < learnts_n; i++) clause_activity(learnts[i]) *= 1e-20;
		clause_inc *= 1e-20;
	}
}

static inline void
clause_learnts_push(const Tclause clause)
{
	learnts_n++;
	STACK_RESIZE_EXP(learnts, learnts_n, learnts_size, sizeof(Tclause));
	learnts[learnts_n - 1] = clause;
}

static inline int
clause_propagating(const Tclause clause)
{
	return SAT_lit_reason(stack_clause[clause].lit[0]) == clause;
}

double
SAT_clause_get_activity(Tclause clause)
{
	assert(clause <= stack_clause_n);
	return clause_activity(clause);
}

/*
  --------------------------------------------------------------
  Clause elimination
  --------------------------------------------------------------
*/

/**
   \brief remove all deleted clauses from watch lists */
static inline void
clause_eliminate_from_watch(void)
{
	unsigned i;
	for (i = 2; i <= (SAT_stack_var_n << 1) + 1; i++)
		if (watch[i].n) /* otherwise Pclause random.  Just cleaner to test */
		{
			Tclause *j, *k, *n;
			j = k = watch[i].Pclause;
			n = j + watch[i].n;
			for (; j != n; j++)
				if (!stack_clause[*j].deleted && stack_clause[*j].watched) *(k++) = *j;
			watch[i].n -= (unsigned)(j - k);
		}
}

/**
   \brief compare clause activity
   \param clause1 a pointer to clause
   \param clause2 a pointer to clause
   \return 1 if clause2 has larger activity
   \remark suitable for qsort */
static int
clause_activity_cmp(const Tclause* clause1, const Tclause* clause2)
{
	if (stack_clause[*clause1].activity < stack_clause[*clause2].activity)
		return 1;
	return -1;
}

/**
   \brief remove false literals from clauses, and true clauses */
static inline void
purge_valid(void)
{
#ifdef SIMP
	TSclause *i, *n;
	assert(SAT_level == ROOT_LEVEL);
	assert(SAT_status != SAT_STATUS_UNSAT);
	assert(stack_lit_to_propagate == stack_lit_n);
	i = stack_clause + 1;
	n = stack_clause + stack_clause_size;
	for (; i != n; i++) {
		Tlit *k, *l, *m;
		if (
			i->deleted || !i->watched ||
			(SAT_lit_value(i->lit[0]) == VAL_TRUE &&
			 stack_clause + SAT_lit_reason(i->lit[0]) == i))
			continue;
		k = l = i->lit;
		m = i->lit + i->n;
		for (; k != m; k++)
			if (SAT_lit_value_undef(*k))
				*(l++) = *k;
			else if (SAT_lit_value_is_true(*k)) {
				if (i->learnt) /* delete valid clauses and remove false lits */
					clause_del((unsigned)(i - stack_clause));
				else
					i->watched = 0;
				/* TODO: we leave here the non-learned clause in a bad shape
               Not OK for backtracking, and not nice in any way */
				goto next_clause;
			}
		i->n -= (unsigned)(k - l);
	next_clause:;
	}
	{
		Tclause *i, *j, *n;
		i = j = learnts;
		n = learnts + learnts_n;
		for (; i != n; i++)
			if (!stack_clause[*i].deleted) *(j++) = *i;
		learnts_n -= (unsigned)(n - j);
	}
	clause_eliminate_from_watch();
#endif /* SIMP */
}

static inline void
purge(void)
{
	/* IMPROVE could try to be a bit more agressive towards purging:
     - sort binary clauses to the beginning,
     - eliminate all clauses beyond threshold
     - remove a portion of the rest */
	double threshold = clause_inc / learnts_n;
	Tclause *i, *j, *n;
#if STAT_LEVEL >= 1
	stats_counter_inc(stat_n_purge);
#endif /* STAT_LEVEL >= 1 */
#if STAT_LEVEL >= 4
	fprintf(stderr, "Starting purge\n");
#endif
	veriT_qsort(learnts, learnts_n, sizeof(Tclause), (TFcmp)clause_activity_cmp);
	i = j = learnts;
	n = learnts + learnts_n / 2;
	for (; i != n; i++)
		if (
			stack_clause[*i].activity < threshold && stack_clause[*i].n > 2 &&
			!clause_propagating(*i))
			clause_del(*i);
		else
			*(j++) = *i;
	assert(i == learnts + learnts_n / 2);
	n = learnts + learnts_n;
	for (; i != n; i++)
		if (stack_clause[*i].n > 2 && !clause_propagating(*i))
			clause_del(*i);
		else
			*(j++) = *i;
#if STAT_LEVEL >= 4
	fprintf(stderr, "%d learnt, %ld eliminated\n", learnts_n, n - j);
#endif
	learnts_n -= (unsigned)(n - j);
	clause_eliminate_from_watch();
}

/*
  --------------------------------------------------------------
  Proof checker
  --------------------------------------------------------------
*/

static unsigned SAT_proof_stack_size = 0;

unsigned SAT_proof_stack_n = 0;
Tlit* SAT_proof_stack_lit = NULL;
Tclause* SAT_proof_stack_clause = NULL;

/* since clause for external propagation might be added in the analysis,
   i.e. while computing (and proving) another clause, and since adding this
   clause may also trigger another proof with unit literals, two proof
   threads might be necessary */

static unsigned SAT_proof_stack_size_2 = 0;
static unsigned SAT_proof_stack_n_2 = 0;
static Tlit* SAT_proof_stack_lit_2 = NULL;
static Tclause* SAT_proof_stack_clause_2 = NULL;

static void
SAT_proof_begin(Tclause clause)
{
	if (SAT_proof_stack_n != 0) {
		/* PF second level proof:
         may come from simplification in added clause while explaining hint
         in conflict */
		assert(SAT_proof_stack_n_2 == 0);
		SWAP(SAT_proof_stack_n, SAT_proof_stack_n_2);
		SWAP(SAT_proof_stack_size, SAT_proof_stack_size_2);
		SWAP(SAT_proof_stack_lit, SAT_proof_stack_lit_2);
		SWAP(SAT_proof_stack_clause, SAT_proof_stack_clause_2);
	}
	SAT_proof_stack_n = 1;
	if (!SAT_proof_stack_size) {
		MY_MALLOC(SAT_proof_stack_lit, sizeof(Tlit));
		MY_MALLOC(SAT_proof_stack_clause, sizeof(Tclause));
		SAT_proof_stack_size = 1;
	}
	SAT_proof_stack_clause[0] = clause;
}

static void
SAT_proof_resolve(Tlit lit, Tclause clause)
{
	assert(SAT_proof_stack_size > 0);
	assert(clause != CLAUSE_LAZY);
	SAT_proof_stack_n++;
	while (SAT_proof_stack_size < SAT_proof_stack_n) {
		SAT_proof_stack_size *= 2;
		MY_REALLOC(SAT_proof_stack_lit, SAT_proof_stack_size * sizeof(Tlit));
		MY_REALLOC(SAT_proof_stack_clause, SAT_proof_stack_size * sizeof(Tclause));
	}
	SAT_proof_stack_lit[SAT_proof_stack_n - 2] = lit;
	SAT_proof_stack_clause[SAT_proof_stack_n - 1] = clause;
}

static inline int
SAT_proof_update_lit(Tlit lit)
{
	TSvar* PSvar = SAT_stack_var + SAT_lit_var(lit);
	switch (PSvar->state) {
		case STATE_INIT:
			PSvar->state = SAT_lit_pol(lit) ? STATE_POSITIVE : STATE_NEGATIVE;
			return 1;
		case STATE_POSITIVE:
			if (!SAT_lit_pol(lit)) {
				PSvar->state = STATE_FAILED;
				my_error("proof error\n");
			}
			return 0;
		case STATE_NEGATIVE:
			if (SAT_lit_pol(lit)) {
				PSvar->state = STATE_FAILED;
				my_error("proof error\n");
			}
			return 0;
		case STATE_POSITIVE_RESOLVING:
			if (SAT_lit_pol(lit)) {
				PSvar->state = STATE_FAILED;
				my_error("proof error\n");
			}
			PSvar->state = STATE_RESOLVED;
			return -1;
		case STATE_NEGATIVE_RESOLVING:
			if (!SAT_lit_pol(lit)) {
				PSvar->state = STATE_FAILED;
				my_error("proof error\n");
			}
			PSvar->state = STATE_RESOLVED;
			return -1;
		case STATE_RESOLVED:
		case STATE_FAILED:
		default:
			my_error("proof error\n");
			PSvar->state = STATE_FAILED;
			return 0;
	}
}

static inline void
SAT_proof_resolve_lit(Tlit lit)
{
	TSvar* PSvar = SAT_stack_var + SAT_lit_var(lit);
	switch (PSvar->state) {
		case STATE_INIT:
			PSvar->state = STATE_FAILED;
			my_error("proof error\n");
			break;
		case STATE_POSITIVE: PSvar->state = STATE_POSITIVE_RESOLVING; break;
		case STATE_NEGATIVE: PSvar->state = STATE_NEGATIVE_RESOLVING; break;
		case STATE_POSITIVE_RESOLVING:
		case STATE_NEGATIVE_RESOLVING:
		case STATE_RESOLVED:
		case STATE_FAILED:
		default: my_error("proof error\n"); PSvar->state = STATE_FAILED;
	}
}

static void
SAT_proof_end(Tclause clause)
{
	unsigned i, j;
	int count = 0;
	TSclause* PSclause;
#ifdef PROOF_PRINT
	SAT_proof_print(clause);
#endif
	/* first traversal */
	for (i = 0; i < SAT_proof_stack_n; ++i) {
		if (i) SAT_proof_resolve_lit(SAT_proof_stack_lit[i - 1]);
		PSclause = stack_clause + SAT_proof_stack_clause[i];
		for (j = 0; j < PSclause->n; j++)
			count += SAT_proof_update_lit(PSclause->lit[j]);
	}
	/* second traversal */
	PSclause = stack_clause + clause;
	if (count < 0 || PSclause->n != (unsigned)count) my_error("proof error\n");
	for (j = 0; j < PSclause->n; j++)
		switch (SAT_stack_var[SAT_lit_var(PSclause->lit[j])].state) {
			case STATE_POSITIVE:
			case STATE_NEGATIVE:
				SAT_stack_var[SAT_lit_var(PSclause->lit[j])].state = STATE_INIT;
				break;
			default: my_error("proof error\n");
		}
	for (i = 0; i < SAT_proof_stack_n; ++i) {
		PSclause = stack_clause + SAT_proof_stack_clause[i];
		for (j = 0; j < PSclause->n; j++)
			switch (SAT_stack_var[SAT_lit_var(PSclause->lit[j])].state) {
				case STATE_INIT:
				case STATE_RESOLVED:
					SAT_stack_var[SAT_lit_var(PSclause->lit[j])].state = STATE_INIT;
					break;
				case STATE_POSITIVE:
				case STATE_NEGATIVE:
				case STATE_POSITIVE_RESOLVING:
				case STATE_NEGATIVE_RESOLVING:
				case STATE_FAILED:
					SAT_stack_var[SAT_lit_var(PSclause->lit[j])].state = STATE_INIT;
					my_error("proof error\n");
			}
	}
#ifdef INSIDE_VERIT
	SAT_proof_notify(clause);
#endif
	SAT_proof_stack_n = 0;
	if (SAT_proof_stack_n_2 != 0) {
		/* PF restore second level proof */
		SWAP(SAT_proof_stack_n, SAT_proof_stack_n_2);
		SWAP(SAT_proof_stack_size, SAT_proof_stack_size_2);
		SWAP(SAT_proof_stack_lit, SAT_proof_stack_lit_2);
		SWAP(SAT_proof_stack_clause, SAT_proof_stack_clause_2);
	}
}

#ifdef PROOF_PRINT

static void
SAT_proof_print(Tclause clause)
{
	unsigned i;
	for (i = 0; i + 1 < SAT_proof_stack_n; ++i) {
		printf("%u", SAT_proof_stack_clause[i]);
#ifdef PROOF_PRINT_CLAUSES
		printf(" (");
		clause_print(SAT_proof_stack_clause[i]);
		printf(")");
#endif
		printf(" [%d] ", SAT_lit_var(SAT_proof_stack_lit[i]));
	}
	printf("%u", SAT_proof_stack_clause[i]);
#ifdef PROOF_PRINT_CLAUSES
	printf(" (");
	clause_print(SAT_proof_stack_clause[i]);
	printf(")");
#endif
	printf(" --> %d", clause);
#ifdef PROOF_PRINT_CLAUSES
	printf(" (");
	clause_print(clause);
	printf(")");
#endif
	printf("\n");
}

#endif /* PROOF_PRINT */

/*
  --------------------------------------------------------------
  Core helpers
  --------------------------------------------------------------
*/

/**
   \brief computes the numbers of the luby series
   \return the luby number of index i after i calls
   \note http://citeseer.ist.psu.edu/viewdoc/summary?doi=10.1.1.47.5558 */
static inline unsigned
luby(void)
{
	/*  static int u = 0, v = 0; to keep veriT trace equiv with older vers */
	static int u = 1, v = 1;
	/* u & -u is the list significant non null bit of u */
	/* u records when to restart */
	if ((u & -u) == v) {
		u++;
		v = 1;
	} else
		v *= 2;
	return (unsigned)v;
}

static inline unsigned
restart_suite(void)
{
	return luby() << RESTART_MIN_INTERVAL;
}

/**
   \brief get the next decision (1st part)
   \return the next literal to decide or LIT_UNDEF if none
   \remark the literal should be assigned next */
static inline Tlit
decision_get(void)
{
	Tvar next;
#ifdef HINT_AS_DECISION
	while (hint_p < hint_n) {
		Tlit lit = hints[hint_p++];
		if (SAT_lit_value_undef(lit)) return lit;
	}
	hint_p = hint_n = 0;
#endif
#ifdef RANDOMIZE_DECISION
	if (fastrand(RANDOMIZE_FREQ << 2) < 4) {
		next = heap_var[fastrand(heap_var_n)];
		if (SAT_var_value(next) == VAL_UNDEF && SAT_var_decision(next))
			return SAT_lit(next, fastrand(2));
	}
#endif
	while (!heap_var_empty()) {
		next = heap_var_remove_min();
		if (SAT_var_value(next) == VAL_UNDEF && SAT_var_decision(next))
			/* IMPROVE here optionally randomize polarity a bit */
			return SAT_lit(next, SAT_var_phase_cache(next));
	}
	return LIT_UNDEF;
}

/*
  --------------------------------------------------------------
  Propagation
  --------------------------------------------------------------
*/

/**
   \brief propagates all implications
   \return conflicting clause if conflict, CLAUSE_UNDEF otherwise
   \remark we choose here to return after the first conflict.
   \remark literal stack is truncated after the first conflict. */
__attribute__((noinline)) static Tclause
propagate(void)
{
#if STAT_LEVEL >= 2
	unsigned old_stack_lit_to_propagate = stack_lit_to_propagate;
	stats_counter_inc(stat_prop_call);
	if (stack_lit_to_propagate >= stack_lit_n)
		stats_counter_inc(stat_prop_call_waste);
#endif
	while (stack_lit_to_propagate < stack_lit_n) {
		Tlit lit = stack_lit[stack_lit_to_propagate];
		Tclause *i, *j, *n;
#if STAT_LEVEL >= 2
		stats_counter_inc(stat_n_prop);
		if (!watch[lit].n) stats_counter_inc(stat_prop_lit_call_nowatch);
#endif
		stack_lit_to_propagate++;
		lit = SAT_lit_neg(lit);
		if (!watch[lit].n) continue;
		i = j = watch[lit].Pclause;
		n = i + watch[lit].n;
		for (; i != n; ++i) {
			TSclause* PSclause = stack_clause + *i;
			Tlit* lits;
			unsigned k;
			if (SAT_lit_value_is_true(PSclause->blocker)) {
				*(j++) = *i;
				continue;
			}
			lits = PSclause->lit;
#if STAT_LEVEL >= 2
			stats_counter_inc(stat_n_watched);
#endif
			/* PF put lit in position 1 */
			const Tlit other_lit = lits[0] ^ lits[1] ^ lit;
			const Tvalue v_other_lit = SAT_lit_value(other_lit);
			lits[0] = other_lit;
			lits[1] = lit;
			// branchless equivalent to:
			/* if (lits[0] == lit) { */
			/* 	lits[0] = lits[1]; */
			/* 	lits[1] = lit; */
			/* } */
			/* PF satisfied clause ? */
			if (v_other_lit == VAL_TRUE) {
				/* PF leaving the clause in the watch, but no need to find
				   another watch since this literal will remain true */
				PSclause->blocker = lits[0];
				*(j++) = *i;
				continue;
			}
			/* PF look for a new watch */
			const unsigned end = PSclause->n;
			const unsigned middle =
				(PSclause->saved_pos < end ? PSclause->saved_pos : 2);
			k = middle;
			Tvalue v = VAL_FALSE;
			while (k < end && ((v = SAT_lit_value(lits[k])) == VAL_FALSE))
				++k;
			if (v == VAL_FALSE) {
				k = 2;
				while (k < middle && ((v = SAT_lit_value(lits[k])) == VAL_FALSE))
					++k;
			}

			PSclause->saved_pos = (k + 1);
			assert(PSclause->saved_pos < end + 2);
			if (v == VAL_TRUE) {
				/* PF leaving the clause in the watch, but no need to find
				   another watch since this literal will remain true */
				PSclause->blocker = lits[k];
				*(j++) = *i;
				goto next_watch;
			}
			if (v != VAL_FALSE) {
				lits[1] = lits[k];
				lits[k] = lit;
				lit_watch(lits[1], *i);
				/* delete the clause from the watch list:
				   j is not incremented */
				goto next_watch;
			}
			/* clause is either propagating or conflicting.
			   Keep in watch list */
			*(j++) = *i;
			/* Propagation at ROOT_LEVEL: compute explicit unit clause */
			if (SAT_level == ROOT_LEVEL) {
				Tclause clause;
				assert(PSclause->n > 1);
				if (SAT_proof) SAT_proof_begin(*i);
				for (k = 1; k < PSclause->n; k++) {
					assert(SAT_lit_value(lits[k]) == VAL_FALSE);
					if (SAT_proof) {
						clause = SAT_lit_reason(lits[k]);
						assert(clause != CLAUSE_LAZY);
						SAT_proof_resolve(lits[k], clause);
					}
				}
				if (SAT_lit_value(lits[0]) == VAL_FALSE) {
					Tclause clause = clause_new(0, NULL, false, false, false);
					PSclause = stack_clause + *i; /* DD remove this line? */
					/* conflicting clause */
					i++; /* remove from watch */
					memmove(j, i, (unsigned)(((char*)n) - ((char*)i)));
					watch[lit].n -= (unsigned)(i - j);
					if (SAT_proof) {
						Tclause clause2 = SAT_lit_reason(lits[0]);
						assert(clause2 != CLAUSE_LAZY);
						SAT_proof_resolve(lits[0], clause2);
						SAT_proof_end(clause);
					}
					return clause;
				} else {
					Tclause clause;
					Tlit* Plit2;
					MY_MALLOC(Plit2, sizeof(Tlit));
					Plit2[0] = lits[0];
					clause = clause_new(1, Plit2, false, false, false);
					stack_lit_add(lits[0], clause);
					if (SAT_proof) SAT_proof_end(clause);
					goto next_watch;
				}
			}
			if (v_other_lit == VAL_FALSE) {
				/* conflicting clause */
				Tclause clause = *i;
				i++; /* remove from watch */
				/* First version
				   for ( ; i != n; ++i, ++j)
				     *j = *i; */
				/* Second version: no better
				     if ((unsigned char *) i - (unsigned char *) j >
				     (unsigned char *) n - (unsigned char *) i)
				     memcpy(j, i, ((unsigned char *) n - (unsigned char *) i));
				     else
				     memcpy(j, (unsigned char *) n -
				     ((unsigned char *) i - (unsigned char *) j),
				     ((unsigned char *) i - (unsigned char *) j));
				*/
				/* Third version: very slight improvement */
				memmove(j, i, (unsigned)(((char*)n) - ((char*)i)));
				watch[lit].n -= (unsigned)(i - j);
#if DEBUG_SAT > 1
				for (k = 0; k < PSclause->n; ++k)
					assert(SAT_lit_value(lits[k]) == VAL_FALSE);
#endif
#if STAT_LEVEL >= 2
				if (old_stack_lit_to_propagate + 1 == stack_lit_n)
					stats_counter_inc(stat_prop_call_noprop);
#endif
				return clause;
			}
			/* propagating clause */
			stack_lit_add(lits[0], *i);
		next_watch:;
		}
		watch[lit].n -= (unsigned)(i - j);
	}
#if STAT_LEVEL >= 2
	if (old_stack_lit_to_propagate + 1 == stack_lit_n)
		stats_counter_inc(stat_prop_call_noprop);
#endif
	return CLAUSE_UNDEF;
}

/*
  --------------------------------------------------------------
  Analyse
  --------------------------------------------------------------
*/

typedef unsigned Talevel;

/**
   \brief repair clauses from conflicting state
   \param clause the conflicting or propagating clause
   \param level at which the clause is propagating
   \remark this should work for empty clauses
   \remark this should work for unit clauses
   \remark this should work for propagating clauses
   \remark this should work for clauses propagating at root level */
static inline void
repair_conflict(Tclause clause, Tlevel level)
{
	assert(stack_clause[clause].n != 0 || level == ROOT_LEVEL);
	assert(stack_clause[clause].n != 1 || level == ROOT_LEVEL);
	if (stack_clause[clause].n == 0) {
		level_backtrack(level);
		return;
	}
	if (stack_clause[clause].n == 1) {
		level_backtrack(level);
		stack_lit_add(stack_clause[clause].lit[0], clause);
		return;
	}
	assert(SAT_lit_value(stack_clause[clause].lit[0]) == VAL_FALSE);
	assert(SAT_lit_value(stack_clause[clause].lit[1]) == VAL_FALSE);
	assert(
		SAT_lit_level(stack_clause[clause].lit[1]) <
		SAT_lit_level(stack_clause[clause].lit[0]));
	assert(SAT_lit_level(stack_clause[clause].lit[1]) == level);
	level_backtrack(level);
	assert(SAT_lit_value_undef(stack_clause[clause].lit[0]));
	stack_lit_add(stack_clause[clause].lit[0], clause);
}

static inline bool
analyse_required_clause(Tlit lit, Talevel alevel)
{
	unsigned j;
	TSclause* Pclause = stack_clause + SAT_lit_reason(lit);
	assert(SAT_lit_reason(lit) != CLAUSE_UNDEF);
#ifdef HINTS
	assert(SAT_lit_reason(lit) != CLAUSE_LAZY);
#endif
	STACK_RESIZE_EXP(
		misc_stack, misc_stack_n + Pclause->n, misc_stack_size, sizeof(Tlit));
	assert(Pclause->lit[0] == SAT_lit_neg(lit));
	/* PF first add literals that are not already in conflict on misc_stack */
	for (j = 1; j < Pclause->n; j++)
		if (!SAT_lit_seen(Pclause->lit[j])) {
			if (
				SAT_lit_reason(Pclause->lit[j]) == CLAUSE_UNDEF ||
#ifdef HINTS
				SAT_lit_reason(Pclause->lit[j]) == CLAUSE_LAZY ||
#endif
				((1u << (SAT_lit_level(Pclause->lit[j]) & 31u)) & alevel) == 0)
				return true;
			SAT_lit_set_seen(Pclause->lit[j]);
			misc_stack[misc_stack_n++] = Pclause->lit[j];
		}
	return false;
}

static inline bool
analyse_required(Tlit lit, Talevel alevel)
{
	/* Use misc_stack from misc_stack_n */
	unsigned i, top = misc_stack_n;
	if (analyse_required_clause(lit, alevel)) goto clean;
	/* PF then recursively add, for each lit on stack, the reasons */
	for (i = top; i < misc_stack_n; i++)
		if (analyse_required_clause(misc_stack[i], alevel)) goto clean;
	return false;
clean:
	for (i = top; i < misc_stack_n; i++) SAT_lit_set_unseen(misc_stack[i]);
	misc_stack_n = top;
	return true;
}

__attribute__((noinline))
/**
   \brief learns the clauses from conflicting state
   \param clause the conflicting clause
   \remark increasing clause activity of conflict clause proved
   slightly counterproductive */
static void
analyse(Tclause clause)
{
	Tlevel level;
	unsigned i, j, index = stack_lit_n - 1, counter = 0;
	Tlit p;
	Tlit* Plit = stack_clause[clause].lit;
	unsigned n = stack_clause[clause].n;
	clause_increase_activity(clause);
	assert(SAT_level != ROOT_LEVEL && stack_lit_n > 1);
	misc_stack_n = 1;
#if DEBUG_SAT > 1
	for (i = 0; i < n; ++i) assert(SAT_lit_value(Plit[i]) == VAL_FALSE);
	for (i = 0; i < n; i++)
		if (SAT_lit_level(Plit[i]) == SAT_level) break;
	assert(i < n);
#endif /* DEBUG_SAT */
	if (SAT_proof) SAT_proof_begin(clause);
	do {
		STACK_RESIZE_EXP(
			misc_stack, misc_stack_n + n, misc_stack_size, sizeof(Tlit));
		for (i = 0; i < n; i++)
			if (!SAT_lit_seen(Plit[i])) {
				Tvar var = SAT_lit_var(Plit[i]);
				assert(SAT_lit_value(Plit[i]) == VAL_FALSE);
				assert(SAT_proof || SAT_var_level(var) > ROOT_LEVEL);
				SAT_var_set_seen(var);
				var_increase_activity(var);
				if (SAT_var_level(var) == SAT_level)
					counter++; /* count all literals at current level */
				else
					misc_stack[misc_stack_n++] = Plit[i];
			}
		/* take next clause */
		/* by proceding by reverse order, last assigned lit are taken last
         initially there should be at least one literal at the current
         level (further than the conflicting literal) that is to be
         examined, otherwise clause would have been propagating.
         counter == 0 iff no more literals of the current level are to be
         examined */
		while (!SAT_lit_seen(stack_lit_get(index))) {
			assert(index > 0 && SAT_lit_level(stack_lit_get(index)) == SAT_level);
			index--;
		}
		p = stack_lit_get(index);
		SAT_lit_set_unseen(p);
		counter--;
		if (counter != 0) {
			clause = SAT_lit_reason(p);
#ifdef HINTS
			if (clause == CLAUSE_LAZY) {
				assert(SAT_lit_value(p) == VAL_TRUE);
				hint_explain(p);
				clause = SAT_lit_reason(p);
			}
#endif
			assert(clause && clause != CLAUSE_LAZY);
			clause_increase_activity(clause);
			Plit = stack_clause[clause].lit + 1;
			n = stack_clause[clause].n - 1;
			assert(SAT_lit_value(*Plit) == VAL_FALSE);
			if (SAT_proof) SAT_proof_resolve(p, clause);
			assert(clause != CLAUSE_UNDEF && p == stack_clause[clause].lit[0]);
		}
	} while (counter != 0);
	misc_stack[0] = SAT_lit_neg(p);
	MY_MALLOC(Plit, misc_stack_n * sizeof(Tlit));
	memcpy(Plit, misc_stack, misc_stack_n * sizeof(Tlit));
	n = misc_stack_n;
	if (SAT_proof) {
		for (i = 1, j = 1; i < n; i++)
			if (SAT_lit_level(Plit[i]) != ROOT_LEVEL)
				Plit[j++] = Plit[i];
			else
				SAT_proof_resolve(Plit[i], SAT_lit_reason(Plit[i]));
	} else {
		Talevel alevels = 0;
		for (i = 1; i < n; i++) alevels |= 1u << (SAT_lit_level(Plit[i]) & 31);
		for (i = 1, j = 1; i < n; i++)
			if (
				SAT_lit_reason(Plit[i]) == CLAUSE_UNDEF ||
#ifdef HINTS
				SAT_lit_reason(Plit[i]) == CLAUSE_LAZY ||
#endif /* HINTS */
				analyse_required(Plit[i], alevels))
				Plit[j++] = Plit[i];
	}
	n = j;
	level = ROOT_LEVEL;
	if (n > 1) {
		for (i = 1, j = 1; i < n; i++)
			if (level < SAT_lit_level(Plit[i])) level = SAT_lit_level(Plit[j = i]);
		p = Plit[1];
		Plit[1] = Plit[j];
		Plit[j] = p;
	}
	assert(!SAT_lit_seen(misc_stack[0]));
	for (i = 1; i < misc_stack_n; i++) SAT_lit_set_unseen(misc_stack[i]);
#if DEBUG_SAT > 1
	for (i = 0; i < n; ++i) assert(SAT_lit_value(Plit[i]) == VAL_FALSE);
#endif
#if STAT_LEVEL >= 1
	stats_counter_inc(stat_n_conflict);
	stats_counter_add(stat_n_conflict_lit, (int)n);
#endif
	MY_REALLOC(Plit, n * sizeof(Tlit));
	clause = clause_new(n, Plit, true, true, false);
	repair_conflict(clause, level);
	if (SAT_proof) SAT_proof_end(clause);
}

/*
  --------------------------------------------------------------
  Solving
  --------------------------------------------------------------
*/

#ifdef HINTS
inline void
SAT_sanitize_root_level(void)
{
	if (SAT_proof && SAT_level == ROOT_LEVEL) {
		unsigned i;
		for (i = stack_lit_to_propagate; i < stack_lit_n; i++)
			if (SAT_lit_reason(stack_lit[i]) == CLAUSE_LAZY)
				hint_explain(stack_lit[i]);
	}
}
#endif

Tstatus
SAT_propagate(void)
{
	static unsigned conflict_restart_n = 1 << RESTART_MIN_INTERVAL;
	static unsigned learnts_max = 0, learnts_n_adj_cnt = LEARNTS_ADJ_INIT;
	static double learnts_n_adj_cnt_restart = LEARNTS_ADJ_INIT;
	static bool next_purge_valid = false;
	Tclause conflict;
	/* IMPROVE REMOVE THIS TEST TO SEE IF SIGNIFICANT OVERHEAD */
	if (SAT_status != SAT_STATUS_UNDEF) return SAT_status;
#ifdef HINTS
	SAT_sanitize_root_level();
#endif
	if (!learnts_max) /* PF First call to SAT_propagate after adding clauses */
	{
		/* PF First call to SAT_propagate after adding clauses */
		learnts_max = (unsigned)(stack_clause_n * LEARNTS_FACT_INIT + 1);
#ifdef SIMP
		if (!SAT_proof) {
			if (propagate() != CLAUSE_UNDEF) return (SAT_status = SAT_STATUS_UNSAT);
			purge_valid();
		}
#endif /* SIMP */
	}
#if DEBUG_SAT > 1
	check_consistency();
	check_consistency_heap();
#endif
	while ((conflict = propagate()) != CLAUSE_UNDEF) {
		if (SAT_level == ROOT_LEVEL) return (SAT_status = SAT_STATUS_UNSAT);
		analyse(conflict);
		if (conflict_restart_n-- == 0) {
#if STAT_LEVEL >= 1
			stats_counter_inc(stat_n_restart);
#endif
			level_backtrack(find_level_on_restart());
			conflict_restart_n = restart_suite();
			next_purge_valid = true;
		}
		if (--learnts_n_adj_cnt == 0) {
			/* number of conflict between adjustment */
			learnts_n_adj_cnt_restart *= LEARNTS_ADJ_FACT;
			learnts_n_adj_cnt = (unsigned)learnts_n_adj_cnt_restart;
			learnts_max = (unsigned)(learnts_max * LEARNTS_MAX_FACT);
		}
		conflict_nb++;
		var_decrease_activity();
		clause_decrease_activity();
	}
	if (SAT_level == ROOT_LEVEL && next_purge_valid) {
		if (!SAT_proof) purge_valid();
		next_purge_valid = false;
	}
	if (learnts_n >= learnts_max + stack_lit_n) { purge(); }
#if DEBUG_SAT > 1
	check_consistency();
	check_consistency_heap();
	check_consistency_propagation();
#endif
#if DEBUG_SAT > 2
	print_stack();
#endif
	return SAT_STATUS_UNDEF;
}

#ifdef HINTS
/**
   \brief adds hint, i.e. propagated literal with lazy clause */
void
SAT_hint(Tlit lit)
{
	assert(SAT_lit_var(lit) <= SAT_stack_var_n);
	if (!SAT_lit_value_undef(lit)) return;
#if STAT_LEVEL >= 1
	stats_counter_inc(stat_n_tp);
#endif /* STAT_LEVEL >= 1 */
#ifdef HINT_AS_DECISION
	if (hint_n + 1 > hint_size) {
		hint_size *= 2;
		MY_REALLOC(hints, hint_size * sizeof(Tlit));
	}
	hints[hint_n++] = lit;
#else
	stack_lit_add(lit, CLAUSE_LAZY);
#endif /* HINT_AS_DECISION */
	/* Clauses at root level will be explained in propagate()
     Not here because DP work may not be completed */
}
#endif /* HINTS */

bool
SAT_decide(void)
{
	Tlit lit;
#if DEBUG_SAT > 1
	check_consistency_propagation();
#endif
	lit = decision_get();
	if (!lit) /* All variables assigned */
	{
#if DEBUG_SAT > 1
		check_consistency_final();
#endif
		SAT_status = SAT_STATUS_SAT;
		return false;
	}
	assert(SAT_lit_value_undef(lit));
#if STAT_LEVEL >= 1
	stats_counter_inc(stat_n_decision);
#endif /* STAT_LEVEL >= 1 */
	level_push(lit);
	return true;
}

void
SAT_restart(void)
{
	level_backtrack(ROOT_LEVEL);
	if (SAT_propagate() == SAT_STATUS_UNDEF && !SAT_proof) purge_valid();
}

Tstatus
SAT_solve(void)
{
	while (SAT_propagate() == SAT_STATUS_UNDEF) SAT_decide();
	return SAT_status;
}

/*
  --------------------------------------------------------------
  Adding clauses
  --------------------------------------------------------------
*/

/**
   \brief adds a clause
   \param n the number of literals
   \param lit an array of n literals
   \param conflict flag indicating if it is a conflict clause
   \remark this may be called at any time
   \remark destructive for the array of literals
   \remark returns CLAUSE_UNDEF if valid clause or problem already found unsat
   \return clause id or CLAUSE_UNDEF
   \remark added clause may require different treatment

   <ul>
   \li if the clause has at least two literals not falsified (true or
   undefined), the clause would not have been propagating in the current
   decision trail. It can safely be added with those two literals as watchers

   \li if the clause has just one literal true, and the level of the true
   literal is <= than the highest level of the false literals, the clause would
   not have been propagating in the current decision trail, or propagation has
   been made through another clause.  It can safely be added with the true
   literal and the highest false literals as watchers

   \li if the clause has just one literal undefined or true, backtrack
   to the highest level of falsified literals and propagate

   \li if the clause has only falsified literals, and the highest
   level is met with only one literal, backtrack to the fore-highest
   level, and propagate

   \li if the clause has only falsified literals, and the highest
   level is met with several literals, backtrack to this level and do
   a conflict analysis
   </ul> */
Tclause
SAT_clause_new(unsigned n, Tlit* lit, unsigned flags)
{
	unsigned i, j;
	Tclause clause;
	if (SAT_status == SAT_STATUS_UNSAT) {
#ifdef INSIDE_VERIT
		if (SAT_proof) SAT_proof_set_id(CLAUSE_UNDEF);
#endif /* INSIDE_VERIT */
		free(lit);
		return CLAUSE_UNDEF;
	}
	SAT_status = SAT_STATUS_UNDEF;
#if DEBUG_SAT > 1
	check_consistency();
	check_consistency_heap();
#endif
#if DEBUG_SAT > 1
	fprintf(stderr, "SAT_clause_new :");
	for (i = 0; i < n; ++i) fprintf(stderr, " %d", lit[i]);
	fprintf(stderr, "\n");
#endif
	if (n == 0) {
		/* input clause is empty clause */
		clause = clause_new(n, NULL, false, false, true);
#ifdef INSIDE_VERIT
		if (SAT_proof) SAT_proof_set_id(clause);
#endif /* INSIDE_VERIT */
		SAT_status = SAT_STATUS_UNSAT;
		return clause;
	}
	veriT_qsort(lit, n, sizeof(Tlit), (TFcmp)SAT_lit_compare);
	/* checking for complementary literals, true literals,
     and eliminating duplicates */
	if (
		SAT_lit_value(lit[0]) == VAL_TRUE && SAT_lit_level(lit[0]) == ROOT_LEVEL) {
		/* true literal, valid clause */
#ifdef INSIDE_VERIT
		if (SAT_proof) SAT_proof_set_id(CLAUSE_UNDEF);
#endif /* INSIDE_VERIT */
		free(lit);
		return CLAUSE_UNDEF;
	}
	for (j = 1, i = 1; i < n; ++i)
		if (lit[i] == lit[j - 1])
			continue;
		else if (
			SAT_lit_var(lit[i]) == SAT_lit_var(lit[j - 1]) ||
			(SAT_lit_value(lit[i]) == VAL_TRUE &&
			 SAT_lit_level(lit[i]) == ROOT_LEVEL)) {
			/* complementary literals or true literal, valid clause */
			free(lit);
#ifdef INSIDE_VERIT
			if (SAT_proof) SAT_proof_set_id(CLAUSE_UNDEF);
#endif /* INSIDE_VERIT */
			return CLAUSE_UNDEF;
		} else
			lit[j++] = lit[i];

	n = j;
	for (j = 0, i = 0; i < n; ++i)
		if (
			SAT_lit_value(lit[i]) != VAL_FALSE || SAT_lit_level(lit[i]) != ROOT_LEVEL)
			lit[j++] = lit[i];
		else if (SAT_proof) {
			if (i == j) {
				Tlit* Plit2;
				MY_MALLOC(Plit2, n * sizeof(Tlit));
				memcpy(Plit2, lit, n * sizeof(Tlit));
				clause = clause_new(n, Plit2, false, false, true);
#ifdef INSIDE_VERIT
				SAT_proof_set_id(clause);
#endif /* INSIDE_VERIT */
				SAT_proof_begin(clause);
			}
			SAT_proof_resolve(lit[i], SAT_lit_reason(lit[i]));
		}
	if (SAT_proof && n != j) {
		n = j;
		MY_REALLOC(lit, n * sizeof(Tlit));
		veriT_qsort(lit, n, sizeof(Tlit), (TFcmp)SAT_lit_compare_level);
		clause = clause_new(n, lit, (flags & SAT_CLAUSE_LEARNT) != 0, true, true);
		SAT_proof_end(clause);
	} else {
		n = j;
		MY_REALLOC(lit, n * sizeof(Tlit));
		veriT_qsort(lit, n, sizeof(Tlit), (TFcmp)SAT_lit_compare_level);
		clause = clause_new(n, lit, (flags & SAT_CLAUSE_LEARNT) != 0, true, true);
#ifdef INSIDE_VERIT
		if (SAT_proof) SAT_proof_set_id(clause);
#endif /* INSIDE_VERIT */
	}

	if (n == 0) /* empty clause */
	{
		level_backtrack(ROOT_LEVEL);
		SAT_status = SAT_STATUS_UNSAT;
	} else if (n == 1) /* unit clause */
	{
		level_backtrack(ROOT_LEVEL);
		/* should be propagating otherwise is empty clause */
		stack_lit_add(stack_clause[clause].lit[0], clause);
	} else if (SAT_lit_value(lit[1]) != VAL_FALSE)
	/* first case: clause would never have been propagating
       no backtracking required
       clause can be safely added */
	{
	} else if (
		SAT_lit_value_is_true(lit[0]) &&
		SAT_lit_level(lit[0]) <= SAT_lit_level(lit[1]))
	/* second case: clause may never have been propagating since blocked
       by true literal
       clause can be safely added */
	{
	} else if (
		SAT_lit_value(lit[0]) != VAL_FALSE ||
		SAT_lit_level(lit[0]) != SAT_lit_level(lit[1])) /* > */
	/*  assert (SAT_lit_value(lit[1]) == VAL_FALSE);
        assert (SAT_lit_value(lit[0]) != VAL_TRUE ||
        SAT_lit_level(lit[0]) > SAT_lit_level(lit[1])); */
	{
		/* third and fourth case */
		level_backtrack(SAT_lit_level(lit[1]));
		stack_lit_add(stack_clause[clause].lit[0], clause);
	} else
	/*
      assert(SAT_lit_value(lit[0]) == VAL_FALSE);
      assert(SAT_lit_value(lit[1]) == VAL_FALSE);
      assert(SAT_lit_level(lit[0]) == SAT_lit_level(lit[1])); */
	{
		/* last case: clause is conflicting */
		level_backtrack(SAT_lit_level(lit[0]));
		assert(SAT_level != ROOT_LEVEL);
		/* not root level otherwise reduced to empty clause earlier */
		analyse(clause);
	}
#if DEBUG_SAT > 1
	check_consistency();
	check_consistency_heap();
#endif
	return clause;
}

Tclause
SAT_clause_new_lazy(unsigned n, Tlit* lit)
{
	unsigned i = 0, j = 0;
	Tclause clause;
#if DEBUG_SAT > 0
	static bool called = false;
	assert(!called);
	called = true;
#endif
#if DEBUG_SAT > 1
	check_consistency();
	check_consistency_heap();
#endif
#if DEBUG_SAT > 1
	print_stack();
	fprintf(stderr, "SAT_clause_new_lazy :");
	for (i = 0; i < n; ++i) fprintf(stderr, " %d", lit[i]);
	fprintf(stderr, "\n");
#endif
	veriT_qsort(lit, n, sizeof(Tlit), (TFcmp)SAT_lit_compare_level);
	/* checking for complementary literals, true literals,
     and eliminating duplicates */
#if DEBUG_SAT > 0
	assert(n && SAT_lit_value_is_true(lit[0]));
	assert(SAT_lit_reason(lit[0]) == CLAUSE_LAZY);
	for (i = 0; i < n; i++)
		assert(
			(i == 0 || SAT_lit_value(lit[i]) == VAL_FALSE) &&
			SAT_lit_level(lit[i]) <= SAT_lit_level(lit[0]) &&
			(i == 0 || lit[i] != lit[i - 1]));
#endif
	for (j = 1, i = 1; i < n; ++i)
		if (
			SAT_lit_value(lit[i]) != VAL_FALSE || SAT_lit_level(lit[i]) != ROOT_LEVEL)
			lit[j++] = lit[i];
		else if (SAT_proof) {
			if (i == j) {
				Tlit* Plit2;
				MY_MALLOC(Plit2, n * sizeof(Tlit));
				memcpy(Plit2, lit, n * sizeof(Tlit));
				clause = clause_new(n, Plit2, false, false, true);
#ifdef INSIDE_VERIT
				SAT_proof_set_id(clause);
#endif
				SAT_proof_begin(clause);
			}
			assert(SAT_lit_reason(lit[i]) && SAT_lit_reason(lit[i]) != CLAUSE_LAZY);
			SAT_proof_resolve(lit[i], SAT_lit_reason(lit[i]));
		}
	if (SAT_proof && n != j) {
		n = j;
		MY_REALLOC(lit, n * sizeof(Tlit));
		clause = clause_new(n, lit, false, true, true);
		SAT_proof_end(clause);
	} else {
		n = j;
		MY_REALLOC(lit, n * sizeof(Tlit));
		clause = clause_new(n, lit, false, true, true);
#ifdef INSIDE_VERIT
		if (SAT_proof) SAT_proof_set_id(clause);
#endif
	}
#ifdef EXPERIMENT_WITH_ACTIVITY
	var_decrease_activity();
	for (i = 0; i < n; i++) var_increase_activity(SAT_lit_var(lit[i]));
	var_decrease_activity();
	for (i = 0; i < n; i++) var_increase_activity(SAT_lit_var(lit[i]));
#endif
	SAT_var_set_reason(SAT_lit_var(lit[0]), clause);
#if DEBUG_SAT > 1
	check_consistency();
	check_consistency_heap();
#endif
#if DEBUG_SAT > 0
	called = false;
#endif
	return clause;
}

/*
  --------------------------------------------------------------
  minimal models
  --------------------------------------------------------------
*/

#ifdef INSIDE_VERIT

TSstack(_clause, Tclause);

/**
   \brief Is the literal required or not */
bool* prime_required = NULL;

/**
   \brief Implements Algorithm 2 from [Deharbe et al. 2013] */
void
SAT_prime_implicant(void)
{
	unsigned i, j;
	Tlit lit;
	TSclause clause;
	/* Stores lits and trues of a clause */
	unsigned* clause_sat_n; /*<< How many literals asserted true */
	/* Each lit points to a set of clauses where it appears */
	Tstack_clause* index_clauses;
	MY_REALLOC(prime_required, (SAT_stack_var_n + 1) * 2 * sizeof(bool));
	memset(prime_required, 0, (SAT_stack_var_n + 1) * 2 * sizeof(bool));
	MY_MALLOC(index_clauses, (SAT_stack_var_n + 1) * 2 * sizeof(Tstack_clause));
	memset(index_clauses, 0, (SAT_stack_var_n + 1) * 2 * sizeof(Tstack_clause));
	MY_MALLOC(clause_sat_n, (stack_clause_n + 1) * sizeof(unsigned));
	memset(clause_sat_n, 0, (stack_clause_n + 1) * sizeof(unsigned));
	/* Collect decision and propagated literals. The latter compose
     the initial set of required literals. */
	for (i = 0; i < stack_lit_n; ++i) {
		prime_required[stack_lit[i]] =
			(SAT_stack_var[SAT_lit_var(stack_lit[i])].reason != CLAUSE_UNDEF);
		/* TODO: should be done for those which are required? */
		stack_INIT(index_clauses[stack_lit[i]]);
	}
	/* Collect original clauses */
	for (i = 1; i <= stack_clause_n; ++i)
		if (!stack_clause[i].deleted && !stack_clause[i].learnt) {
			clause = stack_clause[i];
			/* Put clause in index of its literals */
			for (j = 0; j < clause.n; ++j)
				if (index_clauses[clause.lit[j]])
					stack_push(index_clauses[clause.lit[j]], i);
		}
	/* For each literal, increase counter of every clause where it
     appears */
	for (i = 0; i < stack_lit_n; ++i)
		if (index_clauses[stack_lit[i]])
			for (j = 0; j < stack_size(index_clauses[stack_lit[i]]); ++j)
				clause_sat_n[stack_get(index_clauses[stack_lit[i]], j)]++;
	/* Required literals are those for which there is a clause with
     "trues == 1", i.e., they are the sole literal that makes that
     clause true. If none, decrease counter from all its clauses */
	for (i = stack_lit_n; i-- != 0;) {
		lit = stack_lit[i];
		if (prime_required[lit] || !index_clauses[stack_lit[i]]) continue;
		for (j = 0; j < stack_size(index_clauses[lit]); ++j)
			if (clause_sat_n[stack_get(index_clauses[lit], j)] == 1)
				break;
			else
				assert(clause_sat_n[stack_get(index_clauses[lit], j)] != 0);
		/* lit is required */
		if (j < stack_size(index_clauses[lit])) {
			prime_required[lit] = true;
			continue;
		}
		/* Mark literal as not required in all of its clauses */
		for (j = 0; j < stack_size(index_clauses[lit]); ++j)
			clause_sat_n[stack_get(index_clauses[lit], j)]--;
	}
	/* Frees index of each literal */
	for (i = 0; i < stack_lit_n; ++i)
		if (index_clauses[stack_lit[i]]) stack_free(index_clauses[stack_lit[i]]);
	free(index_clauses);
	free(clause_sat_n);
}

#endif

/*
  --------------------------------------------------------------
  init and done
  --------------------------------------------------------------
*/

void
SAT_init(void)
{
	var_inc = 1.0f;
	var_decay = 0.95f;
	SAT_status = SAT_STATUS_SAT;
	SAT_level = ROOT_LEVEL;
	conflict_nb = 0;
	stack_lit_to_propagate = 0;
	stack_lit_hold = 0;
	stack_lit_unit = 0;
	MY_MALLOC(SAT_stack_var, sizeof(TSvar));
	SAT_stack_var_size = 1;
	MY_MALLOC(assign, sizeof(Tvalue));
	assign[0] = VAL_UNDEF;
#ifdef HINT_AS_DECISION
	hint_n = hint_p = 0;
	hint_size = 4;
	MY_MALLOC(hints, hint_size * sizeof(Tlit));
#endif
	MY_MALLOC(watch, 2 * sizeof(Twatch));
	memset(watch, 0, 2 * sizeof(Twatch));
	SAT_stack_var[VAR_UNDEF].phase_cache = 0;
	SAT_stack_var[VAR_UNDEF].seen = 0;
	SAT_stack_var[VAR_UNDEF].decide = 0;
	SAT_stack_var[VAR_UNDEF].state = STATE_INIT;
	SAT_stack_var[VAR_UNDEF].misc = 0;
	SAT_stack_var[VAR_UNDEF].level = 0;
	SAT_stack_var[VAR_UNDEF].reason = CLAUSE_UNDEF;
	SAT_stack_var[VAR_UNDEF].activity = 0;
	stack_clause_size = 1;
	MY_MALLOC(stack_clause, stack_clause_size * sizeof(TSclause));
#ifdef INSIDE_VERIT
	stack_INIT(SAT_markups);
#endif
#if DEBUG_SAT > 1
	check_consistency();
#endif
#if STAT_LEVEL >= 1
#ifndef INSIDE_VERIT
	stats_init();
#endif
	stat_n_conflict =
		stats_counter_new("SAT_n_conflict", "Number of conflicts in SAT", "%9d");
	stat_n_conflict_lit = stats_counter_new(
		"SAT_n_conflict_lit", "Number of literals in conflicts in SAT", "%9d");
	stat_n_decision =
		stats_counter_new("SAT_n_dec", "Number of decisions in SAT", "%9d");
	stat_n_tp = stats_counter_new(
		"SAT_n_tp", "Number of theory propagations in SAT", "%9d");
	stat_n_delete =
		stats_counter_new("SAT_n_del", "Number of clause deletions in SAT", "%9d");
	stat_n_restart =
		stats_counter_new("SAT_n_restart", "Number of restarts in SAT", "%6d");
	stat_n_purge =
		stats_counter_new("SAT_n_purge", "Number of purges in SAT", "%6d");
	stat_n_clauses =
		stats_counter_new("SAT_n_clauses", "Number of clauses added in SAT", "%9d");
	stat_n_prop = stats_counter_new("SAT_n_prop", "Number of propagation", "%9d");
#if STAT_LEVEL >= 2
	stat_n_watched = stats_counter_new(
		"SAT_n_watch", "Number of clauses examined by watched", "%9d");
	stat_prop_lit_call_nowatch = stats_counter_new(
		"SAT_prop_lit_call_nowatch", "Number of calls to prop_lit with no watchers",
		"%9d");
	stat_prop_call =
		stats_counter_new("SAT_prop_call", "Number of calls to propagate", "%9d");
	stat_prop_call_waste = stats_counter_new(
		"SAT_prop_call_waste",
		"Number of calls to propagate with nothing to propagate", "%9d");
	stat_prop_call_noprop = stats_counter_new(
		"SAT_prop_call_noprop",
		"Number of calls to propagate without further propagation", "%9d");
#endif
#endif
}

void
SAT_done(void)
{
	unsigned i;
	SAT_proof_stack_size = 0;
	free(SAT_proof_stack_lit);
	SAT_proof_stack_lit = NULL;
	free(SAT_proof_stack_clause);
	SAT_proof_stack_clause = NULL;
	SAT_proof_stack_size_2 = 0;
	free(SAT_proof_stack_lit_2);
	SAT_proof_stack_lit_2 = NULL;
	free(SAT_proof_stack_clause_2);
	SAT_proof_stack_clause_2 = NULL;
	for (i = 1; i <= SAT_stack_var_n; ++i) SAT_var_free(i);
	for (i = (SAT_stack_var_n + 1) << 1; i < 2 * SAT_stack_var_size; ++i)
		free(watch[i].Pclause);
	free(watch);
	free(SAT_stack_var);
	SAT_stack_var = NULL;
	SAT_stack_var_n = 0;
	SAT_stack_var_size = 0;
	for (i = 1; i < stack_clause_size; ++i) SAT_clause_free(i);
	free(stack_clause);
	stack_clause = NULL;
	stack_clause_n = 0;
	stack_clause_size = 0;
	free(learnts);
	learnts = NULL;
	learnts_n = 0;
	learnts_size = 0;
	heap_var_free();
	free(stack_lit);
	stack_lit = NULL;
	stack_lit_n = 0;
	free(stack_level);
	stack_level = NULL;
	SAT_level = ROOT_LEVEL;
	free(misc_stack);
	misc_stack = NULL;
	misc_stack_size = 0;
	misc_stack_n = 0;
	free(assign);
#ifdef HINT_AS_DECISION
	free(hints);
	hint_n = hint_p = hint_size = 0;
#endif
#ifdef INSIDE_VERIT
	stack_free(SAT_markups);
	free(prime_required);
#else
#if STAT_LEVEL >= 1
	stats_fprint(stdout);
	stats_done();
#endif
#endif
}

void
SAT_reset(void)
{
	unsigned i;
	SAT_proof_stack_size = 0;
	free(SAT_proof_stack_lit);
	SAT_proof_stack_lit = NULL;
	free(SAT_proof_stack_clause);
	SAT_proof_stack_clause = NULL;
	SAT_proof_stack_size_2 = 0;
	free(SAT_proof_stack_lit_2);
	SAT_proof_stack_lit_2 = NULL;
	free(SAT_proof_stack_clause_2);
	SAT_proof_stack_clause_2 = NULL;
	for (i = 1; i <= SAT_stack_var_n; ++i) SAT_var_free(i);
	for (i = (SAT_stack_var_n + 1) << 1; i < 2 * SAT_stack_var_size; ++i)
		free(watch[i].Pclause);
	free(watch);
	free(SAT_stack_var);
	SAT_stack_var = NULL;
	SAT_stack_var_n = 0;
	SAT_stack_var_size = 0;
	for (i = 1; i < stack_clause_size; ++i) SAT_clause_free(i);
	free(stack_clause);
	stack_clause_size = 1;
	MY_MALLOC(stack_clause, stack_clause_size * sizeof(TSclause));
	free(learnts);
	learnts = NULL;
	learnts_n = 0;
	learnts_size = 0;
	heap_var_free();
	free(stack_lit);
	stack_lit = NULL;
	stack_lit_n = 0;
	free(stack_level);
	stack_level = NULL;
	SAT_level = ROOT_LEVEL;
	free(misc_stack);
	misc_stack = NULL;
	misc_stack_size = 0;
	misc_stack_n = 0;
	free(assign);
#ifdef HINT_AS_DECISION
	free(hints);
#endif
	var_inc = 1;
	var_decay = 0.95;
	SAT_status = SAT_STATUS_SAT;
	SAT_level = ROOT_LEVEL;
	conflict_nb = 0;
	stack_lit_to_propagate = 0;
	stack_lit_hold = 0;
	stack_lit_unit = 0;
	MY_MALLOC(SAT_stack_var, sizeof(TSvar));
	SAT_stack_var_size = 1;
	MY_MALLOC(assign, sizeof(Tvalue));
	assign[0] = VAL_UNDEF;
#ifdef HINT_AS_DECISION
	hint_n = hint_p = 0;
	hint_size = 4;
	MY_MALLOC(hints, hint_size * sizeof(Tlit));
#endif
	MY_MALLOC(watch, 2 * sizeof(Twatch));
	memset(watch, 0, 2 * sizeof(Twatch));
	SAT_stack_var[VAR_UNDEF].level = 0;
	SAT_stack_var[VAR_UNDEF].reason = CLAUSE_UNDEF;
	SAT_stack_var[VAR_UNDEF].activity = 0.0f;
	SAT_stack_var[VAR_UNDEF].phase_cache = 0;
	SAT_stack_var[VAR_UNDEF].state = STATE_INIT;
	SAT_stack_var[VAR_UNDEF].seen = 0;
	SAT_stack_var[VAR_UNDEF].decide = 0;
	SAT_stack_var[VAR_UNDEF].misc = 0;
#if DEBUG_SAT > 1
	check_consistency();
#endif
}

/*
  --------------------------------------------------------------
  Invariant checking
  --------------------------------------------------------------
*/

#if DEBUG_SAT > 1

static void
check_consistency(void)
{
	int* count_watch = NULL;
	unsigned i, j, k;
	Tvar var;
	Tlit lit;
	MY_MALLOC(count_watch, stack_clause_size * sizeof(int));
	for (i = 0; i < stack_clause_size; ++i) count_watch[i] = 0;
	for (var = 1; var <= SAT_stack_var_n; var++) {
		Tclause clause;
		assert(
			SAT_var_value(var) != VAL_UNDEF || !SAT_stack_var[var].decide ||
			heap_var_in(var));
		clause = SAT_stack_var[var].reason;
		if (clause == CLAUSE_LAZY || clause == CLAUSE_UNDEF) continue;
		assert(SAT_lit_var(stack_clause[clause].lit[0]) == var);
		for (i = 0, j = 0; i < stack_clause[clause].n; i++)
			if (SAT_lit_value(stack_clause[clause].lit[i]) == VAL_FALSE)
				j++;
			else
				assert(
					SAT_lit_value(stack_clause[clause].lit[i]) == VAL_TRUE &&
					SAT_lit_var(stack_clause[clause].lit[i]) == var);
		assert(j == stack_clause[clause].n - 1);
	}
	for (lit = 2; lit < (SAT_stack_var_n + 1) << 1; lit++)
		for (j = 0; j < watch[lit].n; j++) {
			TSclause* PSclause = stack_clause + watch[lit].Pclause[j];
			assert(watch[lit].Pclause[j] < stack_clause_size);
			assert(PSclause->watched);
			assert(PSclause->n >= 2);
			assert(PSclause->lit[0] == lit || PSclause->lit[1] == lit);
			if (PSclause->lit[0] == lit) {
				assert(!(count_watch[watch[lit].Pclause[j]] & 1));
				count_watch[watch[lit].Pclause[j]] |= 1;
			} else {
				assert(!(count_watch[watch[lit].Pclause[j]] & 2));
				count_watch[watch[lit].Pclause[j]] |= 2;
			}
		}
	for (i = 1; i < stack_clause_size; ++i) {
		assert(stack_clause[i].n != 0 || count_watch[i] == 0);
		assert(stack_clause[i].n != 1 || count_watch[i] == 0);
		assert(
			stack_clause[i].n < 2 || stack_clause[i].deleted ||
			!stack_clause[i].watched || count_watch[i] == 3);
	}
	free(count_watch);

	for (i = 0, k = 0; i < stack_lit_n; i++) {
		assert(SAT_lit_value(stack_lit_get(i)) == VAL_TRUE);
		if (SAT_lit_reason(stack_lit_get(i)) == CLAUSE_UNDEF) k++;
		SAT_stack_var[SAT_lit_var(stack_lit_get(i))].misc = 1;
		if (SAT_proof) continue;
		assert(
			SAT_lit_level(stack_lit_get(i)) > ROOT_LEVEL ||
			SAT_lit_seen(stack_lit_get(i)));
	}
	assert(SAT_level == k + ROOT_LEVEL);
	for (var = 1; var <= SAT_stack_var_n; var++)
		assert(
			SAT_stack_var[var].misc ||
			(SAT_var_value(var) == VAL_UNDEF &&
			 SAT_stack_var[var].reason == CLAUSE_UNDEF));
	for (i = 0; i < stack_lit_n; i++)
		SAT_stack_var[SAT_lit_var(stack_lit_get(i))].misc = 0;
}

static void
check_consistency_propagation(void)
{
	unsigned i;
	for (i = 1; i < stack_clause_size; ++i) {
		unsigned count_p = 0, count_n = 0, j;
		TSclause* PSclause = stack_clause + i;
		if (!PSclause->watched) continue;
		for (j = 0; j < PSclause->n; j++) switch (SAT_lit_value(PSclause->lit[j])) {
				case VAL_FALSE: count_n++; break;
				case VAL_TRUE: count_p++; break;
			}
		assert(count_n < PSclause->n); /* otherwise conflicting */
		assert(count_p > 0 || count_n + 1 < PSclause->n); /* otherwise prop */
		if (!count_p) {
			assert(SAT_lit_value_undef(PSclause->lit[0]));
			assert(SAT_lit_value_undef(PSclause->lit[1]));
		}
	}
}

static void
print_clause(Tclause clause)
{
	unsigned k;
	assert(clause < stack_clause_size);
	fprintf(stderr, "%d : ", clause);
	for (k = 0; k < stack_clause[clause].n; k++)
		fprintf(stderr, "%d ", stack_clause[clause].lit[k]);
	fprintf(stderr, "\n");
}

void
print_stack(void)
{
	static int count = 0;
	unsigned i, k;
	for (i = 1; i < stack_clause_size; i++)
		if (!stack_clause[i].deleted) {
			fprintf(stderr, "%d : ", i);
			for (k = 0; k < stack_clause[i].n; k++)
				fprintf(stderr, "%d ", stack_clause[i].lit[k]);
			fprintf(stderr, "\n");
		}

	fprintf(stderr, "stack size %u (nb var %u)\n", stack_lit_n, SAT_stack_var_n);
	for (i = 0; i < stack_lit_n; i++)
		if (SAT_lit_reason(stack_lit_get(i)) == CLAUSE_UNDEF)
			fprintf(stderr, "[%d] ", stack_lit_get(i));
		else
			fprintf(
				stderr, "%d (%u) ", stack_lit_get(i), SAT_lit_reason(stack_lit_get(i)));
	fprintf(stderr, "\n");
	fprintf(stderr, "call %d\n", ++count);
}

static void
check_consistency_heap(void)
{
	/* check if all unassigned variables are in the heap */
	unsigned i;
	unsigned count = 0;
	for (i = 0; i < heap_var_n; ++i)
		if (SAT_var_value(heap_var[i]) == VAL_UNDEF) count++;
	if (count + stack_lit_n != SAT_stack_var_n) {
		for (i = 1; i < SAT_stack_var_n; ++i)
			assert(SAT_var_value(i) != VAL_UNDEF || heap_var_in(i));
		assert(count + stack_lit_n == SAT_stack_var_n);
	}
	for (i = 0; i < heap_var_n; ++i) assert(heap_index[heap_var[i]] == i);
	for (i = 0; i < heap_index_size; ++i)
		assert(heap_index[i] == HEAP_INDEX_UNDEF || heap_var[heap_index[i]] == i);
}

static void
check_consistency_final(void)
{
	unsigned i, j, ok;
	print_stack();
	for (i = 1; i < stack_clause_size; ++i)
		if (!stack_clause[i].deleted) {
			for (ok = 0, j = 0; !ok && j < stack_clause[i].n; ++j)
				ok |= SAT_lit_value(stack_clause[i].lit[j]) == VAL_TRUE;
			if (!ok) {
				printf("unsatisfied clause found:");
				print_clause(i);
				assert(0);
			}
		}
}

#endif
