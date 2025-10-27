/*
  --------------------------------------------------------------
  Literal management Module
  --------------------------------------------------------------
*/

#include "bool/literal.h"

#include "symbolic/DAG-print.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/DAG.h"
#include "symbolic/recursion.h"
#include "utils/general.h"
#include "utils/stack.h"
#include "veriT-config.h"

Tvar var_max = 0;
Tvar var_available = 0;
static unsigned var_alloc = 0;
static TDAG* var2DAG = NULL;
static Tvar* DAG2var = NULL;

bool* bool_required = NULL;
#ifdef POLARITY_FILTER
/**
   \brief counter for number of occurrences of literals */
static unsigned* bool_counter = NULL;

/* \brief Stores the original formula (pre-processed) along with lemmas */
Tstack_DAG orig_formula;
#endif

/* #define DEBUG_BOOL */

static void
literal_DAG_hook_resize(unsigned old_alloc, unsigned new_alloc)
{
	unsigned i;
	MY_REALLOC(DAG2var, new_alloc * sizeof(Tvar));
	for (i = old_alloc; i < new_alloc; i++) DAG2var[i] = 0;
}

Tlit
DAG_is_lit(TDAG DAG)
{
	return DAG2var[DAG] << 1;
}

Tlit
DAG_to_lit(TDAG DAG)
{
	unsigned positive = 1;
	while (DAG_symb(DAG) == CONNECTOR_NOT) {
		DAG = DAG_arg0(DAG);
		positive = !positive;
	}
	if (!DAG2var[DAG]) {
		var_max++;
		if (var_alloc == var_max) {
			unsigned i;
			var_alloc *= 2;
			MY_REALLOC(var2DAG, var_alloc * sizeof(TDAG));
			MY_REALLOC(bool_required, 2 * var_alloc * sizeof(bool));
#ifdef POLARITY_FILTER
			MY_REALLOC(bool_counter, 2 * var_alloc * sizeof(unsigned));
#endif
			for (i = var_max; i < var_alloc; i++) {
				var2DAG[i] = 0;
				bool_required[i << 1] = false;
				bool_required[(i << 1) | 1] = false;
#ifdef POLARITY_FILTER
				bool_counter[i << 1] = 0;
				bool_counter[(i << 1) | 1] = 0;
#endif
			}
		}
		DAG2var[DAG] = var_max;
		var2DAG[var_max] = DAG_dup(DAG);
		SAT_var_new_id(var_max);
#ifndef POLARITY_FILTER
		bool_required[var_max << 1] = bool_required[(var_max << 1) | 1] =
			!boolean_connector(DAG_symb(DAG)) && DAG_arity(DAG);
#endif
#ifdef DEBUG_BOOL
		my_DAG_message("DAG variable: %D, %d\n", DAG, var_max);
#endif
	}
	return lit_make(DAG2var[DAG], positive);
}

TDAG
lit_to_DAG(Tlit lit)
{
	return var2DAG[lit_var(lit)];
}

TDAG
DAG_to_var(TDAG DAG)
{
	return DAG2var[DAG];
}

TDAG
var_to_DAG(Tvar var)
{
	assert(var <= var_max);
	return var2DAG[var];
}

#ifdef POLARITY_FILTER
void
bool_lit_inc(Tlit lit)
{
	if (
		boolean_connector(DAG_symb(var_to_DAG(lit_var(lit)))) ||
		!DAG_arity(var_to_DAG(lit_var(lit))))
		return;
	/* my_DAG_message("%d %D\n", lit_var(lit)); */
	assert(lit_var(lit) <= var_max);
	bool_counter[lit]++;
	bool_required[lit] = true;
}

void
bool_lit_dec(Tlit lit)
{
	if (
		boolean_connector(DAG_symb(var_to_DAG(lit_var(lit)))) ||
		!DAG_arity(var_to_DAG(lit_var(lit))))
		return;
	DAG_print(lit_var(lit));
	assert(bool_counter[lit]);
	bool_counter[lit]--;
	if (!bool_counter[lit]) bool_required[lit] = false;
}

/*
  ----------------------------------------------------------------
  Pruning CNF model according to original formula and added lemmas
  ----------------------------------------------------------------
*/

Tboolean_value* value_of;
Tboolean_value* input_req;

bool* original_required = NULL;

/**
   \brief Goes through DAG and determines which subDAGs are required
   for it to keep its truth value.

   \remark Only explores branches which may be required. Does not
   assign un-requiredness.

   \remark This is non-deterministic. It does not yields the minimal
   scenario. */
