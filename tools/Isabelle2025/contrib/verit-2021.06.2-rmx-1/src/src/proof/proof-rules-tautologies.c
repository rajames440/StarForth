#include "proof/proof-rules-tautologies.h"

#include "bool/bool.h"
#include "proof/proof-checking.h"
#include "proof/proof-output.h"
#include "proof/proof-step-table.h"
#include "symbolic/DAG-symb.h"
#include "symbolic/veriT-DAG.h"

/*
  --------------------------------------------------------------
  Tautologies for theories
  --------------------------------------------------------------
*/

/* PF add a valid step, and returns its steps id
   arguments (and elmnts of lits) are DAGs
   Implemented for eq_* */
Tproof
proof_step_valid(Tproof_type type, unsigned nb_lits, ...) {
    unsigned i;
    va_list adpar;
    Tproof_step proof_step = proof_step_new();
    va_start(adpar, nb_lits);
    for (i = 0; i < nb_lits; ++i)
        proof_step_add_DAG(proof_step, va_arg(adpar, TDAG));
#ifdef DEBUG_PROOF
    my_message("proof_step_list: input step:\n");
    print_proof_step(proof_step, top_steps, 0, stderr, false);
#endif
    /* TODO: add proof checking only if in debug mode */
    proof_step->type = type;
    return steps_push(proof_step);
}

/* PF add a valid step, and returns its steps id
   arguments (and elmnts of lits) are DAGs
   Implemented for eq_* */
Tproof
proof_step_valid_stack(Tproof_type type, Tstack_DAG lits) {
    Tproof_step proof_step = proof_step_new();
    for (unsigned i = 0; i < stack_size(lits); ++i)
        proof_step_add_DAG(proof_step, DAG_dup(stack_get(lits, i)));
#ifdef DEBUG_PROOF
    my_message("proof_step_list: input step:\n");
    print_proof_step(proof_step, top_steps, 0, stderr, false);
#endif
    /* TODO: add proof checking only if in debug mode */
    proof_step->type = type;
    return steps_push(proof_step);
}

Tproof
proof_step_valid_stack_args(Tproof_type type, Tstack_DAG lits, Tstack_DAG args) {
    Tproof_step proof_step = proof_step_new();
    for (unsigned i = 0; i < stack_size(lits); ++i)
        proof_step_add_DAG(proof_step, DAG_dup(stack_get(lits, i)));
    for (unsigned i = 0; i < stack_size(args); ++i)
        proof_step_add_arg(proof_step, DAG_dup(stack_get(args, i)));
#ifdef DEBUG_PROOF
    my_message("proof_step_list: input step:\n");
    print_proof_step(proof_step, top_steps, 0, stderr, false);
#endif
    /* TODO: add proof checking only if in debug mode */
    proof_step->type = type;
    return steps_push(proof_step);
}

/*
  --------------------------------------------------------------
  Tautologies as clauses
  --------------------------------------------------------------
*/

/* TRUE */
Tproof
proof_true(void) {
    Tproof_step proof_step = proof_step_new();
    proof_step_add_DAG(proof_step, DAG_dup(DAG_TRUE));
    proof_step->type = ps_type_true;
    return steps_push(proof_step);
}

/* NOT FALSE */
Tproof
proof_false(void) {
    Tproof_step proof_step = proof_step_new();
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG_FALSE)));
    proof_step->type = ps_type_false;
    return steps_push(proof_step);
}

/* NOT [A_1 AND ... A_n] OR A_i */
Tproof
proof_and_pos(TDAG DAG, unsigned i) {
    Tproof_step proof_step = proof_step_new();
    if (DAG_symb(DAG) != CONNECTOR_AND || i >= DAG_arity(DAG))
        proof_error("proof_and_pos", NULL);
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG)));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_arg(DAG, i)));
    proof_step->type = ps_type_and_pos;
    proof_step_add_param(proof_step, i);
    return steps_push(proof_step);
}

