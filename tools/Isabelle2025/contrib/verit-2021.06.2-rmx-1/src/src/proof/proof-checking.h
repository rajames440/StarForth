#ifndef __PROOF_CHECKING_H
#define __PROOF_CHECKING_H

#include "proof/proof-step.h"

extern void proof_error(char* str, Tproof_step proof_step);

extern void check_proof();

#endif
