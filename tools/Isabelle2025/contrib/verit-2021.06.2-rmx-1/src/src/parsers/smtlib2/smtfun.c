#include "parsers/smtlib2/smtfun.h"

#include "pre/HOL.h"
#include "pre/simplify.h"
#include "proof/proof.h"
#include "response.h"
#include "symbolic/DAG-arith.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-prop.h"
#include "symbolic/DAG-ptr.h"
#include "symbolic/DAG-symb-DAG.h"
#include "symbolic/DAG.h"
#include "symbolic/dbg-trigger.h"
#include "utils/general.h"
#include "utils/h-util.h"
#include "utils/hashmap.h"
#include "utils/options.h"
#include "utils/stack.h"
#include "utils/statistics.h"
#include "veriT-config.h"
#include "veriT.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define yylineno yy2lineno

extern int yylineno;

#define ptr_of_sort(s) DAG_ptr_of_sort(s)
#define sort_of_ptr(s) DAG_sort_of_ptr(s)

enum Tkw
{
	KW_NONE = 0,
	/* SMT-LIB 2.0 options and info keywords */
	KW_ALL_STATISTICS,
	KW_AUTHORS,
	KW_CATEGORY,
	KW_DIAGNOSTIC_OUTPUT_CHANNEL,
	KW_ERROR_BEHAVIOR,
	KW_EXPAND_DEFINITIONS,
	KW_INTERACTIVE_MODE,
	KW_LICENSE,
	KW_NAME,
	KW_NOTES,
	KW_PRINT_SUCCESS,
	KW_PRODUCE_ASSIGNMENTS,
	KW_PRODUCE_MODELS,
	KW_PRODUCE_PROOFS,
	KW_PRODUCE_UNSAT_CORES,
	KW_RANDOM_SEED,
	KW_REASON_UNKNOWN,
	KW_REGULAR_OUTPUT_CHANNEL,
	KW_SMT_LIB_VERSION,
	KW_SOURCE,
	KW_STATUS,
	KW_VERBOSITY,
	KW_VERSION,
	/* veriT own options */
	KW_PRINT_REPORT,
	/* dummy value */
	KW_SIZE
};

struct kw_name
{
	enum Tkw kw;
	char* name;
};

struct kw_name kw_name_table[] = {
	/* standard SMT2 identifiers */
	{ KW_NONE, "none" },
	{ KW_ALL_STATISTICS, ":all-statistics" },
	{ KW_AUTHORS, ":authors" },
	{ KW_CATEGORY, ":category" },
	{ KW_DIAGNOSTIC_OUTPUT_CHANNEL, ":diagnostic-output-channel" },
	{ KW_ERROR_BEHAVIOR, ":error-behavior" },
	{ KW_EXPAND_DEFINITIONS, ":expand-definitions" },
	{ KW_INTERACTIVE_MODE, ":interactive-mode" },
	{ KW_NAME, ":name" },
	{ KW_LICENSE, ":license" },
	{ KW_NOTES, ":notes" },
	{ KW_PRINT_SUCCESS, ":print-success" },
	{ KW_PRODUCE_ASSIGNMENTS, ":produce-assignments" },
	{ KW_PRODUCE_MODELS, ":produce-models" },
	{ KW_PRODUCE_PROOFS, ":produce-proofs" },
	{ KW_PRODUCE_UNSAT_CORES, ":produce-unsat-cores" },
	{ KW_RANDOM_SEED, ":random-seed" },
	{ KW_REASON_UNKNOWN, ":reason-unknown" },
	{ KW_REGULAR_OUTPUT_CHANNEL, ":regular-output-channel" },
	{ KW_SMT_LIB_VERSION, ":smt-lib-version" },
	{ KW_SOURCE, ":source" },
	{ KW_STATUS, ":status" },
	{ KW_VERBOSITY, ":verbosity" },
	{ KW_VERSION, ":version" },

	/* specific to veriT */
	{ KW_PRINT_REPORT, ":print-report" },

};

Tstatus smt2_status = UNDEF; /**< status as given in the input */

/* ------------------------------------------------------- */
/*                      SMT2 solver state                  */
/* ------------------------------------------------------- */

bool smt2_check_sat_done = false;
bool smt2_logic_set = false;

/* ------------------------------------------------------- */
/*                      SMT2 solver options                */
/* ------------------------------------------------------- */

bool smt2_print_success; /**< required SMT2 option */
char* smt2_diagnostic_output_channel; /**< required SMT2 option */
char* smt2_regular_output_channel; /**< required SMT2 option */
bool smt2_interactive_mode; /**< optional SMT2 option */
bool smt2_produce_proofs = false; /**< optional SMT2 option */
bool smt2_produce_unsat_cores = false; /**< optional SMT2 option */

#if STATS_LEVEL > 1
static unsigned stat_nb_nodes, stat_nb_nodes_tree, stat_nb_atoms;
#endif
static unsigned stat_result;

/*
  --------------------------------------------------------------
  help functions
  --------------------------------------------------------------
*/

static enum Tkw
defstring(char* str)
{
	int i;
	for (i = 1; i < KW_SIZE; ++i)
		if (!strcmp(kw_name_table[i].name, str)) return kw_name_table[i].kw;
	return KW_NONE;
}

static char*
smt2_status_str(int status)
{
	switch (status) {
		case UNDEF: return "unknown";
		case SAT: return "sat";
		case UNSAT: return "unsat";
		default: veriT_error("unknown status %i", status); return "";
	}
}

/* PF returns 1 if a decimal, 0 if integer, error if none of both */
static int
check_decimal(char* str)
{
	if (!isdigit(*str)) veriT_error("ill-formed on line %d", yylineno);
	++str;
	while (isdigit(*str)) ++str;
	if (*str == 0) return 0;
	if (*str != '.') veriT_error("ill-formed decimal on line %d", yylineno);
	++str;
	if (!isdigit(*str)) veriT_error("ill-formed decimal on line %d", yylineno);
	++str;
	while (isdigit(*str)) ++str;
	if (*str == 0) return 1;
	veriT_error("ill-formed decimal on line %d", yylineno);
	return 0;
}

