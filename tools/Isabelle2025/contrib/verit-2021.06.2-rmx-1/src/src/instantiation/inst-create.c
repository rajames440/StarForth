#include "instantiation/inst-create.h"

#include "float.h"
#include "instantiation/free-vars.h"
#include "instantiation/inst-pre.h"
#include "instantiation/inst-trigger-selection.h"
#include "pre/pre.h"
#include "proof/proof.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-ptr.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/DAG.h"
#include "utils/bitset.h"
#include "utils/options.h"
#include "utils/stack.h"
#include "utils/statistics.h"

/*
  --------------------------------------------------------------
  Stats
  --------------------------------------------------------------
*/

#ifdef STATS_INST

/* \brief How much time handling unifirers consumed */
static unsigned insts_stats_time;

/* for interal control */
float insts_time;
static float tmp_insts_time;

#endif

/*
  --------------------------------------------------------------
  Setting
  --------------------------------------------------------------
*/

Tstack_DAG lemmas;

/** \brief store instantiations (in tree form) from triggers and sorts */
static Tstack_Tinst qform_insts;

/** Accumulates all instances generated */
static Tbs insts_bs = NULL;

/*
  --------------------------------------------------------------
  Instantiations management
  --------------------------------------------------------------
*/

static void
insts_bs_hook_resize(unsigned old_alloc, unsigned new_alloc)
{
	if (!new_alloc) return;
	new_alloc = (new_alloc + 7) >> 3;
	assert(new_alloc >= insts_bs->size);
	MY_REALLOC(insts_bs, sizeof(struct TSbs) + new_alloc * sizeof(unsigned char));
	memset(
		insts_bs->v + insts_bs->size, '\0',
		(new_alloc - insts_bs->size) * sizeof(unsigned char));
	insts_bs->size = new_alloc;
	/* bitset_enlarge(insts_bs, new_alloc); */
}

int
inst_cmp_q(Tinst* Pinst1, Tinst* Pinst2)
{
	return (int)Pinst1->DAG - (int)Pinst2->DAG;
}

void
inst_free(Tinst inst)
{
	unsigned i;
	assert(inst.size);
	for (i = 0; i < inst.size; ++i)
		if (inst.next[i].size) /* Whether it's not a leaf */
			inst_free(inst.next[i]);
	free(inst.next);
}

static bool
inst_redundant(Tinst inst, Tunifier unifier)
{
	unsigned i, var_pos = 0;
	Tinst tmp_inst;
	tmp_inst = inst;
	while (tmp_inst.next) {
		for (i = 0; i < tmp_inst.size; ++i)
			if (tmp_inst.next[i].DAG == unifier->val[var_pos].term) break;
		if (i == tmp_inst.size) return false;
		tmp_inst = tmp_inst.next[i];
		++var_pos;
	}
	return true;
}

/* Needs to be destructive, if reachs unifier is not fresh */
static Tinst
build_inst_tree(Tinst inst, Tunifier unifier, unsigned var_pos)
{
	unsigned i;
	if (var_pos == unifier->size) return inst;
	for (i = 0; i < inst.size; ++i)
		if (inst.next[i].DAG == unifier->val[var_pos].term) {
			inst.next[i] = build_inst_tree(inst.next[i], unifier, var_pos + 1);
			return inst;
		}
	MY_REALLOC(inst.next, (inst.size + 1) * sizeof(Tinst));
	inst.next[inst.size].DAG = unifier->val[var_pos].term;
	inst.next[inst.size].size = 0;
	inst.next[inst.size++].next = NULL;
	if (var_pos == unifier->size - 1) return inst;
	inst.next[inst.size - 1] =
		build_inst_tree(inst.next[inst.size - 1], unifier, var_pos + 1);
	return inst;
}

/* TODO: does not account for different unifiers generating the same
   instance, for that it would be required to have the instance DAG
   pinned down... only relevant if it happens ofter, check this */
