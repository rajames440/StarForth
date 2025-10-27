/*
                                  ***   StarForth   ***

  PolynomialNormalizer.hpp- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:04.169-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/vampire-4.8/src/Kernel/PolynomialNormalizer.hpp
 */


#ifndef __POLYNOMIAL_NORMALIZER_HPP__
#define __POLYNOMIAL_NORMALIZER_HPP__

#include "Lib/Int.hpp"
#include "Forwards.hpp"

#include "Signature.hpp" 
#include "SortHelper.hpp"
#include "TermIterators.hpp"
#include "Term.hpp"
#include "Theory.hpp"
#include "NumTraits.hpp"
#include "Debug/Tracer.hpp"
#include "Lib/Coproduct.hpp"
#include <algorithm>
#include <utility>
#include <type_traits>
#include <functional>
#include "Lib/Hash.hpp"
#include "Lib/Environment.hpp"
#include "Lib/Option.hpp"
#include "Debug/Tracer.hpp"
#include "Kernel/Polynomial.hpp"
#include "Kernel/BottomUpEvaluation.hpp"
#include "Kernel/BottomUpEvaluation/TermList.hpp"
#include "Kernel/BottomUpEvaluation/PolyNf.hpp"
#include "Inferences/InferenceEngine.hpp"


namespace Kernel {

using LitSimplResult = Inferences::SimplifyingGeneratingLiteralSimplification::Result;

using NormalizationResult = Coproduct<PolyNf 
        , Polynom< IntTraits>
        , Polynom< RatTraits>
        , Polynom<RealTraits>
        , MonomFactors< IntTraits>
        , MonomFactors< RatTraits>
        , MonomFactors<RealTraits>
        >;

PolyNf normalizeTerm(TypedTermList t);

} // namespace Kernel

#endif // __POLYNOMIAL_NORMALIZER_HPP__