static int
check_binary(char* str)
{
	while (*str == '0' || *str == '1') str++;
	if (*str == 0) return 1;
	veriT_error("ill-formed binary on line %d", yylineno);
	return 0;
}

static int
check_hex(char* str)
{
	while (isxdigit(*str)) str++;
	if (*str == 0) return 1;
	veriT_error("ill-formed hexadecimal on line %d", yylineno);
	return 0;
}

static int
check_str(char* str)
{
	while (*str != '\0' && *str != '"')
		if (*str == '\\')
			str += 2;
		else
			str++;
	if (*str == '"' && *(str + 1) == '\0') return 1;
	veriT_error("ill-formed string on line %d", yylineno);
	return 0;
}

static void
check_protected_symbol(char* str)
{
	if (!strncmp(str, "veriT__", 7) || !strncmp(str, "?veriT__", 8))
		veriT_error("reserved symbol used on line %d", yylineno);
}

/*
  --------------------------------------------------------------
  symbol finding functions
  --------------------------------------------------------------
*/

/* PF this section is necessary because quantifiers and let may
   define atomic terms */

Thashmap hashmap_str = NULL;

static unsigned
hash_function(char* str)
{
	unsigned hash = hash_one_at_a_time(str);
	return hash_one_at_a_time_inc_end(hash);
}

static Tsigned
hash_str_equal(char* str1, char* str2)
{
	return !strcmp(str1, str2);
}

static void
hash_key_free(char* str)
{
	/* free(str); */
#ifdef PEDANTIC
	printf("%p\n", str);
#endif
}

static void
hash_value_free(Tstack_symb stack_symb)
{
	if (stack_symb) stack_free(stack_symb);
}

/**
    add a link for str to symb in LIFO style
    \param str a string for the symbol
    \param symb the symbol to link to str
*/
static void
declare_str(char* str, Tsymb symb)
{
	Tstack_symb stack_symb;
	Tstack_symb* Pstack_symb = (Tstack_symb*)hashmap_lookup(hashmap_str, str);
	if (Pstack_symb) {
		assert(*Pstack_symb);
		stack_push(*Pstack_symb, symb);
		return;
	}
	stack_INIT_s(stack_symb, 1);
	stack_push(stack_symb, symb);
	hashmap_insert(hashmap_str, str, stack_symb);
}

/**
    remove up link for str (should be to symb)
    \param str a string for the symbol
    \param symb the symbol to unlink to str */
static void
undeclare_str(char* str, Tsymb symb)
{
	Tstack_symb* Pstack_symb = (Tstack_symb*)hashmap_lookup(hashmap_str, str);
	assert(
		Pstack_symb && *Pstack_symb && stack_size(*Pstack_symb) &&
		stack_top(*Pstack_symb) == symb);
	stack_dec(*Pstack_symb);
	if (!stack_size(*Pstack_symb)) hashmap_remove(hashmap_str, str);
}

/**
    \brief find symbol linked to str
    \param str a string for the symbol
    \return the symbol linked to str */
static Tsymb
lookup_str(char* str)
{
	Tstack_symb* Pstack_symb = (Tstack_symb*)hashmap_lookup(hashmap_str, str);
	if (!Pstack_symb) return DAG_SYMB_NULL;
	assert(*Pstack_symb && stack_size(*Pstack_symb));
	return stack_top(*Pstack_symb);
}

/*
  --------------------------------------------------------------
  Commands
  --------------------------------------------------------------
*/

/**
   \brief macro responsible for emitting an error message and exiting the
   program if the command (check-sat) has been previously issued.
*/

#define VERIFY_CHECK_SAT \
	if (smt2_check_sat_done) veriT_error("unsupported");

#define VERIFY_LOGIC_NOT_SET \
	if (!smt2_logic_set) veriT_error("set-logic not issued");

#define VERIFY_LOGIC_SET \
	if (smt2_logic_set) veriT_error("set-logic already issued");

#define PRINT_SUCCESS \
	if (smt2_print_success) veriT_out("success");
#define PRINT_UNSUPPORTED veriT_out("unsupported");

void
smt2_set_logic(char* symbol)
{
	VERIFY_LOGIC_SET;
	VERIFY_CHECK_SAT;
	veriT_logic(symbol);
	free(symbol);
	smt2_logic_set = true;
	PRINT_SUCCESS;
}

/* PF (declare-sort Array 2) */
void
smt2_declare_sort(char* symbol, char* numeral)
{
	unsigned arity;
	long int tmp;
	VERIFY_LOGIC_NOT_SET;
	VERIFY_CHECK_SAT;
	check_protected_symbol(symbol);
	tmp = strtol(numeral, NULL, 10);
	if (tmp > UINT_MAX) veriT_error("smt2_declare_sort: arity too large\n");
	arity = (unsigned)tmp;
	if (DAG_sort_lookup(symbol))
		veriT_error("line %d: redefining sort %s\n", yylineno, symbol);
	if (arity > 0)
		DAG_sort_new_param(symbol, arity);
	else
		DAG_sort_new(symbol, 0, NULL);
	free(symbol);
	free(numeral);
	PRINT_SUCCESS;
}

/* PF (define-sort Integer_Array_of_List (Integer List) Array) */
void
smt2_define_sort(char* symbol, Tlist symbol_list, Tsort sort)
{
	unsigned i, arity;
	Tsort* sub;
	VERIFY_LOGIC_NOT_SET;
	VERIFY_CHECK_SAT;
	check_protected_symbol(symbol);
	arity = list_length(symbol_list);
	MY_MALLOC(sub, arity * sizeof(Tsort));
	for (i = 0; i < arity; ++i, symbol_list = list_cdr(symbol_list)) {
		char* symbol = (char*)list_car(symbol_list);
		Tsort sort2 = DAG_sort_lookup(symbol);
		if (!sort2) veriT_error("unknown sort %s on line %d.", symbol, yylineno);
		sub[i] = sort2;
	}
	DAG_sort_new_inst(symbol, sort, arity, sub);
	list_apply(symbol_list, free);
	list_free(&symbol_list);
	free(symbol);
	PRINT_SUCCESS;
}

