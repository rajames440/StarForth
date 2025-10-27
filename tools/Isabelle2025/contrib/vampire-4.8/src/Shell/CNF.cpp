/*
                                  ***   StarForth   ***

  CNF.cpp- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:02.363-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/vampire-4.8/src/Shell/CNF.cpp
 */
/**
 * @file CNF.cpp
 * Implements class CNF implementing CNF transformation.
 * @since 19/01/2004 Manchester
 * @since 27/12/2007 Manchester, changed completely to a new implementation
 */

#include "Debug/Tracer.hpp"

#include "Kernel/Clause.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/Inference.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "CNF.hpp"

using namespace Kernel;
using namespace Shell;

/**
 * Initialise the CNF object.
 * @since 27/12/2007 Manchester
 */
CNF::CNF()
  : _literals(16),
    _formulas(16)
{
} // CNF::CNF

/**
 * Convert @b unit to CNF and push the resulting clauses on @b stack
 * @pre @b unit must be a formula unit
 * @since 27/12/2007 Manchester
 */
void CNF::clausify (Unit* unit,Stack<Clause*>& stack)
{
  CALL("CNF::clausify/2");
  ASS(! unit->isClause());

  _unit = static_cast<FormulaUnit*>(unit);
  _result = &stack;
  _literals.reset();
  _formulas.reset();

  Formula* f = _unit->formula();
  switch (f->connective()) {
  case TRUE:
    return;
  case FALSE:
    {
      // create an empty clause and push it in the stack
      Clause* clause = new(0) Clause(0,
				     FormulaTransformation(InferenceRule::CLAUSIFY,unit));
      stack.push(clause);
    }
    return;
  default:
    clausify(f);
  }
} // CNF::clausify()


/**
 * Clausify the formula f \/ F1 \/ ... \/ Fn \/ L1 \/ ... \/ Lm,
 * where [F1,...,Fn] is the content of _formulas and [L1,...,Lm]
 * is the content of _literals. After the clausification restore
 * the stacks _formulas and _literals to their state before the call.
 *
 * @since 27/12/2007 Manchester
 */
void CNF::clausify (Formula* f)
{
  CALL("CNF::clausify/1");

  switch (f->connective()) {
  case LITERAL:
    _literals.push(f->literal());
    if (_formulas.isEmpty()) {
      // collect the clause
      int length = _literals.length();
      Clause* clause = new(length) Clause(length,
          FormulaTransformation(InferenceRule::CLAUSIFY,_unit));
      for (int i = length-1;i >= 0;i--) {
	(*clause)[i] = _literals[i];
      }
      _result->push(clause);
    }
    else {
      f = _formulas.pop();
      clausify(f);
      _formulas.push(f);
    }
    _literals.pop();
    return;

  case AND:
    {
      FormulaList::Iterator fs(f->args());
      while (fs.hasNext()) {
	clausify(fs.next());
      }
    }
    return;

  case OR:
    {
      int ln = _formulas.length();

      FormulaList::Iterator fs(f->args());
      while (fs.hasNext()) {
	_formulas.push(fs.next());
      }
      clausify(_formulas.pop());
      _formulas.truncate(ln);
    }
    return;

  case FORALL:
    clausify(f->qarg());
    return;

  default:
    ASSERTION_VIOLATION;
  }
} // CNF::clausify

