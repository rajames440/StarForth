/*
                                  ***   StarForth   ***

  BackwardDemodulation.hpp- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:01.731-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/vampire-4.8/src/Inferences/BackwardDemodulation.hpp
 */
/**
 * @file SLQueryBackwardSubsumption.hpp
 * Defines class SLQueryBackwardSubsumption.
 */


#ifndef __BackwardDemodulation__
#define __BackwardDemodulation__

#include "Forwards.hpp"
#include "Indexing/TermIndex.hpp"

#include "InferenceEngine.hpp"

namespace Inferences {

using namespace Indexing;
using namespace Kernel;

template <class SubtermIt>
class BackwardDemodulation
: public BackwardSimplificationEngine
{
public:
  CLASS_NAME(BackwardDemodulation);
  USE_ALLOCATOR(BackwardDemodulation);

  void attach(SaturationAlgorithm* salg);
  void detach();

  void perform(Clause* premise, BwSimplificationRecordIterator& simplifications);
private:
  struct RemovedIsNonzeroFn;
  struct RewritableClausesFn;
  struct ResultFn;

  DemodulationSubtermIndex<SubtermIt>* _index;
};

};

#endif /* __BackwardDemodulation__ */