void
smt2_declare_poly_fun(
	char* symbol, Tlist sort_list1, Tlist sort_list2, Tsort sort)
{
	unsigned i, arity, type;
	Tsort* sub;
	VERIFY_LOGIC_NOT_SET;
	VERIFY_CHECK_SAT;
	type = (sort == SORT_BOOLEAN) ? SYMB_PREDICATE : 0;
	check_protected_symbol(symbol);
	if (!sort_list2) {
		DAG_symb_new(symbol, type, sort);
		free(symbol);
		return;
	}
	arity = list_length(sort_list2);
	MY_MALLOC(sub, (arity + 1) * sizeof(Tsort));
	for (i = 0; i < arity; i++, sort_list2 = list_cdr(sort_list2))
		sub[i] = sort_of_ptr(list_car(sort_list2));
	sub[i] = sort;
	DAG_symb_new(symbol, type, DAG_sort_new(NULL, arity + 1, sub));
	free(symbol);
	list_free(&sort_list1);
	list_free(&sort_list2);
	PRINT_SUCCESS;
}

void
smt2_declare_fun(char* symbol, Tlist sort_list, Tsort sort)
{
	Tsymb_type type;
	VERIFY_LOGIC_NOT_SET;
	VERIFY_CHECK_SAT;
	type = (sort == SORT_BOOLEAN) ? SYMB_PREDICATE : 0;
	check_protected_symbol(symbol);
	if (!sort_list) {
		DAG_symb_new(symbol, type, sort);
		free(symbol);
	} else {
		unsigned i, arity;
		Tsort* sub;

		arity = list_length(sort_list);
		MY_MALLOC(sub, (arity + 1) * sizeof(Tsort));
		for (i = 0; i < arity; i++, sort_list = list_cdr(sort_list))
			sub[i] = sort_of_ptr(list_car(sort_list));
		sub[i] = sort;
		DAG_symb_new(symbol, type, DAG_sort_new(NULL, arity + 1, sub));
		free(symbol);
		list_free(&sort_list);
	}
	PRINT_SUCCESS;
}

void
smt2_define_fun(char* symbol, Tstack_symb stack_var, Tsort sort, TDAG term)
{
	if (proof_on)
		veriT_error("smt2_define_fun: not supported in proof mode.");
	unsigned i;
	Tsymb symb;
	TDAG DAG;
	Tstack_DAG stack_arg;
	VERIFY_LOGIC_NOT_SET;
	VERIFY_CHECK_SAT;
	stack_INIT(stack_arg);
	check_protected_symbol(symbol);
	for (i = 0; i < stack_var->size; i++)
		stack_push(stack_arg, DAG_new_nullary(stack_var->data[i]));
	stack_push(stack_arg, term);
	DAG = DAG_dup(DAG_new_stack(LAMBDA, stack_arg));
	if (DAG_sort(term) != sort) veriT_error("smt2_define_fun: sort mismatch");
	symb = DAG_symb_new(symbol, SYMB_MACRO, DAG_sort(DAG));
	DAG_symb_macro(symb, DAG);
	stack_free(stack_arg);
	DAG_free(term);
	free(symbol);
	PRINT_SUCCESS;
}

void
smt2_define_poly_fun(
	char* symbol, /*T_SORT_LIST sort_list,*/
	Tstack_symb stack_var, /*T_SORT Tsort,*/ T_TERM term)
{
	if (proof_on)
		veriT_error("smt2_define_fun: not supported in proof mode.");
	unsigned i;
	Tsymb symb;
	TDAG DAG;
	Tstack_DAG stack_arg;
	VERIFY_LOGIC_NOT_SET;
	VERIFY_CHECK_SAT;
	stack_INIT(stack_arg);
	check_protected_symbol(symbol);
	for (i = 0; i < stack_var->size; i++)
		stack_push(stack_arg, DAG_new_nullary(stack_var->data[i]));
	stack_push(stack_arg, term);
	DAG = DAG_dup(DAG_new_stack(LAMBDA, stack_arg));
	symb = DAG_symb_new(symbol, SYMB_MACRO, DAG_sort(DAG));
	DAG_symb_macro(symb, DAG);
	stack_free(stack_arg);
	DAG_free(term);
	free(symbol);
	PRINT_SUCCESS;
}

void
smt2_define_fun_short(char* symbol, TDAG term)
{
	if (proof_on)
		veriT_error("smt2_define_fun: not supported in proof mode.");
	Tsymb symb;
	VERIFY_LOGIC_NOT_SET;
	VERIFY_CHECK_SAT;
	check_protected_symbol(symbol);
	symb = DAG_symb_new(symbol, SYMB_MACRO, DAG_sort(term));
	DAG_symb_macro(symb, term);
	free(symbol);
	PRINT_SUCCESS;
}

void
smt2_push(char* numeral)
{
	unsigned n;
	char* tmp;
	long l;

	VERIFY_LOGIC_NOT_SET;
	veriT_error("unsupported");

	l = strtol(numeral, &tmp, 10);
	if (l < 0 || l > UINT_MAX) veriT_error("push: range");
	n = (unsigned)l;
	if (*tmp != '\0') veriT_error("smt2_push: internal error");
	if ((errno == ERANGE && n == UINT_MAX) || (errno != 0 && n == 0))
		veriT_error("push: range");
	veriT_push(n);
	free(numeral);
	PRINT_SUCCESS;
}

void
smt2_pop(char* numeral)
{
	unsigned n;
	char* tmp;
	long l;

	VERIFY_LOGIC_NOT_SET;
	veriT_error("unsupported");

	l = strtol(numeral, &tmp, 10);
	if (l < 0 || l > UINT_MAX) veriT_error("push: range");
	n = (unsigned)l;
	if (*tmp != '\0') veriT_error("smt2_push: internal error");
	if ((errno == ERANGE && n == UINT_MAX) || (errno != 0 && n == 0))
		veriT_error("push: range");
	veriT_pop(n);
	free(numeral);
}

