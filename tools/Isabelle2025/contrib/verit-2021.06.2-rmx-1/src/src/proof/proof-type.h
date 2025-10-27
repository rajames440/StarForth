#ifndef __PROOF_TYPE_H
#define __PROOF_TYPE_H

#include <stdarg.h>
#include <stdio.h>

/**
   \brief proof unit type: gives hint of the rule used to deduce */
typedef enum Tproof_type
{
	ps_type_none,
	ps_type_assume,
	ps_type_true,
	ps_type_false,
	ps_type_not_not,
	ps_type_and_pos,
	ps_type_and_neg,
	ps_type_or_pos,
	ps_type_or_neg,
	ps_type_xor_pos1,
	ps_type_xor_pos2,
	ps_type_xor_neg1,
	ps_type_xor_neg2,
	ps_type_implies_pos,
	ps_type_implies_neg1,
	ps_type_implies_neg2,
	ps_type_equiv_pos1,
	ps_type_equiv_pos2,
	ps_type_equiv_neg1,
	ps_type_equiv_neg2,
	ps_type_ite_pos1,
	ps_type_ite_pos2,
	ps_type_ite_neg1,
	ps_type_ite_neg2,
	ps_type_eq_reflexive,
	ps_type_eq_transitive,
	ps_type_eq_congruent,
	ps_type_eq_congruent_pred,
	ps_type_distinct_elim,
	ps_type_la_rw_eq,
	ps_type_la_generic,
	ps_type_lia_generic,
	ps_type_disequality_lemma,
	ps_type_totality_lemma,
	ps_type_la_tautology,
	ps_type_forall_inst_lemma,
	ps_type_qnt_join,
	ps_type_qnt_rm_unused,
	ps_type_th_resolution,
	ps_type_SAT_resolution,
	ps_type_refl,
	ps_type_trans,
	ps_type_cong,
	ps_type_and,
	ps_type_tautology,
	ps_type_not_or,
	ps_type_or,
	ps_type_not_and,
	ps_type_xor1,
	ps_type_xor2,
	ps_type_not_xor1,
	ps_type_not_xor2,
	ps_type_implies,
	ps_type_not_implies1,
	ps_type_not_implies2,
	ps_type_equiv1,
	ps_type_equiv2,
	ps_type_not_equiv1,
	ps_type_not_equiv2,
	ps_type_ite1,
	ps_type_ite2,
	ps_type_not_ite1,
	ps_type_not_ite2,
	ps_type_ite_intro,
	ps_type_contraction,
	ps_type_connective_def,
	ps_type_ite_simplify,
	ps_type_eq_simplify,
	ps_type_and_simplify,
	ps_type_or_simplify,
	ps_type_not_simplify,
	ps_type_implies_simplify,
	ps_type_equiv_simplify,
	ps_type_bool_simplify,
	ps_type_qnt_simplify,
	ps_type_div_simplify,
	ps_type_prod_simplify,
	ps_type_unary_minus_simplify,
	ps_type_minus_simplify,
	ps_type_sum_simplify,
	ps_type_comp_simplify,
	ps_type_nary_elim,
	ps_type_ac_simp,
	ps_type_bfun_elim,
	ps_type_deep_skolemize,
	ps_type_qnt_cnf,

	/* Proof rules containing subproofs */
	ps_type_subproof,

	/* Proof rules containing subproofs and might change the context */
	ps_type_bind,
	ps_type_let_elim,
	ps_type_onepoint,
	ps_type_sko_ex,
	ps_type_sko_all,

	PS_TYPE_MAX
} Tproof_type;

/*
  sed "s/.*{\"\(.*\)\", \".*\"},/\1/;/^$/d;s/\(.*\)/  type_\1 = proof_clause_types_get(\"\1\");/" afile.txt
  echo "  ps_type_none,"; sed "s/.*{\"\(.*\)\", \".*\"},/\1/;/^$/d;s/\(.*\)/  ps_type_\1,/" afile.txt
*/

/**
   @author Pascal Fontaine

   Table of description of proof unit types.
*/
typedef struct Tproof_type_desc
{
	char* name; /**< name of clause type: specifies how it is deduced */
	char* descr; /**< human readable description for documentation */
	int nb_reasons;
	unsigned nb_params;
} Tproof_type_desc;

extern Tproof_type_desc ps_type_desc[PS_TYPE_MAX + 2];

#endif