/**
   \brief adds a given instantiation, if fresh, to set of
   instantiations of given formula
   \param form a formula being instantiated
   \param unifier an instantiation
   \return true if provided instantiation is fresh, false otherwise

   \remark Assumes the variables of the unifier are in the same order
   of the variables of formula */
bool
inst_add(TDAG form, Tunifier unifier)
{
	int imid, imin, imax;
	Tinst inst;
	/* Look if formula already has instantiations */
	assert(qform_insts);
	if (!stack_is_empty(qform_insts)) {
		imin = 0;
		imax = stack_size(qform_insts) - 1;
		do {
			imid = imin + (imax - imin) / 2;
			if (form == stack_get(qform_insts, imid).DAG) break;
			if (form < stack_get(qform_insts, imid).DAG)
				imax = imid - 1;
			else if (form > stack_get(qform_insts, imid).DAG)
				imin = imid + 1;
		} while (imin <= imax);
		/* Try to add unifier to inst tree */
		if (imin <= imax) {
			if (inst_redundant(stack_get(qform_insts, imid), unifier)) {
#if DEBUG_INST > 3
				my_message("unifier\n");
				unify_print(unifier);
				my_message("is redundant with inst:\n");
				print_Tinst(stack_get(qform_insts, imid), 0);
				my_message_return();
#endif
				return false;
			}
			stack_set(
				qform_insts, imid,
				build_inst_tree(stack_get(qform_insts, imid), unifier, 0));
#if DEBUG_INST > 3
			my_message("unifier\n");
			unify_print(unifier);
			my_DAG_message("added into:\n");
			print_Tinst(stack_get(qform_insts, imid), 0);
			my_message_return();
#endif
			return true;
		}
	}
	/* First instantiation */
	inst.DAG = form;
	inst.size = 0;
	inst.next = NULL;
	inst = build_inst_tree(inst, unifier, 0);
#if DEBUG_INST > 3
	my_message("unifier\n");
	unify_print(unifier);
	my_DAG_message("added into:\n");
	print_Tinst(inst, 0);
#endif
	stack_push(qform_insts, inst);
	stack_sort(qform_insts, inst_cmp_q);
	return true;
}

/*
  --------------------------------------------------------------
  Creating instantiation lemmas
  --------------------------------------------------------------
*/

/**
   \brief traverses a DAG and instantiates it according to what is in DAG_tmp.
   \input src a DAG
   \return whether the instantiated DAG is different than the given one
   \remark uses DAG_tmp */
static unsigned
DAG_tmp_inst(TDAG src)
{
	unsigned i, res = 0;
	TDAG *PDAG, tmp_DAG;
	Tstack_DAG DAGs;
	if (DAG_tmp_DAG[src]) return DAG_tmp_DAG[src] != src;
	for (i = 0; i < DAG_arity(src); i++) res |= DAG_tmp_inst(DAG_arg(src, i));
	if (res) {
		if (DAG_symb(src) == QUANTIFIER_FORALL) {
			stack_INIT(DAGs);
			for (i = 0; i < DAG_arity(src) - 1; i++)
				if (DAG_tmp_DAG[DAG_arg(src, i)] == DAG_arg(src, i))
					stack_push(DAGs, DAG_arg(src, i));
			if (!stack_is_empty(DAGs)) {
				stack_push(DAGs, DAG_tmp_DAG[DAG_arg_last(src)]);
				tmp_DAG = DAG_new_stack(QUANTIFIER_FORALL, DAGs);
			} else
				tmp_DAG = DAG_tmp_DAG[DAG_arg_last(src)];
			stack_free(DAGs);
			DAG_tmp_DAG[src] = tmp_DAG;
			return 1;
		}
		MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
		for (i = 0; i < DAG_arity(src); i++) PDAG[i] = DAG_tmp_DAG[DAG_arg(src, i)];
		tmp_DAG = DAG_new(DAG_symb(src), DAG_arity(src), PDAG);
		DAG_tmp_DAG[src] = tmp_DAG;
		return 1;
	}
	DAG_tmp_DAG[src] = src;
	return 0;
}