void
smt2_assert(TDAG term)
{
	VERIFY_LOGIC_NOT_SET;
	VERIFY_CHECK_SAT;
	if (DAG_sort(term) != SORT_BOOLEAN)
		veriT_error("asserting a non Boolean term on line %d", yylineno);
	veriT_assert(term);
#if STATS_LEVEL > 1
	stats_counter_add(stat_nb_nodes, DAG_count_nodes(term));
	stats_counter_add(stat_nb_nodes_tree, DAG_count_nodes_tree(term));
	stats_counter_add(stat_nb_atoms, DAG_count_atoms(term));
#endif
	DAG_free(term);
	PRINT_SUCCESS;
}

void
smt2_check_sat(void)
{
	Tstatus status;
	VERIFY_LOGIC_NOT_SET;
	VERIFY_CHECK_SAT;
	status = veriT_check_sat();
	#if defined(HAVE_POSIX_TIMER) || defined(HAVE_WINDOWS_H)
	veriT_disarm_timer();
	#endif
	smt2_check_sat_done = true;
	switch (status) {
		case UNSAT:
			veriT_out("unsat");
			stats_counter_set(stat_result, 0);
			break;
		case SAT:
			veriT_out("sat");
			stats_counter_set(stat_result, 1);
			break;
		case UNDEF:
			veriT_out("unknown");
			stats_counter_set(stat_result, -1);
			/* TODO: here include completion test */
			break;
		default: veriT_error("strange returned status");
	}
	PRINT_SUCCESS;
}

void
smt2_get_assertions(void)
{
	unsigned i;
	if (!smt2_interactive_mode)
		veriT_error("get-assertions only available in interactive mode");
	/* TODO: */
	veriT_out("(");
	for (i = 0; i < stack_size(veriT_assertion_set_stack); ++i) {
		Tassertion_set assertion_set = stack_get(veriT_assertion_set_stack, i);
		unsigned j;
		veriT_out(";; assertions at level %i", i);
		for (j = 0; j < stack_size(assertion_set.DAG_stack); ++j) {
			if (j > 0) veriT_out("");
			DAG_fprint(veriT_out_file, stack_get(assertion_set.DAG_stack, j));
		}
	}
	veriT_out("\n)");
	PRINT_SUCCESS;
}

void
smt2_get_proof(void)
{
	if (!smt2_produce_proofs)
		veriT_error("option :produce-proofs has not been set.");
	if (veriT_status() != UNSAT) veriT_error("status is not unsat.");
	veriT_get_proof();
	PRINT_SUCCESS;
}

void
smt2_get_unsat_core(void)
{
	if (!smt2_produce_unsat_cores)
		veriT_error("option :produce-unsat-core has not been set.");
	if (veriT_status() != UNSAT) veriT_error("status is not unsat.");
	veriT_get_unsat_core();
	PRINT_SUCCESS;
}

void
smt2_get_model(void)
{
	veriT_model();
	PRINT_SUCCESS;
}

void
smt2_echo(char* str)
{
	veriT_out("%s", str);
	free(str);
}

void
smt2_get_value(Tlist term_list)
{
	/* TODO: */
	PRINT_UNSUPPORTED;
	list_free(&term_list);
}

void
smt2_get_assignment(void)
{
	/* TODO: */
	PRINT_UNSUPPORTED;
}

void
smt2_get_info(char* keyword)
{
	switch (defstring(keyword)) {
		case KW_ERROR_BEHAVIOR:
			veriT_out("(%s %s)", keyword, "immediate-exit");
			PRINT_SUCCESS;
			break;
		case KW_NAME:
			veriT_out("(%s \"%s\")", keyword, PACKAGE_NAME);
			PRINT_SUCCESS;
			break;
		case KW_VERSION:
			/* TODO: make-distrib script should fill this automatically */
			veriT_out("(%s \"%s\")", keyword, PACKAGE_VERSION);
			PRINT_SUCCESS;
			break;
		case KW_AUTHORS:
			veriT_out(
				"(%s \"%s\")", keyword, "main developers: P. Fontaine, D. Deharbe");
			PRINT_SUCCESS;
			break;
		case KW_STATUS:
			veriT_out("(%s \"%s\")", keyword, smt2_status_str(veriT_status()));
			PRINT_SUCCESS;
			break;
		default: PRINT_UNSUPPORTED; break;
	}
	free(keyword);
}

void
smt2_get_option(char* keyword)
{
	switch (defstring(keyword)) {
		case KW_PRINT_SUCCESS:
			veriT_out("%s", smt2_print_success ? "true" : "false");
			PRINT_SUCCESS;
			break;
		case KW_DIAGNOSTIC_OUTPUT_CHANNEL:
			veriT_out("\"%s\"", smt2_diagnostic_output_channel);
			PRINT_SUCCESS;
			break;
		case KW_REGULAR_OUTPUT_CHANNEL:
			veriT_out("\"%s\"", smt2_regular_output_channel);
			PRINT_SUCCESS;
			break;
		case KW_INTERACTIVE_MODE:
			veriT_out(smt2_interactive_mode ? "true" : "false");
			PRINT_SUCCESS;
			break;
		default: PRINT_UNSUPPORTED; break;
	}
	free(keyword);
}

static bool smt2_exited = false;

void
smt2_exit(void)
{
	Tstatus status = veriT_status();
	if (smt2_status != UNDEF && status != UNDEF && smt2_status != status)
		veriT_error("soundness error");
	/*  veriT_exit(0); */
	smt2_exited = true;
	PRINT_SUCCESS;
}

void
smt2_set_option(char* keyword)
{
	smt2_set_option_bool(keyword, true);
}

void
smt2_set_option_num(char* keyword, char* str)
{
	unsigned num;
	sscanf(str, "%u", &num);
	free(str);
	switch (defstring(keyword)) {
		default: PRINT_UNSUPPORTED;
	}
	free(keyword);
}

