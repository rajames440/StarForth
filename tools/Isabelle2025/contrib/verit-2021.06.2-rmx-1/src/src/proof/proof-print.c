#include "proof/proof-print.h"

#include "limits.h"
#include "proof/proof-checking.h"
#include "proof/proof-output.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-prop.h"
#include "symbolic/DAG-tmp.h"

#define shared DAG_tmp_unsigned
#define is_marked(x) ((shared[x] & 1) != 0)
#define is_var(x) \
	(!DAG_arity(x) && (DAG_symb_type(DAG_symb(x)) & SYMB_VARIABLE))

/**
    \brief Prints a space if i is > 0. */
static inline void
print_list_seperator(FILE* file, unsigned i)
{
	if (i > 0)
		fputc(' ', file);
}

/**
   \brief prints the DAG representing a term, without linefeed.
     Introduces definitions e.g. (! (+ a 1) :named p_12),
     so that p_12 may later be used.
   \param file the output file
   \param DAG the term to output
   \param substitute_defs Replace internal constants (such as skolems)
     with their definition. If `DAG_tmp_unsigned[DAG_NULL]` is non-zero
     sharing is used and it is assumed that `DAG_tmp_unsigned` is set up
     properly. If false `DAG_tmp_unsigned` is not used.
   \remark Uses DAG_tmp and expects proper setup as done by write_proof. */
void
print_term(FILE* file, TDAG DAG, bool substitute_defs)
{
	unsigned i, idx;
	bool sharing = false;
	assert(DAG != DAG_NULL);

	if (DAG_arity(DAG) == 0) {
		/* HB for skolem choice functions or ite csts */
		if (substitute_defs && shared[DAG] > 0)
			print_term(file, shared[DAG] >> 1, substitute_defs);
		else
			fprintf(file, "%s", DAG_symb_name_rectify(DAG_symb(DAG)));
		return;
	}

	idx = shared[DAG] >> 1;
	if (substitute_defs && idx > 0)
		sharing = true;

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
			print_list_seperator(file, i);
			fprintf(file, "(%s ", DAG_symb_name_rectify(DAG_symb(DAG_arg(DAG, i))));
			DAG_sort_fprint(file, DAG_symb_sort(DAG_symb(DAG_arg(DAG, i))));
			fprintf(file, ")");
		}
		fprintf(file, ") ");
		print_term(file, DAG_arg_last(DAG), substitute_defs);
	} else if (DAG_symb(DAG) == LET) {
		fprintf(file, " (");
		for (i = 0; i + 1 < DAG_arity(DAG); i += 2) {
			fprintf(file, "(%s ", DAG_symb_name_rectify(DAG_symb(DAG_arg(DAG, i))));
			print_term(file, DAG_arg(DAG, i + 1), substitute_defs);
			fprintf(file, ")");
		}
		fprintf(file, ") ");
		print_term(file, DAG_arg_last(DAG), substitute_defs);
	} else
		for (i = 0; i < DAG_arity(DAG); i++) {
			fprintf(file, " ");
			print_term(file, DAG_arg(DAG, i), substitute_defs);
		}

	if (sharing && idx > 0)
		fprintf(file, ") :named @p_%i", idx);

	fprintf(file, ")");
}