/* NOT NOT NOT A or A */
Tproof
proof_not_not(TDAG DAG) {
    Tproof_step proof_step = proof_step_new();
    if (DAG_symb(DAG) != CONNECTOR_NOT || DAG_symb(DAG_arg0(DAG)) != CONNECTOR_NOT)
        proof_error("proof_not_not", NULL);
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG)));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_arg0(DAG_arg0(DAG))));
    proof_step->type = ps_type_not_not;
    stack_push(top_steps, proof_step);
    /* We have to add this directly since proof_step_clean will try to remove the
	   double negation */
#ifdef DEBUG_PROOF
    my_message("Adding step (%d)\n", stack_size(top_steps) - 1);
    print_proof_step(
        proof_step, top_steps, stack_size(top_steps) - 1, stderr, false);
#endif
    return stack_size(top_steps) - 1;
}

/* [A_1 AND ... A_n] OR NOT A_1 OR ... NOT A_n */
Tproof
proof_and_neg(TDAG DAG) {
    unsigned i;
    Tproof_step proof_step = proof_step_new();
    if (DAG_symb(DAG) != CONNECTOR_AND) proof_error("proof_and_neg", NULL);
    proof_step_add_DAG(proof_step, DAG_dup(DAG));
    for (i = 0; i < DAG_arity(DAG); i++)
        proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG_arg(DAG, i))));
    proof_step->type = ps_type_and_neg;
    return steps_push(proof_step);
}

/* NOT [A_1 OR ... A_n] OR A_1 OR ... NOT A_n */
Tproof
proof_or_pos(TDAG DAG) {
    unsigned i;
    Tproof_step proof_step = proof_step_new();
    if (DAG_symb(DAG) != CONNECTOR_OR) proof_error("proof_or_pos", NULL);
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG)));
    for (i = 0; i < DAG_arity(DAG); i++)
        proof_step_add_DAG(proof_step, DAG_dup(DAG_arg(DAG, i)));
    proof_step->type = ps_type_or_pos;
    return steps_push(proof_step);
}

/* [A_1 OR ... A_n] OR NOT A_i */
Tproof
proof_or_neg(TDAG DAG, unsigned i) {
    Tproof_step proof_step = proof_step_new();
    if (DAG_symb(DAG) != CONNECTOR_OR || i >= DAG_arity(DAG))
        proof_error("proof_or_neg", NULL);
    proof_step_add_DAG(proof_step, DAG_dup(DAG));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG_arg(DAG, i))));
    proof_step->type = ps_type_or_neg;
    proof_step_add_param(proof_step, i);
    return steps_push(proof_step);
}

/* NOT [A_1 XOR A_2] OR A_1 OR A_2 */
Tproof
proof_xor_pos1(TDAG DAG) {
    Tproof_step proof_step = proof_step_new();
    if (DAG_symb(DAG) != CONNECTOR_XOR || DAG_arity(DAG) != 2)
        proof_error("proof_xor_pos1", NULL);
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG)));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_arg0(DAG)));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_arg1(DAG)));
    proof_step->type = ps_type_xor_pos1;
    return steps_push(proof_step);
}

/* NOT [A_1 XOR A_2] OR NOT A_1 OR NOT A_2 */
Tproof
proof_xor_pos2(TDAG DAG) {
    Tproof_step proof_step = proof_step_new();
    if (DAG_symb(DAG) != CONNECTOR_XOR || DAG_arity(DAG) != 2)
        proof_error("proof_xor_pos2", NULL);
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG)));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG_arg0(DAG))));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG_arg1(DAG))));
    proof_step->type = ps_type_xor_pos2;
    return steps_push(proof_step);
}

/* [A_1 XOR A_2] OR A_1 OR NOT A_2 */
Tproof
proof_xor_neg1(TDAG DAG) {
    Tproof_step proof_step = proof_step_new();
    if (DAG_symb(DAG) != CONNECTOR_XOR || DAG_arity(DAG) != 2)
        proof_error("proof_xor_neg1", NULL);
    proof_step_add_DAG(proof_step, DAG_dup(DAG));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_arg0(DAG)));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG_arg1(DAG))));
    proof_step->type = ps_type_xor_neg1;
    return steps_push(proof_step);
}

