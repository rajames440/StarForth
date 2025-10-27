/*
                                  ***   StarForth   ***

  Random.cpp- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:04.496-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/vampire-4.8/src/Lib/Random.cpp
 */
/**
 * @file Random.cpp
 * Implements random number generation.
 *
 * @since 20/02/2000 Manchester
 * modified Ioan Dragan
 */

#include "Random.hpp"

using namespace Lib;

unsigned Random::_seed = 1;
std::mt19937 Random::_eng(Random::_seed);