void
print_simple_step_open(
	FILE* file, Tproof_step proof_step, Tstack_proof_step steps, Tproof id,
	const char* step_prefix, bool substitute_defs, bool with_args)
{
	unsigned i, nb_reasons;
	Tproof_type type = proof_step->type;

	fprintf(file, "(step %st%d (", step_prefix, id);

	/* print resulting term. If no DAGs are present it's the empty clause. */
	fprintf(file, "cl");
	for (i = 0; i < stack_size(proof_step->DAGs); ++i) {
		fprintf(file, " ");
		print_term(file, stack_get(proof_step->DAGs, i), substitute_defs);
	}

	fprintf(file, ") :rule %s", ps_type_desc[type].name);

	/* get number of proof steps as reasons */
	assert(
		ps_type_desc[type].nb_reasons != -1 || ps_type_desc[type].nb_params == 0);

	if (ps_type_desc[type].nb_reasons == -1)
		nb_reasons = (proof_step->reasons ? stack_size(proof_step->reasons) : 0);
	else
		nb_reasons = ps_type_desc[type].nb_reasons;

	assert(
		nb_reasons == 0 ||
		(proof_step->reasons && nb_reasons <= stack_size(proof_step->reasons)));

	/* print premises */
	if (nb_reasons > 0) {
		fprintf(file, " :premises (");
		for (i = 0; i < nb_reasons; ++i) {
			assert(stack_get(proof_step->reasons, i) <= stack_size(steps));

			Tproof_step reason = stack_get(steps, stack_get(proof_step->reasons, i));

			print_list_seperator(file, i);
			/* the premise is a hypothesis and hence might have a name */
			if (reason->type == ps_type_assume) {
				if (stack_size(reason->DAGs) != 1)
					my_error("print_proof_step: internal error\n");

				char** Pname = DAG_prop_get(stack_get(reason->DAGs, 0), DAG_PROP_NAMED);
				if (Pname)
					fprintf(file, "%s", *Pname);
				else
					fprintf(
						file, "%sh%d", step_prefix, stack_get(proof_step->reasons, i));
			} else
				fprintf(file, "%st%d", step_prefix, stack_get(proof_step->reasons, i));
		}
		fprintf(file, ")");
	}

	if (proof_step->args && with_args) {
		assert(!stack_is_empty(proof_step->args));

		fprintf(file, " :args (");
		for (i = 0; i < stack_size(proof_step->args); ++i) {
			print_list_seperator(file, i);
			if (type == ps_type_forall_inst_lemma) {
				/* Actually: proof_step->args should have an even number of args */
				assert(stack_size(proof_step->args) >= 2);
				/* We need to read two args: first variable name, then instantiation term */
				fprintf(file, "(:= ");
				print_term(file, stack_get(proof_step->args, i), substitute_defs);
				fprintf(file, " ");
				print_term(file, stack_get(proof_step->args, i + 1), substitute_defs);
				fprintf(file, ")");
				++i;
			} else
				print_term(file, stack_get(proof_step->args, i), substitute_defs);
		}
		fprintf(file, ")");
	}
	return;
}

/**
  \brief Creates a new term prefix of the form
    `step_prefix`t`id`. Allocates new memory
  \param id the proof id used in the prefix
  \param step_prefix the prefix to prefix the new prefix */
char*
create_term_prefix(Tproof id, const char* step_prefix)
{
	char* prefix_buffer;

	unsigned new_prefix_len = strlen(step_prefix) + ul_str_size(id) + 3;
	MY_MALLOC(prefix_buffer, new_prefix_len * sizeof(char));
	snprintf(
		prefix_buffer, new_prefix_len * sizeof(char), "%st%d.", step_prefix, id);
	return prefix_buffer;
}

/**
   \brief Prints proof step (for outputting the proof and debugging purposes)
           Uses the third iteration of the proof format.
   \param proof_step the proof step
   \param steps the stack of all proof steps
   \param id the proof step id
   \param file the file to write to
   \param step_prefix prefix for the step name
   \param substitute_defs Replace internal constants (such as skolems)
     with their definition. If `DAG_tmp_unsigned[DAG_NULL]` is non-zero
     sharing is used and it is assumed that `DAG_tmp_unsigned` is set up
     properly. If false `DAG_tmp_unsigned` is not used. */
