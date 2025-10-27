/**
   \file proof_id.h
   \brief proof id definition
   This module provides API functions to memorize the proofs done in
   veriT */
#ifndef __PROOF_ID_H
#define __PROOF_ID_H

#include "symbolic/DAG.h"
#include "symbolic/veriT-status.h"
#include "utils/stack.h"
#include "veriT-config.h"

typedef unsigned Tproof;

TSstack(_proof, Tproof); /* typedefs Tstack_proof */
TSstack(_proof_stack, Tstack_proof); /* typedefs Tstack_proof_stack */

/** \brief associates a DAG to its proof */
typedef struct TDAG_proof
{
	TDAG DAG;
	Tstack_proof proof;
} TDAG_proof;

TSstack(_DAG_proof, TDAG_proof); /* typedefs Tstack_DAG_proof */

extern Tstatus proof_status;
extern Tproof empty_clause;

#endif /* __PROOF_ID_H */