void
smt2_set_option_str(char* keyword, char* str)
{
	char* str_no_quotes;
	size_t len = strlen(str) - 2;
	MY_MALLOC(str_no_quotes, sizeof(char) * (len + 1));
	strncpy(str_no_quotes, str + 1, len);
	str_no_quotes[len] = (char)0;
	free(str);
	switch (defstring(keyword)) {
		case KW_DIAGNOSTIC_OUTPUT_CHANNEL:
			if (strcmp(str_no_quotes, smt2_diagnostic_output_channel) != 0) {
				free(smt2_diagnostic_output_channel);
				smt2_diagnostic_output_channel = str_no_quotes;
				veriT_set_err_file(str_no_quotes);
			}
			PRINT_SUCCESS;
			break;
		case KW_REGULAR_OUTPUT_CHANNEL:
			if (strcmp(str_no_quotes, smt2_regular_output_channel) != 0) {
				free(smt2_regular_output_channel);
				smt2_regular_output_channel = str_no_quotes;
				veriT_set_out_file(str_no_quotes);
			}
			PRINT_SUCCESS;
			break;
		default: free(str_no_quotes); PRINT_UNSUPPORTED;
	}
	free(keyword);
}

void
smt2_set_option_bool(char* keyword, const bool value)
{
	switch (defstring(keyword)) {
		case KW_INTERACTIVE_MODE:
			VERIFY_LOGIC_SET;
			smt2_interactive_mode = true;
			PRINT_SUCCESS;
			break;
		case KW_PRODUCE_PROOFS:
			VERIFY_LOGIC_SET;
			smt2_produce_proofs = value;
			if (value) {
				proof_on = true;
				option_proof_merge = true;
				option_proof_prune = true;
			} else {
				proof_on = false;
			}
			PRINT_SUCCESS;
			break;
		case KW_PRODUCE_UNSAT_CORES:
			VERIFY_LOGIC_SET;
			smt2_produce_unsat_cores = value;
			if (value) {
				proof_on = true;
				option_proof_merge = true;
				option_proof_prune = true;
			} else {
				proof_on = false;
			}
			PRINT_SUCCESS;
			break;
		case KW_PRODUCE_MODELS:
		case KW_PRODUCE_ASSIGNMENTS:
			VERIFY_LOGIC_SET;
			PRINT_UNSUPPORTED;
			break;
		case KW_PRINT_SUCCESS:
			smt2_print_success = value;
			PRINT_SUCCESS;
			break;
		case KW_PRINT_REPORT:
			veriT_print_report = value;
			PRINT_SUCCESS;
			break;
		default: PRINT_UNSUPPORTED; break;
	}
	/* TODO: */
	free(keyword);
}

void
smt2_set_info(char* keyword)
{
	free(keyword);
	PRINT_UNSUPPORTED;
}

void
smt2_set_info_str(char* keyword, char* str)
{
	switch (defstring(keyword)) {
		case KW_CATEGORY: PRINT_SUCCESS; break;
		case KW_NOTES: PRINT_SUCCESS; break;
		case KW_STATUS:
			if (!strcmp(str, "sat"))
				smt2_status = SAT;
			else if (!strcmp(str, "unsat"))
				smt2_status = UNSAT;
			else if (!strcmp(str, "unknown"))
				smt2_status = UNDEF;
			PRINT_SUCCESS;
			break;
		case KW_SMT_LIB_VERSION:
			if (strcmp(str, "2.0") && strcmp(str, "2.5") && strcmp(str, "2.6"))
				veriT_err("unknown SMT-LIB version");
			PRINT_SUCCESS;
			break;
		case KW_SOURCE: PRINT_SUCCESS; break;
		case KW_LICENSE: PRINT_SUCCESS; break;
		default: PRINT_UNSUPPORTED;
	}
	free(keyword);
	free(str);
}

/*
  --------------------------------------------------------------
  TDAG
  --------------------------------------------------------------
*/

TDAG
smt2_term_const(char* str)
{
	TDAG DAG = DAG_NULL;
	/* PF string may be:
     NUMERAL
     DECIMAL
     HEXADECIMAL
     BINARY
     STRING
  */
	if (str[0] == 0)
		veriT_error("unexpected constant on line %d", yylineno);
	else if (isdigit(str[0])) {
		if (check_decimal(str))
			DAG = DAG_new_decimal_str(str);
		else
			DAG = DAG_new_integer_str(str);
	} else if (str[1] == 0)
		veriT_error("unexpected constant on line %d", yylineno);
	else if (str[0] == '#') {
		if (str[1] == 'b') {
			check_binary(str + 2);
			DAG = DAG_new_binary_str(str);
		} else if (str[1] == 'x') {
			check_hex(str + 2);
			DAG = DAG_new_hex_str(str);
		} else
			veriT_error("unexpected constant on line %d", yylineno);
	} else if (str[0] == '"') {
		check_str(str + 2);
		DAG = DAG_new_str(str);
	} else
		veriT_error("unexpected constant on line %d", yylineno);
	free(str);
	return DAG_dup(DAG);
}

TDAG
smt2_term(char* str)
{
	Tsymb symb = lookup_str(str);
	if (!symb) symb = DAG_symb_lookup(str, 0, NULL, DAG_SORT_NULL);
	if (!symb)
		veriT_error("smt2_term: unresolved symbol %s on line %d", str, yylineno);
	free(str);
	if (DAG_symb_type(symb) & SYMB_MACRO) {
		TDAG DAG = DAG_symb_DAG[symb];
		if (DAG_symb(DAG) == LAMBDA && DAG_arity(DAG) == 1)
			return DAG_dup(DAG_arg0(DAG));
	}
	return DAG_dup(DAG_new_nullary(symb));
}

