#include "symbolic/context-recursion.h"

#include "symbolic/DAG-print.h"
#include "symbolic/DAG-prop.h"
#include "symbolic/DAG-symb-DAG.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/qnt-utils.h"
#include "utils/h-util.h"

/*
  --------------------------------------------------------------
  DAG_tmp handling
  --------------------------------------------------------------
*/

/**
   \brief retrieves DAG associated with <DAG, context>, if any
   \param DAG a formula or term
   \param context a recursion context
   \return DAG associated with the given pair, if any, otherwise DAG_NULL
   \remark assumes that there is an association to the given pair */
static inline TDAG
DAG_for_ctx(TDAG DAG)
{
	assert(DAG_ctx[DAG]);
	if (!context.pols) return DAG_ctx[DAG]->DAG;
	return DAG_ctx[DAG]->pol_DAG[ctx_pol];
}

/**
   \brief retrieves valuation of DAG in the current recursion
   \param DAG a formula or term
   \return the DAG associated with the given pair, if any, otherwise the DAG itself */
static inline TDAG
get_DAG_ctx(TDAG DAG)
{
	assert(DAG_ctx[DAG]);
	if (!context.pols) return DAG_ctx[DAG] ? DAG_ctx[DAG]->DAG : DAG;
	assert(!stack_is_empty(context.pols));
	return (DAG_ctx[DAG] && DAG_ctx[DAG]->pol_DAG[ctx_pol])
		? DAG_ctx[DAG]->pol_DAG[ctx_pol]
		: DAG;
}

/**
   \brief associates a DAG with a <DAG, context>
   \param DAG a formula or term
   \param new_DAG the DAG to be associated
   \remark assumes this is always a fresh setting */
static inline void
set_DAG_ctx(TDAG DAG, TDAG new_DAG)
{
	assert(!DAG_ctx[DAG] || !DAG_for_ctx(DAG));
	if (!context.pols) {
		if (!DAG_ctx[DAG]) MY_MALLOC(DAG_ctx[DAG], sizeof(TDAG));
		DAG_ctx[DAG]->DAG = new_DAG;
		return;
	}
	if (!DAG_ctx[DAG]) {
		MY_MALLOC(DAG_ctx[DAG], 4 * sizeof(TDAG));
		DAG_ctx[DAG]->pol_DAG[POL_NONE] = DAG_NULL;
		DAG_ctx[DAG]->pol_DAG[POL_POS] = DAG_NULL;
		DAG_ctx[DAG]->pol_DAG[POL_NEG] = DAG_NULL;
		DAG_ctx[DAG]->pol_DAG[POL_BOTH] = DAG_NULL;
	}
	DAG_ctx[DAG]->pol_DAG[ctx_pol] = new_DAG;
}

/**
   \brief releases all associations of sub DAGs of given DAG
   \param DAG a formula or term
   \param contex_type the type of context used */
static inline void
DAG_tmp_free_DAG_ctx_aux(TDAG DAG)
{
	if (!context.pols)
		DAG_free(DAG_ctx[DAG]->DAG);
	else {
		assert(!DAG_ctx[DAG]->pol_DAG[POL_NONE]);
		if (DAG_ctx[DAG]->pol_DAG[POL_POS])
			DAG_free(DAG_ctx[DAG]->pol_DAG[POL_POS]);
		if (DAG_ctx[DAG]->pol_DAG[POL_NEG])
			DAG_free(DAG_ctx[DAG]->pol_DAG[POL_NEG]);
		if (DAG_ctx[DAG]->pol_DAG[POL_BOTH])
			DAG_free(DAG_ctx[DAG]->pol_DAG[POL_BOTH]);
	}
	free(DAG_ctx[DAG]);
	DAG_ctx[DAG] = NULL;
}

/**
   \brief does the actual release for the respective DAG
   \param DAG a formula or term
   \param contex_type the type of context used */
static void
DAG_tmp_free_DAG_ctx(TDAG DAG)
{
	unsigned i;
	if (!DAG_ctx[DAG]) return;
	DAG_tmp_free_DAG_ctx_aux(DAG);
	for (i = 0; i < DAG_arity(DAG); i++) DAG_tmp_free_DAG_ctx(DAG_arg(DAG, i));
}

/*
  --------------------------------------------------------------
  Auxiliary
  --------------------------------------------------------------
*/