/* [A_1 XOR A_2] OR NOT A_1 OR A_2 */
Tproof
proof_xor_neg2(TDAG DAG) {
    Tproof_step proof_step = proof_step_new();
    if (DAG_symb(DAG) != CONNECTOR_XOR || DAG_arity(DAG) != 2)
        proof_error("proof_xor_neg2", NULL);
    proof_step_add_DAG(proof_step, DAG_dup(DAG));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG_arg0(DAG))));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_arg1(DAG)));
    proof_step->type = ps_type_xor_neg2;
    return steps_push(proof_step);
}

/* NOT[A_1 IMPLIES A_2] OR NOT A_1 OR A_2 */
Tproof
proof_implies_pos(TDAG DAG) {
    Tproof_step proof_step = proof_step_new();
    if (DAG_symb(DAG) != CONNECTOR_IMPLIES || DAG_arity(DAG) != 2)
        proof_error("proof_implies_pos", NULL);
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG)));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG_arg0(DAG))));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_arg1(DAG)));
    proof_step->type = ps_type_implies_pos;
    return steps_push(proof_step);
}

/* [A_1 IMPLIES A_2] OR A_1 */
Tproof
proof_implies_neg1(TDAG DAG) {
    Tproof_step proof_step = proof_step_new();
    if (DAG_symb(DAG) != CONNECTOR_IMPLIES || DAG_arity(DAG) != 2)
        proof_error("proof_implies_neg1", NULL);
    proof_step_add_DAG(proof_step, DAG_dup(DAG));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_arg0(DAG)));
    proof_step->type = ps_type_implies_neg1;
    return steps_push(proof_step);
}

/* [A_1 IMPLIES A_2] OR NOT A_2 */
Tproof
proof_implies_neg2(TDAG DAG) {
    Tproof_step proof_step = proof_step_new();
    if (DAG_symb(DAG) != CONNECTOR_IMPLIES || DAG_arity(DAG) != 2)
        proof_error("proof_implies_neg2", NULL);
    proof_step_add_DAG(proof_step, DAG_dup(DAG));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG_arg1(DAG))));
    proof_step->type = ps_type_implies_neg2;
    return steps_push(proof_step);
}

/* NOT [A_1 EQUIV A_2] OR A_1 OR NOT A_2 */
Tproof
proof_equiv_pos1(TDAG DAG) {
    Tproof_step proof_step = proof_step_new();
    if (DAG_symb(DAG) != CONNECTOR_EQUIV || DAG_arity(DAG) != 2)
        proof_error("proof_equiv_pos1", NULL);
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG)));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_arg0(DAG)));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG_arg1(DAG))));
    proof_step->type = ps_type_equiv_pos1;
    return steps_push(proof_step);
}

/* NOT [A_1 EQUIV A_2] OR NOT A_1 OR A_2 */
Tproof
proof_equiv_pos2(TDAG DAG) {
    Tproof_step proof_step = proof_step_new();
    if (DAG_symb(DAG) != CONNECTOR_EQUIV || DAG_arity(DAG) != 2)
        proof_error("proof_equiv_pos2", NULL);
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG)));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG_arg0(DAG))));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_arg1(DAG)));
    proof_step->type = ps_type_equiv_pos2;
    return steps_push(proof_step);
}

/* [A_1 EQUIV A_2] OR NOT A_1 OR NOT A_2 */
Tproof
proof_equiv_neg1(TDAG DAG) {
    Tproof_step proof_step = proof_step_new();
    if (DAG_symb(DAG) != CONNECTOR_EQUIV || DAG_arity(DAG) != 2)
        proof_error("proof_equiv_neg1", NULL);
    proof_step_add_DAG(proof_step, DAG_dup(DAG));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG_arg0(DAG))));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG_arg1(DAG))));
    proof_step->type = ps_type_equiv_neg1;
    return steps_push(proof_step);
}

/* [A_1 EQUIV A_2] OR A_1 OR A_2 */
Tproof
proof_equiv_neg2(TDAG DAG) {
    Tproof_step proof_step = proof_step_new();
    if (DAG_symb(DAG) != CONNECTOR_EQUIV || DAG_arity(DAG) != 2)
        proof_error("proof_equiv_neg2", NULL);
    proof_step_add_DAG(proof_step, DAG_dup(DAG));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_arg0(DAG)));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_arg1(DAG)));
    proof_step->type = ps_type_equiv_neg2;
    return steps_push(proof_step);
}

