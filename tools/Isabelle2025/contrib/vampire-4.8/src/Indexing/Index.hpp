/*
                                  ***   StarForth   ***

  Index.hpp- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:03.297-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/vampire-4.8/src/Indexing/Index.hpp
 */
/**
 * @file Indexing/Index.hpp
 * Defines abstract Index class and some other auxiliary classes.
 */

#ifndef __Indexing_Index__
#define __Indexing_Index__

#include "Forwards.hpp"
#include "Debug/Output.hpp"

#include "Lib/Event.hpp"
#include "Lib/Exception.hpp"
#include "Lib/VirtualIterator.hpp"
#include "Saturation/ClauseContainer.hpp"
#include "ResultSubstitution.hpp"
#include "Kernel/MismatchHandler.hpp"

#include "Lib/Allocator.hpp"

namespace Indexing
{
using namespace Kernel;
using namespace Lib;
using namespace Saturation;


/**
 * Class of objects which contain results of single literal queries.
 */
template<class Unifier>
struct LQueryRes
{
  LQueryRes() {}
  LQueryRes(Literal* l, Clause* c, Unifier unifier)
  : literal(l), clause(c), unifier(std::move(unifier)) {}


  Literal* literal;
  Clause* clause;
  Unifier unifier;

  struct ClauseExtractFn
  {
    Clause* operator()(const LQueryRes& res)
    {
      return res.clause;
    }
  };
};
template<class Unifier>
LQueryRes<Unifier> lQueryRes(Literal* l, Clause* c, Unifier unifier)
{ return LQueryRes<Unifier>(l,c,std::move(unifier)); }

/**
 * Class of objects which contain results of term queries.
 */
template<class Unifier>
struct TQueryRes
{
  TQueryRes() {}
  TQueryRes(TermList t, Literal* l, Clause* c, Unifier unifier)
  : term(t), literal(l), clause(c), unifier(std::move(unifier)) {}

  TermList term;
  Literal* literal;
  Clause* clause;

  Unifier unifier;

  friend std::ostream& operator<<(std::ostream& out, TQueryRes const& self)
  { 
    return out 
      << "{ term: " << self.term 
      << ", literal: " << outputPtr(self.literal)
      << ", clause: " << outputPtr(self.literal)
      << ", unifier: " << self.unifier
      << "}";
  }
};

template<class Unifier>
TQueryRes<Unifier> tQueryRes(TermList t, Literal* l, Clause* c, Unifier unifier) 
{ return TQueryRes<Unifier>(t,l,c,std::move(unifier)); }

struct ClauseSResQueryResult
{
  ClauseSResQueryResult() {}
  ClauseSResQueryResult(Clause* c)
  : clause(c), resolved(false) {}
  ClauseSResQueryResult(Clause* c, unsigned rqlIndex)
  : clause(c), resolved(true), resolvedQueryLiteralIndex(rqlIndex) {}
  
  Clause* clause;
  bool resolved;
  unsigned resolvedQueryLiteralIndex;
};

struct FormulaQueryResult
{
  FormulaQueryResult() {}
  FormulaQueryResult(FormulaUnit* unit, Formula* f, ResultSubstitutionSP s=ResultSubstitutionSP())
  : unit(unit), formula(f), substitution(s) {}

  FormulaUnit* unit;
  Formula* formula;
  ResultSubstitutionSP substitution;
};

using TermQueryResult = TQueryRes<ResultSubstitutionSP>;
using SLQueryResult   = LQueryRes<ResultSubstitutionSP>;

using TermQueryResultIterator = VirtualIterator<TermQueryResult>;
using SLQueryResultIterator = VirtualIterator<SLQueryResult>;
typedef VirtualIterator<ClauseSResQueryResult> ClauseSResResultIterator;
typedef VirtualIterator<FormulaQueryResult> FormulaQueryResultIterator;

class Index
{
public:
  CLASS_NAME(Index);
  USE_ALLOCATOR(Index);

  virtual ~Index();

  void attachContainer(ClauseContainer* cc);
protected:
  Index() {}

  void onAddedToContainer(Clause* c)
  { handleClause(c, true); }
  void onRemovedFromContainer(Clause* c)
  { handleClause(c, false); }

  virtual void handleClause(Clause* c, bool adding) {}

  //TODO: postponing index modifications during iteration (methods isBeingIterated() etc...)

private:
  SubscriptionData _addedSD;
  SubscriptionData _removedSD;
};



class ClauseSubsumptionIndex
: public Index
{
public:
  CLASS_NAME(ClauseSubsumptionIndex);
  USE_ALLOCATOR(ClauseSubsumptionIndex);

  virtual ClauseSResResultIterator getSubsumingOrSResolvingClauses(Clause* c, 
    bool subsumptionResolution)
  { NOT_IMPLEMENTED; };
};

};
#endif /*__Indexing_Index__*/
