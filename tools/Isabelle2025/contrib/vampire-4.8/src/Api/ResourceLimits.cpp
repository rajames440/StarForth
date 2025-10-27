/*
                                  ***   StarForth   ***

  ResourceLimits.cpp- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:04.548-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/vampire-4.8/src/Api/ResourceLimits.cpp
 */
/**
 * @file ResourceLimits.cpp
 * Implements class ResourceLimits.
 */

#include "ResourceLimits.hpp"

#include "Debug/Assertion.hpp"

#include "Lib/Allocator.hpp"
#include "Lib/Environment.hpp"
#include "Lib/System.hpp"

#include "Shell/Options.hpp"

namespace Api
{

using namespace Lib;

//#ifdef VAPI_LIBRARY

//here we ensure that if Vampire is used as a library, we do not impose
//any time or memory limit by default

struct __InitHelper
{
  static void init()
  {
    ResourceLimits::disableLimits();

    env.options->setOutputAxiomNames(true);
  }

  __InitHelper()
  {
    System::addInitializationHandler(init,0);
  }
};

__InitHelper initializerAuxObject;

//#endif

void ResourceLimits::setLimits(size_t memoryInBytes, int timeInDeciseconds)
{
  CALL("ResourceLimits::setLimits");

  env.options->setMemoryLimitOptionValue(memoryInBytes);
  Allocator::setMemoryLimit(memoryInBytes);

  env.options->setTimeLimitInDeciseconds(timeInDeciseconds);
}

}