TDAG
DAG_tree_subst(TDAG src)
{
	unsigned i;
	bool changed;
	TDAG dest, *PDAG;
	if (!DAG_arity(src))
		return DAG_symb_DAG[DAG_symb(src)] ? DAG_dup(DAG_symb_DAG[DAG_symb(src)])
																			 : DAG_dup(src);
	MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
	for (i = 0, changed = false; i < DAG_arity(src); ++i) {
		PDAG[i] = DAG_tree_subst(DAG_arg(src, i));
		changed |= PDAG[i] != DAG_arg(src, i);
	}
	if (changed)
		dest = DAG_dup(DAG_new(DAG_symb(src), DAG_arity(src), PDAG));
	else {
		free(PDAG);
		dest = DAG_dup(src);
	}
	return dest;
}

/*
  --------------------------------------------------------------
  Context dependent structural recursion
  --------------------------------------------------------------
*/

/** For the mappings it will rely on DAG_symb_DAG... this seems a reasonable
    workaround, since the stuff to be mapped (in let and skolem, at least) is
    only variables */

static void (*context_rec_push)(TDAG, unsigned*) = NULL;
static void (*context_rec_pop)(TDAG, unsigned) = NULL;
static TDAG (*context_rec_reduce)(TDAG) = NULL;
static bool (*context_rec_cont)(TDAG) = NULL;

TDAG
context_tree_rec(TDAG src)
{
	unsigned i, j, pos;
	bool changed;
	TDAG dest, tmp;
	Tstack_DAG DAGs, trigger;
	Tstack_DAGstack *Ptriggers, triggers;
	if (context_rec_cont && !context_rec_cont(src)) return DAG_dup(src);
	changed = false;
	stack_INIT(DAGs);
	/* TODO: could have a flag for only getting here if in a "binder" rule, such
     as let, renaming etc */
	if (binder_all(DAG_symb(src))) {
		pos = 0;
		context_rec_push(src, &pos);
		for (i = pos; i < DAG_arity(src) - 1; ++i)
			stack_push(DAGs, DAG_tree_subst(DAG_arg(src, i)));
		stack_push(DAGs, context_tree_rec(DAG_arg_last(src)));
		dest = DAG_dup(
			stack_top(DAGs) == DAG_arg_last(src)
				? src
				: (stack_size(DAGs) == 1 ? stack_top(DAGs)
																 : DAG_new_stack(DAG_symb(src), DAGs)));
		/* Moving changed triggers, if any */
		if (dest != src && quantifier(DAG_symb(dest))) {
			Ptriggers = DAG_prop_get(src, DAG_PROP_TRIGGER);
			if (Ptriggers) {
				stack_INIT(triggers);
				for (i = 0; i < stack_size(*Ptriggers); ++i) {
					trigger = stack_get(*Ptriggers, i);
					stack_inc(triggers);
					stack_INIT(stack_top(triggers));
					for (j = 0; j < stack_size(trigger); ++j)
						stack_push(
							stack_top(triggers), DAG_tree_subst(stack_get(trigger, j)));
				}
				DAG_prop_set(dest, DAG_PROP_TRIGGER, &triggers);
			}
		}
		context_rec_pop(src, 0);
	} else {
		for (i = 0; i < DAG_arity(src); ++i) {
			context_rec_push(src, &i);
			stack_push(DAGs, context_tree_rec(DAG_arg(src, i)));
			context_rec_pop(src, i);
			changed |= stack_get(DAGs, i) != DAG_arg(src, i);
		}
		dest = changed ? DAG_dup(DAG_new_stack(DAG_symb(src), DAGs)) : DAG_dup(src);
	}
	stack_apply(DAGs, DAG_free);
	stack_free(DAGs);
	tmp = context_rec_reduce(dest);
	return tmp;
}

/**
   \brief builds a new DAG by applying a reduction function *either* a priori or
   a posteriori
   \param src the input DAG
   \param context the recursion context
   \remark src is never modified (dupped or freed) by reduction function
   \remark the context is updated accordingly whenever the function is applied
   in a sub DAG
   \remark whenever finds a binder, changes to tree recursion, for simplicity
   \remark a continuation condition may be set, tested w.r.t. the context */
