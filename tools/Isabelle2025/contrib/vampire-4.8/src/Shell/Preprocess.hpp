/*
                                  ***   StarForth   ***

  Preprocess.hpp- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:04.200-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/vampire-4.8/src/Shell/Preprocess.hpp
 */
/**
 * @file Shell/Preprocess.hpp
 * Defines class Preprocess implementing problem preprocessing.
 * @since 05/01/2004 Manchester
 */

#ifndef __Preprocess__
#define __Preprocess__

#include "Kernel/Unit.hpp"
#include "Forwards.hpp"

namespace Shell {

using namespace Kernel;

class Property;
class Options;

/**
 * Class implementing preprocessing-related procedures.
 * @author Andrei Voronkov
 * @since 16/04/2005 Manchester, made non-static
 * @since 02/07/2013 Manchester, _clausify added to support the preprocess mode
 */
class Preprocess
{
public:
  /** Initialise the preprocessor */
  explicit Preprocess(const Options& options)
  : _options(options),
    _clausify(true),_stillSimplify(false)
  {}
  void preprocess(Problem& prb);

  void preprocess1(Problem& prb);
  /** turn off clausification, can be used when only preprocessing without clausification is needed */
  void turnClausifierOff() {_clausify = false;}
  void keepSimplifyStep() {_stillSimplify = true; }
private:
  void preprocess2(Problem& prb);
  void naming(Problem& prb);
  Unit* preprocess3(Unit* u, bool appify = false/*higher order stuff*/);
  void preprocess3(Problem& prb);
  void clausify(Problem& prb);

#if VHOL
  void findAbstractions(UnitList*& units);
#endif
  
  void newCnf(Problem& prb);

  /** Options used in the normalisation */
  const Options& _options;
  /** If true, clausification is included in preprocessing */
  bool _clausify;
  bool _stillSimplify;
}; // class Preprocess


}

#endif
