#include "complete.h"

#include "symbolic/DAG-arith.h"
#include "symbolic/DAG.h"
#include "symbolic/recursion.h"
#include "symbolic/veriT-DAG.h"
#include "utils/general.h"
#include "veriT-config.h"

#include <string.h>

/* DD a predefined SMT logic has been set */
static bool predefined_logic_set = false;
/* DD a predefined SMT logic has been set and we are complete */
static bool predefined_logic_complete = false;

/* PF interpreted (non arithmetic) symbol in input ? */
static bool interpreted_non_arith_symbols = false;
/* PF quantifier in input ? */
static bool quantified_formulas = false;
/* PF Non Linear Arithmetic ? */
static bool NonLinearArithmetic = false;

static int
numeric_literal(TDAG DAG)
{
	return DAG_is_number(DAG) ||
		(unary_minus(DAG_symb(DAG)) && DAG_is_number(DAG_arg0(DAG)));
}

static void
inspect_formula(TDAG DAG)
{
	Tsymb symb = DAG_symb(DAG);
	if (quantifier(symb)) {
		quantified_formulas = true;
		return;
	}
	if (!(DAG_symb_type(symb) & SYMB_INTERPRETED)) return;
	if (
		boolean_connector(symb) || boolean_constant(symb) ||
		symb == FUNCTION_ZERO_VARIABLE || DAG_is_number(DAG))
		return;
	if (
		symb != PREDICATE_LESS && symb != PREDICATE_LEQ && symb != PREDICATE_EQ &&
		!arith_function(symb)) {
		interpreted_non_arith_symbols = true;
		return;
	}
	if (symb == FUNCTION_PROD || symb == FUNCTION_DIV) {
		unsigned i, c;
		for (i = 0, c = 0; i < DAG_arity(DAG); i++)
			if (!numeric_literal(DAG_arg(DAG, i))) c++;
		NonLinearArithmetic |= (c > 1);
	}
}

void
complete_logic(char* logic)
{
	if (logic == NULL || !strcmp(logic, "UNKNOWN")) return;
	predefined_logic_set = true;
	predefined_logic_complete =
		(strcmp(logic, "QF_UF") == 0 || strcmp(logic, "QF_IDL") == 0 ||
		 strcmp(logic, "QF_RDL") == 0 || strcmp(logic, "QF_UFIDL") == 0 ||
		 strcmp(logic, "QF_UFLIA") == 0 || strcmp(logic, "QF_LIA") == 0 ||
		 strcmp(logic, "QF_LRA") == 0 || strcmp(logic, "QF_LIRA") == 0 ||
		 strcmp(logic, "QF_UFLRA") == 0);
}

void
complete_add(TDAG DAG)
{
	if (!predefined_logic_set) structural_recursion_void(DAG, inspect_formula);
}

bool
complete_check(void)
{
	if (predefined_logic_set) return predefined_logic_complete;
	/* only quantifier-free linear arithmetic on int/real with UF */
	return !quantified_formulas && !interpreted_non_arith_symbols &&
		!NonLinearArithmetic;
}