static void
context_DAG_rec(TDAG src)
{
	unsigned i, pos, arg0, arg1;
	bool changed;
	TDAG *PDAG, dest, tmp;
	if (DAG_ctx[src] && DAG_for_ctx(src)) return;
	if (context_rec_cont && !context_rec_cont(src)) {
		set_DAG_ctx(src, DAG_dup(src));
		return;
	}
	if (binder_all(DAG_symb(src))) {
		dest = context_tree_rec(src);
		set_DAG_ctx(src, dest);
		return;
	}
	switch (DAG_arity(src)) {
		case 0: dest = DAG_dup(src); break;
		case 1:
			pos = 0;
			context_rec_push(src, &pos);
			context_DAG_rec(DAG_arg0(src));
			arg0 = get_DAG_ctx(DAG_arg0(src));
			context_rec_pop(src, 0);
			if (DAG_arg0(src) != arg0)
				dest = DAG_dup(DAG_new_unary(DAG_symb(src), arg0));
			else
				dest = DAG_dup(src);
			break;
		case 2:
			pos = 0;
			context_rec_push(src, &pos);
			context_DAG_rec(DAG_arg0(src));
			arg0 = get_DAG_ctx(DAG_arg0(src));
			context_rec_pop(src, 0);
			pos = 1;
			context_rec_push(src, &pos);
			context_DAG_rec(DAG_arg1(src));
			arg1 = get_DAG_ctx(DAG_arg1(src));
			context_rec_pop(src, 1);
			if (DAG_arg0(src) != arg0 || DAG_arg1(src) != arg1)
				dest = DAG_dup(DAG_new_binary(DAG_symb(src), arg0, arg1));
			else
				dest = DAG_dup(src);
			break;
		default:
			MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
			for (i = 0, changed = false; i < DAG_arity(src); i++) {
				context_rec_push(src, &i);
				context_DAG_rec(DAG_arg(src, i));
				PDAG[i] = get_DAG_ctx(DAG_arg(src, i));
				context_rec_pop(src, i);
				changed |= PDAG[i] != DAG_arg(src, i);
			}
			if (changed)
				dest = DAG_dup(DAG_new(DAG_symb(src), DAG_arity(src), PDAG));
			else {
				free(PDAG);
				dest = DAG_dup(src);
			}
	}
	/* TODO: could test here whether dest was already set, instead of applying
     reduction function. Worth it? */
	tmp = context_rec_reduce(dest);
	set_DAG_ctx(src, tmp);
}

TDAG
context_structural_recursion(
	TDAG src, void (*f_init)(void), void (*f_push)(TDAG, unsigned*),
	void (*f_pop)(TDAG, unsigned), TDAG (*f_reduce)(TDAG), bool (*cont)(TDAG))
{
	TDAG dest;
	f_init();
	context_rec_push = f_push;
	context_rec_pop = f_pop;
	context_rec_reduce = f_reduce;
	context_rec_cont = cont;
	DAG_tmp_reserve();
	context_DAG_rec(src);
	dest = DAG_dup(get_DAG_ctx(src));
	DAG_tmp_free_DAG_ctx(src);
	DAG_tmp_release();
	ctx_free(context);
	return dest;
}

void
context_structural_recursion_array(
	unsigned n, TDAG* Psrc, void (*f_init)(void), void (*f_push)(TDAG, unsigned*),
	void (*f_pop)(TDAG, unsigned), TDAG (*f_reduce)(TDAG), bool (*cont)(TDAG))
{
	unsigned i;
	TDAG* dest;
	f_init();
	context_rec_push = f_push;
	context_rec_pop = f_pop;
	context_rec_reduce = f_reduce;
	context_rec_cont = cont;
	DAG_tmp_reserve();
	MY_MALLOC(dest, n * sizeof(TDAG));
	for (i = 0; i < n; ++i) {
		context_DAG_rec(Psrc[i]);
		dest[i] = DAG_dup(get_DAG_ctx(Psrc[i]));
	}
	for (i = 0; i < n; ++i) {
		DAG_tmp_free_DAG_ctx(Psrc[i]);
		DAG_free(Psrc[i]);
		Psrc[i] = dest[i];
	}
	free(dest);
	DAG_tmp_release();
	ctx_free(context);
}

/*
  --------------------------------------------------------------
  Contextual recursion that does not need substitutions
  --------------------------------------------------------------
*/

