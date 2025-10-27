#ifndef __PROOF_SUBPROOF_H
#define __PROOF_SUBPROOF_H

#include "proof/proof-step-table.h"

/**
   \brief retrieves from current subproof the transformed DAG in its last
   conclusion
   \return the modified DAG */
extern TDAG get_transformed_in_last_conclusion_subproof(void);

/**
   \remark stores subproof information in unused first step of new step pool */
extern void proof_subproof_begin(Tproof_type type);
extern void proof_subproof_begin_context(
	Tproof_type type, Tstack_DAG context, Tstack_proof reasons);
extern void proof_subproof_begin_sko(
	Tproof_type type, Tstack_DAG context, Tstack_proof reasons, TDAG src);

extern void proof_subproof_remove(void);

/**
   build clause from inputs into last conclusion */
extern Tproof proof_subproof_end_input(void);

/**
   build equivalence */
extern Tproof proof_subproof_end_transformation(TDAG src, TDAG dest);

#endif