void
set_required(TDAG DAG)
{
	unsigned i;
	Tsymb top_symbol = DAG_symb(DAG);
	Tboolean_value sufficient_value;
	bool req_all;
	if (DAG_tmp_bool[DAG]) return;
	DAG_tmp_bool[DAG] = 1;
	assert(value_of[DAG] != BOOL_UNDEFINED);
	input_req[DAG] = BOOL_TRUE;
	if (quantifier(top_symbol)) return;
	if (top_symbol == CONNECTOR_NOT) {
		set_required(DAG_arg0(DAG));
		return;
	}
	if (top_symbol == CONNECTOR_AND || top_symbol == CONNECTOR_OR) {
		req_all = value_of[DAG] ? (top_symbol == CONNECTOR_AND ? true : false)
														: (top_symbol == CONNECTOR_AND ? false : true);
		sufficient_value = top_symbol == CONNECTOR_AND ? BOOL_FALSE : BOOL_TRUE;
		if (req_all) {
			for (i = 0; i < DAG_arity(DAG); ++i) set_required(DAG_arg(DAG, i));
			return;
		}
		/* TODO: HB Probably better to go in the one that has more
         nodes; or the one that has more literals in the model
         (expensive to compute? Probably doesn't pay off)... */
		for (i = 0; i < DAG_arity(DAG); ++i)
			if (value_of[DAG_arg(DAG, i)] == sufficient_value) {
				set_required(DAG_arg(DAG, i));
				return;
			}
		assert(false);
		return;
	}
	if (top_symbol == CONNECTOR_IMPLIES) {
		/* Only first arg is required */
		if (value_of[DAG] && value_of[DAG_arg0(DAG)] == BOOL_FALSE) {
			set_required(DAG_arg0(DAG));
			return;
		}
		/* Either false or arg0 is true, therefore both are required */
		set_required(DAG_arg0(DAG));
		set_required(DAG_arg1(DAG));
		return;
	}
	if (top_symbol == CONNECTOR_EQUIV) {
		set_required(DAG_arg0(DAG));
		set_required(DAG_arg1(DAG));
		return;
	}
	if (top_symbol == CONNECTOR_ITE) {
		set_required(DAG_arg(DAG, 0));
		/* If cond is false, arg2 is required, otherwise it's arg1 */
		set_required(DAG_arg(DAG, value_of[DAG_arg(DAG, 0)] ? 1 : 2));
		return;
	}
	assert(DAG_literal(DAG));
}

/* \brief Assigns truth value to DAG according to SAT_stack  */
void
eval_DAG(TDAG DAG)
{
	unsigned i;
	Tboolean_value sufficient_value;
	bool has_undef;
	Tsymb top_symbol = DAG_symb(DAG);
	if (DAG_tmp_bool[DAG] || quantifier(top_symbol)) return;
	DAG_tmp_bool[DAG] = 1;
	/* If arg is FALSE/TRUE, negation is opposite; otherwise UNDEF */
	if (top_symbol == CONNECTOR_NOT) {
		eval_DAG(DAG_arg0(DAG));
		assert(!DAG_literal(DAG) || value_of[DAG_arg0(DAG)] != BOOL_UNDEFINED);
		value_of[DAG] = value_of[DAG_arg0(DAG)] == BOOL_UNDEFINED
			? BOOL_UNDEFINED
			: !value_of[DAG_arg0(DAG)];
		return;
	}
	/* For AND: if any arg is FALSE, result is FALSE; otherwise it's UNDEF if any
     arg is UNDEF; otherwise TRUE. For OR: converse. */
	if (top_symbol == CONNECTOR_AND || top_symbol == CONNECTOR_OR) {
		sufficient_value = top_symbol == CONNECTOR_AND ? BOOL_FALSE : BOOL_TRUE;
		has_undef = false;
		/* Set value of all args */
		for (i = 0; i < DAG_arity(DAG); ++i) eval_DAG(DAG_arg(DAG, i));
		for (i = 0; i < DAG_arity(DAG); ++i) {
			if (value_of[DAG_arg(DAG, i)] == sufficient_value) {
				value_of[DAG] = sufficient_value;
				return;
			}
			if (value_of[DAG_arg(DAG, i)] == BOOL_UNDEFINED) has_undef = true;
		}
		value_of[DAG] = has_undef ? BOOL_UNDEFINED : !sufficient_value;
		return;
	}
	if (top_symbol == CONNECTOR_IMPLIES) {
		eval_DAG(DAG_arg0(DAG));
		eval_DAG(DAG_arg1(DAG));
		if (value_of[DAG_arg0(DAG)] == BOOL_FALSE) {
			value_of[DAG] = BOOL_TRUE;
			return;
		}
		if (value_of[DAG_arg0(DAG)] == BOOL_UNDEFINED) {
			value_of[DAG] = BOOL_UNDEFINED;
			return;
		}
		value_of[DAG] = value_of[DAG_arg1(DAG)];
		return;
	}
	if (top_symbol == CONNECTOR_EQUIV) {
		eval_DAG(DAG_arg0(DAG));
		eval_DAG(DAG_arg1(DAG));
		if (
			value_of[DAG_arg0(DAG)] == BOOL_UNDEFINED ||
			value_of[DAG_arg1(DAG)] == BOOL_UNDEFINED) {
			value_of[DAG] = BOOL_UNDEFINED;
			return;
		}
		value_of[DAG] = value_of[DAG_arg0(DAG)] == value_of[DAG_arg1(DAG)]
			? BOOL_TRUE
			: BOOL_FALSE;
		return;
	}
	if (top_symbol == CONNECTOR_ITE) {
		eval_DAG(DAG_arg(DAG, 0));
		eval_DAG(DAG_arg(DAG, 1));
		eval_DAG(DAG_arg(DAG, 2));
		if (value_of[DAG_arg(DAG, 0)] == BOOL_UNDEFINED) {
			value_of[DAG] = BOOL_UNDEFINED;
			return;
		}
		/* eval_DAG(DAG_arg(DAG, 2-value_of[DAG_arg(DAG, 0)])); */
		value_of[DAG] = value_of[DAG_arg(DAG, 0)] ? value_of[DAG_arg(DAG, 1)]
																							: value_of[DAG_arg(DAG, 2)];
		return;
	}
	assert(DAG_literal(DAG));
}

