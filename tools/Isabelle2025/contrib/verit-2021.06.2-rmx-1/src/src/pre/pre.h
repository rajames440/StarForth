/*
  --------------------------------------------------------------
  Symbolic Preprocessing for formulas
  --------------------------------------------------------------
*/
#include "proof/proof.h"
#include "symbolic/DAG.h"

void pre_init(void);
void pre_logic(char*);
void pre_done(void);

/**
   \brief Pre-process a formula
   \param `src` a formula
   \returns The pre-processed formula
   \remark Destructive */
TDAG pre_process(TDAG src);

/**
   \brief Pre-process a formula coming from instantation.
   \param `src` a formula
   \returns The pre-processed formula
   \remark Destructive

   Like `pre_rocess`, but benefit from the fact that some pre processing
   steps have already been applied on the formula generating instances
   Anyway, some preprocessing step in `pre_process` should never be applied
   on instances

   The reference counter of the result DAG is at least one. */
TDAG pre_process_instance(TDAG src);

/**
   \brief Pre-process a formula coming from instantation and generate a proof.
   \param src A formula
   \param Pproof A pointer to an Tproof.
   \returns The pre-processed formula
   \remark Destructive

   See `pre_process_instance`. */
TDAG pre_process_instance_proof(TDAG src, Tproof* Pproof);

/* PF like the previous but for array of formulas */
/**
   \brief Pre-process an array of formulas and generate proofs.
   \param n The number of formulas
   \param Psrc Pointer to an array of formulas
   \param Pproof An array of Tproof
   \remark Destructive */
void pre_process_array_proof(unsigned n, TDAG Psrc[n], Tproof Pproof[n]);

/**
   \brief Perform formula level pre-processing. Normalizes quantifiers and ite.
   \param src The formula to pre-process.
   \returns The preprocessed formula.
   \remark Destructive */
TDAG pre_quant_ite(TDAG src);
