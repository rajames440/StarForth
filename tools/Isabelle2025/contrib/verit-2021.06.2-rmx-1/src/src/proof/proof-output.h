#ifndef __PROOF_OUTPUT_H
#define __PROOF_OUTPUT_H

#include "proof/proof-print.h"
#include "proof/proof-step-table.h"
#include "proof/proof-step.h"

#include <stdarg.h>
#include <stdio.h>

/**
   \brief Prints a proof step (for outputting the proof and debugging purposes)
   \param proof_step the proof step
   \param steps the stack of all proof steps
   \param id the proof step id
   \param file the file to write to 
   \param with_sharing wether names should be introduced and used for shared 
          subterms */
extern void print_proof_step(
	Tproof_step proof_step, Tstack_proof_step steps, Tproof id, FILE* file,
	bool with_sharing);

/**
   \brief outputs proof to file */
extern void write_proof(FILE* file);

#endif
