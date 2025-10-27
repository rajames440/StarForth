/*
                                  ***   StarForth   ***

  PrimitiveInstantiation.hpp- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:04.232-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/vampire-4.8/src/Inferences/PrimitiveInstantiation.hpp
 */
/**
 * @file PrimitiveInstantiation.hpp
 * Defines class PrimitiveInstantiation.
 */


#ifndef __PrimitiveInstantiation__
#define __PrimitiveInstantiation__

#if VHOL

#include "Forwards.hpp"
#include "Indexing/TermIndex.hpp"

#include "InferenceEngine.hpp"

namespace Inferences {

using namespace Kernel;
using namespace Indexing;
using namespace Saturation;

class PrimitiveInstantiation
: public GeneratingInferenceEngine
{
public:
  CLASS_NAME(PrimitiveInstantiation);
  USE_ALLOCATOR(PrimitiveInstantiation);

  ClauseIterator generateClauses(Clause* premise);
  
private:

  struct ResultFn;  
  struct IsInstantiable;  
};


};

#endif

#endif /* __PrimitiveInstantiation__ */