extern bool prime_implicant_off;

/* TODO: this appears to be wrong. Check it. */
void
prune_cnf_model(void)
{
	unsigned i;
	TDAG DAG;
	/* TODO: Use a DAG_tmp struct with being set, the value and requiredness */
	MY_MALLOC(input_req, stack_size(DAG_table) * (sizeof(Tboolean_value)));
	MY_MALLOC(value_of, stack_size(DAG_table) * (sizeof(Tboolean_value)));
	memset(value_of, 2, stack_size(DAG_table) * (sizeof(Tboolean_value)));
	memset(input_req, 2, stack_size(DAG_table) * (sizeof(Tboolean_value)));
	/* Set value of literals */
	for (i = 0; i < SAT_literal_stack_n; ++i) {
		if (!prime_implicant_off && !prime_required[SAT_literal_stack[i]]) continue;
		DAG = lit_to_DAG(SAT_literal_stack[i]);
		if (!DAG_literal(DAG) && !quantifier(DAG_symb(DAG))) continue;
		value_of[DAG] = lit_pol(SAT_literal_stack[i]);
	}
	/* Set value of subformulas */
	DAG_tmp_reserve();
	stack_apply(orig_formula, eval_DAG);
	stack_apply(orig_formula, DAG_tmp_reset_bool);
	DAG_tmp_release();
#ifdef DEBUG
	for (i = 0; i < stack_size(orig_formula); ++i)
		assert(value_of[stack_get(orig_formula, i)] == BOOL_TRUE);
#endif
	/* Set requiredness of all DAGs */
	DAG_tmp_reserve();
	stack_apply(orig_formula, set_required);
	stack_apply(orig_formula, DAG_tmp_reset_bool);
	DAG_tmp_release();
	/* TODO: What is the number of literals??? */
	MY_REALLOC(original_required, 2 * stack_size(DAG_table) * sizeof(bool));
	memset(original_required, 1, 2 * stack_size(DAG_table) * sizeof(bool));
	/* Set requiredness of literals */
	for (i = 0; i < SAT_literal_stack_n; ++i)
		if (input_req[lit_to_DAG(SAT_literal_stack[i])] != BOOL_TRUE)
			original_required[SAT_literal_stack[i]] = false;
	free(value_of);
	free(input_req);
}

#endif

void
literal_init(void)
{
	assert(!var2DAG);
	assert(!DAG2var);
	assert(!bool_required);
#ifdef POLARITY_FILTER
	assert(!bool_counter);
	stack_INIT(orig_formula);
#endif
	var_alloc = 64;
	MY_MALLOC(var2DAG, var_alloc * sizeof(TDAG));
	memset(var2DAG, 0, var_alloc * sizeof(TDAG));
	MY_MALLOC(bool_required, 2 * var_alloc * sizeof(bool));
	memset(bool_required, 0, 2 * var_alloc * sizeof(bool));
#ifdef POLARITY_FILTER
	MY_MALLOC(bool_counter, 2 * var_alloc * sizeof(unsigned));
	memset(bool_counter, 0, 2 * var_alloc * sizeof(unsigned));
#endif
	var2DAG[0] = DAG_NULL;
	var_max = 0;
	DAG_set_hook_resize(literal_DAG_hook_resize);
}

void
literal_reset(void)
{
	unsigned i;
	assert(var2DAG);
	assert(bool_required);
#ifdef POLARITY_FILTER
	assert(bool_counter);
#endif
	for (i = 1; i <= var_max; i++) {
		DAG2var[var2DAG[i]] = 0;
		DAG_free(var2DAG[i]);
		var2DAG[i] = 0;
		bool_required[i << 1] = false;
		bool_required[(i << 1) | 1] = false;
#ifdef POLARITY_FILTER
		bool_counter[i << 1] = 0;
		bool_counter[(i << 1) | 1] = 0;
#endif
	}
	var_max = 0;
}

void
literal_done(void)
{
	unsigned i;
	assert(var2DAG);
	for (i = 1; i <= var_max; i++) DAG_free(var2DAG[i]);
	free(var2DAG);
	free(bool_required);
#ifdef POLARITY_FILTER
	free(bool_counter);
	free(original_required);
	stack_apply(orig_formula, DAG_free);
	stack_free(orig_formula);
#endif
}