/* NOT [IF A THEN B ELSE C] OR A OR C */
Tproof
proof_ite_pos1(TDAG DAG) {
    Tproof_step proof_step = proof_step_new();
    if (DAG_symb(DAG) != CONNECTOR_ITE || DAG_arity(DAG) != 3)
        proof_error("proof_ite_pos1", NULL);
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG)));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_arg(DAG, 0)));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_arg(DAG, 2)));
    proof_step->type = ps_type_ite_pos1;
    return steps_push(proof_step);
}

/* NOT [IF A THEN B ELSE C] OR NOT A OR B */
Tproof
proof_ite_pos2(TDAG DAG) {
    Tproof_step proof_step = proof_step_new();
    if (DAG_symb(DAG) != CONNECTOR_ITE || DAG_arity(DAG) != 3)
        proof_error("proof_ite_pos2", NULL);
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG)));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG_arg(DAG, 0))));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_arg(DAG, 1)));
    proof_step->type = ps_type_ite_pos2;
    return steps_push(proof_step);
}

/* [IF A THEN B ELSE C] OR A OR NOT C */
Tproof
proof_ite_neg1(TDAG DAG) {
    Tproof_step proof_step = proof_step_new();
    if (DAG_symb(DAG) != CONNECTOR_ITE || DAG_arity(DAG) != 3)
        proof_error("proof_ite_neg1", NULL);
    proof_step_add_DAG(proof_step, DAG_dup(DAG));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_arg(DAG, 0)));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG_arg(DAG, 2))));
    proof_step->type = ps_type_ite_neg1;
    return steps_push(proof_step);
}

/* [IF A THEN B ELSE C] OR NOT A OR NOT B */
Tproof
proof_ite_neg2(TDAG DAG) {
    Tproof_step proof_step = proof_step_new();
    if (DAG_symb(DAG) != CONNECTOR_ITE || DAG_arity(DAG) != 3)
        proof_error("proof_ite_neg2", NULL);
    proof_step_add_DAG(proof_step, DAG_dup(DAG));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG_arg(DAG, 0))));
    proof_step_add_DAG(proof_step, DAG_dup(DAG_not(DAG_arg(DAG, 1))));
    proof_step->type = ps_type_ite_neg2;
    return steps_push(proof_step);
}

/*
  --------------------------------------------------------------
  Tautologies with premisses
  --------------------------------------------------------------
*/

/* A_1 OR ... A_i ... NOT A_i ... OR A_n --> TRUE */
Tproof
proof_tautology(Tproof clause) {
    Tproof_step new_proof_step = proof_step_new();
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_TRUE));
    new_proof_step->type = ps_type_tautology;
    proof_step_add_reason(new_proof_step, clause);
    return steps_push(new_proof_step);
}

/* A_1 AND ... A_i ... AND A_n --> A_i */
Tproof
proof_and(Tproof id, unsigned i) {
    TDAG DAG;
    Tproof_step new_proof_step = proof_step_new();
    if (stack_size(stack_get(top_steps, id)->DAGs) != 1)
        proof_error("proof_and", stack_get(top_steps, id));
    DAG = stack_get(stack_get(top_steps, id)->DAGs, 0);
    if (DAG_symb(DAG) != CONNECTOR_AND || i >= DAG_arity(DAG))
        proof_error("proof_and", stack_get(top_steps, id));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_arg(DAG, i)));
    new_proof_step->type = ps_type_and;
    proof_step_add_reason(new_proof_step, id);
    proof_step_add_param(new_proof_step, i);
    return steps_push(new_proof_step);
}