/**
   \brief determine if the body of the quantified formula or one of the clauses
   in its CNF is going to be instantiated
   \input DAGinst object in which quantified formula and clause, if any, are
   \return the DAG to be instantiated
   \remark if a clause is going to be instantiated, a clause DAG is built, since
   clauses are represented as lists of literals */
static TDAG
get_instance(TDAGinst DAGinst)
{
	if (!DAGinst.clause) return DAG_arg_last(DAGinst.qform);
	if (stack_size(DAGinst.clause) == 1)
		return DAG_dup(stack_get(DAGinst.clause, 0));
	return DAG_dup(DAG_new_stack(CONNECTOR_OR, DAGinst.clause));
}

void
inst_unifiers(TDAGinst DAGinst)
{
	unsigned i;
	TDAG DAG, instance, lemma = DAG_NULL;
	Tunifier unifier;
	DAG = get_instance(DAGinst);
#ifdef STATS_INST
	stats_timer_start(insts_stats_time);
	tmp_insts_time = stats_timer_get(insts_stats_time);
#endif
#if DEBUG_INST > 1
	if (!DAGinst.clause)
		my_DAG_message("Instances of {%d}%D\n", DAGinst.qform, DAGinst.qform);
	else
		my_DAG_message(
			"Instances of clause from {%d}%D\n", DAGinst.qform, DAGinst.qform);
#endif
	/* I only don't need the thing of redundant insts if I'm saturating sort
     classes. Just do it and get this over with. */
	while (!stack_is_empty(DAGinst.insts)) {
		unifier = stack_pop(DAGinst.insts);
		if (!DAGinst.clause && !inst_add(DAGinst.qform, unifier)) {
			/* #ifdef STATS_INST */
			/*           if (doing_triggers) */
			/*             stats_counter_inc(insts_stats_redundant_triggers_tree); */
			/*           else */
			/*             stats_counter_inc(insts_stats_redundant_sorts); */
			/* #endif */
			unify_free(unifier);
			continue;
		}
#ifdef DEBUG
		for (i = 0; i < unifier->size; ++i)
			assert(DAG_sort(unifier->val[i].var) == DAG_sort(unifier->val[i].term));
#endif
		DAG_tmp_reserve();
		for (i = 0; i < unifier->size; ++i)
			DAG_tmp_DAG[unifier->val[i].var] = unifier->val[i].term;
		DAG_tmp_inst(DAG);
		instance = DAG_dup(DAG_tmp_DAG[DAG]);
		if (proof_on) {
			DAG_tmp_reset_DAG(DAG);
			/* Since pre-processing not removing "y" in (forall xy. p(x)) */
			for (i = 0; i < unifier->size; ++i)
				DAG_tmp_DAG[unifier->val[i].var] = DAG_NULL;
		} else
			DAG_tmp_reset_DAG(DAG);
		DAG_tmp_release();
		lemma = DAG_dup(DAG_or2(DAG_not(DAGinst.qform), instance));
		DAG_free(instance);
		if (!bitset_in(insts_bs, lemma)) {
/* #ifdef STATS_INST */
/*           if (doing_ccfv) */
/*             stats_counter_inc(insts_stats_ccfv); */
/*           else if (doing_triggers) */
/*             stats_counter_inc(insts_stats_triggers); */
/*           else */
/*             stats_counter_inc(insts_stats_sorts); */
/* #endif */
#if DEBUG_INST > 1
			unify_print(unifier);
			my_DAG_message("\t{%d}%D\n", instance, instance);
#endif
			bitset_insert(insts_bs, DAG_dup(lemma));
			if (proof_on) {
				Tproof proof = 0;
				TDAG* PDAG;
				MY_MALLOC(PDAG, 2 * unifier->size * sizeof(TDAG));
				for (i = 0; i < unifier->size; ++i) {
					PDAG[2 * i] = unifier->val[i].var;
					PDAG[2 * i + 1] = unifier->val[i].term;
				}

				/* We need add the proof of the clausification */
				if (DAGinst.clause && DAG != DAG_arg_last(DAGinst.qform))  {

					/* Build the clausification implicatino
					   {not (forall x. f(x)),  forall x. f'(x)}
					   where f'(x) is a clause of f(x) */
					Tstack_DAG quantifier_args;
					stack_INIT_s(quantifier_args, stack_size(get_fvars(DAG)) + 1);
					stack_merge(quantifier_args, get_fvars(DAG));
					stack_push(quantifier_args, DAG);
					TDAG cnf_q = DAG_new_stack(QUANTIFIER_FORALL, quantifier_args);
					TDAG imp = DAG_dup(DAG_or2(DAG_not(DAGinst.qform), cnf_q));
					stack_free(quantifier_args);

					Tproof clausify_lemma = proof_lemma_get(imp);
					assert(clausify_lemma);
#ifdef DEBUG
					Tstack_DAG clausify_DAGs = stack_get(top_steps, clausify_lemma)->DAGs;
					assert(stack_size(clausify_DAGs) == 1);
					assert(stack_get(clausify_DAGs, 0) == imp);
#endif /* DEBUG */
					clausify_lemma = proof_or(clausify_lemma);

					/* instantiation step into the clause */
					TDAG sub_lemma = DAG_dup(DAG_or2(DAG_not(cnf_q), instance));
					proof = proof_add_forall_inst_lemma(sub_lemma, 2 * unifier->size, PDAG);
					Tproof proof1 = proof_or(proof);

					/* 'lemma' is the target */
					Tproof proof2 = proof_or_neg(lemma, 0);
					Tproof proof3 = proof_or_neg(lemma, 1);

					/* Build the resolution */
					proof = proof_resolve(4, clausify_lemma, proof1, proof2, proof3);
				}
				if (!proof)
					proof = proof_add_forall_inst_lemma(lemma, 2 * unifier->size, PDAG);

				lemma = pre_process_instance_proof(lemma, &proof);
				proof_set_lemma_id(lemma, proof);
				free(PDAG);
			} else
				lemma = pre_process_instance(lemma);
			stack_push(lemmas, lemma);
			unify_free(unifier);
			continue;
		}
		/* #ifdef STATS_INST */
		/*       if (doing_ccfv) */
		/*         stats_counter_inc(insts_stats_redundant_ccfv); */
		/*       else if (doing_triggers) */
		/*         stats_counter_inc(insts_stats_redundant_triggers); */
		/*       else */
		/*         stats_counter_inc(insts_stats_redundant_sorts); */
		/* #endif */
		DAG_free(lemma);
		unify_free(unifier);
	}
	stack_free(DAGinst.insts);
	if (DAGinst.clause) {
		DAG_free(DAG);
		return;
	}
#ifdef STATS_INST
	insts_time += stats_timer_get(insts_stats_time) - tmp_insts_time;
	stats_timer_stop(insts_stats_time);
#endif
}

