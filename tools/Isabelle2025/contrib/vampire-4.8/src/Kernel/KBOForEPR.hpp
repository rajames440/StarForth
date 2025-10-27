/*
                                  ***   StarForth   ***

  KBOForEPR.hpp- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:03.595-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/vampire-4.8/src/Kernel/KBOForEPR.hpp
 */
/**
 * @file KBOForEPR.hpp
 * Defines class KBOForEPR for instances of the Knuth-Bendix ordering for EPR problems
 */

#ifndef __KBOForEPR__
#define __KBOForEPR__

#include "Forwards.hpp"

#include "KBO.hpp"

namespace Kernel {

using namespace Lib;

/**
 * Class for instances of the Knuth-Bendix orderings
 * @since 30/04/2008 flight Brussels-Tel Aviv
 */
class KBOForEPR
: public PrecedenceOrdering
{
public:
  CLASS_NAME(KBOForEPR);
  USE_ALLOCATOR(KBOForEPR);

  KBOForEPR(Problem& prb, const Options& opt);

  using PrecedenceOrdering::compare;
  Result compare(TermList tl1, TermList tl2) const override;
  void showConcrete(ostream&) const override;
protected:
  Result comparePredicates(Literal* l1, Literal* l2) const override;
};

}
#endif