TDAG
smt2_term_app(char* str, Tlist term_list)
{
	unsigned i, arity = list_length(term_list);
	Tsymb symb = DAG_SYMB_NULL;
	TDAG *sub, DAG;
	MY_MALLOC(sub, arity * sizeof(TDAG));
	for (i = 0; i < arity; i++, term_list = list_cdr(term_list))
		sub[i] = DAG_of_ptr(list_car(term_list));
	assert(!(strcmp(str, "=") == 0) || arity >= 2);
	if (strcmp(str, "=") == 0) {
		if (DAG_sort(sub[0]) == SORT_BOOLEAN)
			symb = CONNECTOR_EQUIV;
		else
			symb = PREDICATE_EQ;
	} else if (strcmp(str, "ite") == 0) {
		if (arity != 3)
			veriT_error(
				"smt2_term_app: wrong arity for ITE symbol on line %d.", yylineno);
		if (
			DAG_sort(sub[0]) != SORT_BOOLEAN || DAG_sort(sub[1]) != DAG_sort(sub[2]))
			veriT_error(
				"smt2_term_app: typing error in ite application on line %d.", yylineno);
		else if (DAG_sort(sub[1]) == SORT_BOOLEAN)
			symb = CONNECTOR_ITE;
		else
			symb = FUNCTION_ITE;
	} else {
		Tsort* Psort;
		MY_MALLOC(Psort, arity * sizeof(Tsort));
		for (i = 0; i < arity; i++) Psort[i] = DAG_sort(sub[i]);
		symb = DAG_symb_lookup(str, arity, Psort, DAG_SORT_NULL);
		free(Psort);
	}
	if (!symb)
		veriT_error(
			"smt2_term_app: unresolved symbol %s on line %d.", str, yylineno);
	if (DAG_symb_type(symb) & SYMB_MACRO) {
		TDAG macro_DAG = DAG_symb_DAG[symb];
		if (DAG_symb(macro_DAG) == LAMBDA) {
			TDAG *app_sub;
			MY_MALLOC(app_sub, (arity + 1) * sizeof(TDAG));
			app_sub[0] = macro_DAG;
			for (i = 0; i < arity; i++)
				app_sub[i + 1] = sub[i];
			DAG = DAG_dup(DAG_new(APPLY_LAMBDA, arity + 1, app_sub));
			for (i = 0; i < arity; i++)
				DAG_free(sub[i]);
			free(str);
			list_free(&term_list);
			return DAG;
		}
	}
	DAG = DAG_dup(DAG_new(symb, arity, sub));
	for (i = 0; i < arity; i++) DAG_free(DAG_arg(DAG, i));
	free(str);
	list_free(&term_list);
	return DAG;
}

TDAG
smt2_lambda_app(TDAG lambda, Tlist term_list)
{
	unsigned i, arity = list_length(term_list);
	TDAG *sub, DAG;
	MY_MALLOC(sub, (arity + 1) * sizeof(TDAG));
	sub[0] = lambda;
	for (i = 0; i < arity; i++, term_list = list_cdr(term_list))
		sub[i + 1] = DAG_of_ptr(list_car(term_list));
	DAG = DAG_dup(DAG_new(APPLY_LAMBDA, arity + 1, sub));
	for (i = 0; i < DAG_arity(DAG); i++) DAG_free(DAG_arg(DAG, i));
	list_free(&term_list);
	return DAG;
}

TDAG
smt2_iterm(char* str, Tlist numeral_list)
{
	veriT_error("unimplemented indexed symbol on line %d", yylineno);
	list_free(&numeral_list);
	free(str);
	/* should certainly free elements in numeral_list */
	return DAG_NULL;
}

TDAG
smt2_iterm_app(char* str, Tlist term_list, Tlist numeral_list)
{
	veriT_error("unimplemented indexed symbol on line %d", yylineno);
	/*
  for (i = 0; i < arity; i++)
    DAG_free(DAG_arg(DAG, i));
  */
	list_free(&term_list);
	list_free(&numeral_list);
	free(str);
	/* should certainly free elements in numeral_list */
	return DAG_NULL;
}

static Tsymb
qualified_symb(char* str, Tsort sort)
{
	Tsymb symb;
	symb = DAG_symb_lookup_sort(str, sort);
	if (symb == DAG_SYMB_NULL)
		veriT_error("unresolved qualified symbol %s on line %d.", str, yylineno);
	free(str);
	return symb;
}

TDAG
smt2_term_s(char* str, Tsort sort)
{
	Tsymb symb;
	symb = qualified_symb(str, sort);
	if (symb == DAG_SYMB_NULL) return DAG_NULL;
	return DAG_dup(DAG_new_nullary(symb));
}

TDAG
smt2_term_app_s(char* str, Tlist term_list, Tsort sort)
{
	unsigned i, arity;
	Tsymb symb;
	TDAG *sub, DAG;
	symb = qualified_symb(str, sort);
	if (symb == DAG_SYMB_NULL) return DAG_NULL;
	arity = list_length(term_list);
	MY_MALLOC(sub, arity * sizeof(TDAG));
	for (i = 0; i < arity; i++, term_list = list_cdr(term_list))
		sub[i] = DAG_of_ptr(list_car(term_list));
	DAG = DAG_dup(DAG_new(symb, arity, sub));
	for (i = 0; i < arity; i++) DAG_free(DAG_arg(DAG, i));
	list_free(&term_list);
	return DAG;
}

TDAG
smt2_iterm_s(char* str, Tlist numeral_list, Tsort sort)
{
	veriT_error("unimplemented indexed identifier on line %d", yylineno);
	list_free(&numeral_list);
	free(str);
#ifdef PEDANTIC
	printf("%d\n", sort);
#endif
	/* should certainly free elements in numeral_list */
	return DAG_NULL;
}

TDAG
smt2_iterm_app_s(char* str, Tlist term_list, Tlist numeral_list, Tsort sort)
{
	veriT_error("unimplemented indexed identifier on line %d", yylineno);
	/*
  for (i = 0; i < arity; i++)
    DAG_free(DAG_arg(DAG, i));
  */
	list_free(&term_list);
	list_free(&numeral_list);
	/* should certainly free elements in numeral_list */
	free(str);
#ifdef PEDANTIC
	printf("%d\n", sort);
#endif
	return DAG_NULL;
}

