#include "parsers/dimacs/dimacs.h"

#include "response.h"
#include "symbolic/DAG.h"
#include "utils/general.h"
#include "utils/statistics.h"
#include "veriT.h"

static unsigned lineno = 1;

static unsigned stat_result;
#if STATS_LEVEL > 1
static unsigned stat_nb_vars, stat_nb_clauses, stat_lits;
#endif

static Tsymb
bool_symb(unsigned var)
{
	static char str[10] = "";
	static unsigned assigned = 0;
	assert(var);
	if (var > 9999999) my_error("Too many Boolean variables\n");
/* Since assigned < var < 9999999 the string for var will */
/* never be longer than 8 chars. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
	while (assigned < var) {
		assigned++;
		snprintf(str, 10, "v_%u", assigned);
		DAG_symb_new(str, SYMB_PREDICATE, SORT_BOOLEAN);
	}
	snprintf(str, 10, "v_%u", var);
#pragma GCC diagnostic pop
#if STATS_LEVEL > 1
	if (var > stat_nb_vars) stat_nb_vars = vars;
#endif
	return DAG_symb_lookup(str, 0, NULL, DAG_SORT_NULL);
}

static void
clause_new(unsigned n, int* lit)
{
	unsigned i;
	TDAG *PDAG, clause;
	switch (n) {
		case 0: clause = DAG_FALSE; break;
		case 1:
			clause =
				DAG_new(bool_symb((unsigned)(lit[0] > 0 ? lit[0] : -lit[0])), 0, NULL);
			if (lit[0] < 0) clause = DAG_not(clause);
			break;
		default:
			MY_MALLOC(PDAG, n * sizeof(TDAG));
			for (i = 0; i < n; i++) {
				PDAG[i] = DAG_new(
					bool_symb((unsigned)(lit[i] > 0 ? lit[i] : -lit[i])), 0, NULL);
				if (lit[i] < 0) PDAG[i] = DAG_not(PDAG[i]);
			}
			clause = DAG_new(CONNECTOR_OR, n, PDAG);
	}
	DAG_dup(clause);
	veriT_assert(clause);
	DAG_free(clause);
#if STATS_LEVEL > 1
	stat_nb_lits += n;
	stat_nb_clauses++;
#endif
}

static void
eat_spaces(FILE* file)
{
	int c = 0; /* = 0 is here just to fix spurious compiler warning */
	while (!feof(file) && ((c = fgetc(file)) == ' ' || c == 9))
		;
	if (!feof(file) && c != ' ' && c != 9) ungetc(c, file);
}

static int
parse_int(FILE* file)
{
	unsigned sign = 1;
	int c;
	int res = 0;
	c = fgetc(file);
	if (c == '-') sign = 0;
	if (c == '-' || c == '+') {
		if (feof(file))
			my_error("parse error while parsing integer on line %u\n", lineno);
		if (c == '-') sign = 0;
		c = fgetc(file);
	}
	if (c < '0' && c > '9')
		my_error("parse error while parsing integer on line %u\n", lineno);
	while (c >= '0' && c <= '9') {
		res = res * 10 + c - '0';
		if (!feof(file)) c = fgetc(file);
	}
	if (!feof(file)) ungetc(c, file);
	if (!sign) res = -res;
	return res;
}

static void
parse(FILE* file)
{
	int c;
	unsigned n = 0;
	unsigned size = 0;
	int* lit = NULL;
	if (!file) return;
	eat_spaces(file);
	while (!feof(file)) {
		c = fgetc(file);
		if (c == 'c' || c == 'p') {
			while (!feof(file) && (c = fgetc(file)) != '\n')
				;
			if (!feof(file) && c != '\n') ungetc(c, file);
			if (c == '\n') lineno++;
		} else if ((c >= '0' && c <= '9') || c == '-' || c == '+') {
			int tmp;
			ungetc(c, file);
			if (!size) {
				size = 1;
				MY_MALLOC(lit, size * sizeof(int));
			}
			while (size <= n) {
				size *= 2;
				MY_REALLOC(lit, size * sizeof(int));
			}
			tmp = parse_int(file);
			if (!tmp) {
				clause_new(n, lit);
				n = 0;
			} else {
				lit[n++] = tmp;
			}
		} else if (c == '\n')
			lineno++;
		else
			my_error("parse error on line %u\n", lineno);
		eat_spaces(file);
	}
	free(lit);
}

static void
dimacs_init(void)
{
	stat_result =
		stats_counter_new("res", "0 (UNSAT), 1 (SAT), -1 (UNKNOWN)", "%5d");
#if STATS_LEVEL > 1
	stat_nb_vars =
		stats_counter_new("vars", "Number of distinct vars in the input", "%6d");
	stat_nb_lits =
		stats_counter_new("lits", "Number of literals in the input", "%6d");
	stat_nb_clauses =
		stats_counter_new("clauses", "Number of clauses in the input", "%6d");
#endif
}

static void
dimacs_done(void)
{}

static void
dimacs_check_sat(void)
{
	Tstatus status = veriT_check_sat();
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
}

void
parse_dimacs(FILE* input)
{
	if (!input) veriT_error("parse_dimacs: no input file");
	veriT_logic("QF_UF");
	dimacs_init();
	parse(input);
	dimacs_check_sat();
	dimacs_done();
}