static void
nosub_context_DAG_rec(TDAG src)
{
	unsigned i, pos, arg0, arg1;
	bool changed;
	TDAG *PDAG, dest, tmp;
	Tstack_DAGstack *Ptriggers, triggers;
	if (DAG_tmp_DAG[src]) return;
	switch (DAG_arity(src)) {
		case 0: dest = DAG_dup(src); break;
		case 1:
			pos = 0;
			context_rec_push(src, &pos);
			nosub_context_DAG_rec(DAG_arg0(src));
			arg0 = DAG_tmp_DAG[DAG_arg0(src)];
			context_rec_pop(src, 0);
			if (DAG_arg0(src) != arg0)
				dest = DAG_dup(DAG_new_unary(DAG_symb(src), arg0));
			else
				dest = DAG_dup(src);
			break;
		case 2:
			pos = 0;
			context_rec_push(src, &pos);
			nosub_context_DAG_rec(DAG_arg0(src));
			arg0 = DAG_tmp_DAG[DAG_arg0(src)];
			context_rec_pop(src, 0);
			pos = 1;
			context_rec_push(src, &pos);
			nosub_context_DAG_rec(DAG_arg1(src));
			arg1 = DAG_tmp_DAG[DAG_arg1(src)];
			context_rec_pop(src, 1);
			if (DAG_arg0(src) != arg0 || DAG_arg1(src) != arg1)
				dest = DAG_dup(DAG_new_binary(DAG_symb(src), arg0, arg1));
			else
				dest = DAG_dup(src);
			break;
		default:
			MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
			for (i = 0, changed = false; i < DAG_arity(src); i++) {
				context_rec_push(src, &i);
				nosub_context_DAG_rec(DAG_arg(src, i));
				PDAG[i] = DAG_tmp_DAG[DAG_arg(src, i)];
				context_rec_pop(src, i);
				changed |= PDAG[i] != DAG_arg(src, i);
			}
			if (changed)
				dest = DAG_dup(DAG_new(DAG_symb(src), DAG_arity(src), PDAG));
			else {
				free(PDAG);
				dest = DAG_dup(src);
			}
	}
	/* Moving triggers, if any */
	if (dest != src && quantifier(DAG_symb(dest))) {
		Ptriggers = DAG_prop_get(src, DAG_PROP_TRIGGER);
		if (Ptriggers) {
			triggers = copy_triggers(*Ptriggers);
			DAG_prop_set(dest, DAG_PROP_TRIGGER, &triggers);
		}
	}
	/* TODO: could test here whether dest was already set, instead of applying
     reduction function. Worth it? */
	tmp = context_rec_reduce(dest);
	DAG_tmp_DAG[src] = tmp;
}

TDAG
nosub_context_structural_recursion(
	TDAG src, void (*f_init)(void), void (*f_push)(TDAG, unsigned*),
	void (*f_pop)(TDAG, unsigned), TDAG (*f_reduce)(TDAG), bool (*cont)(TDAG))
{
	TDAG dest;
	f_init();
	context_rec_push = f_push;
	context_rec_pop = f_pop;
	context_rec_reduce = f_reduce;
	context_rec_cont = cont;
	DAG_tmp_reserve();
	nosub_context_DAG_rec(src);
	dest = DAG_dup(DAG_tmp_DAG[src]);
	DAG_tmp_free_DAG(src);
	DAG_tmp_release();
	ctx_free(context);
	return dest;
}

void
nosub_context_structural_recursion_array(
	unsigned n, TDAG* Psrc, void (*f_init)(void), void (*f_push)(TDAG, unsigned*),
	void (*f_pop)(TDAG, unsigned), TDAG (*f_reduce)(TDAG), bool (*cont)(TDAG))
{
	unsigned i;
	TDAG* dest;
	f_init();
	context_rec_push = f_push;
	context_rec_pop = f_pop;
	context_rec_reduce = f_reduce;
	context_rec_cont = cont;
	DAG_tmp_reserve();
	MY_MALLOC(dest, n * sizeof(TDAG));
	for (i = 0; i < n; ++i) {
		nosub_context_DAG_rec(Psrc[i]);
		dest[i] = DAG_dup(DAG_tmp_DAG[Psrc[i]]);
	}
	for (i = 0; i < n; ++i) {
		DAG_tmp_free_DAG(Psrc[i]);
		DAG_free(Psrc[i]);
		Psrc[i] = dest[i];
	}
	free(dest);
	DAG_tmp_release();
	ctx_free(context);
}

/*
  ----------------------------------------------------------------------
  Contextual recursion as in CADE paper
  ----------------------------------------------------------------------
*/

/*
  --------------------------------------------------------------
  Contextual recursion for lets
  --------------------------------------------------------------
*/

TDAG let_context_tree_rec(TDAG src);