TDAG
smt2_let(Tlist bind_list, TDAG term)
{
	TDAG DAG;
	Tstack_DAG stack_DAG = NULL;
	stack_INIT(stack_DAG);
	if (!bind_list) veriT_error("ill-formed let on line %d", yylineno);
	LIST_LOOP_BEGIN(bind_list, Tbind, bind);
	stack_push(stack_DAG, DAG_dup(DAG_new_nullary(bind->symb)));
	stack_push(stack_DAG, bind->DAG);
	LIST_LOOP_END(bind_list);
	stack_push(stack_DAG, term);
	DAG = DAG_dup(DAG_new_stack(LET, stack_DAG));
	stack_apply(stack_DAG, DAG_free);
	stack_free(stack_DAG);
	/* PF bind list will be freed in smt2_undeclare_bind_list */
	return DAG;
}

static TDAG
smt2_term_binder(
	Tsymb binder, Tstack_symb stack_var, TDAG term, const char* str)
{
	unsigned i;
	TDAG DAG;
	Tstack_DAG stack_arg;
	stack_INIT(stack_arg);
	if (!stack_var || !stack_var->size)
		veriT_error("ill-formed %s on line %d", str, yylineno);
	for (i = 0; i < stack_var->size; i++)
		stack_push(stack_arg, DAG_new_nullary(stack_var->data[i]));
	stack_push(stack_arg, term);
	DAG = DAG_dup(DAG_new_stack(binder, stack_arg));
	stack_free(stack_arg);
	DAG_free(term);
	return DAG;
}

TDAG
smt2_term_forall(Tstack_symb stack_var, TDAG term)
{
	return smt2_term_binder(
		QUANTIFIER_FORALL, stack_var, term, "quantified formula");
}

TDAG
smt2_term_exists(Tstack_symb stack_var, TDAG term)
{
	return smt2_term_binder(
		QUANTIFIER_EXISTS, stack_var, term, "quantified formula");
}

TDAG
smt2_term_lambda(Tstack_symb stack_var, TDAG term)
{
	return smt2_term_binder(LAMBDA, stack_var, term, "lambda term");
}

TDAG
smt2_term_attr(TDAG term, char* keyword)
{
	veriT_error("unimplemented attributed term on line %d", yylineno);
	free(keyword);
	return term;
}

TDAG
smt2_term_attr_str(TDAG term, char* keyword, char* str)
{
	if (keyword && !strcmp(keyword, ":named")) {
		char* name = strmake(str);
		DAG_prop_set(term, DAG_PROP_NAMED, &name);
	} else
		veriT_error("unimplemented attributed term on line %d", yylineno);
	free(keyword);
	free(str);
	return term;
}

#if 0
TDAG
smt2_term_attr_value(TDAG term, char * keyword, void * value)
{
  veriT_error("unimplemented attributed term on line %d", yylineno);
  free(keyword);
#ifdef PEDANTIC
  printf("%p\n", value);
#endif
  return term;
}
#endif

TDAG
smt2_term_attr_named(TDAG term, char* str)
{
	DAG_prop_set(term, DAG_PROP_NAMED, &str);
	return term;
}

/*
  DAG_PROP_TRIGGER is a list of lists of DAGs.
  Each time a term_list has been parsed as the value of
  the :pattern keyword, it is added to the property
  value.
*/
TDAG
smt2_term_attr_pattern(TDAG formula, Tlist term_list)
{
	Tstack_DAGstack* Pannot = DAG_prop_get(formula, DAG_PROP_TRIGGER);
	Tstack_DAG trigger;
	stack_INIT_s(trigger, list_length(term_list));
	/* PF terms in term_list already have been DAG_dupped */
	while (term_list) {
		stack_push(trigger, DAG_of_ptr(list_car(term_list)));
		term_list = list_remove(term_list);
	}
	if (!Pannot) {
		Tstack_DAGstack annot;
		stack_INIT(annot);
		stack_push(annot, trigger);
		DAG_prop_set(formula, DAG_PROP_TRIGGER, &annot);
	} else
		stack_push(*Pannot, trigger);

	return formula;
}

Tbind
smt2_bind(char* symbol, TDAG term)
{
	Tbind bind;
	MY_MALLOC(bind, sizeof(struct TSbind));
	bind->symb = DAG_symb_new(symbol, SYMB_VARIABLE, DAG_sort(term));
	free(symbol);
	bind->DAG = term;
	return bind;
}

/*
  --------------------------------------------------------------
  Binders
  --------------------------------------------------------------
*/

void
smt2_declare_bind_list(Tlist bind_list)
{
	LIST_LOOP_BEGIN(bind_list, Tbind, bind);
	assert(DAG_symb_type(bind->symb) & SYMB_NAMED);
	declare_str(DAG_symb_name2(bind->symb), bind->symb);
	LIST_LOOP_END(bind_list);
}

void
smt2_undeclare_bind_list(Tlist bind_list)
{
	LIST_REVLOOP_BEGIN(bind_list, Tbind, bind);
	assert(DAG_symb_type(bind->symb) & SYMB_NAMED);
	undeclare_str(DAG_symb_name2(bind->symb), bind->symb);
	free(bind);
	LIST_REVLOOP_END(bind_list);
	list_free(&bind_list);
}

void
smt2_declare_stack_var(Tstack_symb stack_var)
{
	unsigned i;
	for (i = 0; i < stack_var->size; i++)
		declare_str(DAG_symb_name2(stack_var->data[i]), stack_var->data[i]);
}

void
smt2_undeclare_stack_var(Tstack_symb stack_var)
{
	unsigned i;
	for (i = stack_var->size; i-- > 0;)
		undeclare_str(DAG_symb_name2(stack_var->data[i]), stack_var->data[i]);
	/* PF Should we do this? free(symb); */
	stack_free(stack_var);
}

/*
  --------------------------------------------------------------
  Tsymb
  --------------------------------------------------------------
*/

Tsymb
smt2_var(char* symbol, Tsort sort)
{
	Tsymb symb = DAG_symb_new(symbol, SYMB_VARIABLE, sort);
	if (!symb)
		veriT_error("unable to create variable %s on line", symbol, yylineno);
	free(symbol);
	return symb;
}

/*
  --------------------------------------------------------------
  Sorts
  --------------------------------------------------------------
*/

/**
   \brief Creates a sort variable
   \param str The name of the sort variable */
Tsort
smt2_sort_var(char* str)
{
	Tsort sort = DAG_sort_new_var(str);
	free(str);
	return sort;
}

