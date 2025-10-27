#include "proof/proof-output.h"

#include "instantiation/free-vars.h"
#include "limits.h"
#include "proof/proof-checking.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-prop.h"
#include "symbolic/DAG-tmp.h"

/* TODO: avoid this workaround */
extern bool proof_with_sharing;
extern bool option_proof_prune;
extern bool option_proof_merge;
extern bool option_proof_define_skolems;

#define shared DAG_tmp_unsigned
#define is_marked(x) ((shared[x] & 1) != 0)
#define mark(x) (shared[DAG] = shared[DAG] | 1)
#define get_idx(DAG) (shared[DAG] >> 1)

static inline void
set_idx_unmarked(TDAG DAG, unsigned idx) {
	assert(idx <= (UINT_MAX >> 1));
	shared[DAG] = idx << 1;
}

static inline void
set_idx_marked(TDAG DAG, unsigned idx) {
	assert(idx <= (UINT_MAX >> 1));
	shared[DAG] = idx << 1 | 1;
}

/**
   \brief resets DAG_tmp of all DAGs used during printing */
static void
proof_reset_DAG_tmp(Tproof_step proof_step)
{
	unsigned i, nb_bound_vars = 0;

	for (i = 0; i < stack_size(proof_step->DAGs); ++i)
		DAG_tmp_reset_unsigned(stack_get(proof_step->DAGs, i));

	if (proof_step->args)
		for (i = 0; i < stack_size(proof_step->args); ++i)
			DAG_tmp_reset_unsigned(stack_get(proof_step->args, i));

	if (proof_step->type < ps_type_subproof) return;

	for (i = 1; i < stack_size(proof_step->subproof_steps); ++i)
		proof_reset_DAG_tmp(stack_get(proof_step->subproof_steps, i));

	if (proof_step->args) {
		nb_bound_vars = stack_top(proof_step->args);

#ifdef DEBUG
		for (i = 0; i < nb_bound_vars; ++i)
			assert(!DAG_tmp_unsigned[stack_get(proof_step->args, i)]);
#endif

		for (i = nb_bound_vars; i < stack_size(proof_step->args) - 1u; i += 2) {
			assert(!DAG_tmp_unsigned[stack_get(proof_step->args, i)]);
			DAG_tmp_reset_unsigned(stack_get(proof_step->args, i + 1));
		}
	}
}

/**
   \brief prints the DAG in SMT-LIB 2 notation, without linefeed.
     Introduces definitions e.g. (! (+ a 1) :named p_12),
     so that p_12 may later be used.
   \param file the output file
   \param DAG the formula to output
   \param substitute_defs Replace internal constants (such as skolems)
     with their definition. If `DAG_tmp_unsigned[DAG_NULL]` is non-zero
     sharing is used and it is assumed that `DAG_tmp_unsigned` is set up
     properly. If false `DAG_tmp_unsigned` is not used.
   \remark Uses DAG_tmp and expects proper setup as done by write_proof. */
