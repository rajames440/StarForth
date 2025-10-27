/**
   \file proof.c

   \brief proof module.

   This module provides API functions to memorize the proofs done in
   veriT.
*/

#include "proof/proof.h"

#include "bool/bool.h"
#include "proof/proof-id.h"
#include "symbolic/DAG-tmp.h"
#include "symbolic/DAG.h"
#include "symbolic/context-recursion-proof.h"
#include "symbolic/veriT-status.h"
#include "utils/general.h"
#include "utils/options.h"
#include "utils/statistics.h"
#include "veriT-config.h"
#include "veriT-state.h"

#include <string.h>

/* #define DEBUG_PROOF */

bool proof_on = false;

char* option_proof_filename = NULL;
bool proof_no_replacement = false;
bool print_open = false;
bool proof_with_sharing = false;
bool option_proof_prune = false;
bool option_proof_merge = false;
bool option_proof_define_skolems = false;

/**
   \addtogroup arguments_developer
   - --print-proof-file-from-input
   Set proof output file name from input file name by adding .proof */
bool option_proof_file_from_input;
/**
   \addtogroup arguments_developer
   - --proof-stats
   Output proof statistics (incompatible with actual proof output) */
bool proof_stats = false;

/*
  --------------------------------------------------------------
  Documentation
  --------------------------------------------------------------
*/

void
proof_doc(FILE* file)
{
	int i;
	fprintf(
		file,
		"The proof format is a sequence of commands with annotations\n"
		" - each step adds a clause (cl L1 ... Lm)\n"
		" - :rule is the rule name\n"
		" - :premises is a (possibly empty) sequence of step identifiers\n"
		" - :args is an optional sequence of arguments.\n"
		"   The annotations depend on the rule type\n"
		"A full documentation of the proof format is available on the veriT homepage.\n");

	fprintf(
		file,
		"The following rules are currently used\n");

	for (i = 1; i < PS_TYPE_MAX; i++)
		fprintf(
			file, " * %-17s : %s\n", ps_type_desc[i].name, ps_type_desc[i].descr);

	fprintf(
		file,
		"The following deduction types require exactly one premise:\n");
	for (i = 1; i < PS_TYPE_MAX; i++)
		if (ps_type_desc[i].nb_reasons == 1)
			fprintf(file, " * %-17s\n", ps_type_desc[i].name);
	fprintf(
		file,
		"The following deduction types may have any number of premises:\n");
	for (i = 1; i < PS_TYPE_MAX; i++)
		if (ps_type_desc[i].nb_reasons == -1)
			fprintf(file, " * %-17s\n", ps_type_desc[i].name);
	fprintf(
		file,
		"The following deduction types require exactly an integer argument:\n");
	for (i = 1; i < PS_TYPE_MAX; i++)
		if (ps_type_desc[i].nb_params == 1)
			fprintf(file, " * %-17s\n", ps_type_desc[i].name);

	fprintf(file, "\n");
	fprintf(
		file,
		"Please notice that symmetry of equality is silently used.\n\n");

	fprintf(
		file,
		"Skolemization is proof producing.\n"
		"The user is responsible for providing an adequately written formula.\n\n");

	fprintf(
		file,
		"Option --proof-with-sharing enables DAG sharing in the"
		"output proof.\n"
		"The first occurrence of a term or formula is annotated with a standard\n"
		"SMT-LIB :named annotation. Every later occurrence of the term is replaced\n"
		"by the name.\n");
	fprintf(
		file,
		"Option --proof-prune eliminates every unused step in the proof.\n");

	fprintf(
		file,
		"Option --proof-merge merges deductions with the same conclusion.\n\n");

	fprintf(
		file,
		"The following features may be implemented if requested:\n"
		" - Eliminate redundancy in the proof format\n"
		" - Transform n-ary resolution to binary resolution\n"
		" - Make symmetry of equality explicit\n"
		);
}

/*
  --------------------------------------------------------------
  Statistics
  --------------------------------------------------------------
*/

static unsigned
proof_size(void)
{
	unsigned i;
	unsigned size = 0;
	for (i = 1; i < stack_size(top_steps); ++i)
		size += stack_size(stack_get(top_steps, i)->DAGs);
	return size;
}

static unsigned
proof_length(void)
{
	return stack_size(top_steps);
}