void
let_elim_push_let(TDAG src)
{
	unsigned i;
	TDAG* PDAG;
	MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
	/* Get the variable values */
	for (i = 1; i < DAG_arity(src); i += 2)
		PDAG[i] = let_context_tree_rec(DAG_arg(src, i));
	for (i = 0; i < DAG_arity(src) - 1u; i += 2) {
		/* Necessary because shadowing is allowed */
		stack_push(context.DAGs, DAG_arg(src, i));
		stack_push(context.DAGs, DAG_symb_DAG[DAG_symb(DAG_arg(src, i))]);
		/* Set new mapping */
		DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = PDAG[i + 1];
	}
	free(PDAG);
}

void
let_elim_pop_let(TDAG src)
{
	unsigned i, offset;
	/* Removing mapping from last substitution and resetting */
	offset = stack_size(context.DAGs) - (DAG_arity(src) - 1u);
	for (i = 0; i < DAG_arity(src) - 1u; i += 2) {
		DAG_free(DAG_symb_DAG[DAG_symb(stack_get(context.DAGs, i + offset))]);
		DAG_symb_DAG[DAG_symb(stack_get(context.DAGs, i + offset))] =
			stack_get(context.DAGs, i + offset + 1);
	}
	stack_dec_n(context.DAGs, DAG_arity(src) - 1u);
}

void
let_elim_push_quant(TDAG src)
{
	unsigned i;
	TDAG tmp;
	for (i = 0; i < DAG_arity(src) - 1u; ++i) {
		/* Account for shadowed variable */
		stack_push(context.DAGs, DAG_arg(src, i));
		stack_push(context.DAGs, DAG_symb_DAG[DAG_symb(DAG_arg(src, i))]);
		/* Variable already bound, rename it */
		if (DAG_symb_DAG[DAG_symb(DAG_arg(src, i))]) {
			tmp = DAG_new_nullary(DAG_symb_variable(DAG_sort(DAG_arg(src, i))));
			DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = DAG_dup(tmp);
			continue;
		}
		/* Mark that variables is bound */
		DAG_symb_DAG[DAG_symb(DAG_arg(src, i))] = DAG_arg(src, i);
	}
}

void
let_elim_pop_quant(TDAG src)
{
	unsigned i, offset;
	TDAG var;
	offset = stack_size(context.DAGs) - 2 * (DAG_arity(src) - 1u);
	for (i = 0; i < 2 * (DAG_arity(src) - 1u); i += 2) {
		var = stack_get(context.DAGs, i + offset);
		/* Variable was renamed */
		if (DAG_symb_DAG[DAG_symb(var)] && DAG_symb_DAG[DAG_symb(var)] != var)
			DAG_free(DAG_symb_DAG[DAG_symb(var)]);
		DAG_symb_DAG[DAG_symb(var)] = stack_get(context.DAGs, i + offset + 1);
	}
	stack_dec_n(context.DAGs, 2 * (DAG_arity(src) - 1u));
}