/* NOT(A_1 OR ... A_i ... OR A_n) --> NOT A_i */
Tproof
proof_not_or(Tproof id, unsigned i) {
    TDAG DAG;
    Tproof_step new_proof_step = proof_step_new();
    if (stack_size(stack_get(top_steps, id)->DAGs) != 1)
        proof_error("proof_not_or", stack_get(top_steps, id));
    DAG = stack_get(stack_get(top_steps, id)->DAGs, 0);
    if (DAG_symb(DAG) != CONNECTOR_NOT || DAG_arity(DAG) != 1)
        proof_error("proof_not_or", stack_get(top_steps, id));
    DAG = DAG_arg0(DAG);
    if (DAG_symb(DAG) != CONNECTOR_OR || i >= DAG_arity(DAG))
        proof_error("proof_not_or", stack_get(top_steps, id));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_not(DAG_arg(DAG, i))));
    new_proof_step->type = ps_type_not_or;
    proof_step_add_reason(new_proof_step, id);
    proof_step_add_param(new_proof_step, i);
    return steps_push(new_proof_step);
}

/* A_1 OR ... A_n --> {A_1, ... A_n} */
Tproof
proof_or(Tproof id) {
    unsigned i;
    TDAG DAG;
    Tproof_step new_proof_step = proof_step_new();
    if (stack_size(stack_get(top_steps, id)->DAGs) != 1)
        proof_error("proof_or", stack_get(top_steps, id));
    DAG = stack_get(stack_get(top_steps, id)->DAGs, 0);
    if (DAG_symb(DAG) != CONNECTOR_OR)
        proof_error("proof_or", stack_get(top_steps, id));
    for (i = 0; i < DAG_arity(DAG); i++)
        proof_step_add_DAG(new_proof_step, DAG_dup(DAG_arg(DAG, i)));
    new_proof_step->type = ps_type_or;
    proof_step_add_reason(new_proof_step, id);
    return steps_push(new_proof_step);
}

/* NOT(A_1 AND ... A_n) --> {NOT A_1, ... NOT A_n} */
Tproof
proof_not_and(Tproof id) {
    unsigned i;
    TDAG DAG;
    Tproof_step new_proof_step = proof_step_new();
    if (stack_size(stack_get(top_steps, id)->DAGs) != 1)
        proof_error("proof_not_and", stack_get(top_steps, id));
    DAG = stack_get(stack_get(top_steps, id)->DAGs, 0);
    if (
        DAG_symb(DAG) != CONNECTOR_NOT || DAG_symb(DAG_arg0(DAG)) != CONNECTOR_AND)
        proof_error("proof_not_and", stack_get(top_steps, id));
    DAG = DAG_arg0(DAG);
    for (i = 0; i < DAG_arity(DAG); i++)
        proof_step_add_DAG(new_proof_step, DAG_dup(DAG_not(DAG_arg(DAG, i))));
    new_proof_step->type = ps_type_not_and;
    proof_step_add_reason(new_proof_step, id);
    return steps_push(new_proof_step);
}

/* A XOR B --> {A, B} */
Tproof
proof_xor1(Tproof id) {
    TDAG DAG;
    Tproof_step new_proof_step = proof_step_new();
    if (stack_size(stack_get(top_steps, id)->DAGs) != 1)
        proof_error("proof_xor1", stack_get(top_steps, id));
    DAG = stack_get(stack_get(top_steps, id)->DAGs, 0);
    if (DAG_symb(DAG) != CONNECTOR_XOR || DAG_arity(DAG) != 2)
        proof_error("proof_xor1", stack_get(top_steps, id));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_arg0(DAG)));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_arg1(DAG)));
    new_proof_step->type = ps_type_xor1;
    proof_step_add_reason(new_proof_step, id);
    return steps_push(new_proof_step);
}

/* A XOR B --> {NOT A, NOT B} */
Tproof
proof_xor2(Tproof id) {
    TDAG DAG;
    Tproof_step new_proof_step = proof_step_new();
    if (stack_size(stack_get(top_steps, id)->DAGs) != 1)
        proof_error("proof_xor2", stack_get(top_steps, id));
    DAG = stack_get(stack_get(top_steps, id)->DAGs, 0);
    if (DAG_symb(DAG) != CONNECTOR_XOR || DAG_arity(DAG) != 2)
        proof_error("proof_xor2", stack_get(top_steps, id));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_not(DAG_arg0(DAG))));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_not(DAG_arg1(DAG))));
    new_proof_step->type = ps_type_xor2;
    proof_step_add_reason(new_proof_step, id);
    return steps_push(new_proof_step);
}