static void
proof_stat_compute(void)
{
	unsigned stat_proof_time;
	unsigned stat_proof_length;
	unsigned stat_proof_size;
	unsigned stat_proof_length_pruned;
	unsigned stat_proof_size_pruned;
	unsigned stat_proof_length_merged;
	unsigned stat_proof_size_merged;
	stat_proof_time = stats_timer_new(
		"proof_time", "Time to compute proof stats", "%7.2f", STATS_TIMER_ALL);
	stat_proof_length =
		stats_counter_new("proof_length", "Number of proof steps", "%7d");
	stat_proof_size =
		stats_counter_new("proof_size", "Number of literals in proof", "%7d");
	stat_proof_length_pruned = stats_counter_new(
		"proof_length_pruned", "Number of proof steps (pruned)", "%7d");
	stat_proof_size_pruned = stats_counter_new(
		"proof_size_pruned", "Number of literals in proof (pruned)", "%7d");
	stat_proof_length_merged = stats_counter_new(
		"proof_length_merged", "Number of proof steps (merged)", "%7d");
	stat_proof_size_merged = stats_counter_new(
		"proof_size_merged", "Number of literals in proof (merged)", "%7d");
	stats_timer_start(stat_proof_time);
	stats_counter_set(stat_proof_length, (int)proof_length());
	stats_counter_set(stat_proof_size, (int)proof_size());
	steps_prune();
	stats_counter_set(stat_proof_length_pruned, (int)proof_length());
	stats_counter_set(stat_proof_size_pruned, (int)proof_size());
	steps_merge();
	steps_prune();
	stats_counter_set(stat_proof_length_merged, (int)proof_length());
	stats_counter_set(stat_proof_size_merged, (int)proof_size());
	stats_timer_stop(stat_proof_time);
}

/*
  --------------------------------------------------------------
  Init/Done
  --------------------------------------------------------------
*/

void
proof_init(void)
{
	proof_status = OPEN;

	options_new_string(
		0, "proof", "Sets a file name to output proof (- for stdout)", "FILENAME",
		&option_proof_filename);
	options_new(0, "proof-open", "Print open proof", &print_open);
	options_new(
		0, "proof-no-replacement",
		"Uses congruence and resolution instead of shallow replacements",
		&proof_no_replacement);
	options_new(
		0, "proof-with-sharing", "Use sharing in the output proof",
		&proof_with_sharing);
	options_new(
		0, "proof-prune", "Prune the proof of unused deduction",
		&option_proof_prune);
	options_new(
		0, "proof-merge", "Merge identical clauses in the proof",
		&option_proof_merge);
	options_new(
		0, "proof-define-skolems",
		"Intrudce define-fun commands for skolems in the proof output",
		&option_proof_define_skolems);
	options_new(
		0, "proof-file-from-input", "Use filename+.proof as output file",
		&option_proof_file_from_input);
	/* TODO: make proof stat compatible with proof output */
	options_new(
		0, "proof-stats",
		"Output proof statistics (incompatible with actual proof output)",
		&proof_stats);
	steps_init();
	proof_SAT_init();
	proof_lemma_init();
}

static char* input_filename = NULL;

void
proof_set_input_file(char* filename)
{
	input_filename = strmake(filename);
}

void
proof_done(void)
{
	if (proof_on && proof_status == OPEN && !print_open)
		my_warning("proof_done: status is still open\n");
	proof_lemma_done();
	proof_SAT_done();
	if (proof_on && option_proof_file_from_input && input_filename) {
		free(option_proof_filename);
		MY_MALLOC(option_proof_filename, strlen(input_filename) + 6 + 1);
		strcpy(option_proof_filename, input_filename);
		strcat(option_proof_filename, ".proof");
	}
	if (
		proof_on && proof_stats && !option_proof_filename &&
		!option_proof_file_from_input && proof_status == UNSAT)
		proof_stat_compute();
	else if (proof_on && option_proof_filename) {
		FILE* file = stdout;
		if (strcmp(option_proof_filename, "-"))
			if (!(file = fopen(option_proof_filename, "w"))) {
				my_warning("Unable to open proof file %s\n", option_proof_filename);
				file = stdout;
			}
		if (proof_status == SAT) {
			fprintf(file, "Formula is Satisfiable\n");
			if (print_open) write_proof(file);
		} else if (proof_status == UNSAT || print_open)
			write_proof(file);
		if (strcmp(option_proof_filename, "-")) fclose(file);
	}
	steps_done();
	free(input_filename);
}
