/*
                                  ***   StarForth   ***

  InnerRewriting.hpp- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:03.514-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/vampire-4.8/src/Inferences/InnerRewriting.hpp
 */
/**
 * @file InnerRewriting.hpp
 * Defines class InnerRewriting.
 */

#ifndef __InnerRewriting__
#define __InnerRewriting__

#include "Forwards.hpp"
#include "Shell/Options.hpp"

#include "InferenceEngine.hpp"

namespace Inferences
{

using namespace Kernel;
using namespace Saturation;

class InnerRewriting
: public ForwardSimplificationEngine
{
public:
  CLASS_NAME(InnerRewriting);
  USE_ALLOCATOR(InnerRewriting);
  
  bool perform(Clause* cl, Clause*& replacement, ClauseIterator& premises) override;
};

};

#endif // __InnerRewriting__
