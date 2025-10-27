/**
   \file proof.h

   \brief proof module.

   This module provides API functions to memorize the proofs done in
   veriT. */

#ifndef __PROOF_H
#define __PROOF_H

#include "bool/bool.h"
#include "proof/proof-lemma-hash.h"
#include "proof/proof-output.h"
#include "proof/proof-rules-tautologies.h"
#include "proof/proof-rules.h"
#include "proof/proof-sat-solver.h"
#include "proof/proof-step-table.h"
#include "proof/proof-subproof.h"
#include "proof/proof-unsat-core.h"
#include "symbolic/DAG.h"
#include "utils/stack.h"
#include "veriT-config.h"

#include <stdarg.h>
#include <stdio.h>

extern bool proof_on;

extern char* option_proof_filename;
extern bool option_proof_file_from_input;
extern bool option_proof_stat;

extern bool proof_no_replacement;

extern bool proof_with_sharing;
extern bool option_proof_prune;
extern bool option_proof_merge;

/**
   \addtogroup arguments_user

   - --proof-stats

   outputs statistics about proofs */
extern bool proof_stats;

/*
  --------------------------------------------------------------
  Init/Done
  --------------------------------------------------------------
*/

/**
   \brief module initialisation */
extern void proof_init(void);

/**
   \brief module release */
extern void proof_done(void);

/**
   \brief outputs proof documentation to file */
extern void proof_doc(FILE* file);

/**
   \brief notifies module of the input file name */
extern void proof_set_input_file(char* filename);

#ifdef PEDANTIC
void proof_done(void);
#endif

#endif /* __PROOF_H */