TDAG
let_context_tree_rec(TDAG src)
{
	unsigned i, j;
	bool changed;
	TDAG dest, tmp, arg0, arg1;
	Tstack_DAG DAGs, trigger;
	Tstack_DAGstack *Ptriggers, triggers;
	if (DAG_symb(src) == LET) {
		let_elim_push_let(src);
		dest = let_context_tree_rec(DAG_arg_last(src));
		let_elim_pop_let(src);
		return dest;
	} else if (quantifier(DAG_symb(src))) {
		stack_INIT(DAGs);
		let_elim_push_quant(src);
		for (i = 0; i < DAG_arity(src) - 1; ++i)
			stack_push(DAGs, DAG_dup(DAG_symb_DAG[DAG_symb(DAG_arg(src, i))]));
		stack_push(DAGs, let_context_tree_rec(DAG_arg_last(src)));
		if (stack_top(DAGs) != src) {
			dest = DAG_dup(DAG_new_stack(DAG_symb(src), DAGs));
			/* Moving changed triggers, if any */
			{
				Ptriggers = DAG_prop_get(src, DAG_PROP_TRIGGER);
				if (Ptriggers) {
					stack_INIT(triggers);
					for (i = 0; i < stack_size(*Ptriggers); ++i) {
						trigger = stack_get(*Ptriggers, i);
						stack_inc(triggers);
						stack_INIT(stack_top(triggers));
						for (j = 0; j < stack_size(trigger); ++j)
							stack_push(
								stack_top(triggers), DAG_tree_subst(stack_get(trigger, j)));
					}
					DAG_prop_set(dest, DAG_PROP_TRIGGER, &triggers);
				}
			}
		} else
			dest = DAG_dup(src);
		stack_apply(DAGs, DAG_free);
		stack_free(DAGs);
		let_elim_pop_quant(src);
		return dest;
	} else {
		switch (DAG_arity(src)) {
			case 0: dest = DAG_dup(src); break;
			case 1:
				arg0 = let_context_tree_rec(DAG_arg0(src));
				if (DAG_arg0(src) != arg0)
					dest = DAG_dup(DAG_new_unary(DAG_symb(src), arg0));
				else
					dest = DAG_dup(src);
				DAG_free(arg0);
				break;
			case 2:
				arg0 = let_context_tree_rec(DAG_arg0(src));
				arg1 = let_context_tree_rec(DAG_arg1(src));
				if (DAG_arg0(src) != arg0 || DAG_arg1(src) != arg1)
					dest = DAG_dup(DAG_new_binary(DAG_symb(src), arg0, arg1));
				else
					dest = DAG_dup(src);
				DAG_free(arg0);
				DAG_free(arg1);
				break;
			default:
				stack_INIT(DAGs);
				for (i = 0, changed = false; i < DAG_arity(src); ++i) {
					stack_push(DAGs, let_context_tree_rec(DAG_arg(src, i)));
					changed |= stack_get(DAGs, i) != DAG_arg(src, i);
				}
				if (changed)
					dest = DAG_dup(DAG_new_stack(DAG_symb(src), DAGs));
				else
					dest = DAG_dup(src);
				stack_apply(DAGs, DAG_free);
				stack_free(DAGs);
		}
	}
	tmp = context_rec_reduce(dest);
	return tmp;
}

static void
let_context_DAG_rec(TDAG src)
{
	unsigned i, arg0, arg1;
	bool changed;
	TDAG *PDAG, dest;
	if (DAG_tmp_DAG[src]) return;
	if (binder_all(DAG_symb(src))) {
		dest = let_context_tree_rec(src);
		DAG_tmp_DAG[src] = dest;
		return;
	}
	switch (DAG_arity(src)) {
		case 0: dest = DAG_dup(src); break;
		case 1:
			let_context_DAG_rec(DAG_arg0(src));
			arg0 = DAG_tmp_DAG[DAG_arg0(src)];
			if (DAG_arg0(src) != arg0)
				dest = DAG_dup(DAG_new_unary(DAG_symb(src), arg0));
			else
				dest = DAG_dup(src);
			break;
		case 2:
			let_context_DAG_rec(DAG_arg0(src));
			arg0 = DAG_tmp_DAG[DAG_arg0(src)];
			let_context_DAG_rec(DAG_arg1(src));
			arg1 = DAG_tmp_DAG[DAG_arg1(src)];
			if (DAG_arg0(src) != arg0 || DAG_arg1(src) != arg1)
				dest = DAG_dup(DAG_new_binary(DAG_symb(src), arg0, arg1));
			else
				dest = DAG_dup(src);
			break;
		default:
			MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
			for (i = 0, changed = false; i < DAG_arity(src); i++) {
				let_context_DAG_rec(DAG_arg(src, i));
				PDAG[i] = DAG_tmp_DAG[DAG_arg(src, i)];
				changed |= PDAG[i] != DAG_arg(src, i);
			}
			if (changed)
				dest = DAG_dup(DAG_new(DAG_symb(src), DAG_arity(src), PDAG));
			else {
				free(PDAG);
				dest = DAG_dup(src);
			}
	}
	DAG_tmp_DAG[src] = dest;
}

TDAG
let_context_structural_recursion(
	TDAG src, void (*f_init)(void), void (*f_push)(TDAG, unsigned*),
	void (*f_pop)(TDAG, unsigned), TDAG (*f_reduce)(TDAG))
{
	TDAG dest;
	f_init();
	context_rec_push = f_push;
	context_rec_pop = f_pop;
	context_rec_reduce = f_reduce;
	DAG_tmp_reserve();
	let_context_DAG_rec(src);
	dest = DAG_dup(DAG_tmp_DAG[src]);
	DAG_tmp_free_DAG(src);
	DAG_tmp_release();
	ctx_free(context);
	return dest;
}

/*
  --------------------------------------------------------------
  Init/Done
  --------------------------------------------------------------
*/

void
ctx_rec_init(void)
{
	context.pols = NULL;
	context.binders = 0;
	context.DAGs = NULL;
}

void
ctx_rec_done(void)
{}