void
DAG_proof_print(FILE* file, TDAG DAG, bool substitute_defs)
{
	unsigned i, idx;
	bool sharing = false;
	assert(DAG != DAG_NULL);

	if (DAG_arity(DAG) == 0) {
		/* HB for skolem choice functions or ite csts */
		if (substitute_defs && shared[DAG])
			DAG_proof_print(file, shared[DAG], substitute_defs);
		else
			fprintf(file, "%s", DAG_symb_name_rectify(DAG_symb(DAG)));
		return;
	}

	if (substitute_defs && shared[DAG_NULL] > 0) {
		sharing = true;
		idx = shared[DAG] >> 1;
	}

	/* Sharing is active and this term gets a name */
	if (sharing && idx > 0) {
		/* We need to print a name for this term */
		if (is_marked(DAG)) {
			assert(is_marked(DAG));
			shared[DAG] = shared[DAG] & (~1); /* Remove mark */
			fprintf(file, "(! ");
		}
		/* We have already printed a name for this term */
		else {
			fprintf(file, "@p_%i", idx);
			return;
		}
	}

	/* print DAG symbol */
	fprintf(file, "(%s", DAG_symb_name_rectify(DAG_symb(DAG)));

	/* print DAG args */
	if (
		quantifier(DAG_symb(DAG)) || DAG_symb(DAG) == LAMBDA ||
		DAG_symb(DAG) == CHOICE_FUNCTION) {
		fprintf(file, " (");
		for (i = 0; i + 1 < DAG_arity(DAG); i++) {
			fprintf(
				file, "%s(%s ", i ? " " : "",
				DAG_symb_name_rectify(DAG_symb(DAG_arg(DAG, i))));
			DAG_sort_fprint(file, DAG_symb_sort(DAG_symb(DAG_arg(DAG, i))));
			fprintf(file, ")");
		}
		fprintf(file, ") ");
		DAG_proof_print(file, DAG_arg_last(DAG), substitute_defs);
	} else if (DAG_symb(DAG) == LET) {
		fprintf(file, " (");
		for (i = 0; i + 1 < DAG_arity(DAG); i += 2) {
			fprintf(file, "(%s ", DAG_symb_name_rectify(DAG_symb(DAG_arg(DAG, i))));
			DAG_proof_print(file, DAG_arg(DAG, i + 1), substitute_defs);
			fprintf(file, ")");
		}
		fprintf(file, ") ");
		DAG_proof_print(file, DAG_arg_last(DAG), substitute_defs);
	} else
		for (i = 0; i < DAG_arity(DAG); i++) {
			fprintf(file, " ");
			DAG_proof_print(file, DAG_arg(DAG, i), substitute_defs);
		}

	if (sharing && idx > 0) fprintf(file, ") :named @p_%i", idx);

	fprintf(file, ")");
}

void
update_symbol_prefix(Tproof_step proof_step)
{
	unsigned i;
	for (i = 0; i < stack_size(proof_step->DAGs); ++i)
		update_symbol_prefix_DAG(stack_get(proof_step->DAGs, i));
	if (proof_step->subproof_steps)
		for (i = 1; i < stack_size(proof_step->subproof_steps); ++i)
			update_symbol_prefix(stack_get(proof_step->subproof_steps, i));

	/* No need to update symbol_prefix for proof_step->args since those will
     only contain symbols from the other formulas and internal constants */
}

static void
setup_sharing_DAG(TDAG DAG)
{
	assert(DAG != DAG_NULL);

	if (DAG_arity(DAG) == 0) {
		/* Internal constant */
		if (shared[DAG]) {
			/* The marking of constants is only useful when we define constants */
			if (option_proof_define_skolems && !is_marked(DAG))
				mark(DAG);
			unsigned idx = get_idx(DAG);
			setup_sharing_DAG(idx);
		}
		return;
	}

	/* term will get a name during output if it has a positive index */
	unsigned idx = shared[DAG] >> 1;
	if (idx > 0) {
		/* Since the mark will be used to check if a term
       * has been named in the output every term which
       * gets a name must also be marked */
		assert(is_marked(DAG));
		return; /* we already print this term */
	}

	/* We have seen this term before: add index */
	if (is_marked(DAG)) {
		idx = shared[DAG_NULL] + 1;
		shared[DAG_NULL] = idx;
		assert(idx <= (UINT_MAX >> 1));
		/* write the index and keep the mark */
		shared[DAG] = (idx << 1) | 1;
		return;
	} else
		shared[DAG] = 1; /* otherwise: mark as seen */

	for (unsigned i = 0; i < DAG_arity(DAG); i++)
		setup_sharing_DAG(DAG_arg(DAG, i));
}

static void
setup_skolems_DAG(TDAG DAG)
{
	/* Simplified version of setup_sharing_DAG that only iterates once and doesn't
	   track index. */
	assert(DAG != DAG_NULL);

	if (DAG_arity(DAG) == 0) {
		/* Internal constant */
		if (shared[DAG]) {
			/* The marking of constants is only useful when we define constants */
			if (!is_marked(DAG)) mark(DAG);
			unsigned idx = get_idx(DAG);
			setup_skolems_DAG(idx);
		}
		return;
	}
	/* We have seen this term before: skip */
	if (!is_marked(DAG)) {
		mark(DAG);
		for (unsigned i = 0; i < DAG_arity(DAG); i++)
			setup_skolems_DAG(DAG_arg(DAG, i));
	}
}

