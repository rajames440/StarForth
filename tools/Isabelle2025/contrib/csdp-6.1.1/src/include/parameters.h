/*
                                  ***   StarForth   ***

  parameters.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:04.071-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/csdp-6.1.1/src/include/parameters.h
 */

/*

  This include file contains declarations for a number of parameters that 
  can affect the performance of CSDP.  You can adjust these parameters by 
 
    1. #include "parameters.h" in your code.
    2. Declare struct paramstruc params;
    3. Call init_params(params); to get default values.
    4. Change the value of the parameter that you're interested in.

  */

struct paramstruc {
  double axtol;
  double atytol;
  double objtol;
  double pinftol;
  double dinftol;
  int maxiter;
  double minstepfrac;
  double maxstepfrac;
  double minstepp;
  double minstepd;
  int usexzgap;
  int tweakgap;
  int affine;
  double perturbobj;
  int fastmode;
};






