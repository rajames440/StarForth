/*
                                  ***   StarForth   ***

  Grounder.hpp- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:03.183-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/vampire-4.8/src/Kernel/Grounder.hpp
 */
/**
 * @file Grounder.hpp
 * Defines class Grounder.
 */

#ifndef __Grounder__
#define __Grounder__

#include "Forwards.hpp"

#include "Lib/DHMap.hpp"
#include "Lib/ScopedPtr.hpp"
#include "Lib/SmartPtr.hpp"

#include "Kernel/Term.hpp"

namespace Kernel {

using namespace Lib;
using namespace SAT;

class Grounder {
public:
  CLASS_NAME(Grounder);
  USE_ALLOCATOR(Grounder);
  
  Grounder(SATSolver* satSolver) : _satSolver(satSolver) {}
  virtual ~Grounder() { CALL("Grounder::~Grounder"); }

  // TODO: sort out the intended semantics and the names of these four beasts:
  SATLiteral groundLiteral(Literal* lit);
  SATClause* ground(Clause* cl);
  SATClause* groundNonProp(Clause* cl, Literal** normLits=0);
  void groundNonProp(Clause* cl, SATLiteralStack& acc, Literal** normLits=0);

  LiteralIterator groundedLits();

protected:
  /**
   * Normalize literals before grounding.
   *
   * The order of literals in @c lits must be preserved.
   */
  virtual void normalize(unsigned cnt, Literal** lits) = 0;

private:
  SATLiteral groundNormalized(Literal*);

  /** Map from positive literals to SAT variable numbers */
  DHMap<Literal*, unsigned> _asgn;
  
  /** Pointer to a SATSolver instance for which the grounded clauses
   * are being prepared. Used to request new variables from the Solver.
   *
   * Also used to communicate source literals with IGGrounder. */
  SATSolver* _satSolver;
};

class GlobalSubsumptionGrounder : public Grounder {
  struct OrderNormalizingComparator;

  bool _doNormalization;
public:
  CLASS_NAME(GlobalSubsumptionGrounder);
  USE_ALLOCATOR(GlobalSubsumptionGrounder);

  GlobalSubsumptionGrounder(SATSolver* satSolver, bool doNormalization=true) 
          : Grounder(satSolver), _doNormalization(doNormalization) {}
protected:
  virtual void normalize(unsigned cnt, Literal** lits);
};

class IGGrounder : public Grounder {
public:
  CLASS_NAME(IGGrounder);
  USE_ALLOCATOR(IGGrounder);

  IGGrounder(SATSolver* satSolver);
private:
  TermList _tgtTerm;
protected:
  virtual void normalize(unsigned cnt, Literal** lits);
private:
  class CollapsingApplicator;
  Literal* collapseVars(Literal* lit);
};


}

#endif // __Grounder__