/* NOT(A XOR B) --> {A, NOT B} */
Tproof
proof_not_xor1(Tproof id) {
    Tproof_step new_proof_step = proof_step_new();
    TDAG DAG;
    if (stack_size(stack_get(top_steps, id)->DAGs) != 1)
        proof_error("proof_not_xor1", stack_get(top_steps, id));
    DAG = stack_get(stack_get(top_steps, id)->DAGs, 0);
    if (
        DAG_symb(DAG) != CONNECTOR_NOT ||
        DAG_symb(DAG_arg0(DAG)) != CONNECTOR_XOR || DAG_arity(DAG_arg0(DAG)) != 2)
        proof_error("proof_not_xor1", stack_get(top_steps, id));
    DAG = DAG_arg0(DAG);
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_arg0(DAG)));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_not(DAG_arg1(DAG))));
    new_proof_step->type = ps_type_not_xor1;
    proof_step_add_reason(new_proof_step, id);
    return steps_push(new_proof_step);
}

/* NOT(A XOR B) --> {NOT A, B} */
Tproof
proof_not_xor2(Tproof id) {
    TDAG DAG;
    Tproof_step new_proof_step = proof_step_new();
    if (stack_size(stack_get(top_steps, id)->DAGs) != 1)
        proof_error("proof_not_xor2", stack_get(top_steps, id));
    DAG = stack_get(stack_get(top_steps, id)->DAGs, 0);
    if (
        DAG_symb(DAG) != CONNECTOR_NOT ||
        DAG_symb(DAG_arg0(DAG)) != CONNECTOR_XOR || DAG_arity(DAG_arg0(DAG)) != 2)
        proof_error("proof_not_xor2", stack_get(top_steps, id));
    DAG = DAG_arg0(DAG);
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_not(DAG_arg0(DAG))));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_arg1(DAG)));
    new_proof_step->type = ps_type_not_xor2;
    proof_step_add_reason(new_proof_step, id);
    return steps_push(new_proof_step);
}

/* A IMPLIES B --> {NOT A, B} */
Tproof
proof_implies(Tproof id) {
    TDAG DAG;
    Tproof_step new_proof_step = proof_step_new();
    if (stack_size(stack_get(top_steps, id)->DAGs) != 1)
        proof_error("proof_implies", stack_get(top_steps, id));
    DAG = stack_get(stack_get(top_steps, id)->DAGs, 0);
    if (DAG_symb(DAG) != CONNECTOR_IMPLIES || DAG_arity(DAG) != 2)
        proof_error("proof_implies", stack_get(top_steps, id));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_not(DAG_arg0(DAG))));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_arg1(DAG)));
    new_proof_step->type = ps_type_implies;
    proof_step_add_reason(new_proof_step, id);
    return steps_push(new_proof_step);
}

/* NOT(A IMPLIES B) --> A */
Tproof
proof_not_implies1(Tproof id) {
    TDAG DAG;
    Tproof_step new_proof_step = proof_step_new();
    if (stack_size(stack_get(top_steps, id)->DAGs) != 1)
        proof_error("proof_not_implies1", stack_get(top_steps, id));
    DAG = stack_get(stack_get(top_steps, id)->DAGs, 0);
    if (
        DAG_symb(DAG) != CONNECTOR_NOT ||
        DAG_symb(DAG_arg0(DAG)) != CONNECTOR_IMPLIES ||
        DAG_arity(DAG_arg0(DAG)) != 2)
        proof_error("proof_not_implies1", stack_get(top_steps, id));
    DAG = DAG_arg0(DAG);
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_arg0(DAG)));
    new_proof_step->type = ps_type_not_implies1;
    proof_step_add_reason(new_proof_step, id);
    return steps_push(new_proof_step);
}