bool
inst_unifier(TDAG qform, Tstack_DAG clause, Tunifier unifier)
{
	unsigned i;
	TDAG DAG, instance, lemma = DAG_NULL;
	/* Determine what is going to be instantiated */
	DAG = !clause
		? DAG_arg_last(qform)
		: (stack_size(clause) == 1 ? DAG_dup(stack_get(clause, 0))
															 : DAG_dup(DAG_new_stack(CONNECTOR_OR, clause)));
#ifdef STATS_INST
	stats_timer_start(insts_stats_time);
	tmp_insts_time = stats_timer_get(insts_stats_time);
#endif
#if DEBUG_INST > 1
	if (!clause)
		my_DAG_message("Instances of {%d}%D\n", qform, qform);
	else
		my_DAG_message("Instances of clause from {%d}%D\n", qform, qform);
#endif
	/* Whether unifier was already used */
	if (!clause && !inst_add(qform, unifier)) return false;
#ifdef DEBUG
	unify_print(unifier);
	for (i = 0; i < unifier->size; ++i)
		assert(DAG_sort(unifier->val[i].var) == DAG_sort(unifier->val[i].term));
#endif
	DAG_tmp_reserve();
	for (i = 0; i < unifier->size; ++i)
		DAG_tmp_DAG[unifier->val[i].var] = unifier->val[i].term;
	DAG_tmp_inst(DAG);
	instance = DAG_dup(DAG_tmp_DAG[DAG]);
	if (proof_on) {
		DAG_tmp_reset_DAG(DAG);
		/* Since pre-processing not removing "y" in (forall xy. p(x)) */
		for (i = 0; i < unifier->size; ++i)
			DAG_tmp_DAG[unifier->val[i].var] = DAG_NULL;
	} else
		DAG_tmp_reset_DAG(DAG);
	DAG_tmp_release();
	lemma = DAG_dup(DAG_or2(DAG_not(qform), instance));
	DAG_free(instance);
	/* TODO: why is it that I need this */
	/* Whether lemma was already used */
	if (bitset_in(insts_bs, lemma)) {
		DAG_free(lemma);
		return false;
	}
#if DEBUG_INST > 1
	unify_print(unifier);
	my_DAG_message("\t{%d}%D\n", instance, instance);
#endif
	bitset_insert(insts_bs, DAG_dup(lemma));
	if (proof_on) {
		Tproof proof;
		TDAG* PDAG;
		MY_MALLOC(PDAG, 2 * unifier->size * sizeof(TDAG));
		for (i = 0; i < unifier->size; ++i) {
			PDAG[2 * i] = unifier->val[i].var;
			PDAG[2 * i + 1] = unifier->val[i].term;
		}
		proof = proof_add_forall_inst_lemma(lemma, 2 * unifier->size, PDAG);
		lemma = pre_process_instance_proof(lemma, &proof);
		proof_set_lemma_id(lemma, proof);
		free(PDAG);
	} else
		lemma = pre_process_instance(lemma);
	stack_push(lemmas, lemma);
	return true;
}

