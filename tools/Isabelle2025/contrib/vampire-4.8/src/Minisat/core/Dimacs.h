/*
                                  ***   StarForth   ***

  Dimacs.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:02.657-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/vampire-4.8/src/Minisat/core/Dimacs.h
 */

/**
 * @file Dimacs.h originally from Minisat.
 * (see https://github.com/niklasso/minisat, commit 37dc6c6)
 */

#ifndef Minisat_Dimacs_h
#define Minisat_Dimacs_h

#include <stdio.h>

#include "Minisat/utils/ParseUtils.h"
#include "Minisat/core/SolverTypes.h"

namespace Minisat {

//=================================================================================================
// DIMACS Parser:

template<class B, class Solver>
static void readClause(B& in, Solver& S, vec<Lit>& lits) {
    int     parsed_lit, var;
    lits.clear();
    for (;;){
        parsed_lit = parseInt(in);
        if (parsed_lit == 0) break;
        var = abs(parsed_lit)-1;
        while (var >= S.nVars()) S.newVar();
        lits.push( (parsed_lit > 0) ? mkLit(var) : ~mkLit(var) );
    }
}

template<class B, class Solver>
static void parse_DIMACS_main(B& in, Solver& S, bool strictp = false) {
    vec<Lit> lits;
    int vars    = 0;
    int clauses = 0;
    int cnt     = 0;
    for (;;){
        skipWhitespace(in);
        if (*in == EOF) break;
        else if (*in == 'p'){
            if (eagerMatch(in, "p cnf")){
                vars    = parseInt(in);
                clauses = parseInt(in);
                // SATRACE'06 hack
                // if (clauses > 4000000)
                //     S.eliminate(true);
            }else{
                printf("PARSE ERROR! Unexpected char: %c\n", *in), exit(3);
            }
        } else if (*in == 'c' || *in == 'p')
            skipLine(in);
        else{
            cnt++;
            readClause(in, S, lits);
            S.addClause_(lits); }
    }
    if (strictp && cnt != clauses)
        printf("PARSE ERROR! DIMACS header mismatch: wrong number of clauses\n");
    
    (void)vars;
}

// Inserts problem into solver.
//
template<class Solver>
static void parse_DIMACS(gzFile input_stream, Solver& S, bool strictp = false) {
    StreamBuffer in(input_stream);
    parse_DIMACS_main(in, S, strictp); }

//=================================================================================================
}

#endif