/* NOT(A IMPLIES B) --> NOT B */
Tproof
proof_not_implies2(Tproof id) {
    TDAG DAG;
    Tproof_step new_proof_step = proof_step_new();
    if (stack_size(stack_get(top_steps, id)->DAGs) != 1)
        proof_error("proof_not_implies2", stack_get(top_steps, id));
    DAG = stack_get(stack_get(top_steps, id)->DAGs, 0);
    if (
        DAG_symb(DAG) != CONNECTOR_NOT ||
        DAG_symb(DAG_arg0(DAG)) != CONNECTOR_IMPLIES ||
        DAG_arity(DAG_arg0(DAG)) != 2)
        proof_error("proof_not_implies2", stack_get(top_steps, id));
    DAG = DAG_arg0(DAG);
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_not(DAG_arg1(DAG))));
    new_proof_step->type = ps_type_not_implies2;
    proof_step_add_reason(new_proof_step, id);
    return steps_push(new_proof_step);
}

/* A EQUIV B --> {NOT A, B} */
Tproof
proof_equiv1(Tproof id) {
    Tproof_step new_proof_step = proof_step_new();
    TDAG DAG;
    if (stack_size(stack_get(top_steps, id)->DAGs) != 1)
        proof_error("proof_equiv1", stack_get(top_steps, id));
    DAG = stack_get(stack_get(top_steps, id)->DAGs, 0);
    if (DAG_symb(DAG) != CONNECTOR_EQUIV || DAG_arity(DAG) != 2)
        proof_error("proof_equiv1", stack_get(top_steps, id));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_not(DAG_arg0(DAG))));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_arg1(DAG)));
    new_proof_step->type = ps_type_equiv1;
    proof_step_add_reason(new_proof_step, id);
    return steps_push(new_proof_step);
}

/* A EQUIV B --> {A, NOT B} */
Tproof
proof_equiv2(Tproof id) {
    Tproof_step new_proof_step = proof_step_new();
    TDAG DAG;
    if (stack_size(stack_get(top_steps, id)->DAGs) != 1)
        proof_error("proof_equiv2", stack_get(top_steps, id));
    DAG = stack_get(stack_get(top_steps, id)->DAGs, 0);
    if (DAG_symb(DAG) != CONNECTOR_EQUIV || DAG_arity(DAG) != 2)
        proof_error("proof_equiv2", stack_get(top_steps, id));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_arg0(DAG)));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_not(DAG_arg1(DAG))));
    new_proof_step->type = ps_type_equiv2;
    proof_step_add_reason(new_proof_step, id);
    return steps_push(new_proof_step);
}

/* NOT(A EQUIV B) --> A OR B */
Tproof
proof_not_equiv1(Tproof id) {
    Tproof_step new_proof_step = proof_step_new();
    TDAG DAG;
    if (stack_size(stack_get(top_steps, id)->DAGs) != 1)
        proof_error("proof_not_equiv1", stack_get(top_steps, id));
    DAG = stack_get(stack_get(top_steps, id)->DAGs, 0);
    if (
        DAG_symb(DAG) != CONNECTOR_NOT ||
        DAG_symb(DAG_arg0(DAG)) != CONNECTOR_EQUIV || DAG_arity(DAG_arg0(DAG)) != 2)
        proof_error("proof_not_equiv1", stack_get(top_steps, id));
    DAG = DAG_arg0(DAG);
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_arg0(DAG)));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_arg1(DAG)));
    new_proof_step->type = ps_type_not_equiv1;
    proof_step_add_reason(new_proof_step, id);
    return steps_push(new_proof_step);
}

/* NOT(A EQUIV B) --> NOT A OR NOT B */
Tproof
proof_not_equiv2(Tproof id) {
    Tproof_step new_proof_step = proof_step_new();
    TDAG DAG;
    if (stack_size(stack_get(top_steps, id)->DAGs) != 1)
        proof_error("proof_not_equiv2", stack_get(top_steps, id));
    DAG = stack_get(stack_get(top_steps, id)->DAGs, 0);
    if (
        DAG_symb(DAG) != CONNECTOR_NOT ||
        DAG_symb(DAG_arg0(DAG)) != CONNECTOR_EQUIV || DAG_arity(DAG_arg0(DAG)) != 2)
        proof_error("proof_not_equiv2", stack_get(top_steps, id));
    DAG = DAG_arg0(DAG);
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_not(DAG_arg0(DAG))));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_not(DAG_arg1(DAG))));
    new_proof_step->type = ps_type_not_equiv2;
    proof_step_add_reason(new_proof_step, id);
    return steps_push(new_proof_step);
}