void
print_step(
	Tproof_step proof_step, Tstack_proof_step steps, Tproof id, FILE* file,
	bool substitute_defs, const char* step_prefix)
{
	unsigned i, nb_bound_vars;
	char* prefix_buffer;
	char** Pname;
	Tstack_unsigned
		subproof_hypotheses; /* To store indices of hypoteses used in subproof branch */

	Tproof_type type = proof_step->type;

	/* Special handling of input clauses (level 0 hypotheses) */
	if (type == ps_type_assume) {
		if (stack_size(proof_step->DAGs) != 1)
			my_error("print_proof_step: internal error\n");

		Pname = DAG_prop_get(stack_get(proof_step->DAGs, 0), DAG_PROP_NAMED);
		if (Pname)
			fprintf(file, "(%s %s ", ps_type_desc[type].name, *Pname);
		else
			fprintf(file, "(%s %sh%d ", ps_type_desc[type].name, step_prefix, id);
		print_term(file, stack_get(proof_step->DAGs, 0), substitute_defs);
		fprintf(file, ")\n");
		return;
	}

	/* Regular rules, no subproofs, no context modification */
	if (type < ps_type_subproof)
		print_simple_step_open(
			file, proof_step, steps, id, step_prefix, substitute_defs, true);

	/* Subproofs */
	if (type == ps_type_subproof) {
		assert(!proof_step->args);

		fprintf(file, "(anchor :step %st%d)\n", step_prefix, id);

		stack_INIT(subproof_hypotheses);
		prefix_buffer = create_term_prefix(id, step_prefix);
		/* Print the subproof steps */
		for (i = 1; i < stack_size(proof_step->subproof_steps); ++i) {
			print_step(
				stack_get(proof_step->subproof_steps, i), proof_step->subproof_steps, i,
				file, substitute_defs, prefix_buffer);
			if (stack_get(proof_step->subproof_steps, i)->type == ps_type_assume)
				stack_push(subproof_hypotheses, i);
		}

		print_simple_step_open(
			file, proof_step, steps, id, step_prefix, substitute_defs, true);

		if (!stack_is_empty(subproof_hypotheses)) {
			fprintf(file, " :discharge (");
			while (stack_size(subproof_hypotheses) > 1)
				fprintf(file, "%sh%d ", step_prefix, stack_pop(subproof_hypotheses));
			fprintf(file, "%sh%d)", step_prefix, stack_pop(subproof_hypotheses));
		}

		stack_free(subproof_hypotheses);
		free(prefix_buffer);
	}

	/* Rules with context */
	if (type > ps_type_subproof) {
		fprintf(file, "(anchor :step %st%d ", step_prefix, id);

		/* There is context */
		if (proof_step->args) {
			fprintf(file, ":args (");

			nb_bound_vars = stack_top(proof_step->args);
			if (nb_bound_vars)
				for (i = 0; i < nb_bound_vars; ++i) {
					print_list_seperator(file, i);
					/* variables are not shared since they're shallow. Therefore we just
             print them directly here so we can print them in a custom manner to
             have their types given */
					TDAG var = DAG_symb(stack_get(proof_step->args, i));
					fprintf(file, "(%s ", DAG_symb_name_rectify(var));
					DAG_sort_fprint(file, DAG_symb_sort(var));
					fprintf(file, ")");
				}

			for (i = nb_bound_vars; i < stack_size(proof_step->args) - 1u; i += 2) {
				fprintf(
					file, "%s(:= ", (nb_bound_vars > 0 || i > nb_bound_vars) ? " " : "");
				/* as above, just print the variable and its type */
				TDAG var = DAG_symb(stack_get(proof_step->args, i));
				fprintf(file, "(%s ", DAG_symb_name_rectify(var));
				DAG_sort_fprint(file, DAG_symb_sort(var));
				fprintf(file, ") ");
					print_term(file, stack_get(proof_step->args, i + 1), substitute_defs);
				fprintf(file, ")");
			}
			fprintf(file, ")");
		}
		fprintf(file, ")\n");

		prefix_buffer = create_term_prefix(id, step_prefix);
		/* Steps of the subproof */
		for (i = 1; i < stack_size(proof_step->subproof_steps); ++i)
			print_step(
				stack_get(proof_step->subproof_steps, i), proof_step->subproof_steps, i,
				file, substitute_defs, prefix_buffer);
		free(prefix_buffer);

		print_simple_step_open(
			file, proof_step, steps, id, step_prefix, substitute_defs, false);
	}
	fprintf(file, ")\n");
}

void
print_proof_step(
	Tproof_step proof_step, Tstack_proof_step steps, Tproof id, FILE* file,
	bool substitute_defs)
{
	print_step(proof_step, steps, id, file, substitute_defs, "");
}
