/*
                                  ***   StarForth   ***

  MainLoop.cpp- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:03.761-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/vampire-4.8/src/Kernel/MainLoop.cpp
 */
/**
 * @file MainLoop.cpp
 * Implements class MainLoop.
 */


#include "Lib/Environment.hpp"
#include "Lib/SmartPtr.hpp"
#include "Lib/System.hpp"

#include "InstGen/IGAlgorithm.hpp"

#include "Saturation/SaturationAlgorithm.hpp"

#include "FMB/FiniteModelBuilder.hpp"

#include "SAT/Z3MainLoop.hpp"

#include "Shell/Options.hpp"
#include "Shell/UIHelper.hpp"

#include "Signature.hpp"
#include "Clause.hpp"
#include "Problem.hpp"

#include "MainLoop.hpp"

using namespace Kernel;
using namespace InstGen;
using namespace Saturation;
using namespace FMB;

void MainLoopResult::updateStatistics()
{
  CALL("MainLoopResult::updateStatistics");

  env.statistics->terminationReason = terminationReason;
  env.statistics->refutation = refutation;
  env.statistics->saturatedSet = saturatedSet;
  if(refutation) {
    env.statistics->maxInductionDepth = refutation->inference().inductionDepth();
  }
}

/**
 * Run the solving algorithm
 */
MainLoopResult MainLoop::run()
{
  CALL("MainLoop::run");
  TIME_TRACE("main loop");

  try {
    TIME_TRACE_EXPR("init", init());
    return TIME_TRACE_EXPR("run", runImpl());
  }
  catch(RefutationFoundException& rs)
  {
    return MainLoopResult(Statistics::REFUTATION, rs.refutation);
  }
  catch(TimeLimitExceededException&)
  {
    return MainLoopResult(Statistics::TIME_LIMIT);
  }
  catch(ActivationLimitExceededException&)
  {
    return MainLoopResult(Statistics::ACTIVATION_LIMIT);
  }
  catch(MainLoopFinishedException& e)
  {
    return e.result;
  }
}

/**
 * Return true iff clause @b c is refutation clause.
 *
 * Deriving a refutation clause means that the saturation algorithm can
 * terminate with success.
 */
bool MainLoop::isRefutation(Clause* cl)
{
  CALL("MainLoop::isRefutation");

  return cl->isEmpty() && cl->noSplits();
}

MainLoop* MainLoop::createFromOptions(Problem& prb, const Options& opt)
{
  CALL("MainLoop::createFromOptions");


#if VZ3
  bool isComplete = false; // artificially prevent smtForGround from running
  /*
  if(isComplete && opt.smtForGround() && prb.getProperty()->allNonTheoryClausesGround() 
                        && prb.getProperty()->hasInterpretedOperations()){
    return new SAT::Z3MainLoop(prb,opt);
  }
  */
#endif


  MainLoop* res;

  switch (opt.saturationAlgorithm()) {
  case Options::SaturationAlgorithm::INST_GEN:
    if(env.property->hasPolymorphicSym() 
#if VHOL
      || env.property->higherOrder()
#endif
      ){
      USER_ERROR("The inst gen calculus is currently not compatible with polymorphism or higher-order constructs");       
    }
    res = new IGAlgorithm(prb, opt);
    break;
  case Options::SaturationAlgorithm::FINITE_MODEL_BUILDING:
    if(env.property->hasPolymorphicSym() 
#if VHOL
      || env.property->higherOrder()
#endif
      ){
      USER_ERROR("Finite model buillding is currently not compatible with polymorphism or higher-order constructs");       
    }
    if(env.options->outputMode() == Shell::Options::Output::UCORE){
      USER_ERROR("Finite model building is not compatible with producing unsat cores");
    }
    //TODO should return inappropriate result instead of error
    res = new FiniteModelBuilder(prb,opt);
    break;
#if VZ3
  case Options::SaturationAlgorithm::Z3:
    if(!isComplete || !prb.getProperty()->allNonTheoryClausesGround()){
      reportSpiderStatus('u');
      USER_ERROR("Z3 saturation algorithm is only appropriate where preprocessing produces a ground problem"); 
      //TODO should return inappropriate result instead of error
    }
    res = new SAT::Z3MainLoop(prb,opt);
    break;
#endif
  default:
    res = SaturationAlgorithm::createFromOptions(prb, opt);
    break;
  }

  return res;
}