void
inst_create_init(void)
{
	stack_INIT(lemmas);
	stack_INIT(qform_insts);

	insts_bs = bitset_new(1);
	DAG_set_hook_resize(insts_bs_hook_resize);

#ifdef STATS_INST
	/* TODO: depends on STATS */
	insts_time = 0;

	insts_stats_time =
		stats_timer_new("insts/time", "Insts time", "%7.2f", STATS_TIMER_ALL);
#endif
}

void
inst_create_done(void)
{
	unsigned i, w;
	for (w = 0; w < insts_bs->size; ++w)
		for (i = 0; i < 8; ++i)
			if (insts_bs->v[w] & (1 << i)) DAG_free(w * 8 + i);
	bitset_free(insts_bs);
	while (!stack_is_empty(qform_insts)) inst_free(stack_pop(qform_insts));
	stack_free(qform_insts);
	stack_free(lemmas);
}

/*
  --------------------------------------------------------------
  Debugging
  --------------------------------------------------------------
*/

#ifdef DEBUG

void
print_Tinst(Tinst inst, unsigned depth)
{
	unsigned i;
	char* shift = NULL;
	MY_MALLOC(shift, (depth * 2 + 1) * sizeof(char));
	memset(shift, ' ', (depth * 2));
	shift[depth * 2] = '\0';
	/* my_DAG_message("%s[%d/%D]\n", shift, inst.DAG, inst.DAG); */
	my_DAG_message("%s[{%d}%D]\n", shift, inst.DAG, inst.DAG);
	for (i = 0; i < inst.size; ++i) print_Tinst(inst.next[i], depth + 1);
	if (depth == 1)
		printf("--------------------------------------------------\n");
	free(shift);
}

void
print_Tstack_Tinst(Tstack_Tinst stack)
{
	unsigned i, j;
	for (i = 0; i < stack_size(stack); ++i) {
		my_DAG_message(
			"Formula: {%d}%D\n", stack_get(stack, i).DAG, stack_get(stack, i).DAG);
		for (j = 0; j < stack_get(stack, i).size; ++j)
			print_Tinst(stack_get(stack, i).next[j], 0);
	}
	my_message_return();
}

#endif /* DEBUG */