/* IF A THEN B ELSE C --> A OR C */
Tproof
proof_ite1(Tproof id) {
    Tproof_step new_proof_step = proof_step_new();
    TDAG DAG;
    if (stack_size(stack_get(top_steps, id)->DAGs) != 1)
        proof_error("proof_ite1", stack_get(top_steps, id));
    DAG = stack_get(stack_get(top_steps, id)->DAGs, 0);
    if (DAG_symb(DAG) != CONNECTOR_ITE || DAG_arity(DAG) != 3)
        proof_error("proof_ite1", stack_get(top_steps, id));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_arg(DAG, 0)));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_arg(DAG, 2)));
    new_proof_step->type = ps_type_ite1;
    proof_step_add_reason(new_proof_step, id);
    return steps_push(new_proof_step);
}

/* IF A THEN B ELSE C --> NOT A OR B */
Tproof
proof_ite2(Tproof id) {
    Tproof_step new_proof_step = proof_step_new();
    TDAG DAG;
    if (stack_size(stack_get(top_steps, id)->DAGs) != 1)
        proof_error("proof_ite2", stack_get(top_steps, id));
    DAG = stack_get(stack_get(top_steps, id)->DAGs, 0);
    if (DAG_symb(DAG) != CONNECTOR_ITE || DAG_arity(DAG) != 3)
        proof_error("proof_ite2", stack_get(top_steps, id));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_not(DAG_arg(DAG, 0))));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_arg(DAG, 1)));
    new_proof_step->type = ps_type_ite2;
    proof_step_add_reason(new_proof_step, id);
    return steps_push(new_proof_step);
}

/* NOT(IF A THEN B ELSE C) --> A OR NOT C */
Tproof
proof_not_ite1(Tproof id) {
    Tproof_step new_proof_step = proof_step_new();
    TDAG DAG;
    if (stack_size(stack_get(top_steps, id)->DAGs) != 1)
        proof_error("proof_not_ite1", stack_get(top_steps, id));
    DAG = stack_get(stack_get(top_steps, id)->DAGs, 0);
    if (
        DAG_symb(DAG) != CONNECTOR_NOT ||
        DAG_symb(DAG_arg(DAG, 0)) != CONNECTOR_ITE ||
        DAG_arity(DAG_arg(DAG, 0)) != 3)
        proof_error("proof_not_ite1", stack_get(top_steps, id));
    DAG = DAG_arg(DAG, 0);
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_arg(DAG, 0)));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_not(DAG_arg(DAG, 2))));
    new_proof_step->type = ps_type_not_ite1;
    proof_step_add_reason(new_proof_step, id);
    return steps_push(new_proof_step);
}

/* NOT(IF A THEN B ELSE C) --> NOT A OR NOT B */
Tproof
proof_not_ite2(Tproof id) {
    Tproof_step new_proof_step = proof_step_new();
    TDAG DAG;
    if (stack_size(stack_get(top_steps, id)->DAGs) != 1)
        proof_error("proof_not_ite2", stack_get(top_steps, id));
    DAG = stack_get(stack_get(top_steps, id)->DAGs, 0);
    if (
        DAG_symb(DAG) != CONNECTOR_NOT ||
        DAG_symb(DAG_arg(DAG, 0)) != CONNECTOR_ITE ||
        DAG_arity(DAG_arg(DAG, 0)) != 3)
        proof_error("proof_not_ite2", stack_get(top_steps, id));
    DAG = DAG_arg(DAG, 0);
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_not(DAG_arg(DAG, 0))));
    proof_step_add_DAG(new_proof_step, DAG_dup(DAG_not(DAG_arg(DAG, 1))));
    new_proof_step->type = ps_type_not_ite2;
    proof_step_add_reason(new_proof_step, id);
    return steps_push(new_proof_step);
}