void
iterate_proof(Tproof_step proof_step, void (*f)(TDAG))
{
	unsigned i, nb_bound_vars;
	for (i = 0; i < stack_size(proof_step->DAGs); ++i)
		(*f)(stack_get(proof_step->DAGs, i));
	if (proof_step->subproof_steps)
		for (i = 1; i < stack_size(proof_step->subproof_steps); ++i)
			iterate_proof(stack_get(proof_step->subproof_steps, i), f);
	if (proof_step->args) {
		if (proof_step->type < ps_type_subproof) {
			for (i = 0; i < stack_size(proof_step->args); ++i)
				(*f)(stack_get(proof_step->args, i));
			return;
		}
		nb_bound_vars = stack_top(proof_step->args);
		for (i = nb_bound_vars; i < stack_size(proof_step->args) - 1u; i += 2)
			(*f)(stack_get(proof_step->args, i + 1));
	}
}

void
write_proof(FILE* file)
{
	unsigned i;
	if (option_proof_prune) {
		if (option_proof_merge) steps_merge();
		steps_prune();
	}

#ifdef DEBUG
	check_proof();
#endif

	/* Prepare rectification prefix */
	for (i = 1; i < stack_size(top_steps); ++i)
		update_symbol_prefix(stack_get(top_steps, i));

	DAG_tmp_reserve();

	/* Used to maintain the highest index for the names of shared terms */
	DAG_tmp_unsigned[DAG_NULL] = 0;

	/* Plan: Do sharing etc. in one go: use marking to see which constants we
	   went into. The non-sharing variant can just skip after the first visit. */

	/* Either setup choices or prepare them for marking if
	   definitions should be introduced */
	for (i = 0; i < stack_size(choices); ++i)
		set_idx_unmarked(stack_get(choices, i).src, stack_get(choices, i).dest);

	/* Collect ite csts */
	for (i = 0; i < stack_size(ite_csts); ++i)
		set_idx_unmarked(stack_get(ite_csts, i).src, stack_get(ite_csts, i).dest);

	/* Prepare sharing index and marks in DAG_tmp_unsigned */
	if (proof_with_sharing) {
		for (i = 1; i < stack_size(top_steps); ++i)
			iterate_proof(stack_get(top_steps, i), setup_sharing_DAG);
	}
	else if (option_proof_define_skolems) {
		for (i = 1; i < stack_size(top_steps); ++i)
			iterate_proof(stack_get(top_steps, i), setup_skolems_DAG);
	}

	/* Print choice definitons that are needed */
	if (option_proof_define_skolems)
		for (i = 0; i < stack_size(choices); ++i)
			if (is_marked(stack_get(choices, i).src)) {
				fprintf(file, "(define-fun ");
				DAG_proof_print(file, stack_get(choices, i).src, false);

				fprintf(file, " () ");
				/* HJ: Probably no free variables thanks to current limited nesting */
#ifdef DEBUG
				Tstack_DAG fvars = get_fvars(DAG_sort(stack_get(choices, i).dest));
				assert(!fvars);
#endif

				DAG_sort_fprint(file, DAG_sort(stack_get(choices, i).dest));
				fprintf(file, " ");
				DAG_proof_print(file, stack_get(choices, i).dest, true);
				fprintf(file, ")\n");
				DAG_tmp_reset_unsigned(stack_get(choices, i).src);
			}

	/* Print steps */
	for (i = 1; i < stack_size(top_steps); ++i)
		print_proof_step(stack_get(top_steps, i), top_steps, i, file, true);

	/* Clean tmp storage */
	for (i = 1; i < stack_size(top_steps); ++i)
		proof_reset_DAG_tmp(stack_get(top_steps, i));
	/* Clean for choice functions */
	for (i = 0; i < stack_size(choices); ++i) {
		DAG_tmp_unsigned[stack_get(choices, i).src] = DAG_NULL;
		DAG_tmp_reset_unsigned(stack_get(choices, i).dest);
	}
	/* Clean for ite csts */
	for (i = 0; i < stack_size(ite_csts); ++i) {
		DAG_tmp_reset_unsigned(stack_get(ite_csts, i).src);
		/* .dest are the ite contants and hence disjoint from src */
		assert(!DAG_tmp_unsigned[stack_get(ite_csts, i).dest]);
	}
	DAG_tmp_reset_unsigned(DAG_NULL);

	DAG_tmp_release();
}