Tlist
smt2_sort_var_list_one(Tsort sort)
{
	return list_add(NULL, ptr_of_sort(sort));
}

Tlist
smt2_sort_var_list_add(Tlist sort_var_list, Tsort sort)
{
	return list_add(sort_var_list, ptr_of_sort(sort));
}

void
smt2_declare_sort_var_list(T_SORT_LIST sort_list)
{
#ifdef PEDANTIC
	printf("%p\n", (void*)sort_list);
#endif
}

void
smt2_undeclare_sort_var_list(T_SORT_LIST sort_list)
{
#ifdef PEDANTIC
	printf("%p\n", (void*)sort_list);
#endif
}

/**
    \brief check if the sort is parametric
    \param sort the sort to check
*/
int
smt2_sort_parametric(Tsort sort)
{
	return DAG_sort_parametric(sort);
}

Tsort
smt2_sort_lookup(char* symbol)
{
	Tsort sort = DAG_sort_lookup(symbol);
	if (!sort) veriT_error("undefined sort %s on line %d", symbol, yylineno);
	free(symbol);
	return sort;
}

Tsort
smt2_sort_lookup_index(char* symbol, Tlist sort_list)
{
	veriT_error("unimplemented");
	list_free(&sort_list);
#ifdef PEDANTIC
	printf("%s\n", symbol);
	printf("%p\n", (void*)sort_list);
#endif
	return DAG_SORT_NULL;
}

Tsort
smt2_sort_functional(Tlist sort_list)
{
	unsigned i, arity = list_length(sort_list);
	Tsort* sub;
	MY_MALLOC(sub, arity * sizeof(Tsort));
	for (i = 0; i < arity; i++, sort_list = list_cdr(sort_list))
		sub[i] = sort_of_ptr(list_car(sort_list));
	list_free(&sort_list);
	return DAG_sort_new(NULL, arity, sub);
}

Tsort
smt2_sort_apply(Tsort sort, Tlist sort_list)
{
	unsigned i, arity;
	Tsort *sub, result;
	arity = list_length(sort_list);
	MY_MALLOC(sub, arity * sizeof(Tsort));
	for (i = 0; i < arity; ++i, sort_list = list_cdr(sort_list)) {
		Tsort sort2 = DAG_sort_of_ptr(list_car(sort_list));
		if (!sort2)
			my_error("line %d: unknown sort %s.", yylineno, DAG_sort_name(sort2));
		sub[i] = sort2;
	}
	result = DAG_sort_new_inst(NULL, sort, arity, sub);
	list_free(&sort_list);
	return result;
}

/*
  --------------------------------------------------------------
  List building functions
  --------------------------------------------------------------
*/

Tlist
smt2_term_list_one(TDAG term)
{
	return list_add(NULL, DAG_ptr_of(term));
}

Tlist
smt2_term_list_add(Tlist term_list, TDAG term)
{
	return list_add(term_list, DAG_ptr_of(term));
}

Tlist
smt2_numeral_list_one(char* numeral)
{
	return list_add(NULL, (void*)numeral);
}

Tlist
smt2_numeral_list_add(Tlist numeral_list, char* numeral)
{
	return list_add(numeral_list, (void*)numeral);
}

Tlist
smt2_bind_list_new(void)
{
	return NULL;
}

Tlist
smt2_bind_list_one(Tbind bind)
{
	return list_add(NULL, (void*)bind);
}

Tlist
smt2_bind_list_add(Tlist bind_list, Tbind bind)
{
	return list_add(bind_list, (void*)bind);
}

Tstack_symb
smt2_stack_var_new(void)
{
	Tstack_symb stack_var;
	stack_INIT(stack_var);
	return stack_var;
}

Tstack_symb
smt2_stack_var_one(Tsymb var)
{
	Tstack_symb stack_var;
	stack_INIT(stack_var);
	stack_push(stack_var, var);
	return stack_var;
}

Tstack_symb
smt2_stack_var_add(Tstack_symb stack_var, Tsymb var)
{
	stack_push(stack_var, var);
	return stack_var;
}

Tlist
smt2_sort_list_new(void)
{
	return NULL;
}

Tlist
smt2_sort_list_one(Tsort sort)
{
	return list_add(NULL, DAG_ptr_of_sort(sort));
}

Tlist
smt2_sort_list_add(Tlist sort_list, Tsort sort)
{
	return list_add(sort_list, DAG_ptr_of_sort(sort));
}

Tlist
smt2_symbol_list_new(void)
{
	return NULL;
}

Tlist
smt2_symbol_list_add(Tlist symbol_list, char* symbol)
{
	return list_add(symbol_list, (void*)symbol);
}

void
smt2_command(void)
{}

/*
  --------------------------------------------------------------
  module init and done
  --------------------------------------------------------------
*/

void
smt2_init(bool option_disable_print_success)
{
	hashmap_str = hashmap_new(
		200, (TFhash)hash_function, (TFequal)hash_str_equal, (TFfree)hash_key_free,
		(TFfree)hash_value_free);
	stat_result =
		stats_counter_new("res", "0 (UNSAT), 1 (SAT), -1 (UNKNOWN)", "%5d");
	stats_counter_set(stat_result, -1);
	smt2_print_success = !option_disable_print_success;
	smt2_diagnostic_output_channel = strmake("stderr");
	smt2_regular_output_channel = strmake("stdout");
#if STATS_LEVEL > 1
	stat_nb_nodes = stats_counter_new(
		"nodes",
		"Number of nodes in the input formula"
		" as a DAG representation",
		"%6d");
	stat_nb_nodes_tree = stats_counter_new(
		"nodes_tree",
		"Number of nodes in the input"
		" formula as a tree"
		" representation",
		"%6d");
	stat_nb_atoms = stats_counter_new(
		"atoms",
		"Number of atoms in the input"
		" formula as a tree representation",
		"%6d");
#endif
}

void
smt2_done(void)
{
	if (!smt2_exited) smt2_exit();
	free(smt2_regular_output_channel);
	free(smt2_diagnostic_output_channel);
	hashmap_free(&hashmap_str);
}
